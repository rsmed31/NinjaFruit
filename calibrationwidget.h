#ifndef CALIBRATIONWIDGET_H
#define CALIBRATIONWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QMutex>
#include <QThread>  // Add this for QThread::msleep
#include <opencv2/opencv.hpp>

class HandDetector;

class CalibrationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CalibrationWidget(QWidget *parent = nullptr, HandDetector* handDetector = nullptr);
    ~CalibrationWidget();

    // Structure to store calibration data
    struct CalibrationData {
        double scale;
        QPointF offset;
        double rotation;
        QPointF points[3]; // Three calibration points
    };
    
    // Get the current calibration data
    CalibrationData getCalibrationData() const;
    
    // Reset calibration
    void resetCalibration();

signals:
    void calibrationFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateFrame();
    void captureCalibrationPoint();

private:
    // OpenCV camera capture
    cv::VideoCapture m_camera;
    cv::Mat m_frame;
    QImage m_qImage;
    QMutex m_mutex;
    
    // Calibration state
    QRect m_calibrationSquare;
    int m_calibrationPhase;
    CalibrationData m_calibData;
    
    // UI elements
    QTimer* m_timer;
    QPushButton* m_captureButton;
    QLabel* m_instructionLabel;
    QLabel* m_statusLabel;
    
    // Hand detector reference
    HandDetector* m_handDetector;
    
    // Helper methods
    QImage matToQImage(const cv::Mat& mat);
    void calculateCalibration();
};

#endif // CALIBRATIONWIDGET_H
