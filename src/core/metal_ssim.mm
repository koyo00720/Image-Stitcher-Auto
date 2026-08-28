#include "metal_ssim.h"

#include <QCoreApplication>

#include <opencv2/imgproc.hpp>

#import <Metal/Metal.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>

namespace
{
constexpr uint32_t kLocalSize = 128;
constexpr quint64 kVramSafetyPercent = 70;

struct MetalJob {
    uint32_t offset1;
    uint32_t offset2;
    uint32_t stride1;
    uint32_t stride2;
    uint32_t roiX1;
    uint32_t roiY1;
    uint32_t roiX2;
    uint32_t roiY2;
    uint32_t width;
    uint32_t height;
    uint32_t startPixel;
    uint32_t pixelCount;
};
static_assert(sizeof(MetalJob) == 48, "MetalJob must match the Metal shader layout");

const char* kMetalSsimSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Job {
    uint offset1;
    uint offset2;
    uint stride1;
    uint stride2;
    uint roiX1;
    uint roiY1;
    uint roiX2;
    uint roiY2;
    uint width;
    uint height;
    uint startPixel;
    uint pixelCount;
};

constant float gaussianWeights[11] = {
    0.0010283801f,
    0.0075987581f,
    0.0360007721f,
    0.1093606895f,
    0.2130055377f,
    0.2660117249f,
    0.2130055377f,
    0.1093606895f,
    0.0360007721f,
    0.0075987581f,
    0.0010283801f
};

int reflect101(int coordinate, int length)
{
    if (length <= 1) {
        return 0;
    }
    while (coordinate < 0 || coordinate >= length) {
        if (coordinate < 0) {
            coordinate = -coordinate;
        } else {
            coordinate = 2 * length - coordinate - 2;
        }
    }
    return coordinate;
}

kernel void ssimBatch(
    device const float* pixels [[buffer(0)]],
    device const Job* jobs [[buffer(1)]],
    device float* partialSums [[buffer(2)]],
    constant uint& jobCount [[buffer(3)]],
    uint3 groupPosition [[threadgroup_position_in_grid]],
    uint localIndex [[thread_index_in_threadgroup]])
{
    const uint groupIndex = groupPosition.x;
    if (groupIndex >= jobCount) {
        return;
    }

    const Job job = jobs[groupIndex];
    const uint pixelIndex = job.startPixel + localIndex;
    float value = 0.0f;

    if (pixelIndex < job.pixelCount) {
        const int localX = int(pixelIndex % job.width);
        const int localY = int(pixelIndex / job.width);

        float mean1 = 0.0f;
        float mean2 = 0.0f;
        float squareMean1 = 0.0f;
        float squareMean2 = 0.0f;
        float productMean = 0.0f;

        for (int ky = -5; ky <= 5; ++ky) {
            const int sampleY = reflect101(localY + ky, int(job.height));
            const float wy = gaussianWeights[ky + 5];
            const uint row1 = job.offset1
                              + (job.roiY1 + uint(sampleY)) * job.stride1;
            const uint row2 = job.offset2
                              + (job.roiY2 + uint(sampleY)) * job.stride2;

            for (int kx = -5; kx <= 5; ++kx) {
                const int sampleX = reflect101(localX + kx, int(job.width));
                const float weight = wy * gaussianWeights[kx + 5];
                const float first = pixels[row1 + job.roiX1 + uint(sampleX)];
                const float second = pixels[row2 + job.roiX2 + uint(sampleX)];

                mean1 += weight * first;
                mean2 += weight * second;
                squareMean1 += weight * first * first;
                squareMean2 += weight * second * second;
                productMean += weight * first * second;
            }
        }

        const float variance1 = squareMean1 - mean1 * mean1;
        const float variance2 = squareMean2 - mean2 * mean2;
        const float covariance = productMean - mean1 * mean2;
        const float c1 = 6.5025f;
        const float c2 = 58.5225f;
        const float numerator = (2.0f * mean1 * mean2 + c1)
                                * (2.0f * covariance + c2);
        const float denominator = (mean1 * mean1 + mean2 * mean2 + c1)
                                  * (variance1 + variance2 + c2);
        value = numerator / denominator;
    }

    threadgroup float reductionValues[128];
    reductionValues[localIndex] = value;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    for (uint stride = 64; stride > 0; stride >>= 1) {
        if (localIndex < stride) {
            reductionValues[localIndex] += reductionValues[localIndex + stride];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    if (localIndex == 0) {
        partialSums[groupIndex] = reductionValues[0];
    }
}
)METAL";

QString errorText(const QString& operation, NSError* error)
{
    const QString detail = error ? QString::fromUtf8(error.localizedDescription.UTF8String)
                                 : QString();
    return detail.isEmpty()
               ? QCoreApplication::translate("MetalSsimEngine", "%1に失敗しました。")
                     .arg(operation)
               : QCoreApplication::translate("MetalSsimEngine", "%1に失敗しました: %2")
                     .arg(operation, detail);
}

cv::Mat toGrayFloat(const cv::Mat& input)
{
    cv::Mat gray;
    if (input.channels() == 4) {
        cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
    } else if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else if (input.channels() == 1) {
        gray = input;
    } else {
        return {};
    }

    cv::Mat grayFloat;
    gray.convertTo(grayFloat, CV_32F);
    return grayFloat.isContinuous() ? grayFloat : grayFloat.clone();
}

struct MetalContext {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLComputePipelineState> pipeline = nil;
};

std::mutex gExecutionMutex;
std::unique_ptr<MetalContext> gContext;
QString gContextError;

struct DefaultMetalDeviceInfo {
    bool available = false;
    QString name;
};

const DefaultMetalDeviceInfo& defaultMetalDeviceInfo()
{
    static const DefaultMetalDeviceInfo info = []() {
        DefaultMetalDeviceInfo detected;
        @autoreleasepool {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            detected.available = device != nil;
            if (device) {
                detected.name = QString::fromUtf8(device.name.UTF8String);
            }
        }
        return detected;
    }();
    return info;
}

bool buildContext(std::unique_ptr<MetalContext>& output, QString& error)
{
    auto context = std::make_unique<MetalContext>();
    context->device = MTLCreateSystemDefaultDevice();
    if (!context->device) {
        error = QCoreApplication::translate(
            "MetalSsimEngine", "計算に利用できるMetal GPUが見つかりません。");
        return false;
    }

    NSError* compileError = nil;
    NSString* source = [NSString stringWithUTF8String:kMetalSsimSource];
    id<MTLLibrary> library = [context->device newLibraryWithSource:source
                                                           options:nil
                                                             error:&compileError];
    if (!library) {
        error = errorText(QCoreApplication::translate(
                              "MetalSsimEngine", "Metal shaderのコンパイル"),
                          compileError);
        return false;
    }

    id<MTLFunction> function = [library newFunctionWithName:@"ssimBatch"];
    if (!function) {
        error = QCoreApplication::translate(
            "MetalSsimEngine", "Metal SSIM kernelが見つかりません。");
        return false;
    }

    NSError* pipelineError = nil;
    context->pipeline = [context->device newComputePipelineStateWithFunction:function
                                                                        error:&pipelineError];
    if (!context->pipeline) {
        error = errorText(QCoreApplication::translate(
                              "MetalSsimEngine", "Metal compute pipelineの作成"),
                          pipelineError);
        return false;
    }
    if (context->pipeline.maxTotalThreadsPerThreadgroup < kLocalSize) {
        error = QCoreApplication::translate(
            "MetalSsimEngine", "選択GPUは必要なMetal threadgroupサイズに対応していません。");
        return false;
    }

    context->queue = [context->device newCommandQueue];
    if (!context->queue) {
        error = QCoreApplication::translate(
            "MetalSsimEngine", "Metal command queueを作成できませんでした。");
        return false;
    }

    output = std::move(context);
    return true;
}

MetalSsimBatchResult computeBatchLocked(
    const cv::Mat& firstBgra,
    const cv::Mat& secondBgra,
    const std::vector<VulkanSsimRoiPair>& rois,
    bool ignoreVramLimit)
{
    MetalSsimBatchResult output;
    output.scores.assign(rois.size(), 0.0);
    if (rois.empty()) {
        output.status = MetalSsimStatus::Success;
        return output;
    }

    if (!gContext && gContextError.isEmpty()) {
        buildContext(gContext, gContextError);
    }
    if (!gContext) {
        output.status = MetalSsimStatus::NoDevice;
        output.message = gContextError;
        return output;
    }
    MetalContext& context = *gContext;

    const cv::Mat firstGray = toGrayFloat(firstBgra);
    const cv::Mat secondGray = toGrayFloat(secondBgra);
    if (firstGray.empty() || secondGray.empty()) {
        output.status = MetalSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "Metal SSIMで扱えない画像形式です。");
        return output;
    }

    const quint64 firstPixels = firstGray.total();
    const quint64 secondPixels = secondGray.total();
    const quint64 totalPixels = firstPixels + secondPixels;
    if (totalPixels > std::numeric_limits<uint32_t>::max()) {
        output.status = MetalSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "画像がMetal SSIMの32-bit index上限を超えています。");
        return output;
    }

