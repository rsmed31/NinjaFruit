#include "mainwindow.h"
#include <QMessageBox>
#include <cmath>
#include <QDebug>
#include <QPainter>
#include <QPen>
#include <QFile> // Add this include for QFile::exists

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isCalibrated(false)
    , handDetector(new HandDetector())
    , gameEngine(new GameEngine(this))
    , handPosWidget(nullptr)
    , multiPositionCheck(nullptr)  // Initialize the checkbox pointer
{
    // Now set up UI that uses handDetector
    setupUI();
    setupConnections();
    
    // Initialize camera with lower resolution
    camera.open(0);
    
    // Set camera buffer size to 1 to avoid lag from buffered frames
    camera.set(cv::CAP_PROP_BUFFERSIZE, 1);
    
    // Set camera to lower resolution for better performance
    camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    
    // Increase camera FPS if supported
    camera.set(cv::CAP_PROP_FPS, 30);
    
    if (!camera.isOpened()) {
        QMessageBox::critical(this, "Error", "Could not open camera");
    }
    
    // Connect processing timer with slightly faster rate
    processingTimer.setInterval(16); // Target 60 FPS
    connect(&processingTimer, &QTimer::timeout, this, &MainWindow::processFrame);
}

MainWindow::~MainWindow()
{
    if (camera.isOpened()) {
        camera.release();
    }
    
    delete handDetector;
    delete gameEngine;
}

void MainWindow::setupUI()
{
    // Set up central widget and main stack
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    mainStack = new QStackedWidget();
    layout->addWidget(mainStack);
    
    // 1. Create welcome screen with camera preview
    welcomeScreen = new QWidget();
    QVBoxLayout* welcomeLayout = new QVBoxLayout(welcomeScreen);
    
    QLabel* titleLabel = new QLabel("Ninja Fruit");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(32);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    
    // Add camera preview to welcome screen
    QHBoxLayout* welcomeMiddleLayout = new QHBoxLayout();
    
    // Add buttons in a vertical layout on left
    QVBoxLayout* welcomeButtonLayout = new QVBoxLayout();
    startButton = new QPushButton("Start Game");
    startButton->setMinimumHeight(50);
    calibrateButton = new QPushButton("Calibrate");
    calibrateButton->setMinimumHeight(50);
    exitButton = new QPushButton("Exit");
    exitButton->setMinimumHeight(50);
    welcomeButtonLayout->addWidget(startButton);
    welcomeButtonLayout->addWidget(calibrateButton);
    welcomeButtonLayout->addWidget(exitButton);
    welcomeButtonLayout->addStretch();
    
    // Add camera preview on right
    QVBoxLayout* welcomeCameraLayout = new QVBoxLayout();
    welcomeCameraFeedLabel = new QLabel("Camera Feed");
    welcomeCameraFeedLabel->setFixedSize(320, 240);
    welcomeCameraFeedLabel->setScaledContents(true);
    welcomeCameraFeedLabel->setStyleSheet("background-color: black; color: white; border: 1px solid gray;");
    welcomeCameraFeedLabel->setAlignment(Qt::AlignCenter);
    
    // Hand position visualization for welcome screen
    welcomeHandVisLabel = new QLabel("Hand Position");
    welcomeHandVisLabel->setFixedSize(320, 80);
    welcomeHandVisLabel->setStyleSheet("background-color: #222; color: white; border: 1px solid gray;");
    
    welcomeCameraLayout->addWidget(welcomeCameraFeedLabel);
    welcomeCameraLayout->addWidget(welcomeHandVisLabel);
    welcomeCameraLayout->addStretch();
    
    // Combine buttons and camera in middle section
    welcomeMiddleLayout->addLayout(welcomeButtonLayout);
    welcomeMiddleLayout->addLayout(welcomeCameraLayout);
    
    // Assemble welcome screen
    welcomeLayout->addStretch();
    welcomeLayout->addWidget(titleLabel);
    welcomeLayout->addSpacing(20);
    welcomeLayout->addLayout(welcomeMiddleLayout);
    welcomeLayout->addStretch();
    
    // Add multi-position calibration checkbox to welcome screen
    multiPositionCheck = new QCheckBox("Enable multi-position hand calibration");
    multiPositionCheck->setObjectName("multiPositionCheck");
    welcomeLayout->addWidget(multiPositionCheck);
    connect(multiPositionCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (calibrationWidget) {
            calibrationWidget->enableMultiPositionCapture(checked);
        }
    });
    
    // 2. Create game screen with improved layout
    gameScreen = new QWidget();
    QHBoxLayout* gameScreenLayout = new QHBoxLayout(gameScreen);
    
    // Left side: Game scene widget
    gameWidget = new GameWidget();
    gameWidget->setMinimumSize(640, 480);
    gameWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Right side: Vertical layout with smaller webcam feed on top and control panel below
    QVBoxLayout* rightLayout = new QVBoxLayout();
    
    // Use a smaller webcam feed size
    webcamFeedLabel = new QLabel("Webcam Feed");
    webcamFeedLabel->setFixedSize(480, 360);
    webcamFeedLabel->setScaledContents(true);
    webcamFeedLabel->setStyleSheet("background-color: black; color: white; border: 1px solid gray;");
    webcamFeedLabel->setAlignment(Qt::AlignCenter);
    
    // Hand visualization for game screen
    handVisualizationLabel = new QLabel("Hand Position");
    handVisualizationLabel->setFixedSize(480, 100);
    handVisualizationLabel->setStyleSheet("background-color: #222; color: white;");
    
    // Control panel with updated proportions
    controlPanel = new QWidget();
    controlPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    controlPanel->setStyleSheet("background-color: #2C3E50; border-radius: 8px; color: white;");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
    QFont font("Arial", 16, QFont::Bold);
    scoreLabel = new QLabel("Score: 0");
    scoreLabel->setFont(font);
    scoreLabel->setStyleSheet("color: #FFD700; background-color: rgba(0,0,0,120); padding:5px; border-radius:5px;");
    timerLabel = new QLabel("Time: 60");
    timerLabel->setFont(font);
    timerLabel->setStyleSheet("color: #00FFFF; background-color: rgba(0,0,0,120); padding:5px; border-radius:5px;");
    livesLabel = new QLabel("Lives: 3");
    livesLabel->setFont(font);
    livesLabel->setStyleSheet("color: #FF6347; background-color: rgba(0,0,0,120); padding:5px; border-radius:5px;");
    returnToMenuButton = new QPushButton("Back to Menu");
    returnToMenuButton->setStyleSheet("background-color: #4682B4; color: white; font-weight: bold; padding:8px;");
    controlLayout->addWidget(new QLabel("<h1 style='color:white;'>Game Status</h1>"));
    controlLayout->addWidget(scoreLabel);
    controlLayout->addWidget(timerLabel);
    controlLayout->addWidget(livesLabel);
    controlLayout->addStretch();
    controlLayout->addWidget(returnToMenuButton);
    
    // Assemble the right column with smaller webcam feed
    rightLayout->addWidget(webcamFeedLabel);
    rightLayout->addWidget(handVisualizationLabel);
    rightLayout->addWidget(controlPanel, 1); // Control panel gets more space
    
    gameScreenLayout->addWidget(gameWidget, 3); // Game scene takes 3/4 of the width 
    gameScreenLayout->addLayout(rightLayout, 1); // Right side takes 1/4
    
    // 3. Create calibration widget with hand detector reference
    // Make sure we're explicitly passing our handDetector instance
    calibrationWidget = new CalibrationWidget(nullptr, handDetector);
    
    // Add all screens to stack
    mainStack->addWidget(welcomeScreen);
    mainStack->addWidget(gameScreen);
    mainStack->addWidget(calibrationWidget);
    
    // Start with welcome screen
    mainStack->setCurrentWidget(welcomeScreen);
    
    // Set window properties
    setWindowTitle("Ninja Fruit Game");
    resize(1024, 768);
}

