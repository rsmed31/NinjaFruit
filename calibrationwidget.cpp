#include "calibrationwidget.h"
#include "handdetector.h"
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QDebug>
#include <cmath>
#include <QMutexLocker>

CalibrationWidget::CalibrationWidget(QWidget *parent, HandDetector* handDetector)
    : QWidget(parent)
    , m_calibrationPhase(0)
    , m_handDetector(handDetector)
{
    // Add verification that handDetector is valid
    if (!m_handDetector) {
        qDebug() << "❌ WARNING: CalibrationWidget created with NULL handDetector!";
    } else {
        qDebug() << "✅ CalibrationWidget created with valid handDetector";
    }
    
    // Use horizontal layout: left for camera view, right for instructions and button.
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    
    // Left side: will show the camera feed (drawn in paintEvent)
    QWidget* cameraArea = new QWidget(this);
    cameraArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Right side: vertical layout with instructions and capture button.
    QWidget* instructionArea = new QWidget(this);
    instructionArea->setFixedWidth(300);
    QVBoxLayout* instLayout = new QVBoxLayout(instructionArea);
    m_instructionLabel = new QLabel("Place your hand inside the red square and press Capture");
    m_instructionLabel->setAlignment(Qt::AlignCenter);
    m_instructionLabel->setStyleSheet("background-color: rgba(0,0,0,128); color: white; padding: 5px;");
    m_statusLabel = new QLabel("Calibration Phase 1/1");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("background-color: rgba(0,0,0,128); color: white; padding: 5px;");
    m_captureButton = new QPushButton("Capture Position");
    connect(m_captureButton, &QPushButton::clicked, this, &CalibrationWidget::captureCalibrationPoint);
    instLayout->addWidget(m_instructionLabel);
    instLayout->addWidget(m_statusLabel);
    instLayout->addWidget(m_captureButton);
    instLayout->addStretch();
    
    mainLayout->addWidget(cameraArea, 3);
    mainLayout->addWidget(instructionArea, 1);
    
    // Set this widget's minimum size based on initial camera frame
    m_camera.open(0);
    m_camera >> m_frame;
    if (!m_frame.empty()) {
        m_qImage = matToQImage(m_frame);
        setMinimumSize(m_qImage.size());
    } else {
        setMinimumSize(1280, 720);
    }
    
    // Calibration square is 50% of the camera area width
    int squareSize = width() / 2;
    m_calibrationSquare = QRect((width() - squareSize) / 2,
                                (height() - squareSize) / 2,
                                squareSize, squareSize);
    
    // Timer and other initialization
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CalibrationWidget::updateFrame);
    m_timer->start(30);
    resetCalibration();
}

CalibrationWidget::~CalibrationWidget()
{
    // Cleanup resources
    m_timer->stop();
    if (m_camera.isOpened()) {
        m_camera.release();
    }
}

void CalibrationWidget::updateFrame()
{
    if (m_camera.isOpened()) {
        m_camera >> m_frame;
        if (!m_frame.empty()) {
            m_qImage = matToQImage(m_frame);
            update(); // Trigger repaint
        }
    }
}

