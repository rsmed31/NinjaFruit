#include "handdetector.h"
#include <iostream>
#include <QDebug>
#include <QMutexLocker>

HandDetector::HandDetector()
    : m_isFirstFrame(true), 
      m_prevHandPos(-1, -1),
      m_frameCounter(0),
      m_contourFrameCounter(0)
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
    
    // Increment frame counters
    m_frameCounter++;
    m_contourFrameCounter++;
    
    //======== STEP 1: Downscale input frame to 50% (less aggressive downscale) ========
    cv::Mat smallFrame;
    const float scaleFactor = 0.5f; // 50% size: smoother, still performant
    cv::resize(frame, smallFrame, cv::Size(), scaleFactor, scaleFactor);

    //======== STEP 2: FLANN matching against calibration images ========
    // Use motion-based ROI for faster processing
    cv::Rect motionROI = detectMotionROI(smallFrame);
    
    // Track last successful position for smoothing
    static cv::Point lastPosition(-1, -1);
    cv::Point result(-1, -1);
    
    // Try primary calibration image every frame
    result = matchImageFLANN(m_calibrationImage, smallFrame, motionROI);
    
    // Try supplementary calibration images only every 5 frames (reduced frequency)
    if (result.x < 0 && !m_additionalCalibrations.empty() && m_frameCounter % 5 == 0) {
        for (const auto& calibImg : m_additionalCalibrations) {
            result = matchImageFLANN(calibImg, smallFrame, motionROI);
            if (result.x >= 0) {
                break;
            }
        }
    }
    
    //======== STEP 5: Position smoothing ========
    // Scale result back to original image size
    if (result.x >= 0) {
        result.x = static_cast<int>(result.x / scaleFactor);
        result.y = static_cast<int>(result.y / scaleFactor);
        
        // Apply 80-20 temporal smoothing if we have a previous position
        if (lastPosition.x >= 0) {
            result.x = static_cast<int>(0.8 * result.x + 0.2 * lastPosition.x);
            result.y = static_cast<int>(0.8 * result.y + 0.2 * lastPosition.y);
        }
        lastPosition = result;
        
        //======== STEP 6-8: Shape validation (only every 3 frames) ========
        if (m_contourFrameCounter % 3 == 0) {
            // Extract ROI around the detected position for contour detection
            int roiSize = 200; // 200x200 ROI
            cv::Rect handROI(
                std::max(0, result.x - roiSize/2),
                std::max(0, result.y - roiSize/2),
                std::min(frame.cols - (result.x - roiSize/2), roiSize),
                std::min(frame.rows - (result.y - roiSize/2), roiSize)
            );
            
            // Validate hand shape in the ROI
            if (handROI.width > 20 && handROI.height > 20) {
                validateHandShape(frame(handROI), result);
            }
            
            // Reset counter
            m_contourFrameCounter = 0;
        }
    }
    
    // Store current frame as previous for next motion detection
    smallFrame.copyTo(m_prevFrame);
    m_prevHandPos = result;
    m_isFirstFrame = false;
    
    return result;
}

cv::Rect HandDetector::detectMotionROI(const cv::Mat &frame)
{
    // Default to full frame ROI
    cv::Rect fullFrameROI(0, 0, frame.cols, frame.rows);
    
    // If first frame or no previous frame, use full frame
    if (m_isFirstFrame || m_prevFrame.empty()) {
        return fullFrameROI;
    }
    
    // Compute frame difference to detect motion
    cv::Mat frameDiff, grayFrame, grayPrevFrame, motionMask;
    
    // Convert frames to grayscale
    cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
    cv::cvtColor(m_prevFrame, grayPrevFrame, cv::COLOR_BGR2GRAY);
    
    // Apply blur to reduce noise
    cv::GaussianBlur(grayFrame, grayFrame, cv::Size(5, 5), 0);
    cv::GaussianBlur(grayPrevFrame, grayPrevFrame, cv::Size(5, 5), 0);
    
    // Compute absolute difference
    cv::absdiff(grayFrame, grayPrevFrame, frameDiff);
    
    // Threshold to create binary mask of motion areas
    cv::threshold(frameDiff, motionMask, 25, 255, cv::THRESH_BINARY);
    
    // Apply morphological operations to clean up the mask
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 15));
    cv::dilate(motionMask, motionMask, kernel);
    
    // Find contours in the motion mask
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(motionMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // If no motion detected or previous hand position unknown, use full frame
    if (contours.empty() || m_prevHandPos.x < 0) {
        return fullFrameROI;
    }
    
    // Find the largest contour (most significant motion)
    int largestIdx = -1;
    double largestArea = 0;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > largestArea) {
            largestArea = area;
            largestIdx = i;
        }
    }
    
    // Get bounding rectangle of largest motion area
    if (largestIdx >= 0 && largestArea > 500) { // Minimum area threshold
        cv::Rect motionRect = cv::boundingRect(contours[largestIdx]);
        
        // Expand ROI by 50% in each direction to ensure hand is fully captured
        int expandX = motionRect.width / 2;
        int expandY = motionRect.height / 2;
        
        motionRect.x = std::max(0, motionRect.x - expandX);
        motionRect.y = std::max(0, motionRect.y - expandY);
        motionRect.width = std::min(frame.cols - motionRect.x, motionRect.width + expandX * 2);
        motionRect.height = std::min(frame.rows - motionRect.y, motionRect.height + expandY * 2);
        
        return motionRect;
    }
    
    // Fallback to searching around the previous hand position
    if (m_prevHandPos.x >= 0) {
        int searchRadius = 100; // Search radius around previous position
        cv::Point scaledPrevPos(m_prevHandPos.x / 2, m_prevHandPos.y / 2); // Adjust for small frame
        
        cv::Rect prevPosRect(
            std::max(0, scaledPrevPos.x - searchRadius),
            std::max(0, scaledPrevPos.y - searchRadius),
            std::min(frame.cols - (scaledPrevPos.x - searchRadius), searchRadius * 2),
            std::min(frame.rows - (scaledPrevPos.y - searchRadius), searchRadius * 2)
        );
        
        return prevPosRect;
    }
    
    return fullFrameROI;
}

