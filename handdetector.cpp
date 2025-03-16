#include "handdetector.h"
#include <iostream>
#include <QDebug>

HandDetector::HandDetector()
    // Default HSV range optimized for skin detection
    : m_hMin(0), m_hMax(20)
    , m_sMin(48), m_sMax(255)
    , m_vMin(80), m_vMax(255)
    , m_minHandArea(3000)  // Minimum hand area in pixels
    , m_handPosition(-1, -1)
{
}

cv::Point HandDetector::detectHand(const cv::Mat &frame)
{
    // If calibration image exists, try FLANN matching first for better accuracy.
    if(!m_calibrationImage.empty()){
        cv::Point detected = detectHandFLANN(frame);
        qDebug() << "FLANN detection returned:" << detected.x << detected.y;
        if(detected.x >= 0 && detected.y >= 0)
            return detected;
    }
    // Fallback to basic skin thresholding detection
    m_handPosition = cv::Point(-1, -1);
    m_handContour.clear();
    m_hullIndices.clear();
    m_defects.clear();
    cv::Mat skinMask = extractSkinMask(frame);
    if (findHandContour(skinMask)) {
        m_handPosition = calculateHandPosition();
        analyzeHandShape();
    }
    qDebug() << "Basic detection returned:" << m_handPosition.x << m_handPosition.y;
    return m_handPosition;
}

cv::Point HandDetector::detectHandFLANN(const cv::Mat &frame)
{
    // Fall back to basic detection if no calibration image is set
    if (m_calibrationImage.empty()) {
        return detectHand(frame);
    }

    // Create SIFT detector
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    std::vector<cv::KeyPoint> keypointsRef, keypointsFrame;
    cv::Mat descriptorsRef, descriptorsFrame;

    // Extract features from calibration image and current frame
    sift->detectAndCompute(m_calibrationImage, cv::noArray(), keypointsRef, descriptorsRef);
    sift->detectAndCompute(frame, cv::noArray(), keypointsFrame, descriptorsFrame);

    if (descriptorsRef.empty() || descriptorsFrame.empty())
        return cv::Point(-1, -1);

    // Use FLANN matcher
    cv::FlannBasedMatcher matcher;
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(descriptorsRef, descriptorsFrame, knnMatches, 2);

    // Filter matches using Lowe's ratio test
    const float ratioThresh = 0.7f;
    std::vector<cv::DMatch> goodMatches;
    for (const auto& match : knnMatches) {
        if (match.size() >= 2 &&
            match[0].distance < ratioThresh * match[1].distance) {
            goodMatches.push_back(match[0]);
        }
    }

    // Compute centroid from good matches
    if (!goodMatches.empty()) {
        cv::Point2f centroid(0.0f, 0.0f);
        for (const auto& match : goodMatches) {
            centroid += keypointsFrame[match.trainIdx].pt;
        }
        centroid.x /= static_cast<float>(goodMatches.size());
        centroid.y /= static_cast<float>(goodMatches.size());
        return cv::Point(static_cast<int>(centroid.x), static_cast<int>(centroid.y));
    }

    return cv::Point(-1, -1);
}

void HandDetector::setCalibrationImage(const cv::Mat &image)
{
    if (!image.empty()) {
        image.copyTo(m_calibrationImage);
        qDebug() << "Calibration image set for FLANN matching.";
    }
}

void HandDetector::setHSVThreshold(int hMin, int hMax, int sMin, int sMax, int vMin, int vMax)
{
    m_hMin = hMin;
    m_hMax = hMax;
    m_sMin = sMin;
    m_sMax = sMax;
    m_vMin = vMin;
    m_vMax = vMax;
}

void HandDetector::setMinHandArea(double area)
{
    m_minHandArea = area;
}

std::vector<cv::Point> HandDetector::getHandContour() const
{
    return m_handContour;
}

std::vector<cv::Vec4i> HandDetector::getConvexityDefects() const
{
    return m_defects;
}

cv::Mat HandDetector::extractSkinMask(const cv::Mat &frame)
{
    cv::Mat skinMask, hsvFrame;
    
    // Convert from BGR to HSV color space
    cv::cvtColor(frame, hsvFrame, cv::COLOR_BGR2HSV);
    
    // Apply skin color thresholding in HSV space
    cv::inRange(hsvFrame, 
                cv::Scalar(m_hMin, m_sMin, m_vMin),
                cv::Scalar(m_hMax, m_sMax, m_vMax),
                skinMask);
    
    // Apply morphological operations to clean up the mask
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_CLOSE, kernel); // Close small holes
    cv::morphologyEx(skinMask, skinMask, cv::MORPH_OPEN, kernel);  // Remove small noise
    
    return skinMask;
}

bool HandDetector::findHandContour(const cv::Mat &skinMask)
{
    // Find all contours in the binary image
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(skinMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return false;
    }
    
    // Find the largest contour (assumed to be the hand)
    size_t maxContourIdx = 0;
    double maxArea = 0;
    
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxContourIdx = i;
        }
    }
    
    // Ensure it's large enough to be a hand
    if (maxArea < m_minHandArea) {
        return false;
    }
    
    m_handContour = contours[maxContourIdx];
    return true;
}

cv::Point HandDetector::calculateHandPosition()
{
    // Use moments to find the centroid of the hand
    cv::Moments moments = cv::moments(m_handContour);
    if (moments.m00 != 0) {
        int cx = static_cast<int>(moments.m10 / moments.m00);
        int cy = static_cast<int>(moments.m01 / moments.m00);
        return cv::Point(cx, cy);
    }
    return cv::Point(-1, -1);
}

void HandDetector::analyzeHandShape()
{
    // Find convex hull of the hand contour
    cv::convexHull(m_handContour, m_hullIndices);
    
    // Find convexity defects - useful for finger detection
    if (m_hullIndices.size() > 3) {
        // The convexity defects function requires the indices to be integers
        std::vector<int> hullForDefects;
        cv::convexHull(m_handContour, hullForDefects, false);
        
        // Compute convexity defects - the "valleys" between fingers
        cv::convexityDefects(m_handContour, hullForDefects, m_defects);
        
        // Note: Further processing could:
        // 1. Filter defects by depth to identify fingers
        // 2. Calculate angles between defect points to distinguish fingers
        // 3. Implement gesture recognition based on number and position of fingers
    }
}