    std::vector<float> pixels(static_cast<size_t>(totalPixels));
    std::memcpy(pixels.data(), firstGray.ptr<float>(),
                static_cast<size_t>(firstPixels) * sizeof(float));
    std::memcpy(pixels.data() + firstPixels, secondGray.ptr<float>(),
                static_cast<size_t>(secondPixels) * sizeof(float));

    std::vector<MetalJob> jobs;
    std::vector<size_t> taskGroupBegin(rois.size(), 0);
    std::vector<size_t> taskGroupCount(rois.size(), 0);
    std::vector<uint32_t> taskPixelCount(rois.size(), 0);

    for (size_t task = 0; task < rois.size(); ++task) {
        const cv::Rect& first = rois[task].first;
        const cv::Rect& second = rois[task].second;
        if (first.empty() || second.empty() || first.size() != second.size()
            || first.x < 0 || first.y < 0
            || first.x + first.width > firstGray.cols
            || first.y + first.height > firstGray.rows
            || second.x < 0 || second.y < 0
            || second.x + second.width > secondGray.cols
            || second.y + second.height > secondGray.rows) {
            continue;
        }

        const quint64 pixelCount64 = static_cast<quint64>(first.width) * first.height;
        if (pixelCount64 == 0 || pixelCount64 > std::numeric_limits<uint32_t>::max()) {
            continue;
        }
        const uint32_t pixelCount = static_cast<uint32_t>(pixelCount64);
        const size_t groupCount = (pixelCount + kLocalSize - 1) / kLocalSize;
        taskGroupBegin[task] = jobs.size();
        taskGroupCount[task] = groupCount;
        taskPixelCount[task] = pixelCount;

        for (size_t group = 0; group < groupCount; ++group) {
            MetalJob job{};
            job.offset1 = 0;
            job.offset2 = static_cast<uint32_t>(firstPixels);
            job.stride1 = static_cast<uint32_t>(firstGray.cols);
            job.stride2 = static_cast<uint32_t>(secondGray.cols);
            job.roiX1 = static_cast<uint32_t>(first.x);
            job.roiY1 = static_cast<uint32_t>(first.y);
            job.roiX2 = static_cast<uint32_t>(second.x);
            job.roiY2 = static_cast<uint32_t>(second.y);
            job.width = static_cast<uint32_t>(first.width);
            job.height = static_cast<uint32_t>(first.height);
            job.startPixel = static_cast<uint32_t>(group * kLocalSize);
            job.pixelCount = pixelCount;
            jobs.push_back(job);
        }
    }

