#include <GL/gl.h>
extern "C" {
#include <GL/glu.h>
}
#include "gamewidget.h"
#include <QtMath>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator> // Add this include for random number generation

// Vertex data for drawing primitives
static const GLfloat cylinderVertices[] = {

};

static const GLuint cylinderIndices[] = {

};

GameWidget::GameWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_handPosition(0.0f, 0.0f, 0.0f)
    , m_cameraDistance(10.0f)
    , m_cameraHeight(5.0f)
    , m_floorSize(20.0f)
    , m_handRangeRadius(3.0f)
    , m_handRangeHeight(8.0f)
    , m_elapsedTime(0.0f)
    , m_program(nullptr)
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
    
    m_vertexBuffer.destroy();
    m_indexBuffer.destroy();
    m_vao.destroy();
    
    doneCurrent();
}

// Update the setHandPosition function for better mapping
void GameWidget::setHandPosition(float x, float y)
{
    if (x == -999.0f && y == -999.0f) {
        m_handPosition = m_lastValidHandPosition;
    } else {
        m_handPosition = QVector3D(x, y, 10.0f);
        m_lastValidHandPosition = m_handPosition;
    }
    update();
}

void GameWidget::initializeGL()
{
    // Suppress unused variable warnings
    (void)cylinderVertices;
    (void)cylinderIndices;

    initializeOpenGLFunctions();
    // Initialize OpenGL functions
    initializeOpenGLFunctions();
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Darker background
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    
    // Enable lighting for better 3D appearance
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    
    // Setup materials
    GLfloat ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat position[] = {0.0f, 10.0f, 0.0f, 1.0f};
    
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    
    // Create shaders and geometry
    createShaders();
    createGeometry();
    
    // Set up view matrix - position camera for a front view
    m_viewMatrix.setToIdentity();
    m_viewMatrix.lookAt(
        QVector3D(0.0f, 2.0f, 15.0f),   // Camera position (from front)
        QVector3D(0.0f, 0.0f, 0.0f),    // Look at center
        QVector3D(0.0f, 1.0f, 0.0f)     // Up vector
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
        0.0, 1.8, 6.0,      // Move camera closer (z=6.0 instead of 8.0)
        0.0, 1.0, -30.0,    // Look further down the z-axis for better depth
        0.0, 1.0, 10.0       // Up vector
    );
    
    // 3. Lighting setup
    glEnable(GL_LIGHTING);
    GLfloat light_position[] = {0.0f, 5.0f, 5.0f, 1.0f};  // Move light closer to camera
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    
    // 4. Draw game elements
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    drawDistanceIndicators();
    drawHitCylinder();       // Draw proper 3D hit zone cylinder
    drawProjectiles();
    drawVirtualHand();       // Draw sword last so it appears on top
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
    m_handPosition.setX(0.85f * m_handPosition.x() + 0.15f * m_lastValidHandPosition.x());
    m_handPosition.setY(0.85f * m_handPosition.y() + 0.15f * m_lastValidHandPosition.y());

    // Check for sword-projectile collisions
    checkHitZoneCollisions();
    
    // Update projectile positions based on physics
    updateProjectilePositions();
    
    // Request a redraw
    update();
}

void GameWidget::launchProjectile(const QVector3D& position, const QVector3D& velocity)
{
    ProjectileRenderData projectile;
    projectile.position = position;
    
    // Modify velocity to ensure projectiles head toward the screen
    // Start with the provided velocity but adjust it to target the hit zone
    QVector3D adjustedVelocity = velocity;
    
    // Push projectile forward toward the player with a minimum z-velocity
    if (adjustedVelocity.z() > -5.0f) {
        adjustedVelocity.setZ(-5.0f - (QRandomGenerator::global()->bounded(5)));  // Ensure strong forward momentum
    }
    
    // Reduce extreme lateral movements
    if (qAbs(adjustedVelocity.x()) > 8.0f) {
        adjustedVelocity.setX(adjustedVelocity.x() > 0 ? 8.0f : -8.0f);
    }
    
    // Ensure upward launch to give time to hit them
    if (adjustedVelocity.y() < 2.0f) {
        adjustedVelocity.setY(2.0f + (QRandomGenerator::global()->bounded(4)));
    }
    
    projectile.velocity = adjustedVelocity;
    projectile.spawnTime = m_elapsedTime;
    projectile.active = true;
    projectile.state = ProjectileRenderData::ACTIVE;
    
    // Set random projectile type among the 4 required types
    int typeValue = QRandomGenerator::global()->bounded(4); // 0 to 3
    projectile.type = static_cast<ProjectileRenderData::Type>(typeValue);

    
    // Verify the projectile will actually reach the screen
    configureProjectileTrajectory(projectile);
    
    m_projectiles.append(projectile);
}

