#include "colorLut.h"

#include "nanolib.h"

using namespace NslOption;

// ---------------------------------------------------------------------------
// Internal helper: convert NslVec3b (fields: b, g, r) to cv::Vec3b (BGR).
// Both types store channels in the same order, so the mapping is direct.
// ---------------------------------------------------------------------------
static inline cv::Vec3b toVec3b(const NslVec3b& c)
{
    return cv::Vec3b(c.b, c.g, c.r);
}

// ---------------------------------------------------------------------------
// rebuild
// ---------------------------------------------------------------------------
void ColorLut::rebuild(int maxDistance, bool grayscale)
{
    std::lock_guard<std::mutex> lock(lutMtx_);

    // I-3: Invalidate tables immediately so a failed/skipped rebuild is never
    //      mistaken for valid state by concurrent or subsequent callers.
    maxDistance_ = 0;
    distTable_.clear();
    ampTable_.clear();

    // I-2: Guard against nonsensical maxDistance values before touching nanolib.
    if (maxDistance <= 0 || maxDistance > MAX_DISTANCE_12MHZ)
        return;

    // Inform nanolib of the active color range and mode
    nsl_setColorRange(maxDistance,
                      MAX_GRAYSCALE_VALUE,
                      grayscale ? FUNCTION_OPTIONS::FUNC_ON
                                : FUNCTION_OPTIONS::FUNC_OFF);

    // Build distance table: indices [0 .. maxDistance]
    distTable_.resize(static_cast<size_t>(maxDistance) + 1);
    for (int i = 0; i <= maxDistance; ++i)
        distTable_[i] = toVec3b(nsl_getDistanceColor(i));

    // Build amplitude table: indices [0 .. MAX_GRAYSCALE_VALUE]
    ampTable_.resize(static_cast<size_t>(MAX_GRAYSCALE_VALUE) + 1);
    for (int i = 0; i <= MAX_GRAYSCALE_VALUE; ++i)
        ampTable_[i] = toVec3b(nsl_getAmplitudeColor(i));

    // I-3: Only commit maxDistance_ after both tables are fully built.
    if (static_cast<int>(distTable_.size()) == maxDistance + 1 &&
        static_cast<int>(ampTable_.size()) == MAX_GRAYSCALE_VALUE + 1)
    {
        maxDistance_ = maxDistance;
    }
}

// ---------------------------------------------------------------------------
// applyDistance
// ---------------------------------------------------------------------------
void ColorLut::applyDistance(const int* src, cv::Mat& dst,
                              int roiX, int roiY, int w, int h)
{
    std::lock_guard<std::mutex> lock(lutMtx_);

    if (distTable_.empty())
        return;

    // I-1: dst must cover the full (roiY+h) x (roiX+w) region
    assert(dst.rows >= roiY + h && dst.cols >= roiX + w);

    cv::parallel_for_(cv::Range(0, h), [this, src, &dst, roiX, roiY, w](const cv::Range& range)
    {
        for (int y = range.start; y < range.end; ++y)
        {
            cv::Vec3b* rowPtr = dst.ptr<cv::Vec3b>(roiY + y);
            for (int x = 0; x < w; ++x)
            {
                const int val = src[(roiY + y) * NSL_LIDAR_TYPE_B_WIDTH + (roiX + x)];
                if (val < 0 || val > maxDistance_)
                    continue;   // leave pixel at its initialized (255,255,255)
                rowPtr[roiX + x] = distTable_[val];
            }
        }
    });
}

// ---------------------------------------------------------------------------
// applyAmplitude
// ---------------------------------------------------------------------------
void ColorLut::applyAmplitude(const int* src, cv::Mat& dst,
                               int roiX, int roiY, int w, int h)
{
    std::lock_guard<std::mutex> lock(lutMtx_);

    if (ampTable_.empty())
        return;

    // I-1: dst must cover the full (roiY+h) x (roiX+w) region
    assert(dst.rows >= roiY + h && dst.cols >= roiX + w);

    cv::parallel_for_(cv::Range(0, h), [this, src, &dst, roiX, roiY, w](const cv::Range& range)
    {
        for (int y = range.start; y < range.end; ++y)
        {
            cv::Vec3b* rowPtr = dst.ptr<cv::Vec3b>(roiY + y);
            for (int x = 0; x < w; ++x)
            {
                const int val = src[(roiY + y) * NSL_LIDAR_TYPE_B_WIDTH + (roiX + x)];
                if (val < 0 || val > MAX_GRAYSCALE_VALUE)
                    continue;   // leave pixel at its initialized (255,255,255)
                rowPtr[roiX + x] = ampTable_[val];
            }
        }
    });
}
