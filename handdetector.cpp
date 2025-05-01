#include "handdetector.h"
#include <iostream>
#include <QDebug>
#include <QMutexLocker>

HandDetector::HandDetector()
{
    qDebug() << "HandDetector initialized";
}

cv::Point HandDetector::detectHand(const cv::Mat &frame)
{
    // Skip processing if frame is empty
    if (frame.empty()) {
        return cv::Point(-1, -1);
    }
    
    QMutexLocker lock(&m_mutex);
    
    // Check if primary calibration exists
    if (m_calibrationImage.empty()) {
        return cv::Point(-1, -1);
    }
    
    // PERFORMANCE OPTIMIZATION: Downscale the input frame to improve speed
    static cv::Mat smallFrame;
    static const float scaleFactor = 0.5f; // Process at half resolution
    cv::resize(frame, smallFrame, cv::Size(), scaleFactor, scaleFactor, cv::INTER_LINEAR);
    
    // Track the last successful position for smoothing
    static cv::Point lastPosition(-1, -1);
    
    // Static counter to skip supplementary calibrations on some frames
    static int frameCounter = 0;
    frameCounter++;
    
    // Try matching with primary calibration image first
    cv::Point result = matchImageFLANN(m_calibrationImage, smallFrame);
    
    // Only check supplementary calibrations every 3 frames if primary fails
    if (result.x < 0 && !m_additionalCalibrations.empty() && frameCounter % 3 == 0) {
        for (const auto& calibImg : m_additionalCalibrations) {
            result = matchImageFLANN(calibImg, smallFrame);
            if (result.x >= 0) {
                break;
            }
        }
    }
    
    // Scale result back to original image size
    if (result.x >= 0) {
        result.x = static_cast<int>(result.x / scaleFactor);
        result.y = static_cast<int>(result.y / scaleFactor);
        
        // Simple position smoothing to reduce jitter
        if (lastPosition.x >= 0) {
            // 80-20 blend of new and old position
            result.x = static_cast<int>(0.8 * result.x + 0.2 * lastPosition.x);
            result.y = static_cast<int>(0.8 * result.y + 0.2 * lastPosition.y);
        }
        lastPosition = result;
    }
    
    return result;
}

cv::Point HandDetector::matchImageFLANN(const cv::Mat &refImage, const cv::Mat &frame)
{
    try {
        // PERFORMANCE OPTIMIZATION: Configure SIFT for fewer features and faster processing
        cv::Ptr<cv::SIFT> sift = cv::SIFT::create(50); // Use fewer features (default is 400)
        std::vector<cv::KeyPoint> keypointsRef, keypointsFrame;
        cv::Mat descriptorsRef, descriptorsFrame;
        
        // Extract features
        sift->detectAndCompute(refImage, cv::noArray(), keypointsRef, descriptorsRef);
        sift->detectAndCompute(frame, cv::noArray(), keypointsFrame, descriptorsFrame);
        
        if (descriptorsRef.empty() || descriptorsFrame.empty() || 
            keypointsRef.size() < 2 || keypointsFrame.size() < 2) {
            return cv::Point(-1, -1);
        }
        
        // PERFORMANCE OPTIMIZATION: Use the fast FLANN configuration
        cv::FlannBasedMatcher matcher;
        std::vector<cv::DMatch> matches;
        // Use just simple matching instead of knnMatch which is slower
        matcher.match(descriptorsRef, descriptorsFrame, matches);
        
        // Filter by distance
        if (matches.empty()) {
            return cv::Point(-1, -1);
        }
        
        // Sort matches by distance
        std::sort(matches.begin(), matches.end(), 
            [](const cv::DMatch& a, const cv::DMatch& b) {
                return a.distance < b.distance;
            });
        
        // Take only the top 10 matches
        const int maxMatches = std::min(10, static_cast<int>(matches.size()));
        matches.resize(maxMatches);
        
        // Compute centroid from good matches
        cv::Point2f centroid(0.0f, 0.0f);
        for (const auto& match : matches) {
            centroid += keypointsFrame[match.trainIdx].pt;
        }
        centroid.x /= matches.size();
        centroid.y /= matches.size();
        
        return cv::Point(static_cast<int>(centroid.x), static_cast<int>(centroid.y));
    }
    catch (const cv::Exception&) {
        // Silently fail on OpenCV errors
        return cv::Point(-1, -1);
    }
    catch (const std::exception&) {
        // Silently fail on other errors
        return cv::Point(-1, -1);
    }
}

