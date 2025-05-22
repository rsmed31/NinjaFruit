#include <GL/gl.h>
extern "C"
{
#include <GL/glu.h>
}
#include "gamewidget.h"
#include <QtMath>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>
#include "physicsutils.h"
#include "hand_renderer.h"

GameWidget::GameWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      m_handPosition(0.0f, 0.0f, 0.0f),
      m_cameraDistance(10.0f),
      m_cameraHeight(5.0f),
      m_floorSize(20.0f),
      m_handRangeRadius(3.0f),
      m_handRangeHeight(8.0f),
      m_elapsedTime(0.0f),
      m_program(nullptr),
      m_projectileRenderer(nullptr),
      m_handRenderer(nullptr),
      m_sceneRenderer(nullptr)
{
    // Set focus policy to accept keyboard input
    setFocusPolicy(Qt::StrongFocus);

    // Initialize timer for animation
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &GameWidget::updateScene);
    m_animationTimer->start(16); // ~60 FPS
}

GameWidget::~GameWidget()
{
    // Make sure the context is current when deleting resources
    makeCurrent();

    delete m_program;
    delete m_projectileRenderer;
    delete m_handRenderer;
    delete m_sceneRenderer;

    m_vertexBuffer.destroy();
    m_indexBuffer.destroy();
    m_vao.destroy();

    doneCurrent();
}

void GameWidget::setHandPosition(float x, float y)
{
    if (x == -999.0f && y == -999.0f)
    {
        m_handPosition = m_lastValidHandPosition;
    }
    else
    {
        m_handPosition = QVector3D(x, y, 10.0f);
        m_lastValidHandPosition = m_handPosition;
    }

    // Now we can properly update the hand renderer
    if (m_handRenderer)
    {
        m_handRenderer->setHandPosition(m_handPosition);
    }

    update();
}

void GameWidget::initializeGL()
{
    // Initialize OpenGL functions
    initializeOpenGLFunctions();
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Darker background
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    loadTextures();

    // Enable lighting for better 3D appearance
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    // Setup materials
    GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat position[] = {0.0f, 10.0f, 0.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    // Enable secondary light source
    glEnable(GL_LIGHT1);

    // Define light1: overhead + angled
    GLfloat light1_pos[] = {-5.0f, 5.0f, 5.0f, 1.0f};    // Slightly behind and above
    GLfloat light1_diffuse[] = {0.3f, 0.3f, 0.3f, 1.0f}; // Soft white light
    GLfloat light1_specular[] = {0.2f, 0.2f, 0.2f, 1.0f};

    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
    // Enable third light source
    glEnable(GL_LIGHT2);
    // Define light2: lower side fill light (e.g. under the player, angled up)
    GLfloat light2_pos[] = {3.0f, -2.0f, 2.0f, 1.0f};     // From below right
    GLfloat light2_diffuse[] = {0.2f, 0.2f, 0.5f, 1.0f};  // Cool bluish light
    GLfloat light2_specular[] = {0.1f, 0.1f, 0.3f, 1.0f}; // Subtle specular highlights

    glLightfv(GL_LIGHT2, GL_POSITION, light2_pos);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);

    // Create shaders and geometry
    createShaders();
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE); // ✅ Add this
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_NORMALIZE);
    // glDisable(GL_CULL_FACE); // optional for test

    // Initialize rendering components
    m_projectileRenderer = new ProjectileRenderer(m_projectiles, m_textures, m_elapsedTime);
    m_handRenderer = new HandRenderer(m_handPosition, m_textures);
    m_sceneRenderer = new SceneRenderer(m_floorSize, m_cameraDistance, m_cameraHeight);

    // Set up view matrix - position camera for a front view
    m_viewMatrix.setToIdentity();
    m_viewMatrix.lookAt(
        QVector3D(0.0f, 2.0f, 15.0f), // Camera position (from front)
        QVector3D(0.0f, 0.0f, 0.0f),  // Look at center
        QVector3D(0.0f, 1.0f, 0.0f)   // Up vector
    );
}

