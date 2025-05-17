#include "handdetector.h"
#include <iostream>
#include <QDebug>
#include <QMutexLocker>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <vector>

HandDetector::HandDetector()
    : m_isFirstFrame(true), 
      m_prevHandPos(-1, -1),
      m_frameCounter(0),
      m_contourFrameCounter(0)
{
    qDebug() << "HandDetector initialized";
}

cv::Point HandDetector::fallbackToORB(const cv::Mat &frame) {
    if (frame.empty()) {
        qDebug() << "❌ Frame is empty in fallbackToORB.";
        return cv::Point(-1, -1);
    }

    try {
        // Initialize ORB detector
        cv::Ptr<cv::ORB> orb = cv::ORB::create(500); // ORB with 500 features
        std::vector<cv::KeyPoint> keypointsFrame;
        cv::Mat descriptorsFrame;

        // Detect and compute ORB features
        orb->detectAndCompute(frame, cv::noArray(), keypointsFrame, descriptorsFrame);

        if (descriptorsFrame.empty() || keypointsFrame.empty()) {
            qDebug() << "❌ No ORB features detected in the frame.";
            return cv::Point(-1, -1);
        }

        // Match descriptors using BFMatcher
        cv::BFMatcher bfMatcher(cv::NORM_HAMMING);
        std::vector<cv::DMatch> matches;
        bfMatcher.match(m_refDescriptors, descriptorsFrame, matches);

        if (matches.empty()) {
            qDebug() << "❌ No ORB matches found.";
            return cv::Point(-1, -1);
        }

        // Compute the centroid of matched keypoints
        cv::Point2f centroid(0.0f, 0.0f);
        for (const auto &match : matches) {
            centroid += keypointsFrame[match.trainIdx].pt;
        }
        centroid.x /= matches.size();
        centroid.y /= matches.size();

        qDebug() << "✅ ORB fallback centroid:" << centroid.x << "," << centroid.y;
        return cv::Point(static_cast<int>(centroid.x), static_cast<int>(centroid.y));
    } catch (const cv::Exception &e) {
        qDebug() << "❌ OpenCV exception in fallbackToORB:" << e.what();
        return cv::Point(-1, -1);
    } catch (const std::exception &e) {
        qDebug() << "❌ Standard exception in fallbackToORB:" << e.what();
        return cv::Point(-1, -1);
    }
}

