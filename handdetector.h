#ifndef HANDDETECTOR_H
#define HANDDETECTOR_H

#include <opencv2/opencv.hpp>
#include <QMutex>

class HandDetector {
public:
    HandDetector();

    /**
     * Main method to detect hand in a frame using FLANN matching
     * @param frame Input video frame
     * @return Point representing hand center position or (-1,-1) if not detected
     */
    cv::Point detectHand(const cv::Mat &frame);

    // For API compatibility - no longer used
    void setHSVThreshold(int hMin, int hMax, int sMin, int sMax, int vMin, int vMax);
    void setMinHandArea(double area);
    
    // Get detected hand data for visualization
    std::vector<cv::Point> getHandContour() const;
    std::vector<cv::Vec4i> getConvexityDefects() const;

    // Set the calibration image (captured during calibration)
    void setCalibrationImage(const cv::Mat &image);

private:
    // Detected hand data
    std::vector<cv::Point> m_handContour;
    std::vector<cv::Vec4i> m_defects;
    
    // Reference image for FLANN matching
    cv::Mat m_calibrationImage;
    
    // Thread safety
    QMutex m_mutex;
};

#endif // HANDDETECTOR_H