// Add a new method to handle projectile splitting
void GameWidget::splitProjectile(int index)
{
    if (index >= 0 && index < m_projectiles.size()) {
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
    for (int z = -15; z <= 10; z += 5) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++) {
            float angle = i * 10.0f * M_PI / 180.0f;
            float x = 5.0f * cos(angle);
            float y = 0.02f;
            glVertex3f(x, y, z);
        }
        glEnd();
        
    }
    
    // After drawing the ground plane and marker rings, add hit region overlay:
    // Move the hit region closer to the player to match the collision detection zone
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw the hit zone closer to the camera (between 0.0f and 1.5f)
    // with a more visible color and border
    glColor4f(0.2f, 1.0f, 0.2f, 0.3f); // More saturated green
    glBegin(GL_QUADS);
      glVertex3f(-8.0f, 0.001f, 2.5f);
      glVertex3f( 8.0f, 0.001f, 2.5f);
      glVertex3f( 8.0f, 0.001f, 3.5f);
      glVertex3f(-8.0f, 0.001f, 3.5f);
    glEnd();
    
    // Draw a border around the hit zone
    glColor4f(1.0f, 1.0f, 1.0f, 0.8f); // Clear white border
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
      glVertex3f(-8.0f, 0.005f, 2.5f);
      glVertex3f( 8.0f, 0.005f, 2.5f);
      glVertex3f( 8.0f, 0.005f, 3.5f);
      glVertex3f(-8.0f, 0.005f, 3.5f);
    glEnd();
    glLineWidth(1.0f);
    
    glPushMatrix();
    glTranslatef(0.0f, 0.01f, 3.0f);    // center at new plane
    // We'd need actual text rendering here - simplified with a marker
    glBegin(GL_LINES);
      glVertex3f(-2.0f, 0.0f, 0.0f);
      glVertex3f(2.0f, 0.0f, 0.0f);
    glEnd();
    glPopMatrix();
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void GameWidget::drawHitCylinder() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();

    const float radius = 6.0f;
    const float z = 3.0f;    // ← match sword plane
    const float yCenter = 1.5f;
    const float height = 9.0f;
    const int segments = 64;
    const int rings = 14;

    glTranslatef(0.0f, yCenter, z);

    glColor4f(0.5f, 1.0f, 1.0f, 0.4f);  // Bright mesh color
    glLineWidth(1.2f);

    // Horizontal rings
    for (int j = 0; j <= rings; ++j) {
        float y = -height / 2.0f + j * (height / rings);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = 2 * M_PI * i / segments;
            float x = radius * cos(angle);
            float z = radius * sin(angle);
            glVertex3f(x, y, z);
        }
        glEnd();
    }

    // Vertical lines
    for (int i = 0; i <= segments; ++i) {  // 🔺 only front half (180°)
        float angle = M_PI * i / (segments / 2);
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j <= rings; ++j) {
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
void GameWidget::getSwordEndpoints(QVector3D& handlePos, QVector3D& tipPos)
{
    // Start with the hand position
    float handX = m_handPosition.x() * 0.25f;  // Match scale in drawVirtualHand
    float handY = m_handPosition.y() * 0.25f;

    // Calculate handle position (base of sword) in world coordinates
    // Match the translation in drawVirtualHand
    handlePos = QVector3D(handX, handY + 0.5f, 3.0f);  // Adjust z-plane
    
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

// Adjust virtual hand position to be much closer to the camera
void GameWidget::drawVirtualHand()
{
    glPushMatrix();
    
    // Map hand position with more responsive tracking
    float angle = m_handPosition.x() * (M_PI / 2.0f); // from -90° to +90°
    float radius = 6.0f; // ✅ match cylinder radius
    float x = m_handPosition.x();  // Already normalized in [-7.5, 7.5]
    float z = 3.0f;  // Bring sword forward into the hit-zone plane
    float y = m_handPosition.y() * 0.6f;  // 🔺 broader vertical sweep
    
    // Position sword closer to camera for better first-person feel
    // Move z value closer to camera (from 0.75f to 3.0f)
    glTranslatef(x, y + 0.8f, z); // Adjust z-plane

    
    // Calculate and emit sword endpoints for collision detection
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);
    
    // Adjust rotation for more natural sword orientation
    glRotatef(15.0f, 0.0f, 0.0f, 1.0f);  // Less tilt on Z axis
    glRotatef(-20.0f, 0.0f, 1.0f, 0.0f); // Less rotation on Y axis
    
    // Increase scale for bigger sword appearance
    float swordScale = 0.12f;
    glScalef(swordScale, swordScale, swordScale);

    
    glDisable(GL_LIGHTING);
    
    // Draw sword with more detailed appearance
    // Handle
    glColor3f(0.45f, 0.3f, 0.15f);  // Dark brown
    glBegin(GL_QUADS);
        glVertex3f(-0.8f, -8.0f, 0.0f);
        glVertex3f( 0.8f, -8.0f, 0.0f);
        glVertex3f( 0.8f, -2.5f, 0.0f);
        glVertex3f(-0.8f, -2.5f, 0.0f);
    glEnd();
    
    // Crossguard with metallic appearance
    glColor3f(0.9f, 0.8f, 0.3f);  // Brighter gold
    glBegin(GL_QUADS);
        glVertex3f(-3.0f, -2.5f, 0.0f);
        glVertex3f( 3.0f, -2.5f, 0.0f);
        glVertex3f( 3.0f, -1.5f, 0.0f);
        glVertex3f(-3.0f, -1.5f, 0.0f);
    glEnd();
    
    // Blade with metallic sheen
    glColor3f(0.95f, 0.95f, 1.0f);  // Bright silver color
    glBegin(GL_QUADS);
        glVertex3f(-1.0f, -2.0f, 0.0f);
        glVertex3f( 1.0f, -2.0f, 0.0f);
        glVertex3f( 0.5f, 15.0f, 0.0f);
        glVertex3f(-0.5f, 15.0f, 0.0f);
    glEnd();
    
    // Blade tip
    glBegin(GL_TRIANGLES);
        glVertex3f(-0.5f, 15.0f, 0.0f);
        glVertex3f( 0.5f, 15.0f, 0.0f);
        glVertex3f( 0.0f, 18.0f, 0.0f);
    glEnd();
    
    // Blade edge highlight
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex3f(0.0f, -2.0f, 0.01f);
        glVertex3f(0.0f, 18.0f, 0.01f);
    glEnd();
    
    // Handle wrap details
    glColor3f(0.3f, 0.2f, 0.1f);
    for (int i = -7; i <= -3; i++) {
        float y = i * 1.0f;
        glBegin(GL_LINES);
            glVertex3f(-0.8f, y, 0.01f);
            glVertex3f( 0.8f, y, 0.01f);
        glEnd();
    }
    
    // Pommel
    glColor3f(0.9f, 0.8f, 0.3f);
    glBegin(GL_QUADS);
        glVertex3f(-1.2f, -9.0f, 0.0f);
        glVertex3f( 1.2f, -9.0f, 0.0f);
        glVertex3f( 1.2f, -8.0f, 0.0f);
        glVertex3f(-1.2f, -8.0f, 0.0f);
    glEnd();
    
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void GameWidget::drawCone() {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);

    glRotatef(-90, 1, 0, 0); // Align cone along Z axis

    float baseRadius = 0.6f;   // Wider base
    float height = 2.0f;       // Taller cone

    gluCylinder(quad, baseRadius, 0.0f, height, 16, 1); // Cone shape


    // Optional: add a base disk to close the bottom
    gluDisk(quad, 0.0f, baseRadius, 16, 1);

    gluDeleteQuadric(quad);
}



void GameWidget::drawCylinder() {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);

    glRotatef(90, 0.0f, 1.0f, 0.0f); // align with X axis

    float radius = 0.2f;  // Reduced from 0.5f
    float length = 1.0f;  // Reduced from 2.0f

    gluCylinder(quad, radius, radius, length, 16, 1);

    // Draw caps
    gluDisk(quad, 0.0f, radius, 16, 1);
    glTranslatef(0.0f, 0.0f, length);
    gluDisk(quad, 0.0f, radius, 16, 1);

    gluDeleteQuadric(quad);
}