cv::Point HandDetector::detectHand(const cv::Mat &frame) {
    if (frame.empty() || m_calibrationImage.empty())
        return fallbackToORB(frame);
    QMutexLocker lock(&m_mutex);

    // 1. Skin segmentation
    const cv::Scalar lower(0, 30, 60), upper(20, 150, 255);
    cv::Mat hsv, mask;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, lower, upper, mask);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, cv::Mat(), cv::Point(-1,-1), 2);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1,-1), 2);

    // 2. Contour + hull centroid
    std::vector<std::vector<cv::Point>> ctrs;
    cv::findContours(mask, ctrs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double maxA=0; int idx=-1;
    for (int i=0; i<(int)ctrs.size(); ++i) {
        double a=cv::contourArea(ctrs[i]);
        if (a>maxA) { maxA=a; idx=i; }
    }
    cv::Point2f hullCentroid(-1,-1);
    std::vector<cv::Point> largest = (idx>=0 && maxA>1000) ? ctrs[idx] : std::vector<cv::Point>();
    if (!largest.empty()) {
        // centroid of contour
        for (auto& p: largest) hullCentroid += cv::Point2f(p);
        hullCentroid *= (1.f/(float)largest.size());
        // convex defects
        std::vector<int> hullIdx; cv::convexHull(largest, hullIdx, false);
        if (hullIdx.size()>3) {
            std::vector<cv::Vec4i> defects;
            cv::convexityDefects(largest, hullIdx, defects);
            int cnt=0;
            for (auto& d: defects) if (d[3]/256.0f>10.0f) ++cnt;
            if (cnt<2) hullCentroid = cv::Point2f(-1,-1);
        }
    }

    // 3. ROI
    cv::Rect roi = (largest.empty())
        ? cv::Rect(0,0,frame.cols,frame.rows)
        : cv::boundingRect(largest);
    int padX=roi.width/5, padY=roi.height/5;
    roi.x = std::max(0, roi.x-padX);
    roi.y = std::max(0, roi.y-padY);
    roi.width = std::min(frame.cols-roi.x, roi.width+2*padX);
    roi.height= std::min(frame.rows-roi.y, roi.height+2*padY);

    // 4. SIFT + FLANN matching
    cv::Mat rf = frame(roi), descF;
    std::vector<cv::KeyPoint> kf;
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    sift->detectAndCompute(rf, cv::noArray(), kf, descF);
    if (descF.empty()) return fallbackToORB(frame);
    cv::FlannBasedMatcher matcher(
        cv::makePtr<cv::flann::KDTreeIndexParams>(4),
        cv::makePtr<cv::flann::SearchParams>(64)
    );
    std::vector<std::vector<cv::DMatch>> knn;
    matcher.knnMatch(m_refDescriptors, descF, knn, 2);
    std::vector<cv::Point2f> ptsRef, ptsFrame;
    for (auto& m: knn) {
        if (m.size()==2 && m[0].distance < RATIO_THRESHOLD*m[1].distance) {
            ptsRef.emplace_back(m_refPoints[m[0].queryIdx]);
            auto pt = kf[m[0].trainIdx].pt + cv::Point2f((float)roi.x, (float)roi.y);
            ptsFrame.push_back(pt);
        }
    }
    if (ptsFrame.size()<4) return fallbackToORB(frame);
    // 5. RANSAC homography → siftCentroid
    cv::Mat inlMask;
    cv::Mat H = cv::findHomography(ptsRef, ptsFrame, cv::RANSAC, 3.0, inlMask);
    if (H.empty()) return fallbackToORB(frame);
    cv::Point2f siftC(0,0); int inlCount=0;
    for (int i=0; i<(int)ptsFrame.size(); ++i) {
        if (inlMask.at<uchar>(i)) {
            siftC += ptsFrame[i]; ++inlCount;
            m_lastInlierPoints.push_back(ptsFrame[i]);
        }
    }
    if (inlCount==0) return fallbackToORB(frame);
    siftC *= (1.f/inlCount);
    m_lastRawPos = siftC;

    // 6. Fusion & smoothing
    float alpha = 0.7f;
    cv::Point2f raw = (hullCentroid.x>=0)
        ? alpha*siftC + (1-alpha)*hullCentroid
        : siftC;
    m_posBuffer.push_back(raw);
    if (m_posBuffer.size()>5) m_posBuffer.pop_front();
    cv::Point2f buf(0,0);
    for (auto& p: m_posBuffer) buf += p;
    buf *= (1.f/m_posBuffer.size());
    // dead-zone
    if (std::abs(buf.x-m_lastReported.x)<5 && std::abs(buf.y-m_lastReported.y)<5)
        buf = m_lastReported;
    // clamp
    buf.x = std::clamp(buf.x, 0.f, (float)frame.cols);
    buf.y = std::clamp(buf.y, 0.f, (float)frame.rows);
    m_lastReported = buf;

    return cv::Point((int)buf.x, (int)buf.y);
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

cv::Point HandDetector::matchImageFLANN(const cv::Mat &refImage,
                                       const cv::Mat &frame,
                                       const cv::Rect &roi) {
    // Validation checks
    if (m_refDescriptors.empty() || m_refPoints.empty() || frame.empty() || 
        roi.width <= 0 || roi.height <= 0) {
        qDebug() << "❌ Invalid frame, ROI, or reference data in matchImageFLANN. Skipping matching.";
        return cv::Point(-1, -1);
    }

    try {
        // Crop and compute SIFT
        cv::Mat roiFrame = frame(roi).clone();
        cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
        std::vector<cv::KeyPoint> keypointsFrame;
        cv::Mat descriptorsFrame;
        sift->detectAndCompute(roiFrame, cv::noArray(), keypointsFrame, descriptorsFrame);

        if (descriptorsFrame.empty() || keypointsFrame.size() < MIN_KEYPOINTS) {
            return cv::Point(-1, -1);
        }

        // Match descriptors via FlannBasedMatcher
        cv::FlannBasedMatcher matcher(
            cv::makePtr<cv::flann::KDTreeIndexParams>(4),
            cv::makePtr<cv::flann::SearchParams>(64)
        );
        std::vector<std::vector<cv::DMatch>> knnMatches;
        matcher.knnMatch(m_refDescriptors, descriptorsFrame, knnMatches, 2);

        // Apply Lowe's ratio test
        std::vector<cv::Point2f> pointsRef, pointsFrame;
        for (const auto &match : knnMatches) {
            if (match.size() >= 2 &&
                match[0].distance < RATIO_THRESHOLD * match[1].distance) {
                pointsRef.push_back(m_refPoints[match[0].queryIdx]);
                cv::Point2f pf = keypointsFrame[match[0].trainIdx].pt;
                pointsFrame.push_back(pf + cv::Point2f((float)roi.x, (float)roi.y));
            }
        }

        if (pointsFrame.size() < MIN_GOOD_MATCHES) {
            return cv::Point(-1, -1);
        }

        cv::Mat inliersMask;
        cv::Mat H = cv::findHomography(pointsRef, pointsFrame,
                                        cv::RANSAC, 3.0, inliersMask);
        if (H.empty() || cv::countNonZero(inliersMask) < MIN_GOOD_MATCHES) {
            return cv::Point(-1, -1);
        }

        // Compute centroid of inlier points
        cv::Point2f rawPos(0, 0);
        int count = 0;
        for (size_t i = 0; i < pointsFrame.size(); ++i) {
            if (inliersMask.at<uchar>(i)) {
                rawPos += pointsFrame[i];
                ++count;
                m_lastInlierPoints.push_back(pointsFrame[i]);
            }
        }
        if (count) rawPos *= (1.f / count);
        m_lastRawPos = rawPos;

        return cv::Point(static_cast<int>(rawPos.x), static_cast<int>(rawPos.y));
    } catch (const cv::Exception &) {
        return cv::Point(-1, -1);
    } catch (const std::exception &) {
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

void HandDetector::setCalibrationImage(const cv::Mat &image) {
    if (image.empty()) {
        qDebug() << "❌ Calibration image empty — skipping";
        return;
    }

    qDebug() << "Setting primary calibration image:" << image.cols << "x" << image.rows 
             << "type:" << image.type() << "channels:" << image.channels();

    // Resize the calibration image to be smaller for faster matching
    cv::Mat resizedImage;
    cv::resize(image, resizedImage, cv::Size(), 0.5, 0.5, cv::INTER_LINEAR);

    // Ensure we have a proper BGR image for SIFT processing
    cv::Mat persistentCopy = resizedImage.clone();
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
        qDebug() << "❌ Failed to convert/copy calibration image";
        return;
    }

    // Detect and compute SIFT features for the calibration image
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    try {
        sift->detectAndCompute(persistentCopy, cv::noArray(), m_refKeypoints, m_refDescriptors);

        // Validation checks
        if (m_refKeypoints.empty() || m_refDescriptors.empty() || 
            m_refDescriptors.rows != static_cast<int>(m_refKeypoints.size()) || 
            m_refDescriptors.type() != CV_32F) {
            qDebug() << "❌ Invalid SIFT descriptors or keypoints in calibration image";
            m_refKeypoints.clear();
            m_refDescriptors.release();
            return;
        }

        // Fill m_refPoints from m_refKeypoints
        m_refPoints.clear();
        for (const auto &kp : m_refKeypoints) {
            m_refPoints.push_back(kp.pt);
        }
    } catch (const cv::Exception &e) {
        qDebug() << "❌ SIFT detection failed on calibration image:" << e.what();
        return;
    }

    // Store the calibration image
    QMutexLocker lock(&m_mutex);
    try {
        persistentCopy.copyTo(m_calibrationImage);
        qDebug() << "✅ Primary calibration image set:" 
                 << m_calibrationImage.cols << "x" 
                 << m_calibrationImage.rows;
    } catch (const cv::Exception &e) {
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

std::vector<cv::Point> HandDetector::getLastContour() const
{
    // Return the last detected contour (replace with actual logic)
    return m_handContour; // Assuming `lastContour` is a member variable
}

std::vector<cv::Vec4i> HandDetector::getLastDefects() const
{
    // Return the last detected defects (replace with actual logic)
    return m_defects; // Assuming `lastDefects` is a member variable
}

std::vector<cv::Point2f> HandDetector::getProjectedCorners() const {
    // Return the projected corners of the homography quad
    // Replace with actual logic if needed
    return std::vector<cv::Point2f>{
        cv::Point2f(0, 0),
        cv::Point2f(100, 0),
        cv::Point2f(100, 100),
        cv::Point2f(0, 100)
    };
}

const std::vector<cv::Point2f>& HandDetector::getLastInlierPoints() const {
    return m_lastInlierPoints;
}

const cv::Point2f& HandDetector::getLastRawPos() const {
    return m_lastRawPos;
}

