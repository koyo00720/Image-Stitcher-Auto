#include "metal_ssim.h"

#include <QCoreApplication>

bool MetalSsimEngine::isBuilt()
{
    return false;
}

bool MetalSsimEngine::isAvailable()
{
    return false;
}

QString MetalSsimEngine::deviceName()
{
    return {};
}

MetalSsimBatchResult MetalSsimEngine::computeBatch(
    const cv::Mat&,
    const cv::Mat&,
    const std::vector<VulkanSsimRoiPair>& rois,
    bool)
{
    MetalSsimBatchResult result;
    result.status = MetalSsimStatus::NotBuilt;
    result.scores.assign(rois.size(), 0.0);
    result.message = QCoreApplication::translate(
        "MetalSsimEngine", "このビルドではMetal計算を利用できません。");
    return result;
}
