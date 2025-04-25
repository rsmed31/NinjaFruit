#include "gameengine.h"
#include "gamewidget.h"  // Add this include to have access to GameWidget class definition
#include <QDebug>
#include <GL/gl.h>

GameEngine::GameEngine(QObject *parent)
    : QObject(parent)
    , m_gameRunning(false)
    , m_score(0)
    , m_lives(MAX_LIVES)
    , m_gameTime(GAME_DURATION)
    , m_elapsedTime(0.0f)
    , m_handPosition(0.0f, 0.0f, 0.0f)
{
    // Set up launch zone to start projectiles farther away
    m_launchZone.center = QVector3D(0.0f, 2.0f, -5.0f); // Update to be farther from screen
    m_launchZone.width = 10.0f;
    m_launchZone.height = 4.0f;
    
    // Connect timers
    connect(&m_gameTimer, &QTimer::timeout, this, &GameEngine::updateGame);
    connect(&m_spawnTimer, &QTimer::timeout, this, &GameEngine::spawnProjectile);
    connect(&m_gameClockTimer, &QTimer::timeout, [this]() {
        m_gameTime--;
        emit timeChanged(m_gameTime);
        
        if (m_gameTime <= 0) {
            pauseGame();
            emit gameOver(m_score);
        }
    });
    
    // Set timer intervals
    m_gameTimer.setInterval(16);      // ~60 FPS
    m_spawnTimer.setInterval(2000);   // 2 seconds between projectiles
    m_gameClockTimer.setInterval(1000); // 1 second for game clock
}

GameEngine::~GameEngine()
{
    pauseGame(); // Ensure timers are stopped
}

void GameEngine::startGame()
{
    // Reset game state
    resetGame();
    
    // Start timers
    m_gameTimer.start();
    m_spawnTimer.start();
    m_gameClockTimer.start();
    
    m_gameRunning = true;
}

void GameEngine::pauseGame()
{
    m_gameTimer.stop();
    m_spawnTimer.stop();
    m_gameClockTimer.stop();
    m_gameRunning = false;
}

void GameEngine::resumeGame()
{
    if (m_lives > 0 && m_gameTime > 0) {
        m_gameTimer.start();
        m_spawnTimer.start();
        m_gameClockTimer.start();
        m_gameRunning = true;
    }
}

void GameEngine::resetGame()
{
    // Reset game state
    m_score = 0;
    m_lives = MAX_LIVES;
    m_gameTime = GAME_DURATION;
    m_elapsedTime = 0.0f;
    
    // Clear projectiles
    m_projectiles.clear();
    
    // Emit signals for UI updates
    emit scoreChanged(m_score);
    emit livesChanged(m_lives);
    emit timeChanged(m_gameTime);
}

void GameEngine::updateHandPosition(const QVector3D& position)
{
    m_handPosition = position;
    // We don't need to calculate m_swordHandle and m_swordTip here anymore
    // They will be updated by the swordPositionUpdated signal from GameWidget
}

QList<Projectile> GameEngine::getProjectiles() const
{
    return m_projectiles;
}

void GameEngine::updateGame()
{
    // Calculate delta time (assumed ~16ms per frame at 60 FPS)
    float deltaTime = 0.016f;
    m_elapsedTime += deltaTime;
    
    // Update projectiles position based on physics
    updateProjectiles(deltaTime);
    
    // Check for collisions with hand
    checkCollisions();
    
    // Handle missed projectiles
    handleMissedProjectiles();
}

void GameEngine::spawnProjectile()
{
    Projectile projectile = createRandomProjectile();
    m_projectiles.append(projectile);
    emit projectileAdded(projectile);
}

void GameEngine::checkCollisions()
{
    // Define hit zone dimensions matching the visual representation
    const float HIT_MIN_X = -8.0f;
    const float HIT_MAX_X = 8.0f;
    const float HIT_MIN_Z = 0.0f;
    const float HIT_MAX_Z = 1.5f;
    
    for (int i = 0; i < m_projectiles.size(); i++) {
        Projectile& projectile = m_projectiles[i];
        if (projectile.getState() != Projectile::ACTIVE)
            continue;
        
        QVector3D pos = projectile.getPosition();
        
        // Check if projectile is within hit zone boundaries (semi-cylindrical area)
        bool inHitZone = (pos.x() >= HIT_MIN_X && pos.x() <= HIT_MAX_X && 
                          pos.z() >= HIT_MIN_Z && pos.z() <= HIT_MAX_Z &&
                          pos.y() >= 0.1f && pos.y() <= 5.0f);
        
        // Use line segment collision detection with the actual sword geometry
        if (inHitZone && projectile.isColliding(m_swordHandle, m_swordTip)) {
            // Mark projectile as sliced
            projectile.split();
            
            // Update score by adding projectile's point value
            m_score += projectile.getPointValue();
            
            emit scoreChanged(m_score);
            emit projectileSplit(i);
        }
    }
}