void GameWidget::drawCube() {
    float s = 0.5f;  // ↗️ demi-longueur du côté, donc cube de taille 2x2x2

    glBegin(GL_QUADS);
    // Front
    glVertex3f(-s, -s, s); glVertex3f(s, -s, s); glVertex3f(s, s, s); glVertex3f(-s, s, s);
    // Back
    glVertex3f(-s, -s, -s); glVertex3f(-s, s, -s); glVertex3f(s, s, -s); glVertex3f(s, -s, -s);
    // Left
    glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(-s, s, s); glVertex3f(-s, s, -s);
    // Right
    glVertex3f(s, -s, -s); glVertex3f(s, s, -s); glVertex3f(s, s, s); glVertex3f(s, -s, s);
    // Top
    glVertex3f(-s, s, -s); glVertex3f(-s, s, s); glVertex3f(s, s, s); glVertex3f(s, s, -s);
    // Bottom
    glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s); glVertex3f(s, -s, s); glVertex3f(-s, -s, s);
    glEnd();
}


void GameWidget::drawPyramid() {
    float h = 1.6f;   // ↗️ hauteur
    float s = 1.0f;   // ↗️ demi-longueur des côtés de la base

    glBegin(GL_TRIANGLES);
    // Face avant
    glVertex3f(0.0f, h, 0.0f); glVertex3f(-s, 0.0f, s); glVertex3f(s, 0.0f, s);
    // Face droite
    glVertex3f(0.0f, h, 0.0f); glVertex3f(s, 0.0f, s); glVertex3f(s, 0.0f, -s);
    // Face arrière
    glVertex3f(0.0f, h, 0.0f); glVertex3f(s, 0.0f, -s); glVertex3f(-s, 0.0f, -s);
    // Face gauche
    glVertex3f(0.0f, h, 0.0f); glVertex3f(-s, 0.0f, -s); glVertex3f(-s, 0.0f, s);
    glEnd();

    // Base carrée
    glBegin(GL_QUADS);
    glVertex3f(-s, 0.0f, -s); glVertex3f(s, 0.0f, -s);
    glVertex3f(s, 0.0f, s); glVertex3f(-s, 0.0f, s);
    glEnd();
}




