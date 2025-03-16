#include "calibrationwidget.h"
#include <QMessageBox>
#include <QPainter>
#include <QPen>  // Ensure QPen is fully defined
#include <QDebug>
#include <cmath>

CalibrationWidget::CalibrationWidget(QWidget *parent)
    : QWidget(parent)
    , m_calibrationPhase(0)
{
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
    
    // Set this widget’s minimum size based on initial camera frame (as before)
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
    // Timer and other initialization remain as before
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
    // Capture a calibration image from the current frame
    if (m_frame.empty())
        return;
    // Instead of three steps, capture one good image
    QImage calibImage = matToQImage(m_frame);
    // For simplicity, save the center region of the image (or whole)
    // and assume it represents the hand. (In a full update you would prompt the user.)
    // Emit signal for calibration finished and store calibration image in calibration data.
    m_calibData.points[0] = m_calibrationSquare.center();  // Dummy data
    m_calibData.scale = 1.0;
    m_calibData.rotation = 0.0;
    m_calibData.offset = m_calibrationSquare.center();
    
    qDebug() << "Calibration completed using one-step capture.";
    emit calibrationFinished();
    
    // Optionally change UI texts:
    m_instructionLabel->setText("Calibration complete!");
    m_statusLabel->setText("Ready to map hand using FLANN matching.");
}

void CalibrationWidget::calculateCalibration()
{
    // Calculate calibration parameters based on the 3 points
    
    // 1. Calculate scale (based on distance between first two points)
    QPointF vec = m_calibData.points[1] - m_calibData.points[0];
    m_calibData.scale = sqrt(vec.x() * vec.x() + vec.y() * vec.y());
    
    // 2. Calculate rotation (angle between horizontal and line from point 0 to 1)
    m_calibData.rotation = atan2(vec.y(), vec.x());
    
    // 3. Calculate offset (average position of all three points)
    m_calibData.offset = (m_calibData.points[0] + m_calibData.points[1] + m_calibData.points[2]) / 3.0;
    
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