QImage CalibrationWidget::matToQImage(const cv::Mat& mat)
{
    // Convert OpenCV Mat to QImage
    if (mat.type() == CV_8UC3) {
        // Convert BGR to RGB for Qt
        cv::Mat rgbMat;
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
        return QImage(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                     static_cast<int>(rgbMat.step), QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC1) {
        // Grayscale image
        return QImage(mat.data, mat.cols, mat.rows, 
                     static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    }
    return QImage();
}

void CalibrationWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    
    // Draw the camera frame if available
    if (!m_qImage.isNull()) {
        painter.drawImage(0, 0, m_qImage);
    }
    
    // Draw calibration square
    painter.setPen(QPen(Qt::red, 3));
    painter.drawRect(m_calibrationSquare);
    
    // Draw calibration points that have been captured
    painter.setPen(QPen(Qt::green, 5));
    for (int i = 0; i < m_calibrationPhase && i < 1; ++i) {
        painter.drawEllipse(m_calibData.points[i].toPoint(), 5, 5);
    }
}

void CalibrationWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        captureCalibrationPoint();
    } else if (event->key() == Qt::Key_R) {
        resetCalibration();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void CalibrationWidget::captureCalibrationPoint()
{
    // Verify handDetector is available
    if (!m_handDetector) {
        qDebug() << "❌ HandDetector is NULL in captureCalibrationPoint";
        QMessageBox::critical(this, "Calibration Failed", 
                           "Internal error: Hand detector not initialized");
        return;
    }

    // Make sure we have a valid frame
    if (m_frame.empty()) {
        qDebug() << "❌ Initial frame is empty - forcing new capture";
        // Force multiple frame captures to ensure we get a valid one
        for (int retries = 0; retries < 10; retries++) {
            if (m_camera.isOpened()) {
                m_camera >> m_frame;
                if (!m_frame.empty()) {
                    qDebug() << "✅ Successfully captured frame on retry" << retries;
                    break;
                }
                QThread::msleep(100); // Short delay between attempts
            }
        }
        
        if (m_frame.empty()) {
            QMessageBox::warning(this, "Calibration Failed", 
                               "Could not capture frame after multiple attempts - check your camera");
            return;
        }
    }
    
    // Store calibration data
    m_calibData.points[0] = m_calibrationSquare.center();
    m_calibData.scale = 1.0;
    m_calibData.rotation = 0.0;
    m_calibData.offset = m_calibrationSquare.center();
    
    // Test SIFT feature detection
    cv::Ptr<cv::SIFT> sift = cv::SIFT::create();
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    
    try {
        sift->detectAndCompute(m_frame, cv::noArray(), keypoints, descriptors);
        qDebug() << "SIFT detected" << keypoints.size() << "keypoints in frame";
        
        if (keypoints.size() < 10) {
            QMessageBox::warning(this, "Poor Calibration Image", 
                               "The current image has few details. Please show your hand more clearly and try again.");
            return;
        }
    } catch (const cv::Exception& e) {
        qDebug() << "❌ SIFT detection failed:" << e.what();
        QMessageBox::warning(this, "Calibration Failed", 
                           "Image analysis failed - please try again with better lighting");
        return;
    }
    
    // Create a deep copy of the frame for permanent storage
    cv::Mat frameCopy = m_frame.clone();
    
    if (frameCopy.empty() || frameCopy.data == nullptr) {
        qDebug() << "❌ Failed to create valid frame copy";
        QMessageBox::warning(this, "Calibration Failed", 
                           "Memory error while processing image - please try again");
        return;
    }
    
    qDebug() << "Frame info: " << frameCopy.cols << "x" << frameCopy.rows 
             << "type:" << frameCopy.type() 
             << "channels:" << frameCopy.channels();

    // Extract ONLY the calibration square region
    cv::Rect roi(m_calibrationSquare.x(), m_calibrationSquare.y(), 
                 m_calibrationSquare.width(), m_calibrationSquare.height());
    
    // Make sure ROI is within frame bounds
    roi = roi & cv::Rect(0, 0, frameCopy.cols, frameCopy.rows);
    
    // Check if ROI is valid
    if (roi.width <= 10 || roi.height <= 10) {
        qDebug() << "❌ Invalid ROI for calibration: " << roi.x << "," << roi.y << " " << roi.width << "x" << roi.height;
        QMessageBox::warning(this, "Calibration Failed", 
                           "Calibration region invalid - please try again");
        return;
    }
    
    // Extract only the hand region (ROI)
    cv::Mat handRegion = frameCopy(roi).clone();
    qDebug() << "Extracted hand region: " << handRegion.cols << "x" << handRegion.rows;
    
    // Stop timer and close camera
    m_timer->stop();
    if (m_camera.isOpened()) {
        m_camera.release();
        qDebug() << "Camera released during calibration";
    }
    
    // Draw visual marker on calibration image for verification
    cv::circle(handRegion, cv::Point(handRegion.cols/2, handRegion.rows/2), 20, cv::Scalar(0, 255, 0), 2);
    cv::putText(handRegion, "HAND", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
    
    // Save a debug copy of the calibration image to user's desktop
    std::string calibFilename = "c:\\Users\\enmoh\\Desktop\\Ninja\\NinjaFruit\\hand_calibration.jpg";
    try {
        cv::imwrite(calibFilename, handRegion);
        qDebug() << "✅ Saved calibration image to: " << calibFilename.c_str();
    } catch (const cv::Exception& e) {
        qDebug() << "❌ Failed to save calibration image: " << e.what();
    }
    
    try {
        // Apply the calibration image to the HandDetector - using ONLY the hand region
        m_handDetector->setCalibrationImage(handRegion);
        
        // Update UI
        m_instructionLabel->setText("Calibration complete!");
        m_statusLabel->setText("Ready to track using FLANN matching");
        m_calibrationPhase = 1; // Mark as calibrated
        update(); // Refresh display
        
        qDebug() << "✅ Calibration image successfully set";
        
        // Signal completion - this will trigger MainWindow::onCalibrationFinished
        emit calibrationFinished();
    }
    catch (const std::exception& e) {
        qDebug() << "❌ Exception setting calibration image:" << e.what();
        QMessageBox::warning(this, "Calibration Failed", 
                           "Error saving calibration image: " + QString(e.what()));
        
        // Try to restart camera on failure
        try {
            m_camera.open(0);
            if (m_camera.isOpened()) {
                m_timer->start(30);
            } else {
                qDebug() << "❌ Failed to reopen camera after calibration failure";
            }
        } catch (const std::exception& e) {
            qDebug() << "❌ Exception reopening camera:" << e.what();
        }
    }
}

void CalibrationWidget::calculateCalibration()
{
    // Calculate calibration parameters based on the stored points
    // This simplified version just sets basic data
    m_calibData.scale = 1.0;
    m_calibData.rotation = 0.0;
    m_calibData.offset = m_calibData.points[0];
    
    qDebug() << "Calibration completed with scale:" << m_calibData.scale
             << "rotation:" << m_calibData.rotation * 180 / M_PI << "degrees"
             << "offset:" << m_calibData.offset;
             
    // Emit signal that calibration is complete
    emit calibrationFinished();
}

void CalibrationWidget::resetCalibration()
{
    m_calibrationPhase = 0;
    m_calibData = CalibrationData();
    
    m_instructionLabel->setText("Place your hand in the calibration square and press Capture");
    m_statusLabel->setText("Calibration Phase 1/1");
    m_captureButton->setText("Capture Position");
    
    disconnect(m_captureButton, &QPushButton::clicked, this, &CalibrationWidget::resetCalibration);
    connect(m_captureButton, &QPushButton::clicked, this, &CalibrationWidget::captureCalibrationPoint);
    
    update();
}

CalibrationWidget::CalibrationData CalibrationWidget::getCalibrationData() const
{
    return m_calibData;
}
