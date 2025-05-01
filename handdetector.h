#ifndef HANDDETECTOR_H
#define HANDDETECTOR_H

#include <opencv2/opencv.hpp>
#include <QMutex>
#include <vector>

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
    
    // Add a supplementary calibration image (for multi-position tracking)
    void addCalibrationImage(const cv::Mat &image);
    
    // Clear all calibration images except the primary one
    void clearSupplementaryCalibrations();

private:
    // Detected hand data
    std::vector<cv::Point> m_handContour;
    std::vector<cv::Vec4i> m_defects;
    
    // Primary calibration image - make static so it persists through recompilation
    cv::Mat m_calibrationImage;
    
    // Additional calibration images for different hand positions
    std::vector<cv::Mat> m_additionalCalibrations;
    
    // Thread safety
    QMutex m_mutex;
    
    // Utility function for FLANN matching against a single reference image
    cv::Point matchImageFLANN(const cv::Mat &refImage, const cv::Mat &frame);
};

#endif // HANDDETECTOR_H
