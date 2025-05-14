#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QOpenGLWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QTimer>

#include "calibrationwidget.h"
#include "handdetector.h"
#include "gamewidget.h"
#include "gameengine.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void startCalibration();
    void startGame();
    void exitGame();
    void onCalibrationFinished();
    void updateScore(int score);
    void updateLives(int lives);
    void updateTimer(int seconds);
    void onGameOver(int finalScore);

private slots:
    void processFrame();

private:
    // Game state
    bool isCalibrated;
    
    // Game logic components
    HandDetector* handDetector;
    GameEngine* gameEngine;
    
    // Main UI components
    QWidget* centralWidget;
    QStackedWidget* mainStack;
    
    // Welcome/Menu screen
    QWidget* welcomeScreen;
    QPushButton* startButton;
    QPushButton* calibrateButton;
    QPushButton* exitButton;
    
    // Game screen
    QWidget* gameScreen;
    QHBoxLayout* mainLayout;
    
    // Game components
    QWidget* gameArea;
    QLabel* webcamFeedLabel;
    GameWidget* gameWidget;
    
    // Control panel components
    QWidget* controlPanel;
    QLabel* scoreLabel;
    QLabel* timerLabel;
    QLabel* livesLabel;
    QPushButton* returnToMenuButton;
    
    // Calibration screen
    CalibrationWidget* calibrationWidget;
    
    // Video processing
    cv::VideoCapture camera;
    cv::Mat currentFrame;
    QTimer processingTimer;
    
    // Calibration data
    CalibrationWidget::CalibrationData calibrationData;
    
    // Hand position visualization
    QWidget* handIndicator;
    QLabel* handPosWidget;

    // Welcome screen camera feed
    QLabel* welcomeCameraFeedLabel;
    QLabel* welcomeHandVisLabel;
    
    // Hand visualization for game screen
    QLabel* handVisualizationLabel;

    // Smoothing variables
    float lastX;  // Last valid X position for smoothing
    float lastY;  // Last valid Y position for smoothing
    
    // Methods
    void setupUI();
    void setupConnections();
    void updateHandVisualization(QLabel* label, float x, float y, bool detected);
    void mapHandToGameSpace(const cv::Point& handPos, float& gameX, float& gameY);
};

#endif // MAINWINDOW_H
