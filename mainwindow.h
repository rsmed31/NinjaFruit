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

private slots:
    void startCalibration();
    void startGame();
    void exitGame();
    void onCalibrationFinished();
    
    // Game state update slots
    void updateScore(int score);
    void updateLives(int lives);
    void updateTimer(int seconds);
    void onGameOver(int finalScore);
    
    // Processing timer
    void processFrame();

private:
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
    
    // Game logic components
    HandDetector* handDetector;
    GameEngine* gameEngine;
    
    // Video processing
    cv::VideoCapture camera;
    cv::Mat currentFrame;
    QTimer processingTimer;
    
    // Calibration data
    CalibrationWidget::CalibrationData calibrationData;
    bool isCalibrated;
    
    // Hand position visualization
    QLabel* handPositionLabel;
    QWidget* handIndicator;
    QLabel* handPosWidget;

    // Welcome screen camera feed
    QLabel* welcomeCameraFeedLabel;
    QLabel* welcomeHandVisLabel;
    
    // Hand visualization for game screen
    QLabel* handVisualizationLabel;
    
    // Method to update hand visualization
    void updateHandVisualization(QLabel* label, float x, float y, bool detected);
    
    // Setup methods
    void setupUI();
    void setupConnections();
    void mapHandToGameSpace(const cv::Point& handPos, float& gameX, float& gameY);
};

#endif // MAINWINDOW_H
