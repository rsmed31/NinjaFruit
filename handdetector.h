#ifndef HANDDETECTOR_H
#define HANDDETECTOR_H

#include <opencv2/opencv.hpp>
#include <QMutex>
#include <vector>
#include <deque>

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
    std::vector<cv::Point> getLastContour() const;
    std::vector<cv::Vec4i> getLastDefects() const;
    std::vector<cv::Point2f> getProjectedCorners() const; // Retrieve projected corners

    // Set calibration image(s)
    void setCalibrationImage(const cv::Mat &image);
    void addCalibrationImage(const cv::Mat &image);
    void clearSupplementaryCalibrations();

    cv::Rect detectMotionROI(const cv::Mat &frame); // Ensure only one declaration exists
    const std::vector<cv::Point2f>& getLastInlierPoints() const; // Getter for m_lastInlierPoints
    const cv::Point2f& getLastRawPos() const; // Getter for m_lastRawPos

private:
    // Detected hand data
    std::vector<cv::Point> m_handContour;
    std::vector<cv::Vec4i> m_defects;
    
    // Primary calibration image
    cv::Mat m_calibrationImage;
    
    // Additional calibration images for different hand positions
    std::vector<cv::Mat> m_additionalCalibrations;
    
    // Cached SIFT features for reference images
    std::vector<std::vector<cv::KeyPoint>> m_cachedRefKeypoints;
    std::vector<cv::Mat> m_cachedRefDescriptors;
    bool m_refFeaturesComputed;

    // Keypoints and descriptors for the calibration image
    std::vector<cv::KeyPoint> m_refKeypoints;
    cv::Mat m_refDescriptors;

    // Plain‐vector of keypoint locations for homography
    std::vector<cv::Point2f> m_refPoints;

    // Thread safety
    QMutex m_mutex;
    
    // Motion detection and previous frame data
    cv::Mat m_prevFrame;
    cv::Point m_prevHandPos;
    bool m_isFirstFrame;
    
    // Frame processing counters
    int m_frameCounter;
    int m_contourFrameCounter;
    
    // Parameters for filtering
    const float MAX_POSITION_SHIFT = 150.0f;  // Max pixel shift between frames
    const float MAX_ANGLE_DIFF = 45.0f;       // Increased for more tolerance
    const float MAX_SCALE_RATIO = 2.0f;       // Increased for more tolerance
    
    // Early detection thresholds
    const int MIN_KEYPOINTS = 5;              // Minimum keypoints to consider a match
    const int MIN_GOOD_MATCHES = 3;           // Minimum good matches for valid detection
    const float RATIO_THRESHOLD = 0.75f;      // Ratio test threshold
    
    // Helper methods for improved detection
    cv::Point matchImageFLANN(const cv::Mat &refImage, const cv::Mat &frame, const cv::Rect &roi);
    bool validateHandShape(const cv::Mat &roiFrame, cv::Point &handPos);
    cv::Point fallbackToORB(const cv::Mat &frame); // Fallback method for ORB-based matching

    // Member variables for position smoothing and reporting
    std::deque<cv::Point2f> m_posBuffer; // Circular buffer for smoothing hand position
    cv::Point2f m_lastReported;         // Last reported smoothed position
    std::vector<cv::Point2f> m_lastInlierPoints; // Store inlier points
    cv::Point2f m_lastRawPos; // Store raw centroid position
};

#endif // HANDDETECTOR_H
