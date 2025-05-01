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
    // First, check if we have a calibration image
    if (frame.empty()) {
        qDebug() << "Empty frame passed to detectHand";
        return cv::Point(-1, -1);
    }
    
    cv::Mat calibImg;
    {
        QMutexLocker lock(&m_mutex);
        if (m_calibrationImage.empty()) {
            static int warningCount = 0;
            if (warningCount++ % 30 == 0) { // Log only occasionally
                qDebug() << "No calibration image available for hand detection";
            }
            return cv::Point(-1, -1);
        }
        calibImg = m_calibrationImage.clone(); // Safe copy under lock
    }
    
    // Now do FLANN detection with SIFT features
    try {
        // Create SIFT detector
        cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
        std::vector<cv::KeyPoint> keypointsRef, keypointsFrame;
        cv::Mat descriptorsRef, descriptorsFrame;
        
        // Extract features
        sift->detectAndCompute(calibImg, cv::noArray(), keypointsRef, descriptorsRef);
        sift->detectAndCompute(frame, cv::noArray(), keypointsFrame, descriptorsFrame);
        
        // Check if we found any features
        if (descriptorsRef.empty() || descriptorsFrame.empty()) {
            return cv::Point(-1, -1);
        }
        
        // Now do FLANN matching
        cv::FlannBasedMatcher matcher;
        std::vector<std::vector<cv::DMatch>> knnMatches;
        matcher.knnMatch(descriptorsRef, descriptorsFrame, knnMatches, 2);
        
        // Apply Lowe's ratio test to filter matches
        const float ratioThresh = 0.7f;
        std::vector<cv::DMatch> goodMatches;
        for (const auto& match : knnMatches) {
            if (match.size() >= 2 && match[0].distance < ratioThresh * match[1].distance) {
                goodMatches.push_back(match[0]);
            }
        }
        
        // Check if we have enough good matches
        if (goodMatches.size() < 4) {
            return cv::Point(-1, -1);
        }
        
        // Compute centroid from good matches
        cv::Point2f centroid(0.0f, 0.0f);
        for (const auto& match : goodMatches) {
            centroid += keypointsFrame[match.trainIdx].pt;
        }
        centroid.x /= static_cast<float>(goodMatches.size());
        centroid.y /= static_cast<float>(goodMatches.size());
        cv::Point handPos(static_cast<int>(centroid.x), static_cast<int>(centroid.y));
        
        // We've successfully found the hand position with FLANN matching 
        // Now we'll try to find a contour for visualization, but we'll skip
        // the convexity defects calculation since it causes errors
        
        try {
            // Create a region of interest around the detected hand
            const int radius = 100; // Smaller ROI for more focused detection
            cv::Rect roi(
                std::max(0, handPos.x - radius),
                std::max(0, handPos.y - radius),
                std::min(frame.cols - (handPos.x - radius), radius * 2),
                std::min(frame.rows - (handPos.y - radius), radius * 2)
            );
            
            // Skip contour detection if ROI is too small
            if (roi.width >= 20 && roi.height >= 20) {
                cv::Mat roiImg = frame(roi);
                cv::Mat gray, blurred, thresh;
                
                // Use a more robust pre-processing pipeline
                cv::cvtColor(roiImg, gray, cv::COLOR_BGR2GRAY);
                cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
                cv::adaptiveThreshold(blurred, thresh, 255, 
                                     cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                     cv::THRESH_BINARY_INV, 11, 2);
                
                // Find contours
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(thresh, contours, cv::RETR_EXTERNAL, 
                                cv::CHAIN_APPROX_SIMPLE);
                
                // Find the largest contour
                if (!contours.empty()) {
                    auto largestContour = *std::max_element(
                        contours.begin(), contours.end(),
                        [](const auto& a, const auto& b) { 
                            return cv::contourArea(a) < cv::contourArea(b); 
                        }
                    );
                    
                    // Simplify contour
                    std::vector<cv::Point> approx;
                    cv::approxPolyDP(largestContour, approx, 
                                    cv::arcLength(largestContour, true) * 0.01, true);
                    
                    // Store the contour in global coordinates
                    m_handContour = approx;
                    for (auto& pt : m_handContour) {
                        pt.x += roi.x;
                        pt.y += roi.y;
                    }
                    
                    // Skip convexity defects calculation - just clear them
                    m_defects.clear();
                }
            }
        }
        catch (const cv::Exception& e) {
            qDebug() << "Contour processing error:" << e.what();
            m_handContour.clear();
            m_defects.clear();
        }
        
        // Return hand position regardless of contour processing success
        return handPos;
    }
    catch (const cv::Exception& e) {
        qDebug() << "OpenCV error in detectHand:" << e.what();
        return cv::Point(-1, -1);
    }
    catch (const std::exception& e) {
        qDebug() << "Error in detectHand:" << e.what();
        return cv::Point(-1, -1);
    }
}

void HandDetector::setCalibrationImage(const cv::Mat &image)
{
    if (image.empty()) {
        qDebug() << "❌ Calibration image empty — skipping";
        return;
    }
    
    qDebug() << "Setting calibration image:" << image.cols << "x" << image.rows 
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
        qDebug() << "✅ Calibration image set:" 
                 << m_calibrationImage.cols << "x" 
                 << m_calibrationImage.rows;
    } catch (const cv::Exception& e) {
        qDebug() << "❌ Failed to set calibration image:" << e.what();
        m_calibrationImage.release();
    }
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
