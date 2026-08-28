#include "vulkan_ssim.h"

#include "vulkan_ssim_shader.h"

#include <QByteArray>
#include <QCoreApplication>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>

#ifdef IMAGE_STITCHER_HAS_VULKAN
#include <vulkan/vulkan.h>

namespace
{
constexpr uint32_t kLocalSize = 128;
constexpr quint64 kVramSafetyPercent = 70;

QString vkError(const QString& operation, VkResult result)
{
    return QCoreApplication::translate(
               "VulkanSsimEngine", "%1に失敗しました (VkResult=%2)。")
        .arg(operation)
        .arg(static_cast<int>(result));
}

QString deviceTypeName(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual";
    default:
        return "Other";
    }
}

quint64 localMemoryBytes(const VkPhysicalDeviceMemoryProperties& memoryProperties)
{
    quint64 total = 0;
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
        if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            total += memoryProperties.memoryHeaps[i].size;
        }
    }
    return total;
}

QString makeDeviceKey(uint32_t index, const VkPhysicalDeviceProperties& properties)
{
    return QString("%1:%2:%3:%4")
        .arg(index)
        .arg(properties.vendorID, 8, 16, QLatin1Char('0'))
        .arg(properties.deviceID, 8, 16, QLatin1Char('0'))
        .arg(QString::fromUtf8(properties.deviceName));
}

bool createInstance(VkInstance& instance, QString& error)
{
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Image Stitcher Auto";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 1);
    applicationInfo.pEngineName = "Image Stitcher Vulkan SSIM";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkanインスタンスの作成"), result);
        return false;
    }
    return true;
}

int findComputeQueueFamily(VkPhysicalDevice device)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    if (count == 0) {
        return -1;
    }

    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

    int fallback = -1;
    for (uint32_t i = 0; i < count; ++i) {
        if (!(properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) || properties[i].queueCount == 0) {
            continue;
        }
        if (!(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            return static_cast<int>(i);
        }
        if (fallback < 0) {
            fallback = static_cast<int>(i);
        }
    }
    return fallback;
}

struct DeviceCandidate {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    int queueFamily = -1;
    VulkanGpuInfo info;
};

std::vector<DeviceCandidate> enumerateCandidates(VkInstance instance, QString& error)
{
    uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan GPUの列挙"), result);
        return {};
    }
    if (count == 0) {
        return {};
    }

    std::vector<VkPhysicalDevice> devices(count);
    result = vkEnumeratePhysicalDevices(instance, &count, devices.data());
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan GPUの列挙"), result);
        return {};
    }

    std::vector<DeviceCandidate> candidates;
    candidates.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        DeviceCandidate candidate;
        candidate.device = devices[i];
        vkGetPhysicalDeviceProperties(candidate.device, &candidate.properties);
        vkGetPhysicalDeviceMemoryProperties(candidate.device, &candidate.memoryProperties);
        candidate.queueFamily = findComputeQueueFamily(candidate.device);

        if (candidate.queueFamily < 0
            || candidate.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU
            || candidate.properties.limits.maxComputeWorkGroupInvocations < kLocalSize
            || candidate.properties.limits.maxComputeWorkGroupSize[0] < kLocalSize) {
            continue;
        }

        candidate.info.key = makeDeviceKey(i, candidate.properties);
        candidate.info.name = QString::fromUtf8(candidate.properties.deviceName);
        candidate.info.deviceType = deviceTypeName(candidate.properties.deviceType);
        candidate.info.localMemoryBytes = localMemoryBytes(candidate.memoryProperties);
        candidates.push_back(candidate);
    }
    return candidates;
}

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    QString deviceKey;

    ~VulkanContext()
    {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
            if (commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, commandPool, nullptr);
            }
            if (descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            }
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pipeline, nullptr);
            }
            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            }
            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            }
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }
};