void HandDetector::setCalibrationImage(const cv::Mat &image)
{
    if (image.empty()) {
        qDebug() << "❌ Calibration image empty — skipping";
        return;
    }
    
    qDebug() << "Setting primary calibration image:" << image.cols << "x" << image.rows 
             << "type:" << image.type() << "channels:" << image.channels();
    
    // Make a persistent copy of the image
    cv::Mat persistentCopy = image.clone();
    
    // Ensure we have a proper BGR image for SIFT processing
    if (persistentCopy.type() != CV_8UC3) {
        qDebug() << "Converting image to 8UC3 format";
        cv::Mat tmp;
        if (persistentCopy.channels() == 1) {
            cv::cvtColor(persistentCopy, tmp, cv::COLOR_GRAY2BGR);
            persistentCopy = tmp;
        } else {
            persistentCopy.convertTo(tmp, CV_8UC3);
            persistentCopy = tmp;
        }
    }
    
    if (persistentCopy.empty()) {
        qDebug() << "❌ Failed to convert/copy calibration image";
        return;
    }
    
    // Verify SIFT can detect features in this image
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    
    try {
        sift->detectAndCompute(persistentCopy, cv::noArray(), keypoints, descriptors);
        qDebug() << "SIFT validation check: detected" << keypoints.size() << "keypoints in calibration image";
        
        if (keypoints.size() < 10) {
            qDebug() << "⚠️ Warning: Few keypoints in calibration image, matching may be unreliable";
        }
    } catch (const cv::Exception& e) {
        qDebug() << "❌ SIFT validation failed on calibration image:" << e.what();
        return;
    }
    
    // Store the image safely
    QMutexLocker lock(&m_mutex);
    try {
        persistentCopy.copyTo(m_calibrationImage);  // Deep copy under lock
        qDebug() << "✅ Primary calibration image set:" 
                 << m_calibrationImage.cols << "x" 
                 << m_calibrationImage.rows;
    } catch (const cv::Exception& e) {
        qDebug() << "❌ Failed to set calibration image:" << e.what();
        m_calibrationImage.release();
    }
}

void HandDetector::addCalibrationImage(const cv::Mat &image)
{
    if (image.empty()) {
        qDebug() << "❌ Supplementary calibration image empty — skipping";
        return;
    }
    
    qDebug() << "Adding supplementary calibration image:" << image.cols << "x" << image.rows;
    
    // Make a persistent copy of the image
    cv::Mat persistentCopy = image.clone();
    
    // Ensure we have a proper BGR image for SIFT processing
    if (persistentCopy.type() != CV_8UC3) {
        cv::Mat tmp;
        if (persistentCopy.channels() == 1) {
            cv::cvtColor(persistentCopy, tmp, cv::COLOR_GRAY2BGR);
            persistentCopy = tmp;
        } else {
            persistentCopy.convertTo(tmp, CV_8UC3);
            persistentCopy = tmp;
        }
    }
    
    if (persistentCopy.empty()) {
        qDebug() << "❌ Failed to convert/copy supplementary calibration image";
        return;
    }
    
    // Verify SIFT can detect features in this image
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    
    try {
        sift->detectAndCompute(persistentCopy, cv::noArray(), keypoints, descriptors);
        qDebug() << "SIFT found" << keypoints.size() << "keypoints in supplementary calibration";
        
        if (keypoints.size() < 10) {
            qDebug() << "⚠️ Warning: Few keypoints in supplementary calibration";
            return; // Skip images with too few keypoints
        }
    } catch (const cv::Exception& e) {
        qDebug() << "❌ SIFT validation failed on supplementary calibration:" << e.what();
        return;
    }
    
    // Store the image safely
    QMutexLocker lock(&m_mutex);
    try {
        m_additionalCalibrations.push_back(persistentCopy);
        qDebug() << "✅ Added supplementary calibration image #" 
                 << m_additionalCalibrations.size();
    } catch (const cv::Exception& e) {
        qDebug() << "❌ Failed to add supplementary calibration image:" << e.what();
    }
}

void HandDetector::clearSupplementaryCalibrations()
{
    QMutexLocker lock(&m_mutex);
    m_additionalCalibrations.clear();
    qDebug() << "Cleared all supplementary calibration images";
}

void HandDetector::setHSVThreshold(int hMin, int hMax, int sMin, int sMax, int vMin, int vMax)
{
    // Kept for API compatibility, but unused now
    Q_UNUSED(hMin); Q_UNUSED(hMax); Q_UNUSED(sMin);
    Q_UNUSED(sMax); Q_UNUSED(vMin); Q_UNUSED(vMax);
}

void HandDetector::setMinHandArea(double area)
{
    // Kept for API compatibility, but unused now
    Q_UNUSED(area);
}

std::vector<cv::Point> HandDetector::getHandContour() const
{
    return m_handContour;
}

std::vector<cv::Vec4i> HandDetector::getConvexityDefects() const
{
    return m_defects;
}
