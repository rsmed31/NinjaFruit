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
      m_handRenderer(nullptr)
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
    if (m_handRenderer) {
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

    // Load textures
    loadTextures();

    // Enable lighting for better 3D appearance
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    // glEnable(GL_COLOR_MATERIAL); // REMOVE this line to avoid color/lighting conflicts

    // Setup materials
    GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat position[] = {0.0f, 10.0f, 0.0f, 1.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    //
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
        0.0, 1.8, 6.0,   // Move camera closer (z=6.0 instead of 8.0)
        0.0, 1.0, -30.0, // Look further down the z-axis for better depth
        0.0, 1.0, 10.0   // Up vector
    );

    // 3. Lighting setup
    glEnable(GL_LIGHTING);
    GLfloat light_position[] = {0.0f, 5.0f, 5.0f, 1.0f}; // Move light closer to camera
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    // 4. Draw game elements
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawDistanceIndicators();
    drawHitCylinder(); // Draw proper 3D hit zone cylinder
    drawProjectiles();
    if (m_handRenderer) {
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

// Add a new method to handle projectile splitting
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

// Modify drawDistanceIndicators to make hit range more visible
void GameWidget::drawDistanceIndicators()
{
    // Disable lighting for the ground plane
    glDisable(GL_LIGHTING);

    // Draw a ground plane with fading colors to indicate distance
    glBegin(GL_QUADS);

    // Near zone - red (danger zone)
    glColor4f(0.7f, 0.0f, 0.0f, 0.3f);
    glVertex3f(-20.0f, 0.0f, 10.0f);
    glVertex3f(20.0f, 0.0f, 10.0f);

    // Middle zone - yellow (warning zone)
    glColor4f(0.7f, 0.7f, 0.0f, 0.3f);
    glVertex3f(20.0f, 0.0f, 0.0f);
    glVertex3f(-20.0f, 0.0f, 0.0f);
    glEnd();

    // Middle to far zone - green and blue gradient
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.7f, 0.0f, 0.3f);
    glVertex3f(-20.0f, 0.0f, 0.0f);
    glVertex3f(20.0f, 0.0f, 0.0f);

    glColor4f(0.0f, 0.0f, 0.7f, 0.3f);
    glVertex3f(20.0f, 0.0f, -20.0f);
    glVertex3f(-20.0f, 0.0f, -20.0f);
    glEnd();

    // Add distance marker rings
    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    for (int z = -15; z <= 10; z += 5)
    {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++)
        {
            float angle = i * 10.0f * M_PI / 180.0f;
            float x = 5.0f * cos(angle);
            float y = 0.02f;
            glVertex3f(x, y, z);
        }
        glEnd();
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw the hit zone closer to the camera (between 0.0f and 1.5f)
    // with a more visible color and border
    glColor4f(0.2f, 1.0f, 0.2f, 0.3f); // More saturated green
    glBegin(GL_QUADS);
    glVertex3f(-8.0f, 0.001f, 0.0f); // Closer to camera
    glVertex3f(8.0f, 0.001f, 0.0f);
    glVertex3f(8.0f, 0.001f, 1.5f);
    glVertex3f(-8.0f, 0.001f, 1.5f);
    glEnd();

    // Draw a border around the hit zone
    glColor4f(1.0f, 1.0f, 1.0f, 0.8f); // Clear white border
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-8.0f, 0.005f, 0.0f);
    glVertex3f(8.0f, 0.005f, 0.0f);
    glVertex3f(8.0f, 0.005f, 1.5f);
    glVertex3f(-8.0f, 0.005f, 1.5f);
    glEnd();
    glLineWidth(1.0f);

    // Add a "HIT ZONE" text indicator (simulated with lines for simplicity)
    glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
    glPushMatrix();
    glTranslatef(0.0f, 0.01f, 0.75f); // Center of hit zone
    // We'd need actual text rendering here - simplified with a marker
    glBegin(GL_LINES);
    glVertex3f(-2.0f, 0.0f, 0.0f);
    glVertex3f(2.0f, 0.0f, 0.0f);
    glEnd();
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void GameWidget::drawHitCylinder()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();

    const float radius = 6.0f;
    const float z = 2.0f;                // Slicing plane
    const float height = 10.0f;          // Full game height
    const float yCenter = height / 2.0f; // Center at 5.0f
    const int segments = 64;             // Number of segments for cylinder
    const int rings = 14;                // Number of rings for height division

    glTranslatef(0.0f, yCenter, z);

    glColor4f(0.5f, 1.0f, 1.0f, 0.4f); // Bright mesh color
    glLineWidth(1.2f);

    // Horizontal rings
    for (int j = 0; j <= rings; ++j)
    {
        float y = -height / 2.0f + j * (height / rings);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i)
        {
            float angle = i * 2.0f * M_PI / segments;
            float x = radius * cos(angle);
            float z = radius * sin(angle);
            glVertex3f(x, y, z);
        }
        glEnd();
    }

    // Vertical lines
    for (int i = 0; i <= segments; ++i)
    { // 🔺 only front half (180°)
        float angle = M_PI * i / (segments / 2);
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j <= rings; ++j)
        {
            float y = -height / 2.0f + j * (height / rings);
            glVertex3f(x, y, z);
        }
        glEnd();
    }

    glLineWidth(1.0f);
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// Add this method near the other helper functions to get sword endpoints in world space
void GameWidget::getSwordEndpoints(QVector3D &handlePos, QVector3D &tipPos)
{
    // Start with the hand position
    float handX = m_handPosition.x() * 0.25f; // Match scale in drawVirtualHand
    float handY = m_handPosition.y() * 0.25f;

    // Calculate handle position (base of sword) in world coordinates
    // Match the translation in drawVirtualHand
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
    if (m_projectileRenderer) {
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

// Add this method to detect collisions between sword and projectiles in the hit zone
void GameWidget::checkHitZoneCollisions()
{
    // Get sword endpoints
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);

    // Adjusted hit cylinder zone
    const float cylinderRadius = 6.0f;
    const float cylinderHeight = 9.0f;
    const QVector3D cylinderCenter(0.0f, 1.5f, -3.5f); // Align with sword plane

    QVector3D swordVector = tipPos - handlePos;
    float swordLength = swordVector.length();

    // Ensure sword length is within a valid range
    if (swordLength < 1.0f || swordLength > 10.0f)
    {
        qDebug() << "Sword length out of bounds, adjusting...";
        swordLength = std::clamp(swordLength, 1.0f, 10.0f);
        tipPos = handlePos + swordVector.normalized() * swordLength;
    }

    QVector3D swordDir = swordVector.normalized();

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
        QVector3D pos = calculateProjectilePosition(proj, t);

        // Basic spatial filter (is it in the hit zone?)
        float dx = pos.x() - cylinderCenter.x();
        float dz = pos.z() - cylinderCenter.z();
        float distXZ = std::sqrt(dx * dx + dz * dz);

        bool inRadius = distXZ <= cylinderRadius;
        bool inHeight = pos.y() >= cylinderCenter.y() &&
                        pos.y() <= cylinderCenter.y() + cylinderHeight;

        if (!(inRadius && inHeight))
        {
            ++index;
            continue;
        }

        // Refine collision detection for projectiles
        float projectileRadius = getProjectileCollisionRadius(static_cast<Projectile::Type>(proj.type));

        if (proj.type == ProjectileRenderData::CYLINDER)
        {
            QVector3D cylCenter = pos;
            QVector3D cylHalfVec = QVector3D(1.0f, 0.0f, 0.0f) * (2.0f * 0.5f);
            QVector3D cylStart = cylCenter - cylHalfVec;
            QVector3D cylEnd = cylCenter + cylHalfVec;

            float dist = distanceBetweenSegments(handlePos, tipPos, cylStart, cylEnd);

            if (dist <= 0.5f)
            {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Cylinder HIT at index" << index;
            }
        }
        else if (proj.type == ProjectileRenderData::CONE)
        {
            QVector3D coneStart = pos;
            QVector3D coneEnd = pos + QVector3D(0.0f, 0.0f, 2.0f);

            float dist = distanceBetweenSegments(handlePos, tipPos, coneStart, coneEnd);

            if (dist <= 0.6f)
            {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Cone HIT at index" << index;
            }
        }
        else if (proj.type == ProjectileRenderData::PYRAMID)
        {
            QVector3D baseCenter = pos;
            QVector3D tip = pos + QVector3D(0.0f, 1.6f, 0.0f);

            float dist = distanceBetweenSegments(handlePos, tipPos, baseCenter, tip);

            if (dist <= 1.0f)
            {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Pyramid HIT at index" << index;
            }
        }
        else
        {
            QVector3D toProj = pos - handlePos;
            float projLen = QVector3D::dotProduct(toProj, swordDir);
            projLen = std::clamp(projLen, 0.0f, swordLength);
            QVector3D closest = handlePos + swordDir * projLen;

            float distToSword = (closest - pos).length();

            if (distToSword <= projectileRadius)
            {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Default HIT at index" << index;
            }
        }

        ++index;
    }
}

void GameWidget::loadTextures()
{
    // Generate texture IDs
    glGenTextures(4, m_textures);

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
    QImage pyramidImg(":/textures/textures/pyramid.jpg");
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
    QImage carrotImg(":/textures/textures/carrot.jpg");
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
    QImage coneImg(":/textures/textures/cone.jpg");
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

    // Unbind texture when done
    glBindTexture(GL_TEXTURE_2D, 0);

    qDebug() << "Textures loaded successfully";
}

// Make sure your random projectile generation includes all types
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

    // Create projectile with the determined type
    Projectile projectile(type);                         // Pass type to constructor instead of using setType
    projectile.setPosition(QVector3D(0.0f, 1.0f, 5.0f)); // Start position
    launchProjectile(projectile);
}
