#ifndef VULKAN_SSIM_H
#define VULKAN_SSIM_H

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <opencv2/core.hpp>

#include <vector>

struct VulkanGpuInfo {
    QString key;
    QString name;
    QString deviceType;
    quint64 localMemoryBytes = 0;
};

struct VulkanDeviceScanResult {
    QVector<VulkanGpuInfo> devices;
    QString error;
};

struct VulkanSsimRoiPair {
    cv::Rect first;
    cv::Rect second;
};

enum class VulkanSsimStatus {
    Success,
    NotBuilt,
    NoDevice,
    VramLimitExceeded,
    Unsupported,
    Failed
};

struct VulkanSsimBatchResult {
    VulkanSsimStatus status = VulkanSsimStatus::Failed;
    std::vector<double> scores;
    quint64 requiredVramBytes = 0;
    quint64 vramLimitBytes = 0;
    QString message;

    bool succeeded() const { return status == VulkanSsimStatus::Success; }
};

class VulkanSsimEngine
{
public:
    static bool isBuilt();
    static VulkanDeviceScanResult detectDevices();

    static VulkanSsimBatchResult computeBatch(
        const cv::Mat& firstBgra,
        const cv::Mat& secondBgra,
        const std::vector<VulkanSsimRoiPair>& rois,
        const QString& preferredDeviceKey,
        bool ignoreVramLimit);
};

#endif // VULKAN_SSIM_H