bool buildContext(const QString& preferredDeviceKey,
                  std::unique_ptr<VulkanContext>& context,
                  QString& error)
{
    auto candidateContext = std::make_unique<VulkanContext>();
    if (!createInstance(candidateContext->instance, error)) {
        return false;
    }

    const std::vector<DeviceCandidate> candidates =
        enumerateCandidates(candidateContext->instance, error);
    if (!error.isEmpty()) {
        return false;
    }
    if (candidates.empty()) {
        error = QCoreApplication::translate(
            "VulkanSsimEngine", "計算に利用できるVulkan GPUが見つかりません。");
        return false;
    }

    const DeviceCandidate* selected = &candidates.front();
    if (!preferredDeviceKey.isEmpty()) {
        const auto it = std::find_if(candidates.begin(), candidates.end(),
                                     [&](const DeviceCandidate& candidate) {
            return candidate.info.key == preferredDeviceKey;
        });
        if (it != candidates.end()) {
            selected = &*it;
        }
    }

    candidateContext->physicalDevice = selected->device;
    candidateContext->properties = selected->properties;
    candidateContext->memoryProperties = selected->memoryProperties;
    candidateContext->queueFamily = static_cast<uint32_t>(selected->queueFamily);
    candidateContext->deviceKey = selected->info.key;

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = candidateContext->queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    VkResult result = vkCreateDevice(candidateContext->physicalDevice,
                                     &deviceInfo, nullptr, &candidateContext->device);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan論理デバイスの作成"), result);
        return false;
    }
    vkGetDeviceQueue(candidateContext->device, candidateContext->queueFamily,
                     0, &candidateContext->queue);

    VkDescriptorSetLayoutBinding bindings[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 3;
    descriptorLayoutInfo.pBindings = bindings;
    result = vkCreateDescriptorSetLayout(candidateContext->device,
                                         &descriptorLayoutInfo, nullptr,
                                         &candidateContext->descriptorSetLayout);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan descriptor layoutの作成"), result);
        return false;
    }

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &candidateContext->descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    result = vkCreatePipelineLayout(candidateContext->device, &pipelineLayoutInfo,
                                    nullptr, &candidateContext->pipelineLayout);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan pipeline layoutの作成"), result);
        return false;
    }

    const QByteArray shaderBytes = QByteArray::fromBase64(kVulkanSsimShaderBase64);
    if (shaderBytes.isEmpty() || shaderBytes.size() % 4 != 0) {
        error = QCoreApplication::translate(
            "VulkanSsimEngine", "組み込みVulkanシェーダーが不正です。");
        return false;
    }
    std::vector<uint32_t> shaderCode(static_cast<size_t>(shaderBytes.size()) / 4);
    std::memcpy(shaderCode.data(), shaderBytes.constData(), static_cast<size_t>(shaderBytes.size()));

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = static_cast<size_t>(shaderBytes.size());
    shaderInfo.pCode = shaderCode.data();
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    result = vkCreateShaderModule(candidateContext->device, &shaderInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan shader moduleの作成"), result);
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = candidateContext->pipelineLayout;
    result = vkCreateComputePipelines(candidateContext->device, VK_NULL_HANDLE, 1,
                                      &pipelineInfo, nullptr, &candidateContext->pipeline);
    vkDestroyShaderModule(candidateContext->device, shaderModule, nullptr);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan compute pipelineの作成"), result);
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 3;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    result = vkCreateDescriptorPool(candidateContext->device, &poolInfo, nullptr,
                                    &candidateContext->descriptorPool);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan descriptor poolの作成"), result);
        return false;
    }

    VkDescriptorSetAllocateInfo setInfo{};
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.descriptorPool = candidateContext->descriptorPool;
    setInfo.descriptorSetCount = 1;
    setInfo.pSetLayouts = &candidateContext->descriptorSetLayout;
    result = vkAllocateDescriptorSets(candidateContext->device, &setInfo,
                                      &candidateContext->descriptorSet);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan descriptor setの割り当て"), result);
        return false;
    }

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                            | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = candidateContext->queueFamily;
    result = vkCreateCommandPool(candidateContext->device, &commandPoolInfo,
                                 nullptr, &candidateContext->commandPool);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan command poolの作成"), result);
        return false;
    }

    context = std::move(candidateContext);
    return true;
}

struct GpuJob {
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
static_assert(sizeof(GpuJob) == 48, "GpuJob must match the std430 shader layout");

struct BufferAllocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkMemoryRequirements requirements{};
    uint32_t memoryType = std::numeric_limits<uint32_t>::max();
    VkDeviceSize logicalSize = 0;
};

void destroyBuffer(VkDevice device, BufferAllocation& buffer)
{
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
    }
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer.memory, nullptr);
    }
    buffer = {};
}

