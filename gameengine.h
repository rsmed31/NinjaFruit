#ifndef GAMEENGINE_H
#define GAMEENGINE_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QVector3D>
#include <QRandomGenerator>
#include <QSet>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "projectile.h"
#include "projectilerenderdata.h" // Add this include

// Forward declaration of GameWidget to avoid circular dependency
class GameWidget;

class GameEngine : public QObject
{
    Q_OBJECT

public:
    explicit GameEngine(QObject *parent = nullptr);
    ~GameEngine();
    
    // Game control
    void startGame();
    void pauseGame();
    void resumeGame();
    void resetGame();
    
    // Hand position for collision detection
    void updateHandPosition(const QVector3D& position);
    
    // Get game elements
    QList<Projectile> getProjectiles() const;
    
    // Game state
    int getScore() const { return m_score; }
    int getLives() const { return m_lives; }
    int getGameTime() const { return m_gameTime; }
    bool isGameRunning() const { return m_gameRunning; }
    
    // Sword geometry for collision detection
    QVector3D getSwordHandle() const { return m_swordHandle; }
    QVector3D getSwordTip() const { return m_swordTip; }
    
    // Connect to GameWidget for sword position updates
    void connectToGameWidget(GameWidget* widget);

    // Add score method declaration
    void addScore(int points);
    
    // Add the missing method declaration
    void addProjectile(const Projectile& projectile);

signals:
    // UI update signals
    void scoreChanged(int score);
    void livesChanged(int lives);
    void timeChanged(int seconds);
    void gameOver(int finalScore);
    
    // Projectile signals
    void projectileAdded(const Projectile& projectile);
    void projectileRemoved(int index);
    void projectileSplit(int index);
    void projectilesCleared(); // New signal to notify when all projectiles are cleared

public slots:
    // Add this slot to receive sword position updates
    void updateSwordPosition(const QVector3D& handlePos, const QVector3D& tipPos);
    void markProjectileSlicedById(int id);

private slots:
    void updateGame();
    void spawnProjectile();

private:
    // Game parameters
    static const int MAX_LIVES = 5;
    static const int GAME_DURATION = 60; // seconds
    
    // Game state
    bool m_gameRunning;
    int m_score;
    int m_lives;
    int m_gameTime; // seconds remaining
    float m_elapsedTime; // total time in seconds
    QVector3D m_handPosition;
    QVector3D m_swordHandle;
    QVector3D m_swordTip;

    
    // Launch parameters
    struct LaunchZone {
        QVector3D center;
        float width;
        float height;
    } m_launchZone;
    
    // Projectiles
    QList<Projectile> m_projectiles;                // Active list
    QList<Projectile> m_projectilePool;             // ← pool for reuse
    QList<ProjectileRenderData> m_projectileRenderData; // List to store render data
    QSet<int> m_slicedProjectiles; // Track sliced projectiles by ID
    
    // Timers
    QTimer m_gameTimer;      // Main game update timer
    QTimer m_spawnTimer;     // Controls projectile spawning
    QTimer m_gameClockTimer; // Updates game remaining time
    
    // Sound effects
    QMediaPlayer *m_swordSound;
    QAudioOutput *m_swordAudioOutput;
    
    // Game methods
    void checkCollisions();
    void updateProjectiles(float deltaTime);
    Projectile createRandomProjectile();
    QVector3D generateRandomLaunchPosition();
    QVector3D generateRandomVelocity();
    void handleMissedProjectiles();
};

#endif // GAMEENGINE_H