// Adjust camera position and field of view for better perspective
void GameWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. Set up field of view
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Use 95 degree FOV for wider view to see incoming objects better
    gluPerspective(95.0, width() / static_cast<float>(height()), 0.1, 100.0);

    // 2. Move camera for better first-person view
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        0.0, 1.8, 6.0,   // Eye
        0.0, 1.0, -20.0, // Center (less tilt)
        0.0, 1.0, 0.0    // Up
    );

    // 3. Lighting setup
    glEnable(GL_LIGHTING);
    GLfloat light_position[] = {0.0f, 5.0f, 5.0f, 1.0f}; // Move light closer to camera
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    // 4. Draw game elements
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Important: Release shader program to ensure fixed-function texturing works
    if (m_program)
    {
        m_program->release();
    }

    if (m_sceneRenderer)
    {
        // Draw arena walls with appropriate textures
        m_sceneRenderer->drawArenaWalls(
            m_textures[4], // Wall texture
            m_textures[5], // Arch texture
            m_textures[6]  // Portal texture
        );

        m_sceneRenderer->drawDistanceIndicators();
        m_sceneRenderer->drawHitCylinder();
    }

    // Optionally rebind program for other rendering that might need it
    // if (m_program) {
    //    m_program->bind();
    // }

    drawProjectiles();

    if (m_handRenderer)
    {
        m_handRenderer->drawVirtualHand();
    }
}

void GameWidget::resizeGL(int width, int height)
{
    // Update projection matrix for new aspect ratio
    float aspectRatio = static_cast<float>(width) / height;
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.perspective(45.0f, aspectRatio, 0.1f, 100.0f);
}

void GameWidget::updateScene()
{
    // Track elapsed time
    m_elapsedTime += 0.016f; // ~16ms per frame at 60 FPS
    m_handPosition.setX(0.8f * m_handPosition.x() + 0.2f * m_lastValidHandPosition.x());
    m_handPosition.setY(0.8f * m_handPosition.y() + 0.2f * m_lastValidHandPosition.y());
    // Check for sword-projectile collisions
    checkHitZoneCollisions();
    // Update projectile positions based on physics
    updateProjectilePositions();
    // Request a redraw
    update();
}

// Fix the function implementation
ProjectileRenderData::Type GameWidget::mapProjectileType(Projectile::Type type)
{
    switch (type)
    {
    case Projectile::CONE:
        return ProjectileRenderData::CONE;
    case Projectile::CYLINDER:
        return ProjectileRenderData::CYLINDER;
    case Projectile::CUBE:
        return ProjectileRenderData::CUBE;
    case Projectile::PYRAMID:
        return ProjectileRenderData::PYRAMID;
    default:
        return ProjectileRenderData::CONE; // Default case
    }
}

// Keep only the Projectile object version
void GameWidget::launchProjectile(const Projectile &projectile)
{
    ProjectileRenderData projData;
    projData.position = projectile.getPosition();
    projData.velocity = projectile.getVelocity();
    projData.type = mapProjectileType(projectile.getType());
    projData.spawnTime = m_elapsedTime;
    projData.active = true;
    projData.state = ProjectileRenderData::ACTIVE;
    projData.id = projectile.getId(); // Assign unique ID

    configureProjectileTrajectory(projData);
    m_projectiles.append(projData);
}

void GameWidget::splitProjectile(int index)
{
    if (index >= 0 && index < m_projectiles.size())
    {
        m_projectiles[index].state = ProjectileRenderData::SPLIT;
        // Could add split animation timing here
    }
}

void GameWidget::clearProjectiles()
{
    m_projectiles.clear();
}

void GameWidget::getSwordEndpoints(QVector3D &handlePos, QVector3D &tipPos)
{
    // Start with the hand position
    float handX = m_handPosition.x() * 0.25f; // Match scale in drawVirtualHand
    float handY = m_handPosition.y() * 0.25f;
    handlePos = QVector3D(handX, handY + 0.5f, 2.0f); // Match cylinder plane

    // Rotation angles from drawVirtualHand (should match exactly)
    float rotZ = 15.0f * M_PI / 180.0f;  // 15° in radians
    float rotY = -20.0f * M_PI / 180.0f; // -20° in radians

    // Sword length in world units (scaled from model units)
    float swordScale = 0.12f;
    float swordLength = 18.0f * swordScale;

    // Calculate the tip position by applying the rotations to a vector pointing upward
    QVector3D direction(0.0f, swordLength, 0.0f);

    // Apply Z rotation
    float tempX = direction.x() * cos(rotZ) - direction.y() * sin(rotZ);
    float tempY = direction.x() * sin(rotZ) + direction.y() * cos(rotZ);
    direction.setX(tempX);
    direction.setY(tempY);

    // Apply Y rotation
    tempX = direction.x() * cos(rotY) + direction.z() * sin(rotY);
    float tempZ = -direction.x() * sin(rotY) + direction.z() * cos(rotY);
    direction.setX(tempX);
    direction.setZ(tempZ);

    // Calculate tip position by adding direction vector to handle position
    tipPos = handlePos + direction;

    // This publishes the handle and tip positions for collision detection
    emit swordPositionUpdated(handlePos, tipPos);
}