cv::Point HandDetector::matchImageFLANN(const cv::Mat &refImage, const cv::Mat &frame, const cv::Rect &roi)
{
    try {
        // Crop frame to ROI for faster processing
        cv::Mat roiFrame = frame(roi).clone();
        
        //======== STEP 2: SIFT feature extraction ========
        // Configure SIFT for much fewer features and faster processing
        cv::Ptr<cv::SIFT> sift = cv::SIFT::create(
            20,     // Reduced from 50 to 20 features
            3,      // Reduced octave layers
            0.04,   // Increased contrast threshold
            10,     // Increased edge threshold 
            1.6     // Standard sigma
        ); 
        
        std::vector<cv::KeyPoint> keypointsRef, keypointsFrame;
        cv::Mat descriptorsRef, descriptorsFrame;
        
        // Extract features only from the reference image once
        static bool refFeaturesComputed = false;
        static std::vector<cv::KeyPoint> cachedKeypointsRef;
        static cv::Mat cachedDescriptorsRef;
        
        // Compute reference features only once and cache them
        if (!refFeaturesComputed) {
            sift->detectAndCompute(refImage, cv::noArray(), cachedKeypointsRef, cachedDescriptorsRef);
            refFeaturesComputed = true;
        }
        keypointsRef = cachedKeypointsRef;
        descriptorsRef = cachedDescriptorsRef;
        
        // Quick check - if reference image has too few descriptors, exit early
        if (descriptorsRef.empty() || keypointsRef.size() < 3) {
            return cv::Point(-1, -1);
        }
        
        // Extract features from current frame
        sift->detectAndCompute(roiFrame, cv::noArray(), keypointsFrame, descriptorsFrame);
        
        // EARLY TRIGGER: If we have enough descriptors, we consider it a potential match
        // Minimum threshold for number of keypoints in frame
        const int MIN_KEYPOINTS = 5;
        if (descriptorsFrame.empty() || keypointsFrame.size() < MIN_KEYPOINTS) {
            return cv::Point(-1, -1);
        }
        
        //======== STEP 3: FLANN matching - more efficient configuration ========
        // Use a more efficient search - fewer trees, fewer checks
        cv::FlannBasedMatcher matcher(
            new cv::flann::KDTreeIndexParams(2),     // Fewer trees
            new cv::flann::SearchParams(32)          // Fewer checks
        );
        
        std::vector<std::vector<cv::DMatch>> knnMatches;
        matcher.knnMatch(descriptorsRef, descriptorsFrame, knnMatches, 2);
        
        // Apply Lowe's ratio test with faster implementation
        // Preallocate vectors to avoid reallocations
        std::vector<cv::Point2f> pointsRef, pointsFrame;
        pointsRef.reserve(knnMatches.size());
        pointsFrame.reserve(knnMatches.size());
        
        // Simplified matching loop with fewer filters for speed
        const float RATIO_THRESHOLD = 0.75f; // Slightly relaxed ratio test
        int goodMatchCount = 0;
        
        for (const auto& match : knnMatches) {
            if (match.size() < 2) continue;
            
            // Simplified Lowe's ratio test only, skip other filters
            if (match[0].distance < RATIO_THRESHOLD * match[1].distance) {
                // Get the keypoints from this match
                cv::KeyPoint kpRef = keypointsRef[match[0].queryIdx];
                cv::KeyPoint kpFrame = keypointsFrame[match[0].trainIdx];
                
                // Store points for homography
                pointsRef.push_back(kpRef.pt);
                pointsFrame.push_back(cv::Point2f(kpFrame.pt.x + roi.x, kpFrame.pt.y + roi.y)); // Adjust for ROI offset
                goodMatchCount++;
            }
        }
        
        // EFFICIENT TRIGGER: Skip expensive RANSAC if we don't have enough good matches
        // Minimum threshold for number of good matches
        const int MIN_GOOD_MATCHES = 3;
        if (goodMatchCount < MIN_GOOD_MATCHES) {
            return cv::Point(-1, -1);
        }
        
        // SIMPLIFY: Skip homography for speed with small point sets
        if (pointsRef.size() < 8) {
            // With fewer points, just use centroid directly
            cv::Point2f centroid(0.0f, 0.0f);
            for (const auto& pt : pointsFrame) {
                centroid += pt;
            }
            centroid.x /= pointsFrame.size();
            centroid.y /= pointsFrame.size();
            
            return cv::Point(static_cast<int>(centroid.x), static_cast<int>(centroid.y));
        }
        
        //======== STEP 4: RANSAC geometric filtering (only for larger point sets) ========
        // Use RANSAC with homography for geometric consistency check
        cv::Mat inliersMask;
        cv::Mat H = cv::findHomography(pointsRef, pointsFrame, cv::RANSAC, 5.0, inliersMask);
        
        // Filter to only use RANSAC inliers
        std::vector<cv::Point2f> inlierPoints;
        inlierPoints.reserve(pointsFrame.size());
        
        for (size_t i = 0; i < pointsFrame.size(); i++) {
            if (inliersMask.at<uchar>(i) > 0) {
                inlierPoints.push_back(pointsFrame[i]);
            }
        }
        
        // If we don't have enough inliers, return no detection
        if (inlierPoints.size() < 3) {
            return cv::Point(-1, -1);
        }
        
        // Compute final position as average of inlier points (barycenter)
        cv::Point2f centroid(0.0f, 0.0f);
        for (const auto& pt : inlierPoints) {
            centroid += pt;
        }
        centroid.x /= inlierPoints.size();
        centroid.y /= inlierPoints.size();
        
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

bool HandDetector::validateHandShape(const cv::Mat &roiFrame, cv::Point &handPos)
{
    try {
        //======== STEP 7: Contour + Convexity Detection ========
        cv::Mat gray, thresh;
        cv::cvtColor(roiFrame, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
        cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
        
        // Apply morphological operations
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
        cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);
        
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (contours.empty()) {
            return false;
        }
        
        // Find largest contour based on contour size
        auto largestContour = *std::max_element(contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return a.size() < b.size(); // Compare by size
            });
        
        // Compute convex hull and convexity defects
        std::vector<int> hull;
        cv::convexHull(largestContour, hull);
        
        if (hull.size() > 3) {
            std::vector<cv::Vec4i> defects;
            std::vector<int> hullIndices;
            cv::convexHull(largestContour, hullIndices, false);
            
            // Compute convexity defects if contour is large enough
            if (largestContour.size() > 3) {
                try {
                    cv::convexityDefects(largestContour, hullIndices, defects);
                    
                    //======== STEP 8: Shape Validation ========
                    // Count significant defects (finger valleys)
                    int significantDefects = 0;
                    for (const auto& defect : defects) {
                        // Extract defect data
                        float depth = defect[3] / 256.0f;
                        if (depth > 10.0f) { // Depth threshold for significant defects
                            significantDefects++;
                        }
                    }
                    
                    // Save contour for visualization
                    m_handContour = largestContour;
                    // Adjust for ROI offset
                    for (auto& pt : m_handContour) {
                        pt.x += handPos.x - roiFrame.cols/2;
                        pt.y += handPos.y - roiFrame.rows/2;
                    }
                    m_defects = defects;
                    
                    // Validate there are at least 2 finger valleys (>= 3 fingers)
                    return significantDefects >= 2;
                }
                catch (const cv::Exception&) {
                    return false;
                }
            }
        }
        
        return false;
    }
    catch (const cv::Exception&) {
        return false;
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
    
    // OPTIMIZATION: Resize the calibration image to be smaller for faster matching
    cv::Mat resizedImage;
    cv::resize(image, resizedImage, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);
    
    // Make a persistent copy of the resized image
    cv::Mat persistentCopy = resizedImage.clone();
    
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
    
    // OPTIMIZATION: Resize the calibration image to be smaller for faster matching
    cv::Mat resizedImage;
    cv::resize(image, resizedImage, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);
    
    // Make a persistent copy of the resized image
    cv::Mat persistentCopy = resizedImage.clone();
    
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