int findMemoryType(const VkPhysicalDeviceMemoryProperties& memoryProperties,
                   uint32_t allowedBits,
                   VkMemoryPropertyFlags requiredFlags)
{
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((allowedBits & (1u << i))
            && (memoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool prepareBuffer(VulkanContext& context,
                   VkDeviceSize size,
                   VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags memoryFlags,
                   BufferAllocation& output,
                   QString& error)
{
    output.logicalSize = size;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(context.device, &bufferInfo, nullptr, &output.buffer);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan bufferの作成"), result);
        return false;
    }

    vkGetBufferMemoryRequirements(context.device, output.buffer, &output.requirements);
    const int memoryType = findMemoryType(context.memoryProperties,
                                          output.requirements.memoryTypeBits,
                                          memoryFlags);
    if (memoryType < 0) {
        error = QCoreApplication::translate(
            "VulkanSsimEngine", "必要なVulkanメモリ形式が見つかりません。");
        return false;
    }
    output.memoryType = static_cast<uint32_t>(memoryType);
    return true;
}

bool allocateBuffer(VulkanContext& context, BufferAllocation& buffer, QString& error)
{
    VkMemoryAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocationInfo.allocationSize = buffer.requirements.size;
    allocationInfo.memoryTypeIndex = buffer.memoryType;
    VkResult result = vkAllocateMemory(context.device, &allocationInfo, nullptr, &buffer.memory);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan memoryの割り当て"), result);
        return false;
    }
    result = vkBindBufferMemory(context.device, buffer.buffer, buffer.memory, 0);
    if (result != VK_SUCCESS) {
        error = vkError(QCoreApplication::translate(
                            "VulkanSsimEngine", "Vulkan buffer memoryの関連付け"), result);
        return false;
    }
    return true;
}

bool memoryTypeUsesLocalHeap(const VulkanContext& context, uint32_t memoryType)
{
    const uint32_t heapIndex = context.memoryProperties.memoryTypes[memoryType].heapIndex;
    return context.memoryProperties.memoryHeaps[heapIndex].flags
           & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
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

std::mutex gExecutionMutex;
std::unique_ptr<VulkanContext> gContext;
QString gFailedDeviceKey;
QString gContextError;

VulkanSsimBatchResult computeBatchLocked(
    const cv::Mat& firstBgra,
    const cv::Mat& secondBgra,
    const std::vector<VulkanSsimRoiPair>& rois,
    const QString& preferredDeviceKey,
    bool ignoreVramLimit)
{
    VulkanSsimBatchResult output;
    output.scores.assign(rois.size(), 0.0);
    if (rois.empty()) {
        output.status = VulkanSsimStatus::Success;
        return output;
    }

    if (!gContext || (!preferredDeviceKey.isEmpty()
                      && gContext->deviceKey != preferredDeviceKey)) {
        gContext.reset();
        if (gFailedDeviceKey != preferredDeviceKey || gContextError.isEmpty()) {
            gContextError.clear();
            if (!buildContext(preferredDeviceKey, gContext, gContextError)) {
                gFailedDeviceKey = preferredDeviceKey;
            } else {
                gFailedDeviceKey.clear();
            }
        }
    }
    if (!gContext) {
        output.status = VulkanSsimStatus::NoDevice;
        output.message = gContextError.isEmpty()
                             ? QCoreApplication::translate(
                                   "VulkanSsimEngine", "Vulkan GPUを初期化できませんでした。")
                             : gContextError;
        return output;
    }
    VulkanContext& context = *gContext;

    const cv::Mat firstGray = toGrayFloat(firstBgra);
    const cv::Mat secondGray = toGrayFloat(secondBgra);
    if (firstGray.empty() || secondGray.empty()) {
        output.status = VulkanSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "VulkanSsimEngine", "Vulkan SSIMで扱えない画像形式です。");
        return output;
    }

    const quint64 firstPixels = firstGray.total();
    const quint64 secondPixels = secondGray.total();
    const quint64 totalPixels = firstPixels + secondPixels;
    if (totalPixels > std::numeric_limits<uint32_t>::max()) {
        output.status = VulkanSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "VulkanSsimEngine", "画像がVulkan SSIMの32-bit index上限を超えています。");
        return output;
    }

    std::vector<float> pixels(static_cast<size_t>(totalPixels));
    std::memcpy(pixels.data(), firstGray.ptr<float>(),
                static_cast<size_t>(firstPixels) * sizeof(float));
    std::memcpy(pixels.data() + firstPixels, secondGray.ptr<float>(),
                static_cast<size_t>(secondPixels) * sizeof(float));

    std::vector<GpuJob> jobs;
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
        jobs.reserve(jobs.size() + groupCount);

        for (size_t group = 0; group < groupCount; ++group) {
            GpuJob job{};
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
        output.status = VulkanSsimStatus::Success;
        return output;
    }
    if (jobs.size() > std::numeric_limits<uint32_t>::max()) {
        output.status = VulkanSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "VulkanSsimEngine", "Vulkan SSIMのworkgroup数上限を超えています。");
        return output;
    }

    const VkDeviceSize pixelBytes = static_cast<VkDeviceSize>(pixels.size() * sizeof(float));
    const VkDeviceSize jobBytes = static_cast<VkDeviceSize>(jobs.size() * sizeof(GpuJob));
    const VkDeviceSize resultBytes = static_cast<VkDeviceSize>(jobs.size() * sizeof(float));
    const VkDeviceSize maxStorageRange = context.properties.limits.maxStorageBufferRange;
    if (pixelBytes > maxStorageRange || jobBytes > maxStorageRange
        || resultBytes > maxStorageRange) {
        output.status = VulkanSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "VulkanSsimEngine", "必要なstorage bufferが選択GPUの上限を超えています。");
        return output;
    }

    BufferAllocation pixelDevice;
    BufferAllocation jobDevice;
    BufferAllocation resultDevice;
    BufferAllocation pixelStaging;
    BufferAllocation jobStaging;
    BufferAllocation resultStaging;
    std::array<BufferAllocation*, 6> allBuffers = {
        &pixelDevice, &jobDevice, &resultDevice,
        &pixelStaging, &jobStaging, &resultStaging
    };
    auto cleanup = [&]() {
        for (BufferAllocation* buffer : allBuffers) {
            destroyBuffer(context.device, *buffer);
        }
    };

    QString error;
    const VkMemoryPropertyFlags hostFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bool prepared =
        prepareBuffer(context, pixelBytes,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pixelDevice, error)
        && prepareBuffer(context, jobBytes,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, jobDevice, error)
        && prepareBuffer(context, resultBytes,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, resultDevice, error)
        && prepareBuffer(context, pixelBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         hostFlags, pixelStaging, error)
        && prepareBuffer(context, jobBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         hostFlags, jobStaging, error)
        && prepareBuffer(context, resultBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         hostFlags, resultStaging, error);
    if (!prepared) {
        cleanup();
        output.status = VulkanSsimStatus::Failed;
        output.message = error;
        return output;
    }

    quint64 requiredLocalBytes = 0;
    for (const BufferAllocation* buffer : allBuffers) {
        if (memoryTypeUsesLocalHeap(context, buffer->memoryType)) {
            requiredLocalBytes += buffer->requirements.size;
        }
    }
    const quint64 localBytes = localMemoryBytes(context.memoryProperties);
    const quint64 safeLimit = localBytes * kVramSafetyPercent / 100;
    output.requiredVramBytes = requiredLocalBytes;
    output.vramLimitBytes = safeLimit;
    if (!ignoreVramLimit && requiredLocalBytes > safeLimit) {
        cleanup();
        output.status = VulkanSsimStatus::VramLimitExceeded;
        output.message = QCoreApplication::translate(
            "VulkanSsimEngine", "Vulkan計算のVRAM見積もりが安全上限を超えました。");
        return output;
    }

    for (BufferAllocation* buffer : allBuffers) {
        if (!allocateBuffer(context, *buffer, error)) {
            cleanup();
            output.status = VulkanSsimStatus::Failed;
            output.message = error;
            return output;
        }
    }

    void* mapped = nullptr;
    VkResult result = vkMapMemory(context.device, pixelStaging.memory, 0,
                                  pixelBytes, 0, &mapped);
    if (result == VK_SUCCESS) {
        std::memcpy(mapped, pixels.data(), static_cast<size_t>(pixelBytes));
        vkUnmapMemory(context.device, pixelStaging.memory);
    }
    if (result != VK_SUCCESS) {
        cleanup();
        output.status = VulkanSsimStatus::Failed;
        output.message = vkError(QCoreApplication::translate(
                                     "VulkanSsimEngine", "pixel staging memoryのmap"), result);
        return output;
    }

    result = vkMapMemory(context.device, jobStaging.memory, 0, jobBytes, 0, &mapped);
    if (result == VK_SUCCESS) {
        std::memcpy(mapped, jobs.data(), static_cast<size_t>(jobBytes));
        vkUnmapMemory(context.device, jobStaging.memory);
    }
    if (result != VK_SUCCESS) {
        cleanup();
        output.status = VulkanSsimStatus::Failed;
        output.message = vkError(QCoreApplication::translate(
                                     "VulkanSsimEngine", "job staging memoryのmap"), result);
        return output;
    }

    VkDescriptorBufferInfo descriptorBuffers[3]{};
    descriptorBuffers[0] = {pixelDevice.buffer, 0, pixelBytes};
    descriptorBuffers[1] = {jobDevice.buffer, 0, jobBytes};
    descriptorBuffers[2] = {resultDevice.buffer, 0, resultBytes};
    VkWriteDescriptorSet descriptorWrites[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = context.descriptorSet;
        descriptorWrites[i].dstBinding = i;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[i].pBufferInfo = &descriptorBuffers[i];
    }
    vkUpdateDescriptorSets(context.device, 3, descriptorWrites, 0, nullptr);

    VkCommandBufferAllocateInfo commandAllocateInfo{};
    commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocateInfo.commandPool = context.commandPool;
    commandAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    result = vkAllocateCommandBuffers(context.device, &commandAllocateInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        cleanup();
        output.status = VulkanSsimStatus::Failed;
        output.message = vkError(QCoreApplication::translate(
                                     "VulkanSsimEngine", "Vulkan command bufferの割り当て"), result);
        return output;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        cleanup();
        output.status = VulkanSsimStatus::Failed;
        output.message = vkError(QCoreApplication::translate(
                                     "VulkanSsimEngine", "Vulkan command bufferの開始"), result);
        return output;
    }

    VkBufferCopy pixelCopy{0, 0, pixelBytes};
    VkBufferCopy jobCopy{0, 0, jobBytes};
    vkCmdCopyBuffer(commandBuffer, pixelStaging.buffer, pixelDevice.buffer, 1, &pixelCopy);
    vkCmdCopyBuffer(commandBuffer, jobStaging.buffer, jobDevice.buffer, 1, &jobCopy);

    VkBufferMemoryBarrier uploadBarriers[2]{};
    uploadBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    uploadBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    uploadBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    uploadBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uploadBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uploadBarriers[0].buffer = pixelDevice.buffer;
    uploadBarriers[0].offset = 0;
    uploadBarriers[0].size = pixelBytes;
    uploadBarriers[1] = uploadBarriers[0];
    uploadBarriers[1].buffer = jobDevice.buffer;
    uploadBarriers[1].size = jobBytes;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 2, uploadBarriers, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, context.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            context.pipelineLayout, 0, 1, &context.descriptorSet,
                            0, nullptr);

    const uint32_t maxX = context.properties.limits.maxComputeWorkGroupCount[0];
    const uint32_t maxY = context.properties.limits.maxComputeWorkGroupCount[1];
    const uint32_t rowWidth = std::min<uint32_t>(static_cast<uint32_t>(jobs.size()), maxX);
    const quint64 rowCount64 = (jobs.size() + rowWidth - 1) / rowWidth;
    if (rowWidth == 0 || rowCount64 > maxY) {
        vkEndCommandBuffer(commandBuffer);
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        cleanup();
        output.status = VulkanSsimStatus::Unsupported;
        output.message = QCoreApplication::translate(
            "VulkanSsimEngine", "Vulkan dispatchのworkgroup上限を超えています。");
        return output;
    }
    const uint32_t rowCount = static_cast<uint32_t>(rowCount64);
    vkCmdPushConstants(commandBuffer, context.pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(rowWidth), &rowWidth);
    vkCmdDispatch(commandBuffer, rowWidth, rowCount, 1);

    VkBufferMemoryBarrier resultBarrier{};
    resultBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    resultBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    resultBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    resultBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    resultBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    resultBarrier.buffer = resultDevice.buffer;
    resultBarrier.offset = 0;
    resultBarrier.size = resultBytes;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 1, &resultBarrier, 0, nullptr);

    VkBufferCopy resultCopy{0, 0, resultBytes};
    vkCmdCopyBuffer(commandBuffer, resultDevice.buffer, resultStaging.buffer,
                    1, &resultCopy);

    result = vkEndCommandBuffer(commandBuffer);
    if (result == VK_SUCCESS) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        result = vkQueueSubmit(context.queue, 1, &submitInfo, VK_NULL_HANDLE);
    }
    if (result == VK_SUCCESS) {
        result = vkQueueWaitIdle(context.queue);
    }
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
        cleanup();
        output.status = VulkanSsimStatus::Failed;
        output.message = vkError(QCoreApplication::translate(
                                     "VulkanSsimEngine", "Vulkan SSIM commandの実行"), result);
        return output;
    }

    std::vector<float> partialSums(jobs.size());
    result = vkMapMemory(context.device, resultStaging.memory, 0,
                         resultBytes, 0, &mapped);
    if (result == VK_SUCCESS) {
        std::memcpy(partialSums.data(), mapped, static_cast<size_t>(resultBytes));
        vkUnmapMemory(context.device, resultStaging.memory);
    }
    vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
    cleanup();
    if (result != VK_SUCCESS) {
        output.status = VulkanSsimStatus::Failed;
        output.message = vkError(QCoreApplication::translate(
                                     "VulkanSsimEngine", "result staging memoryのmap"), result);
        return output;
    }

    for (size_t task = 0; task < rois.size(); ++task) {
        if (taskGroupCount[task] == 0 || taskPixelCount[task] == 0) {
            continue;
        }
        const size_t begin = taskGroupBegin[task];
        const size_t end = begin + taskGroupCount[task];
        const double sum = std::accumulate(partialSums.begin() + static_cast<ptrdiff_t>(begin),
                                           partialSums.begin() + static_cast<ptrdiff_t>(end),
                                           0.0);
        output.scores[task] = sum / static_cast<double>(taskPixelCount[task]);
    }

    output.status = VulkanSsimStatus::Success;
    return output;
}
}