// Update the drawProjectiles method to ensure they appear within view
void GameWidget::drawProjectiles()
{
    // Enable lighting for 3D projectiles
    glEnable(GL_LIGHTING);
    
    for (const ProjectileRenderData& proj : m_projectiles) {
        if (!proj.active) continue;
        
        // Calculate current position based on physics
        QVector3D currentPos = calculateProjectilePosition(proj, m_elapsedTime - proj.spawnTime);
        
        glPushMatrix();
        
        // Position the projectile in world space
        glTranslatef(currentPos.x(), currentPos.y(), currentPos.z());
        
        // Make projectiles brighter and more visible
        GLfloat material_diffuse[] = {0.8f, 0.8f, 0.0f, 1.0f}; // Bright yellow
        glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
        
        // Active vs split states
        if (proj.state == ProjectileRenderData::ACTIVE) {
            glColor3f(0.0f, 0.8f, 0.2f); // Greenish

            switch (proj.type) {
            case ProjectileRenderData::CONE:
                drawCone(); break;

            case ProjectileRenderData::CYLINDER:
                drawCylinder(); break;

            case ProjectileRenderData::CUBE:
                drawCube(); break;

            case ProjectileRenderData::PYRAMID:
                drawPyramid(); break;
            }
        }


        else if (proj.state == ProjectileRenderData::SPLIT) {
            float splitTime = m_elapsedTime - proj.spawnTime - 0.1f;

            // 🔸 POP effect — yellow points around impact
            glDisable(GL_LIGHTING);
            glColor3f(1.0f, 1.0f, 0.0f);
            glPointSize(5.0f);
            glBegin(GL_POINTS);
            for (int i = 0; i < 20; i++) {
                float angle = i * 18.0f;
                float radius = 0.2f + splitTime * 0.7f;
                float x = radius * cos(angle);
                float y = radius * sin(angle);
                glVertex3f(x, y, 0);
            }
            glEnd();

            // 🔸 Then draw the two halves depending on projectile type
            switch (proj.type) {
            case ProjectileRenderData::CYLINDER: {
                glColor3f(0.8f, 0.2f, 0.2f); // Reddish for split parts

                // First half
                glPushMatrix();
                glTranslatef(-0.3f - splitTime * 0.7f, -splitTime * 0.9f, 0);
                glRotatef(splitTime * 180.0f, 0, 1, 0);
                drawCylinder();
                glPopMatrix();

                // Second half
                glPushMatrix();
                glTranslatef(0.3f + splitTime * 0.7f, -splitTime * 0.9f, 0);
                glRotatef(-splitTime * 180.0f, 0, 1, 0);
                drawCylinder();
                glPopMatrix();
                break;
            }
            case ProjectileRenderData::CONE: {
                glColor3f(0.9f, 0.5f, 0.2f); // Orangey slice
                float baseOffset = 0.4f; // how much they slide apart
                float fallOffset = splitTime * 0.8f;

                // First half (left or front)
                glPushMatrix();
                glTranslatef(-baseOffset - splitTime * 0.5f, -fallOffset, 0);
                glRotatef(splitTime * 160.0f, 0, 1, 0); // rotate along Y
                drawCone();
                glPopMatrix();

                // Second half (right or back)
                glPushMatrix();
                glTranslatef(baseOffset + splitTime * 0.5f, -fallOffset, 0);
                glRotatef(-splitTime * 160.0f, 0, 1, 0);
                drawCone();
                glPopMatrix();
                break;
            }
            case ProjectileRenderData::CUBE: {
                glColor3f(0.2f, 0.6f, 1.0f); // Blue-ish cube slice
                float baseOffset = 0.6f;
                float fall = splitTime * 0.7f;
                float rot = splitTime * 180.0f;

                // Left half
                glPushMatrix();
                glTranslatef(-baseOffset - splitTime * 0.5f, -fall, 0);
                glRotatef(rot, 0, 1, 0);
                drawCube();
                glPopMatrix();

                // Right half
                glPushMatrix();
                glTranslatef(baseOffset + splitTime * 0.5f, -fall, 0);
                glRotatef(-rot, 0, 1, 0);
                drawCube();
                glPopMatrix();
                break;
            }
            case ProjectileRenderData::PYRAMID: {
                glColor3f(0.9f, 0.7f, 0.1f); // Yellowish

                float offset = 0.6f + splitTime * 0.5f;
                float fall = splitTime * 0.8f;

                // Left half
                glPushMatrix();
                glTranslatef(-offset, -fall, 0.0f);
                glRotatef(splitTime * 160.0f, 0, 0, 1);
                glScalef(0.5f, 1.0f, 1.0f); // Compress X
                drawPyramid();
                glPopMatrix();

                // Right half
                glPushMatrix();
                glTranslatef(offset, -fall, 0.0f);
                glRotatef(-splitTime * 160.0f, 0, 0, 1);
                glScalef(0.5f, 1.0f, 1.0f); // Compress X
                drawPyramid();
                glPopMatrix();
                break;
            }


            default:
                // ⏩ Old default split for sphere
                glColor3f(0.0f, 0.9f, 0.2f);

                glPushMatrix();
                glTranslatef(-0.3f - splitTime * 0.7f, -splitTime * 1.0f, 0);
                glRotatef(splitTime * 240.0f, 0, 0, 1);
                GLUquadric* quad1 = gluNewQuadric();
                gluQuadricNormals(quad1, GLU_SMOOTH);
                gluSphere(quad1, 0.4f, 16, 8);
                gluDeleteQuadric(quad1);
                glPopMatrix();

                glPushMatrix();
                glTranslatef(0.3f + splitTime * 0.7f, -splitTime * 1.0f, 0);
                glRotatef(-splitTime * 240.0f, 0, 0, 1);
                GLUquadric* quad2 = gluNewQuadric();
                gluQuadricNormals(quad2, GLU_SMOOTH);
                gluSphere(quad2, 0.4f, 16, 8);
                gluDeleteQuadric(quad2);
                glPopMatrix();
                break;
            }

            glEnable(GL_LIGHTING); // reactivate lighting after split drawing
        }

        
        glPopMatrix();
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
    if (!m_program->link()) {
        qDebug() << "Failed to link shader program:" << m_program->log();
        delete m_program;
        m_program = nullptr;
        return;
    }
}

void GameWidget::createGeometry()
{

}

void GameWidget::updateProjectilePositions()
{
    QMutableListIterator<ProjectileRenderData> i(m_projectiles);
    while (i.hasNext()) {
        ProjectileRenderData &proj = i.next();
        
        float timeActive = m_elapsedTime - proj.spawnTime;
        // Avoid unused variable warning
        Q_UNUSED(calculateProjectilePosition(proj, timeActive));
        

    }
}

QVector3D GameWidget::calculateProjectilePosition(const ProjectileRenderData& proj, float time)
{
    // Calculate position based on physics formula:
    // position = initialPosition + velocity*time + 0.5*acceleration*time^2
    const QVector3D gravity(0.0f, -9.8f, 0.0f);
    return proj.position + proj.velocity * time + 0.5f * gravity * time * time;
}

// Add this helper method to ensure projectiles will reach the hit zone
void GameWidget::configureProjectileTrajectory(ProjectileRenderData& projectile)
{
    // Calculate if projectile will reach hit zone (z between 0 and 5)
    // Using projectile motion equations
    bool willHitZone = false;
    
    
    // Time to reach back of hit zone (z = 0)
    if (projectile.velocity.z() < 0) {  // Only if moving toward screen
        float timeToBackOfZone = (0.0f - projectile.position.z()) / projectile.velocity.z();
        // Calculate front of zone time but only use if needed
        float timeToFrontOfZone = (5.0f - projectile.position.z()) / projectile.velocity.z();
        
        // Projectile passes through hit zone if timeToBackOfZone > 0
        if (timeToBackOfZone > 0.0f && timeToFrontOfZone > timeToBackOfZone) {
            willHitZone = true;
        }
    }
    
    // If projectile won't reach hit zone, adjust its trajectory
    if (!willHitZone) {
        // Set a target position in the hit zone
        float targetZ = 3.0f;  // Middle of hit zone
        float targetX = QRandomGenerator::global()->bounded(6) - 3;  // Random x between -3 and 3
        float targetY = 1.0f + (QRandomGenerator::global()->bounded(3));  // Random height between 1 and 4
        
        // Calculate time to reach target (based on z-velocity)
        const float desiredTime = 2.0f;  // 2 seconds to reach target
        
        // Calculate required velocity
        projectile.velocity.setZ((targetZ - projectile.position.z()) / desiredTime);
        projectile.velocity.setX((targetX - projectile.position.x()) / desiredTime);
        
        // Account for gravity when setting y velocity: v_y = (y - y₀)/t + 0.5*g*t
        projectile.velocity.setY((targetY - projectile.position.y()) / desiredTime + 
                                 0.5f * 9.8f * desiredTime);
    }
}
float distanceBetweenSegments(
    const QVector3D& p1, const QVector3D& q1,
    const QVector3D& p2, const QVector3D& q2);

// Add this method to detect collisions between sword and projectiles in the hit zone
void GameWidget::checkHitZoneCollisions()
{
    // Get sword endpoints
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);

    // Adjusted hit cylinder zone
    const float cylinderRadius = 6.0f;
    const float cylinderHeight = 9.0f;
    const QVector3D cylinderCenter(0.0f, 1.5f, 3.0f);  // Align with sword plane
    

    QVector3D swordVector = tipPos - handlePos;
    float swordLength = swordVector.length();

    // Ensure sword length is within a valid range
    if (swordLength < 1.0f || swordLength > 10.0f) {
        qDebug() << "Sword length out of bounds, adjusting...";
        swordLength = std::clamp(swordLength, 1.0f, 10.0f);
        tipPos = handlePos + swordVector.normalized() * swordLength;
    }

    QVector3D swordDir = swordVector.normalized();

    int index = 0;
    QMutableListIterator<ProjectileRenderData> i(m_projectiles);

    while (i.hasNext()) {
        ProjectileRenderData& proj = i.next();

        if (proj.state != ProjectileRenderData::ACTIVE) {
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

        if (!(inRadius && inHeight)) {
            ++index;
            continue;
        }

        // Refine collision detection for projectiles
        float projectileRadius = getProjectileCollisionRadius(proj.type);

        if (proj.type == ProjectileRenderData::CYLINDER) {
            QVector3D cylCenter = pos;
            QVector3D cylHalfVec = QVector3D(1.0f, 0.0f, 0.0f) * (2.0f * 0.5f);
            QVector3D cylStart = cylCenter - cylHalfVec;
            QVector3D cylEnd = cylCenter + cylHalfVec;

            float dist = distanceBetweenSegments(handlePos, tipPos, cylStart, cylEnd);

            if (dist <= 0.5f) {
                splitProjectile(index);
                emit scoreChanged(10);
                qDebug() << "Cylinder HIT at index" << index;
            }
        } else if (proj.type == ProjectileRenderData::CONE) {
            QVector3D coneStart = pos;
            QVector3D coneEnd = pos + QVector3D(0.0f, 0.0f, 2.0f);

            float dist = distanceBetweenSegments(handlePos, tipPos, coneStart, coneEnd);

            if (dist <= 0.6f) {
                splitProjectile(index);
                emit scoreChanged(10);
                qDebug() << "Cone HIT at index" << index;
            }
        } else if (proj.type == ProjectileRenderData::PYRAMID) {
            QVector3D baseCenter = pos;
            QVector3D tip = pos + QVector3D(0.0f, 1.6f, 0.0f);

            float dist = distanceBetweenSegments(handlePos, tipPos, baseCenter, tip);

            if (dist <= 1.0f) {
                splitProjectile(index);
                emit scoreChanged(10);
                qDebug() << "Pyramid HIT at index" << index;
            }
        } else {
            QVector3D toProj = pos - handlePos;
            float projLen = QVector3D::dotProduct(toProj, swordDir);
            projLen = std::clamp(projLen, 0.0f, swordLength);
            QVector3D closest = handlePos + swordDir * projLen;

            float distToSword = (closest - pos).length();

            if (distToSword <= projectileRadius) {
                splitProjectile(index);
                emit scoreChanged(10);
                qDebug() << "Default HIT at index" << index;
            }
        }

        ++index;
    }
}



float GameWidget::getProjectileCollisionRadius(ProjectileRenderData::Type type) const {
    switch (type) {
    case ProjectileRenderData::CYLINDER: return 1.0f;  // length is 2
    case ProjectileRenderData::CONE:     return 0.7f;  // height = 2, base = 0.6
    case ProjectileRenderData::CUBE:     return 1.3f;  // side = 2
    case ProjectileRenderData::PYRAMID:  return 1.2f;  // base 2, height 1.6
    default:                             return 0.6f;
    }
}

float distanceBetweenSegments(
    const QVector3D& p1, const QVector3D& q1,
    const QVector3D& p2, const QVector3D& q2)
{
    QVector3D d1 = q1 - p1;
    QVector3D d2 = q2 - p2;
    QVector3D r = p1 - p2;

    float a = QVector3D::dotProduct(d1, d1);
    float e = QVector3D::dotProduct(d2, d2);
    float f = QVector3D::dotProduct(d2, r);

    float s, t;

    if (a <= 1e-6f && e <= 1e-6f) {
        return (p1 - p2).length();
    }
    if (a <= 1e-6f) {
        s = 0.0f;
        t = std::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = QVector3D::dotProduct(d1, r);
        if (e <= 1e-6f) {
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = QVector3D::dotProduct(d1, d2);
            float denom = a * e - b * b;
            if (denom != 0.0f) {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            t = (b * s + f) / e;
            t = std::clamp(t, 0.0f, 1.0f);
        }
    }

    QVector3D c1 = p1 + d1 * s;
    QVector3D c2 = p2 + d2 * t;

    return (c1 - c2).length();
}
