#include "gameengine.h"
#include "gamewidget.h" // Add this include to have access to GameWidget class definition
#include <QDebug>
#include <GL/gl.h>

GameEngine::GameEngine(QObject *parent)
    : QObject(parent), m_gameRunning(false), m_score(0), m_lives(MAX_LIVES), m_gameTime(GAME_DURATION), m_elapsedTime(0.0f), m_handPosition(0.0f, 0.0f, 0.0f)
{
    // Set up launch zone to start projectiles farther away
    m_launchZone.center = QVector3D(0.0f, 2.0f, -5.0f); // Update to be farther from screen
    m_launchZone.width = 10.0f;
    m_launchZone.height = 4.0f;

    // Connect timers
    connect(&m_gameTimer, &QTimer::timeout, this, &GameEngine::updateGame);
    connect(&m_spawnTimer, &QTimer::timeout, this, &GameEngine::spawnProjectile);
    connect(&m_gameClockTimer, &QTimer::timeout, [this]()
            {
        m_gameTime--;
        emit timeChanged(m_gameTime);
        
        if (m_gameTime <= 0) {
            pauseGame();
            emit gameOver(m_score);
        } });

    // Set timer intervals
    m_gameTimer.setInterval(16);        // ~60 FPS
    m_spawnTimer.setInterval(2000);     // 2 seconds between projectiles
    m_gameClockTimer.setInterval(1000); // 1 second for game clock

    // Pre-allocate pool capacity
    m_projectilePool.reserve(50);
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
    if (m_lives > 0 && m_gameTime > 0)
    {
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
    while (!m_projectiles.isEmpty())
    {
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

void GameEngine::updateHandPosition(const QVector3D &position)
{
    m_handPosition = position;
}

QList<Projectile> GameEngine::getProjectiles() const
{
    return m_projectiles;
}

void GameEngine::updateGame()
{
    float deltaTime = 0.016f;
    m_elapsedTime += deltaTime;

    // 1. Detect hits and mark sliced IDs
    checkCollisions();

    // 2. Move projectiles and apply miss‐logic (lives)
    updateProjectiles(deltaTime);
}

void GameEngine::spawnProjectile()
{
    // 1) get a Projectile instance (reuse or fresh)
    Projectile proj = m_projectilePool.isEmpty()
                          ? Projectile(Projectile::CYLINDER) // default type; will override
                          : m_projectilePool.takeLast();

    // 2) re-init it
    int randomType = QRandomGenerator::global()->bounded(4);
    Projectile::Type type = static_cast<Projectile::Type>(randomType);
    proj = Projectile(type); // reset type, color, id
    proj.setPosition(generateRandomLaunchPosition());
    proj.setVelocity(generateRandomVelocity());
    proj.setCreationTime(m_elapsedTime);

    // 3) publish and add to active list
    m_projectiles.append(proj);
    emit projectileAdded(proj);
}

void GameEngine::checkCollisions()
{
    // Define your sword‐plane and full game‐height
    const float sliceZ = 10.0f;    // Must match GameWidget’s z
    const float zTolerance = 0.3f; // Allow for blade tilt (~2.0±0.3)
    const float minZ = sliceZ - zTolerance;
    const float maxZ = sliceZ + zTolerance;
    const float minY = 0.0f;  // Bottom of play area
    const float maxY = 10.0f; // Top of play area

    for (int i = 0; i < m_projectiles.size(); i++)
    {
        Projectile &projectile = m_projectiles[i];
        if (projectile.getState() != Projectile::ACTIVE)
            continue;

        QVector3D pos = projectile.getPosition();

        // Check if projectile is within hit zone boundaries
        bool inHitZone = (pos.z() >= minZ && pos.z() <= maxZ &&
                          pos.y() >= minY && pos.y() <= maxY);

        if (inHitZone)
        {
            // Use line segment collision detection with the actual sword geometry
            if (projectile.isColliding(m_swordHandle, m_swordTip))
            {
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
}

void GameEngine::updateProjectiles(float deltaTime)
{
    // Update each, then recycle any that go inactive or out-of-bounds.
    for (int i = m_projectiles.size() - 1; i >= 0; --i)
    {
        Projectile &p = m_projectiles[i];
        p.update(deltaTime);

        bool expired = false;
        // off top/back
        if (p.getPosition().z() >= 15.0f)
            expired = true;
        // below floor
        if (p.getPosition().y() < -0.1f && p.getState() != Projectile::ACTIVE)
            expired = true;

        if (expired)
        {
            // emit lives-penalty if needed (existing logic)
            if (p.getState() == Projectile::ACTIVE && !p.wasProcessed())
            {
                m_lives = qMax(0, m_lives - 1);
                emit livesChanged(m_lives);
                p.markAsProcessed();
                p.split();
                if (m_lives <= 0)
                {
                    pauseGame();
                    emit gameOver(m_score);
                }
            }
            // recycle instance into pool
            m_projectilePool.append(p);
            // remove from active list
            m_projectiles.removeAt(i);
        }
    }
}

Projectile GameEngine::createRandomProjectile()
{
    // Randomly select a projectile type
    int randomType = QRandomGenerator::global()->bounded(4); // Generate a random number from 0 to 3
    Projectile::Type type;

    switch (randomType)
    {
    case 0:
        type = Projectile::CONE;
        break;
    case 1:
        type = Projectile::CYLINDER;
        break;
    case 2:
        type = Projectile::CUBE;
        break;
    case 3:
        type = Projectile::PYRAMID;
        break;
    default:
        type = Projectile::CYLINDER; // Fallback, should not happen
    }

    Projectile projectile(type);

    // Set position and velocity
    projectile.setPosition(generateRandomLaunchPosition());
    projectile.setVelocity(generateRandomVelocity());
    projectile.setCreationTime(m_elapsedTime);
    projectile.setId(QRandomGenerator::global()->generate()); // Assign unique ID

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
    float targetZ = 15.0f;                                                        // Consistent target distance
    float targetX = (QRandomGenerator::global()->generateDouble() - 0.5f) * 2.0f; // Narrow x-range
    float targetY = 1.2f;                                                         // Low consistent height

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

void GameEngine::connectToGameWidget(GameWidget *widget)
{
    if (widget)
    {
        connect(widget, &GameWidget::swordPositionUpdated,
                this, &GameEngine::updateSwordPosition);
        // Initialize sword positions to default values
        m_swordHandle = QVector3D(0.0f, 0.0f, 0.0f);
        m_swordTip = QVector3D(0.0f, 1.0f, 0.0f);
    }
}

void GameEngine::updateSwordPosition(const QVector3D &handlePos, const QVector3D &tipPos)
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

void GameEngine::markProjectileSlicedById(int id)
{
    for (Projectile &proj : m_projectiles)
    {
        if (proj.getId() == id && !proj.wasSliced())
        {
            proj.markAsSliced();
            proj.markAsProcessed(); // Prevent lives from decreasing
            proj.split();           // Mark the projectile as split
            m_score += proj.getPointValue();
            emit scoreChanged(m_score);
            break;
        }
    }
}

void GameEngine::addProjectile(const Projectile &projectile)
{
    // Store the original Projectile in m_projectiles
    m_projectiles.push_back(projectile);

    // Create render data for the projectile
    ProjectileRenderData renderData;
    renderData.id = projectile.getId(); // Use the getter method to access the ID
    // ...initialize other renderData fields...

    // Add to the render data list
    m_projectileRenderData.push_back(renderData);

    // Emit signal for UI updating
    emit projectileAdded(projectile);
}