void MainWindow::setupConnections()
{
    // Menu buttons
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startGame);
    connect(calibrateButton, &QPushButton::clicked, this, &MainWindow::startCalibration);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::exitGame);
    connect(returnToMenuButton, &QPushButton::clicked, [this]() {
        gameEngine->pauseGame();
        processingTimer.stop();
        mainStack->setCurrentWidget(welcomeScreen);
    });
    
    // Game engine signals
    connect(gameEngine, &GameEngine::scoreChanged, this, &MainWindow::updateScore);
    connect(gameEngine, &GameEngine::livesChanged, this, &MainWindow::updateLives);
    connect(gameEngine, &GameEngine::timeChanged, this, &MainWindow::updateTimer);
    connect(gameEngine, &GameEngine::gameOver, this, &MainWindow::onGameOver);
    
    // Connect GameWidget's score updates
    connect(gameWidget, &GameWidget::scoreChanged, gameEngine, &GameEngine::addScore);
    connect(gameEngine, &GameEngine::projectileAdded, 
            [this](const Projectile& proj) {
                gameWidget->launchProjectile(proj.getPosition(), proj.getVelocity());
            });
    connect(gameEngine, &GameEngine::projectileSplit, 
            [this](int index) {
                gameWidget->splitProjectile(index);
            });
    
    // Connect GameEngine to GameWidget for sword position updates
    gameEngine->connectToGameWidget(gameWidget);
}