bool VulkanSsimEngine::isBuilt()
{
    return true;
}

VulkanDeviceScanResult VulkanSsimEngine::detectDevices()
{
    VulkanDeviceScanResult output;
    VkInstance instance = VK_NULL_HANDLE;
    if (!createInstance(instance, output.error)) {
        return output;
    }

    QString error;
    const std::vector<DeviceCandidate> candidates = enumerateCandidates(instance, error);
    for (const DeviceCandidate& candidate : candidates) {
        output.devices.push_back(candidate.info);
    }
    output.error = error;
    vkDestroyInstance(instance, nullptr);
    return output;
}

VulkanSsimBatchResult VulkanSsimEngine::computeBatch(
    const cv::Mat& firstBgra,
    const cv::Mat& secondBgra,
    const std::vector<VulkanSsimRoiPair>& rois,
    const QString& preferredDeviceKey,
    bool ignoreVramLimit)
{
    std::lock_guard<std::mutex> lock(gExecutionMutex);
    return computeBatchLocked(firstBgra, secondBgra, rois,
                              preferredDeviceKey, ignoreVramLimit);
}

#else

bool VulkanSsimEngine::isBuilt()
{
    return false;
}

VulkanDeviceScanResult VulkanSsimEngine::detectDevices()
{
    VulkanDeviceScanResult result;
    result.error = QCoreApplication::translate(
        "VulkanSsimEngine", "このビルドではVulkanサポートが有効ではありません。");
    return result;
}

VulkanSsimBatchResult VulkanSsimEngine::computeBatch(
    const cv::Mat&,
    const cv::Mat&,
    const std::vector<VulkanSsimRoiPair>& rois,
    const QString&,
    bool)
{
    VulkanSsimBatchResult result;
    result.status = VulkanSsimStatus::NotBuilt;
    result.scores.assign(rois.size(), 0.0);
    result.message = QCoreApplication::translate(
        "VulkanSsimEngine", "このビルドではVulkanサポートが有効ではありません。");
    return result;
}

#endif
