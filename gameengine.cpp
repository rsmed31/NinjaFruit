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
    // Stop all timers first
    pauseGame();
    
    // Reset game state
    m_score = 0;
    m_lives = MAX_LIVES;
    m_gameTime = GAME_DURATION;
    m_elapsedTime = 0.0f;
    
    // Clear projectiles safely
    while (!m_projectiles.isEmpty()) {
        emit projectileRemoved(0);
        m_projectiles.removeFirst();
    }
    
    // Reset sword positions
    m_swordHandle = QVector3D(0.0f, 0.0f, 0.0f);
    m_swordTip = QVector3D(0.0f, 1.0f, 0.0f);
    
    // Emit signals for UI updates
    emit scoreChanged(m_score);
    emit livesChanged(m_lives);
    emit timeChanged(m_gameTime);
    
    // Reset game running state
    m_gameRunning = false;
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
            qDebug() << "Collision detected with projectile" << i 
                     << "at position" << pos 
                     << "sword:" << m_swordHandle << "->" << m_swordTip;
            
            // Mark projectile as sliced
            projectile.split();
            projectile.markAsSliced();
            
            // Update score by adding projectile's point value
            int points = projectile.getPointValue();
            m_score += points;
            qDebug() << "Adding" << points << "points, new score:" << m_score;
            
            emit scoreChanged(m_score);
            emit projectileSplit(i);
        }
    }
}

void GameEngine::updateProjectiles(float deltaTime)
{
    // Create temporary list of projectiles to remove
    QList<int> toRemove;
    
    // First pass: update and check projectiles
    for (int i = 0; i < m_projectiles.size(); i++) {
        Projectile& projectile = m_projectiles[i];
        projectile.update(deltaTime);

        QVector3D pos = projectile.getPosition();

        qDebug() << "Projectile" << i << "z:" << pos.z() << "state:" << projectile.getState() << "sliced:" << projectile.wasSliced();

        // Only when projectile exits far enough (z >= 15)
        if (pos.z() >= 15.0f && projectile.getState() == Projectile::ACTIVE) {
            // First check if we need to deduct a life
            if (!projectile.wasSliced() && !projectile.wasProcessed() && m_gameRunning) {
                // MISS: lose 1 life
                qDebug() << "MISSED - projectile" << i << "disappeared unsliced at z =" << pos.z();
                qDebug() << "BEFORE lives:" << m_lives;
                m_lives = qMax(0, m_lives - 1);
                qDebug() << "AFTER lives:" << m_lives;
                emit livesChanged(m_lives);
                projectile.markAsProcessed();
                
                if (m_lives <= 0) {
                    pauseGame();
                    emit gameOver(m_score);
                    break;
                }
            }
            
            // Mark projectile for removal
            projectile.split();
        }

        // Remove inactive projectiles
        if (projectile.getState() == Projectile::INACTIVE) {
            toRemove.prepend(i);
        }
    }
    
    // Second pass: remove projectiles safely
    foreach (int i, toRemove) {
        if (i >= 0 && i < m_projectiles.size()) {
            emit projectileRemoved(i);
            m_projectiles.removeAt(i);
        }
    }
}

Projectile GameEngine::createRandomProjectile()
{
    Projectile::Type type = Projectile::CYLINDER;
    
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
    // Calculate consistent trajectories that all reach z=15
    float targetZ = 15.0f; // Consistent target distance
    float targetX = (QRandomGenerator::global()->generateDouble() - 0.5f) * 2.0f; // Narrow x-range
    float targetY = 1.2f; // Low consistent height
    
    QVector3D startPos = generateRandomLaunchPosition();
    
    // Fixed flight time for consistent arcs
    const float time = 2.5f;
    
    // Calculate velocity components
    float vz = (targetZ - startPos.z()) / time;
    float vx = (targetX - startPos.x()) / time;
    
    // Calculate vertical velocity to hit target height
    float vy = (targetY - startPos.y() + 0.5f * 9.8f * time * time) / time;
    
    return QVector3D(vx, vy, vz);
}

void GameEngine::handleMissedProjectiles()
{
    // This function is now empty since we handle missed projectiles
    // in updateProjectiles() to avoid duplicate processing
}

void GameEngine::connectToGameWidget(GameWidget* widget)
{
    if (widget) {
        connect(widget, &GameWidget::swordPositionUpdated, 
                this, &GameEngine::updateSwordPosition);
        // Initialize sword positions to default values
        m_swordHandle = QVector3D(0.0f, 0.0f, 0.0f);
        m_swordTip = QVector3D(0.0f, 1.0f, 0.0f);
    }
}

void GameEngine::updateSwordPosition(const QVector3D& handlePos, const QVector3D& tipPos)
{
    m_swordHandle = handlePos;
    m_swordTip = tipPos;
}

void GameEngine::addScore(int points)
{
    m_score += points;
    qDebug() << "Adding" << points << "points, new score:" << m_score;
    emit scoreChanged(m_score);
}