void MainWindow::startCalibration()
{
    // Ensure the handDetector is available before switching to calibration screen
    if (!handDetector) {
        QMessageBox::critical(this, "Error", "Hand detector not initialized");
        return;
    }

    mainStack->setCurrentWidget(calibrationWidget);
    
    // Stop the processing timer if it's running
    processingTimer.stop();
    
    // Use the member variable directly instead of findChild
    if (calibrationWidget) {
        calibrationWidget->enableMultiPositionCapture(multiPositionCheck->isChecked());
    }
    
    // Connect calibration complete signal
    connect(calibrationWidget, &CalibrationWidget::calibrationFinished,
            this, &MainWindow::onCalibrationFinished, Qt::UniqueConnection);
}

void MainWindow::onCalibrationFinished()
{
    calibrationData = calibrationWidget->getCalibrationData();
    isCalibrated = true;
    
    // Completely close and reopen the camera - more reliable than just checking if it's open
    if (camera.isOpened()) {
        camera.release();
    }
    
    qDebug() << "⚙️ Reopening camera after calibration";
    camera.open(0);
    
    // Apply the same optimized camera settings
    camera.set(cv::CAP_PROP_BUFFERSIZE, 1);
    camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    camera.set(cv::CAP_PROP_FPS, 30);
    
    if (!camera.isOpened()) {
        QMessageBox::critical(this, "Error", "Failed to reopen camera after calibration");
        return;
    }
    
    // Test camera by capturing a frame
    cv::Mat testFrame;
    camera >> testFrame;
    if (testFrame.empty()) {
        qDebug() << "❌ Failed to get frame from reopened camera";
        QMessageBox::warning(this, "Camera Issue", 
                           "Camera reopened but failed to capture frames. Try restarting the application.");
    } else {
        qDebug() << "✅ Successfully captured frame from reopened camera:" 
                << testFrame.cols << "x" << testFrame.rows;
    }
    
    QMessageBox::information(this, "Calibration Complete",
                           "Hand calibration is complete. You can now start the game.");
    
    // Switch back to welcome screen
    mainStack->setCurrentWidget(welcomeScreen);
    
    // Start the processing timer to process frames
    processingTimer.start(16); // Target 60 FPS
}

void MainWindow::startGame()
{
    if (!isCalibrated) {
        QMessageBox::warning(this, "Calibration Required", 
                             "Please calibrate your hand first.");
        return;
    }
    
    mainStack->setCurrentWidget(gameScreen);
    gameEngine->resetGame();
    gameEngine->startGame();
    processingTimer.start(33); // ~30 FPS
}

void MainWindow::exitGame()
{
    processingTimer.stop();
    close();
}