    if (jobs.empty()) {
        output.status = MetalSsimStatus::Success;
        return output;
    }
    if (jobs.size() > std::numeric_limits<uint32_t>::max()) {
        output.status = MetalSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "Metal SSIMのthreadgroup数上限を超えています。");
        return output;
    }

    const quint64 pixelBytes = static_cast<quint64>(pixels.size() * sizeof(float));
    const quint64 jobBytes = static_cast<quint64>(jobs.size() * sizeof(MetalJob));
    const quint64 resultBytes = static_cast<quint64>(jobs.size() * sizeof(float));
    const quint64 maxBufferLength = static_cast<quint64>(context.device.maxBufferLength);
    if (pixelBytes > maxBufferLength || jobBytes > maxBufferLength
        || resultBytes > maxBufferLength) {
        output.status = MetalSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "必要なbufferが選択GPUの上限を超えています。");
        return output;
    }

    output.requiredVramBytes = pixelBytes + jobBytes + resultBytes;
    const quint64 recommendedWorkingSet =
        static_cast<quint64>(context.device.recommendedMaxWorkingSetSize);
    output.vramLimitBytes = recommendedWorkingSet * kVramSafetyPercent / 100;
    if (!ignoreVramLimit
        && (output.vramLimitBytes == 0
            || output.requiredVramBytes > output.vramLimitBytes)) {
        output.status = MetalSsimStatus::VramLimitExceeded;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "Metal計算のメモリ見積もりが安全上限を超えました。");
        return output;
    }

    id<MTLBuffer> pixelBuffer =
        [context.device newBufferWithBytes:pixels.data()
                                    length:static_cast<NSUInteger>(pixelBytes)
                                   options:MTLResourceStorageModeShared];
    id<MTLBuffer> jobBuffer =
        [context.device newBufferWithBytes:jobs.data()
                                    length:static_cast<NSUInteger>(jobBytes)
                                   options:MTLResourceStorageModeShared];
    id<MTLBuffer> resultBuffer =
        [context.device newBufferWithLength:static_cast<NSUInteger>(resultBytes)
                                     options:MTLResourceStorageModeShared];
    if (!pixelBuffer || !jobBuffer || !resultBuffer) {
        output.status = MetalSsimStatus::Failed;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "Metal bufferを確保できませんでした。");
        return output;
    }

    id<MTLCommandBuffer> commandBuffer = [context.queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    if (!commandBuffer || !encoder) {
        output.status = MetalSsimStatus::Failed;
        output.message = QCoreApplication::translate(
            "MetalSsimEngine", "Metal commandを作成できませんでした。");
        return output;
    }

    [encoder setComputePipelineState:context.pipeline];
    [encoder setBuffer:pixelBuffer offset:0 atIndex:0];
    [encoder setBuffer:jobBuffer offset:0 atIndex:1];
    [encoder setBuffer:resultBuffer offset:0 atIndex:2];
    const uint32_t jobCount = static_cast<uint32_t>(jobs.size());
    [encoder setBytes:&jobCount length:sizeof(jobCount) atIndex:3];
    [encoder dispatchThreadgroups:MTLSizeMake(jobs.size(), 1, 1)
              threadsPerThreadgroup:MTLSizeMake(kLocalSize, 1, 1)];
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
        output.status = MetalSsimStatus::Failed;
        output.message = errorText(QCoreApplication::translate(
                                       "MetalSsimEngine", "Metal SSIM commandの実行"),
                                   commandBuffer.error);
        return output;
    }

    std::vector<float> partialSums(jobs.size());
    std::memcpy(partialSums.data(), resultBuffer.contents,
                static_cast<size_t>(resultBytes));
    for (size_t task = 0; task < rois.size(); ++task) {
        if (taskGroupCount[task] == 0 || taskPixelCount[task] == 0) {
            continue;
        }
        const size_t begin = taskGroupBegin[task];
        const size_t end = begin + taskGroupCount[task];
        const double sum = std::accumulate(
            partialSums.begin() + static_cast<ptrdiff_t>(begin),
            partialSums.begin() + static_cast<ptrdiff_t>(end), 0.0);
        output.scores[task] = sum / static_cast<double>(taskPixelCount[task]);
    }

    output.status = MetalSsimStatus::Success;
    return output;
}
} // namespace

bool MetalSsimEngine::isBuilt()
{
    return true;
}

bool MetalSsimEngine::isAvailable()
{
    return defaultMetalDeviceInfo().available;
}

QString MetalSsimEngine::deviceName()
{
    return defaultMetalDeviceInfo().name;
}

MetalSsimBatchResult MetalSsimEngine::computeBatch(
    const cv::Mat& firstBgra,
    const cv::Mat& secondBgra,
    const std::vector<VulkanSsimRoiPair>& rois,
    bool ignoreVramLimit)
{
    @autoreleasepool {
        std::lock_guard<std::mutex> lock(gExecutionMutex);
        return computeBatchLocked(firstBgra, secondBgra, rois, ignoreVramLimit);
    }
}
