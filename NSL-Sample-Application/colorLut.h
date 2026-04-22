#pragma once

#include <cassert>
#include <vector>
#include <opencv2/opencv.hpp>

// ColorLut: pre-builds color lookup tables for distance and amplitude values
// so that per-pixel colorization is a simple indexed array access instead of
// calling the nanolib color functions (~154k times per frame).
//
// Usage:
//   ColorLut lut;
//   lut.rebuild(maxDistance, isGrayscale);   // once at startup / config change
//   lut.applyDistance(pcdData.distance2D[0], distMat, roiX, roiY, w, h);
//   lut.applyAmplitude(pcdData.amplitude[0], ampMat,  roiX, roiY, w, h);

class ColorLut
{
public:
    // Build both lookup tables.
    // maxDistance : configured max distance in mm (1 .. MAX_DISTANCE_12MHZ)
    // grayscale   : true when captureType == GRAYSCALE_MODE or DISTANCE_GRAYSCALE_MODE
    // Out-of-range maxDistance values are silently ignored (tables remain unchanged).
    void rebuild(int maxDistance, bool grayscale);

    // Fill a CV_8UC3 Mat using the distance table.
    // src   : pointer to distance2D[0][0] of NslPCD (row-major int array,
    //         stride = NSL_LIDAR_TYPE_B_WIDTH = 800 columns)
    // dst   : CV_8UC3 Mat sized to at least (roiY+h) rows x (roiX+w) cols,
    //         i.e., (NSL_LIDAR_TYPE_A_HEIGHT x NSL_LIDAR_TYPE_A_WIDTH) for Type A sensors;
    //         caller initialises to Scalar(255,255,255)
    // roiX/roiY : pcdData.roiXMin / pcdData.roiYMin
    // w/h       : pcdData.width  / pcdData.height  (ROI dimensions)
    void applyDistance(const int* src, cv::Mat& dst,
                       int roiX, int roiY, int w, int h);

    // Same as applyDistance but uses the amplitude table and amplitude[0][0] as source.
    // dst   : CV_8UC3 Mat sized to at least (roiY+h) rows x (roiX+w) cols,
    //         i.e., (NSL_LIDAR_TYPE_A_HEIGHT x NSL_LIDAR_TYPE_A_WIDTH) for Type A sensors;
    //         caller initialises to Scalar(255,255,255)
    void applyAmplitude(const int* src, cv::Mat& dst,
                        int roiX, int roiY, int w, int h);

    // Returns true if tables have been successfully built by rebuild().
    bool isBuilt() const { return !distTable_.empty(); }

private:
    std::vector<cv::Vec3b> distTable_;   // size = maxDistance_ + 1
    std::vector<cv::Vec3b> ampTable_;    // size = MAX_GRAYSCALE_VALUE + 1
    int maxDistance_ = 0;
};