void MainWindow::processFrame()
{
    if (!camera.isOpened()) {
        return;
    }
    
    // CRITICAL OPTIMIZATION: First grab frame without decoding
    // This ensures we get the most recent frame and discard buffered ones
    if (!camera.grab()) {
        return;
    }
    
    // Retrieve and decode the grabbed frame immediately
    cv::Mat frame;
    if (!camera.retrieve(frame)) {
        return;
    }
    
    if (frame.empty()) {
        return;
    }
    
    // Don't resize the frame - work with the smaller original size
    // Just make a shallow copy for tracking purposes
    frame.copyTo(currentFrame);
    
    // Try to detect the hand in the frame
    cv::Point handPos(-1, -1);
    try {
        handPos = handDetector->detectHand(currentFrame);
    }
    catch (const std::exception&) {
        handPos = cv::Point(-1, -1);
    }
    
    float gameX = 0.0f, gameY = 0.0f;
    bool handDetected = false;
    
    if (handPos.x >= 0 && handPos.y >= 0) {
        handDetected = true;
        mapHandToGameSpace(handPos, gameX, gameY);
        
        // Update game engine and widget with hand position
        gameEngine->updateHandPosition(QVector3D(gameX, gameY, 0.75f));
        gameWidget->setHandPosition(gameX, gameY);
        
        // OPTIMIZATION: Only draw tracking circle on display frames
        // but don't modify the currentFrame otherwise
    }
    
    // OPTIMIZATION: Update display much less frequently (every 3rd frame)
    static int displayCounter = 0;
    if (++displayCounter % 3 == 0) {
        // Create display copy with circle (don't modify currentFrame)
        cv::Mat displayFrame = currentFrame.clone();
        
        if (handDetected) {
            // Draw hand position indicator
            cv::circle(displayFrame, handPos, 10, cv::Scalar(0,255,0), -1);
        }
        
        // Convert frame to QImage with RGB conversion in one step
        cv::Mat rgbFrame;
        cv::cvtColor(displayFrame, rgbFrame, cv::COLOR_BGR2RGB);
        
        // Create QImage without copying data (faster) - but we must use it before rgbFrame is modified
        QImage qimg(rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
                    static_cast<int>(rgbFrame.step), QImage::Format_RGB888);
        
        // OPTIMIZATION: Create and scale QPixmap in one operation
        QWidget* currentWidget = mainStack->currentWidget();
        
        // Skip updates when not needed
        if (currentWidget == gameScreen) {
            webcamFeedLabel->setPixmap(QPixmap::fromImage(qimg.scaled(
                webcamFeedLabel->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
            updateHandVisualization(handVisualizationLabel, gameX, gameY, handDetected);
        } 
        else if (currentWidget == welcomeScreen && isCalibrated) {
            welcomeCameraFeedLabel->setPixmap(QPixmap::fromImage(qimg.scaled(
                welcomeCameraFeedLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            updateHandVisualization(welcomeHandVisLabel, gameX, gameY, handDetected);
        }
    }
}

void MainWindow::updateHandVisualization(QLabel* label, float x, float y, bool detected)
{
    QPixmap pixmap(label->size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw grid background
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    int gridSpacing = 20;
    for (int i = 0; i < pixmap.width(); i += gridSpacing) {
        painter.drawLine(i, 0, i, pixmap.height());
    }
    for (int i = 0; i < pixmap.height(); i += gridSpacing) {
        painter.drawLine(0, i, pixmap.width(), i);
    }
    
    // If hand is detected, draw position indicator
    if (detected) {
        // Map [-10,10] x range and [0,15] y range to the label's dimensions
        int centerX = label->width() / 2;
        int centerY = label->height() / 2;
        int indicatorX = centerX + static_cast<int>((x / 10.0f) * centerX);
        int indicatorY = centerY - static_cast<int>((y / 7.5f) * centerY);
        
        // Draw sword icon
        int swordLength = 30;
        painter.setPen(QPen(QColor(200, 200, 220), 3));
        painter.drawLine(indicatorX, indicatorY, indicatorX, indicatorY - swordLength);
        
        // Draw sword handle
        painter.setPen(QPen(QColor(139, 69, 19), 5));
        painter.drawLine(indicatorX, indicatorY + 5, indicatorX, indicatorY + 15);
        
        // Draw sword guard
        painter.setPen(QPen(QColor(139, 69, 19), 1));
        painter.drawLine(indicatorX - 10, indicatorY + 5, indicatorX + 10, indicatorY + 5);
        
        // Label position
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(5, 15, QString("X: %1  Y: %2").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1));
    } else {
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "Hand Not Detected");
    }
    
    label->setPixmap(pixmap);
}

void MainWindow::mapHandToGameSpace(const cv::Point& handPos, float& gameX, float& gameY)
{
    // Normalize camera coordinates (0.0 to 1.0)
    float normX = static_cast<float>(handPos.x) / currentFrame.cols;
    float normY = static_cast<float>(handPos.y) / currentFrame.rows;
    
    // Map X coordinates from camera to game space with mirroring
    // This creates a more intuitive left-to-right hand mapping
    gameX = (0.5f - normX) * 16.0f;
    
    // Map Y coordinates from camera to game space
    // The value is inverted since higher Y in camera = lower position in real world
    gameY = (1.0f - normY) * 10.0f; 
    
    // Apply stronger smoothing to reduce jitter
    static float lastX = gameX;
    static float lastY = gameY;
    
    // Apply temporal smoothing for more stable hand position
    gameX = 0.8f * gameX + 0.2f * lastX;
    gameY = 0.8f * gameY + 0.2f * lastY;
    
    // Store current position for next smoothing
    lastX = gameX;
    lastY = gameY;
    
    // Apply bounds to prevent the sword from going off-screen
    gameX = qBound(-7.5f, gameX, 7.5f);
    gameY = qBound(0.5f, gameY, 9.5f);
}

void MainWindow::updateScore(int score)
{
    qDebug() << "Updating score to:" << score;
    scoreLabel->setText(QString("Score: %1").arg(score));
    qDebug() << "Score label text set to:" << scoreLabel->text();
}

void MainWindow::updateLives(int lives)
{
    qDebug() << "Updating lives to:" << lives;
    livesLabel->setText(QString("Lives: %1").arg(lives));
    qDebug() << "Lives label text set to:" << livesLabel->text();
}

void MainWindow::updateTimer(int seconds)
{
    timerLabel->setText(QString("Time: %1").arg(seconds));
}

void MainWindow::onGameOver(int finalScore)
{
    processingTimer.stop();
    QMessageBox msgBox;
    msgBox.setWindowTitle("Game Over");
    msgBox.setText(QString("Game Over! Your final score is %1").arg(finalScore));
    msgBox.setInformativeText("Do you want to play again?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        startGame();
    } else {
        mainStack->setCurrentWidget(welcomeScreen);
    }
}