// Update the drawProjectiles method to ensure they appear within view
void GameWidget::drawProjectiles()
{
    if (m_projectileRenderer)
    {
        m_projectileRenderer->drawProjectiles();
    }
}

void GameWidget::createShaders()
{
    // Create shader program
    m_program = new QOpenGLShaderProgram(this);

    // Add vertex shader
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                       "attribute vec3 position;\n"
                                       "uniform mat4 projectionMatrix;\n"
                                       "uniform mat4 viewMatrix;\n"
                                       "uniform mat4 modelMatrix;\n"
                                       "void main() {\n"
                                       "    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);\n"
                                       "}\n");

    // Add fragment shader
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                       "uniform vec3 objectColor;\n"
                                       "void main() {\n"
                                       "    gl_FragColor = vec4(objectColor, 1.0);\n"
                                       "}\n");
    // Link shader program
    if (!m_program->link())
    {
        qDebug() << "Failed to link shader program:" << m_program->log();
        delete m_program;
        m_program = nullptr;
        return;
    }
}

void GameWidget::updateProjectilePositions()
{
    QMutableListIterator<ProjectileRenderData> i(m_projectiles);
    while (i.hasNext())
    {
        ProjectileRenderData &proj = i.next();

        float timeActive = m_elapsedTime - proj.spawnTime;
        // Avoid unused variable warning
        Q_UNUSED(calculateProjectilePosition(proj, timeActive));
    }
}
void GameWidget::checkHitZoneCollisions()
{
    // Get sword endpoints
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);

    // Adjusted hit cylinder zone
    const float cylinderRadius = 6.0f;
    const float cylinderHeight = 10.0f;
    const QVector3D cylinderCenter(0.0f, 3.0f, -3.5f); // Align with sword plane

    QVector3D swordVector = tipPos - handlePos;
    float swordLength = swordVector.length();

    // Ensure sword length is within a valid range
    if (swordLength < 1.0f || swordLength > 10.0f)
    {
        qDebug() << "Sword length out of bounds, adjusting...";
        swordLength = std::clamp(swordLength, 1.0f, 10.0f);
        tipPos = handlePos + swordVector.normalized() * swordLength;
    }

    int index = 0;
    QMutableListIterator<ProjectileRenderData> i(m_projectiles);

    while (i.hasNext())
    {
        ProjectileRenderData &proj = i.next();

        if (proj.state != ProjectileRenderData::ACTIVE)
        {
            ++index;
            continue;
        }

        float t = m_elapsedTime - proj.spawnTime;
        QVector3D center = calculateProjectilePosition(proj, t);

        // Basic spatial filter (is it in the hit zone?)
        float dx = center.x() - cylinderCenter.x();
        float dz = center.z() - cylinderCenter.z();
        float distXZ = std::sqrt(dx * dx + dz * dz);

        bool inRadius = distXZ <= cylinderRadius;
        bool inHeight = center.y() >= cylinderCenter.y() &&
                        center.y() <= cylinderCenter.y() + cylinderHeight;

        if (!(inRadius && inHeight))
        {
            ++index;
            continue;
        }

        // Get accurate collision radius based on shape geometry
        float radius;
        switch (proj.type)
        {
        case ProjectileRenderData::CYLINDER:
            // Cylinder with radius 0.5, height 2.0
            radius = std::sqrt(0.5f * 0.5f + 1.0f * 1.0f); // ≈ 1.12f
            break;
        case ProjectileRenderData::CONE:
            // Cone with base radius 0.6, height 2.0
            radius = std::sqrt(0.6f * 0.6f + 1.0f * 1.0f); // ≈ 1.17f
            break;
        case ProjectileRenderData::CUBE:
            // Cube with side length 1.2 (2*0.6)
            radius = 0.6f * std::sqrt(3.0f); // ≈ 1.04f
            break;
        case ProjectileRenderData::PYRAMID:
            // Pyramid with base 1.4×1.4 and height 1.5
            radius = std::sqrt(0.7f * 0.7f + 0.7f * 0.7f + 1.5f * 1.5f); // ≈ 1.82f
            break;
        default:
            radius = 1.0f;
            break;
        }

        // Unified sphere-segment collision test
        float dist = distanceBetweenSegments(handlePos, tipPos, center, center);
        if (dist <= radius)
        {
            emit projectileSlicedById(proj.id); // Emit signal with projectile ID
            splitProjectile(index);
            qDebug() << "Hit projectile of type" << proj.type << "at index" << index;
        }

        ++index;
    }
}