void GameEngine::updateProjectiles(float deltaTime)
{
    // Update all projectiles, removing inactive ones
    QMutableListIterator<Projectile> i(m_projectiles);
    int index = 0;
    
    while (i.hasNext()) {
        Projectile& projectile = i.next();
        
        // Update projectile physics
        projectile.update(deltaTime);
        
        // Remove inactive projectiles
        if (projectile.getState() == Projectile::INACTIVE) {
            emit projectileRemoved(index);
            i.remove();
        } else {
            index++;
        }
    }
}

Projectile GameEngine::createRandomProjectile()
{
    // Create a random projectile type
    int typeValue = QRandomGenerator::global()->bounded(100);
    Projectile::Type type;
    
    // Even distribution between fruits
    if (typeValue < 25) {
        type = Projectile::APPLE;
    } else if (typeValue < 50) {
        type = Projectile::ORANGE;
    } else if (typeValue < 75) {
        type = Projectile::BANANA;
    } else {
        type = Projectile::WATERMELON;
    }
    
    Projectile projectile(type);
    
    // Set position and velocity
    projectile.setPosition(generateRandomLaunchPosition());
    projectile.setVelocity(generateRandomVelocity());
    projectile.setCreationTime(m_elapsedTime);
    
    return projectile;
}

QVector3D GameEngine::generateRandomLaunchPosition()
{
    // Create a narrower x-distribution to ensure projectiles hit the screen
    float x = (QRandomGenerator::global()->generateDouble() - 0.5) * 4.0f; // -2.0 to 2.0 range
    
    // Consistent y-height to create predictable arcs that won't go too high or too low
    float y = 2.5f + QRandomGenerator::global()->generateDouble() * 1.5f; // 2.5-4.0 range
    
    // Launch from further back to give time to see and prepare
    float z = -16.0f;
    
    return QVector3D(x, y, z);
}

QVector3D GameEngine::generateRandomVelocity()
{
    // Almost no horizontal divergence to ensure projectiles stay on screen
    float vx = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.3f;  
    
    // Balanced upward component - enough arc without hitting floor
    float vy = 5.0f + QRandomGenerator::global()->generateDouble() * 2.0f;
    
    // Controlled forward velocity ensures all projectiles reach hit zone
    float vz = 12.5f + QRandomGenerator::global()->generateDouble() * 1.0f;
    
    return QVector3D(vx, vy, vz);
}

void GameEngine::handleMissedProjectiles()
{
    const float HIT_MIN_X = -8.0f;
    const float HIT_MAX_X = 8.0f;
    const float HIT_MIN_Z = 0.0f;
    const float HIT_MAX_Z = 1.5f;
    
    for (auto& projectile : m_projectiles) {
        // Only process active projectiles
        if (projectile.getState() != Projectile::ACTIVE) 
            continue;
            
        QVector3D pos = projectile.getPosition();
        
        // Determine if projectile has passed through the hit zone
        bool passedHitZone = pos.z() > HIT_MAX_Z;
        
        // Projectile has passed through visible hit zone without being sliced
        if (passedHitZone) {
            // Verify it was actually in the visible area (not off to sides or too high)
            bool wasInVisibleRange = (pos.x() >= HIT_MIN_X && pos.x() <= HIT_MAX_X &&
                                      pos.y() >= 0.1f && pos.y() <= 5.0f);
            
            // Mark as split for removal and visual effect
            projectile.split();
            
            // Penalize for missing projectiles that were visible
            if (wasInVisibleRange) {
                m_lives--;
                emit livesChanged(m_lives);
                
                // Check for game over
                if (m_lives <= 0) {
                    pauseGame();
                    emit gameOver(m_score);
                }
            }
        }
    }
}

void GameEngine::connectToGameWidget(GameWidget* widget)
{
    if (widget) {
        connect(widget, &GameWidget::swordPositionUpdated, 
                this, &GameEngine::updateSwordPosition);
    }
}

void GameEngine::updateSwordPosition(const QVector3D& handlePos, const QVector3D& tipPos)
{
    m_swordHandle = handlePos;
    m_swordTip = tipPos;
}
