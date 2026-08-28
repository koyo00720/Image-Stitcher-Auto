#ifndef METAL_SSIM_H
#define METAL_SSIM_H

#include "vulkan_ssim.h"

#include <QString>
#include <QtGlobal>

#include <opencv2/core.hpp>

#include <vector>

enum class MetalSsimStatus {
    Success,
    NotBuilt,
    NoDevice,
    VramLimitExceeded,
    Unsupported,
    Failed
};

struct MetalSsimBatchResult {
    MetalSsimStatus status = MetalSsimStatus::Failed;
    std::vector<double> scores;
    quint64 requiredVramBytes = 0;
    quint64 vramLimitBytes = 0;
    QString message;

    bool succeeded() const { return status == MetalSsimStatus::Success; }
};

class MetalSsimEngine
{
public:
    static bool isBuilt();
    static bool isAvailable();
    static QString deviceName();

    static MetalSsimBatchResult computeBatch(
        const cv::Mat& firstBgra,
        const cv::Mat& secondBgra,
        const std::vector<VulkanSsimRoiPair>& rois,
        bool ignoreVramLimit);
};

#endif // METAL_SSIM_H