void GameWidget::loadTextures()
{
    // Generate texture IDs (7 total: 0-3 for projectiles, 4-6 for arena)
    glGenTextures(7, m_textures);
    

    // Load cube texture (index 0)
    QImage cubeImg(":/textures/textures/cube.png");
    if (cubeImg.isNull())
    {
        qDebug() << "Failed to load cube texture";
        cubeImg = QImage(1, 1, QImage::Format_RGBA8888);
        cubeImg.fill(Qt::red);
    }
    cubeImg = cubeImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cubeImg.width(), cubeImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, cubeImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load pyramid texture (index 1)
    QImage pyramidImg(":/textures/textures/pyramid.png");
    if (pyramidImg.isNull())
    {
        qDebug() << "Failed to load pyramid texture";
        pyramidImg = QImage(1, 1, QImage::Format_RGBA8888);
        pyramidImg.fill(Qt::green);
    }
    pyramidImg = pyramidImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pyramidImg.width(), pyramidImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pyramidImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load cylinder (carrot) texture (index 2)
    QImage carrotImg(":/textures/textures/carrot.png");
    if (carrotImg.isNull())
    {
        qDebug() << "Failed to load carrot texture";
        carrotImg = QImage(1, 1, QImage::Format_RGBA8888);
        carrotImg.fill(Qt::blue);
    }
    carrotImg = carrotImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, carrotImg.width(), carrotImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, carrotImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load cone texture (index 3)
    QImage coneImg(":/textures/textures/cone.png");
    if (coneImg.isNull())
    {
        qDebug() << "Failed to load cone texture";
        coneImg = QImage(1, 1, QImage::Format_RGBA8888);
        coneImg.fill(Qt::yellow);
    }
    coneImg = coneImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[3]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, coneImg.width(), coneImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, coneImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load wall texture (index 4)
    QImage wallImg(":/textures/textures/wall.png");
    if (wallImg.isNull())
    {
        qDebug() << "Failed to load wall texture";
        wallImg = QImage(1, 1, QImage::Format_RGBA8888);
        wallImg.fill(QColor(100, 100, 255, 150));
    }
    wallImg = wallImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[4]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, wallImg.width(), wallImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, wallImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Load arch texture (index 5)
    QImage archImg(":/textures/textures/wall.png");
    if (archImg.isNull())
    {
        qDebug() << "Failed to load arch texture";
        archImg = QImage(1, 1, QImage::Format_RGBA8888);
        archImg.fill(QColor(255, 255, 255, 200));
    }
    archImg = archImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[5]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, archImg.width(), archImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, archImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Load portal texture (index 6) - copy exact approach from wall texture
    QImage portalImg(":/textures/textures/arch_portal.png");
    if (portalImg.isNull())
    {
        qDebug() << "Failed to load arch_portal texture";
        portalImg = QImage(1, 1, QImage::Format_RGBA8888);
        portalImg.fill(QColor(100, 100, 255, 150));
    }
    portalImg = portalImg.convertToFormat(QImage::Format_RGBA8888);

    glBindTexture(GL_TEXTURE_2D, m_textures[6]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, portalImg.width(), portalImg.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, portalImg.bits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Unbind texture when done
    glBindTexture(GL_TEXTURE_2D, 0);

    qDebug() << "Textures loaded successfully";
}
void GameWidget::createRandomProjectile()
{
    // Debug output to check type distribution
    int randomType = QRandomGenerator::global()->bounded(4);
    Projectile::Type type;

    switch (randomType)
    {
    case 0:
        type = Projectile::CONE;
        qDebug() << "Creating CONE";
        break;
    case 1:
        type = Projectile::CYLINDER;
        qDebug() << "Creating CYLINDER";
        break;
    case 2:
        type = Projectile::CUBE;
        qDebug() << "Creating CUBE";
        break;
    case 3:
        type = Projectile::PYRAMID;
        qDebug() << "Creating PYRAMID";
        break;
    default:
        type = Projectile::CONE;
        qDebug() << "Default to CONE";
    }
    Projectile projectile(type);                         // Pass type to constructor instead of using setType
    projectile.setPosition(QVector3D(0.0f, 1.0f, 5.0f)); // Start position
    launchProjectile(projectile);
}
