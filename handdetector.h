#ifndef HANDDETECTOR_H
#define HANDDETECTOR_H

#include <opencv2/opencv.hpp>

class HandDetector {
public:
    HandDetector();

    /**
     * Main method to detect hand in a frame
     * @param frame Input video frame
     * @return Point representing hand center position or (-1,-1) if not detected
     */
    cv::Point detectHand(const cv::Mat &frame);

    /**
     * Configure HSV color space thresholds for skin detection
     */
    void setHSVThreshold(int hMin, int hMax, int sMin, int sMax, int vMin, int vMax);
    
    /**
     * Set minimum contour area to be considered as hand
     */
    void setMinHandArea(double area);
    
    /**
     * Get detected hand contour for visualization
     */
    std::vector<cv::Point> getHandContour() const;
    
    /**
     * Get convexity defects for finger detection
     */
    std::vector<cv::Vec4i> getConvexityDefects() const;

    // New method for FLANN-based hand detection using SIFT
    cv::Point detectHandFLANN(const cv::Mat &frame);

    // Set the calibration image (captured during calibration)
    void setCalibrationImage(const cv::Mat &image);

private:
    // Parameters for skin detection in HSV space
    int m_hMin, m_hMax;
    int m_sMin, m_sMax;
    int m_vMin, m_vMax;
    double m_minHandArea;
    
    // Detected hand data
    std::vector<cv::Point> m_handContour;
    std::vector<int> m_hullIndices;
    std::vector<cv::Vec4i> m_defects;
    cv::Point m_handPosition;
    
    // Reference image for FLANN matching
    cv::Mat m_calibrationImage;

    // Processing methods
    cv::Mat extractSkinMask(const cv::Mat &frame);
    bool findHandContour(const cv::Mat &skinMask);
    cv::Point calculateHandPosition();
    void analyzeHandShape();
};

#endif // HANDDETECTOR_H
