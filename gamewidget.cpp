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
    // Basic cylinder vertices would go here
    // For brevity, only part of the data is shown
    // In a real implementation, you would generate vertices
    // programmatically with proper normals and texture coordinates
};

static const GLuint cylinderIndices[] = {
    // Triangle indices for the cylinder
    // Would be generated programmatically
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
    GLfloat light1_pos[] = {-5.0f, 5.0f, 5.0f, 1.0f};  // Slightly behind and above
    GLfloat light1_diffuse[] = {0.3f, 0.3f, 0.3f, 1.0f};  // Soft white light
    GLfloat light1_specular[] = {0.2f, 0.2f, 0.2f, 1.0f};

    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
    // Enable third light source
    glEnable(GL_LIGHT2);

    // Define light2: lower side fill light (e.g. under the player, angled up)
    GLfloat light2_pos[] = {3.0f, -2.0f, 2.0f, 1.0f};  // From below right
    GLfloat light2_diffuse[] = {0.2f, 0.2f, 0.5f, 1.0f}; // Cool bluish light
    GLfloat light2_specular[] = {0.1f, 0.1f, 0.3f, 1.0f}; // Subtle specular highlights

    glLightfv(GL_LIGHT2, GL_POSITION, light2_pos);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);



    // Create shaders and geometry
    createShaders();
    createGeometry();
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE); // ✅ Add this
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_NORMALIZE);
    // glDisable(GL_CULL_FACE); // optional for test

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
    // Reduce inertia so sword follows your hand more closely
    // Add more smoothing to reduce vibration (80% old, 20% new)
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
ProjectileRenderData::Type GameWidget::mapProjectileType(Projectile::Type type) {
    switch (type) {
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
void GameWidget::launchProjectile(const Projectile& projectile) {
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

        // Draw distance text if needed
        // This would require more complex text rendering which
        // is omitted for simplicity
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
      glVertex3f(-8.0f, 0.001f, 0.0f);  // Closer to camera
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

void GameWidget::drawHitCylinder() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();

    const float radius   = 6.0f;
    const float z        = 2.0f;          // Slicing plane
    const float height   = 10.0f;         // Full game height
    const float yCenter  = height / 2.0f; // Center at 5.0f
    const int segments   = 64;            // Number of segments for cylinder
    const int rings      = 14;            // Number of rings for height division

    glTranslatef(0.0f, yCenter, z);

    glColor4f(0.5f, 1.0f, 1.0f, 0.4f);  // Bright mesh color
    glLineWidth(1.2f);

    // Horizontal rings
    for (int j = 0; j <= rings; ++j) {
        float y = -height / 2.0f + j * (height / rings);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = i * 2.0f * M_PI / segments;
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
    handlePos = QVector3D(handX, handY + 0.5f, 2.0f);  // Match cylinder plane
    
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
    float z = -3.0f;  // Bring sword up onto the cylinder mesh
    float y = m_handPosition.y() * 0.6f;  // 🔺 broader vertical sweep
    
    // Position sword on the grid plane and at the cylinder's z position
    glTranslatef(x, y + 0.8f, z); // Updated to z=2.0f to align with cylinder
    
    // Calculate and emit sword endpoints for collision detection
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);
    
    // Adjust rotation for more natural sword orientation
    glRotatef(15.0f, 0.0f, 0.0f, 1.0f);  // Less tilt on Z axis
    glRotatef(-20.0f, 0.0f, 1.0f, 0.0f); // Less rotation on Y axis
    
    // Scale sword to be visible on grid
    float swordScale = 0.15f;  // Larger scale for visibility
    glScalef(swordScale, swordScale, swordScale);

    
    glDisable(GL_LIGHTING);
    
    // Draw sword with bright colors for visibility
    // Handle
    glColor3f(0.8f, 0.6f, 0.4f);  // Brighter brown
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
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[3]); // Cone texture

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);

    glPushMatrix();

    // Flip direction so base faces viewer (Z- direction)
    glRotatef(90, 1, 0, 0); // Cone points toward -Z

    float baseRadius = 0.6f;
    float height = 2.0f;

    // Draw cone
    gluCylinder(quad, baseRadius, 0.0f, height, 16, 1);

    // Draw base at z = 0 (bottom after rotation)
    gluDisk(quad, 0.0f, baseRadius, 16, 1);

    glPopMatrix();

    gluDeleteQuadric(quad);
    glDisable(GL_TEXTURE_2D);
}





void GameWidget::drawCylinder() {
    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[2]); // Cylinder (carrot) texture
    
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE); // Enable texture coordinates
    gluQuadricNormals(quad, GLU_SMOOTH);

    glRotatef(90, 0.0f, 1.0f, 0.0f); // align with X axis

    float radius = 0.2f;
    float length = 1.0f;

    gluCylinder(quad, radius, radius, length, 16, 1);

    // Draw caps
    gluDisk(quad, 0.0f, radius, 16, 1);
    glTranslatef(0.0f, 0.0f, length);
    gluDisk(quad, 0.0f, radius, 16, 1);

    gluDeleteQuadric(quad);
    glDisable(GL_TEXTURE_2D);
}



void GameWidget::drawCube() {
    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[0]); // Cube texture

    float s = 0.5f;  // half-length of side

    glBegin(GL_QUADS);

    // Front face (+Z)
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s,  s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( s, -s,  s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( s,  s,  s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s,  s,  s);

    // Back face (-Z)
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( s, -s, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-s,  s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( s,  s, -s);

    // Left face (-X)
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, -s,  s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-s,  s,  s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s,  s, -s);

    // Right face (+X)
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( s, -s,  s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( s, -s, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( s,  s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( s,  s,  s);

    // Top face (+Y)
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s,  s,  s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( s,  s,  s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( s,  s, -s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s,  s, -s);

    // Bottom face (-Y)
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, -s, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( s, -s, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( s, -s,  s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s, -s,  s);

    glEnd();

    glDisable(GL_TEXTURE_2D);
}


void GameWidget::drawPyramid() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[1]);

    float h = 1.6f; // height
    float s = 1.0f; // half-length of base

    glBegin(GL_TRIANGLES);

    // Front face (+Z)
    glNormal3f(0.0f, 0.707f, 0.707f);
    glTexCoord2f(0.5f, 1.0f); glVertex3f(0.0f, h, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, 0.0f, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(s, 0.0f, s);

    // Right face (+X)
    glNormal3f(0.707f, 0.707f, 0.0f);
    glTexCoord2f(0.5f, 1.0f); glVertex3f(0.0f, h, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(s, 0.0f, s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(s, 0.0f, -s);

    // Back face (-Z)
    glNormal3f(0.0f, 0.707f, -0.707f);
    glTexCoord2f(0.5f, 1.0f); glVertex3f(0.0f, h, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(s, 0.0f, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, 0.0f, -s);

    // Left face (-X)
    glNormal3f(-0.707f, 0.707f, 0.0f);
    glTexCoord2f(0.5f, 1.0f); glVertex3f(0.0f, h, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, 0.0f, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-s, 0.0f, s);

    glEnd();

    // Base face (bottom, Y = 0)
    glBegin(GL_QUADS);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-s, 0.0f, -s);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(s, 0.0f, -s);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(s, 0.0f, s);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-s, 0.0f, s);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void GameWidget::drawHalfCone(bool mirror)
{
    const float baseRadius = 0.6f;
    const float height = 2.0f;
    const int slices = 36;
    const float angleStep = M_PI / slices;
    const float slantHeight = sqrt(baseRadius * baseRadius + height * height);

    // Slope direction for normals
    const float nxBase = height / slantHeight;
    const float nzBase = baseRadius / slantHeight;

    glPushMatrix();
    glRotatef(90, 1, 0, 0); // Align along -Z axis

    if (mirror) {
        glScalef(-1.0f, 1.0f, 1.0f);
        glFrontFace(GL_CW); // correct normal winding after mirror
    }

    // Enable lighting-friendly material
    GLfloat ambient[] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat diffuse[] = {0.7f, 0.4f, 0.1f, 1.0f};  // light orange
    GLfloat specular[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat shininess = 30.0f;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

    // 🔵 Curved lateral surface
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= slices; ++i) {
        float angle = i * angleStep;
        float x = baseRadius * cos(angle);
        float y = baseRadius * sin(angle);

        QVector3D normal = QVector3D(x * height, y * height, baseRadius * baseRadius).normalized();
        glNormal3f(normal.x(), normal.y(), normal.z());

        glTexCoord2f(static_cast<float>(i) / slices, 0.0f);
        glVertex3f(x, y, 0.0f);             // base perimeter
        glTexCoord2f(0.5f, 1.0f);
        glVertex3f(0.0f, 0.0f, height);     // tip
    }
    glEnd();

    // 🟢 Base cap (half-disk)
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(0.5f, 0.5f);
    glVertex3f(0.0f, 0.0f, 0.0f); // center
    for (int i = 0; i <= slices; ++i) {
        float angle = i * angleStep;
        float x = baseRadius * cos(angle);
        float y = baseRadius * sin(angle);
        glTexCoord2f((x / baseRadius + 1.0f) / 2.0f, (y / baseRadius + 1.0f) / 2.0f);
        glVertex3f(x, y, 0.0f);
    }
    glEnd();

    // 🔴 Flat sliced face
    glBegin(GL_QUADS);
    glNormal3f(0.0f, mirror ? -1.0f : 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 0.0f, height);   // tip
    glTexCoord2f(1.0f, 0.0f); glVertex3f(baseRadius, 0.0f, 0.0f); // base
    glTexCoord2f(1.0f, 1.0f); glVertex3f(baseRadius, 0.0f, 0.0f); // base
    glTexCoord2f(0.0f, 1.0f); glVertex3f(0.0f, 0.0f, height);   // tip
    glEnd();

    if (mirror) {
        glFrontFace(GL_CCW); // reset after mirroring
    }

    glPopMatrix();
}



// Update the drawProjectiles method to ensure they appear within view
void GameWidget::drawProjectiles()
{
    glEnable(GL_LIGHTING);

    for (const ProjectileRenderData& proj : m_projectiles) {
        if (!proj.active) continue;

        QVector3D currentPos = calculateProjectilePosition(proj, m_elapsedTime - proj.spawnTime);

        glPushMatrix();
        glTranslatef(currentPos.x(), currentPos.y(), currentPos.z());
        // Add self-rotation
        float rotationSpeed = 120.0f; // degrees per second
        float timeSinceSpawn = m_elapsedTime - proj.spawnTime;
        float angle = fmod(timeSinceSpawn * rotationSpeed, 360.0f); // 0–360 wrap

        glRotatef(angle, 0.0f, 1.0f, 0.0f); // Y-axis spin (adjust axis as needed)

        // Set material properties for textured rendering
        GLfloat material_diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        GLfloat material_ambient[4] = {0.2f, 0.2f, 0.2f, 1.0f};
        glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
        glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

        if (proj.state == ProjectileRenderData::ACTIVE) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
            // Do NOT use glColor3f() here for textured objects

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
            glDisable(GL_TEXTURE_2D);
        }
        else if (proj.state == ProjectileRenderData::SPLIT) {
            float splitTime = m_elapsedTime - proj.spawnTime - 0.1f;

            glDisable(GL_LIGHTING);
            glColor3f(1.0f, 1.0f, 0.0f); // Only for POP effect (not textured)
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

            // For split halves, set material and avoid glColor3f() before textured draw
            switch (proj.type) {
            case ProjectileRenderData::CYLINDER: {
                float offset = 0.3f + splitTime * 0.2f;
                float fall = splitTime * 0.2f;
                float rot = splitTime * 60.0f;

                glEnable(GL_LIGHTING);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
                glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(-offset, -fall, 0);
                glRotatef(rot, 0, 1, 0);
                drawCylinder();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(offset, -fall, 0);
                glRotatef(-rot, 0, 1, 0);
                drawCylinder();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
                break;
            }
            case ProjectileRenderData::CONE: {
                float offset = 0.4f + splitTime * 0.15f;
                float fall = splitTime * 0.15f;
                float rot = splitTime * 50.0f;

                glEnable(GL_LIGHTING);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
                glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

                // LEFT HALF
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(-offset, -fall, 0);
                glRotatef(rot, 0, 1, 0);
                drawHalfCone(false); // Left half
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);

                // RIGHT HALF
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(offset, -fall, 0);
                glRotatef(-rot, 0, 1, 0);
                drawHalfCone(true); // Right half (mirrored)
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
                break;
            }



            case ProjectileRenderData::CUBE: {
                float offset = 0.6f + splitTime * 0.15f;
                float fall = splitTime * 0.15f;
                float rot = splitTime * 45.0f;

                glEnable(GL_LIGHTING);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
                glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(-offset, -fall, 0);
                glRotatef(rot, 0, 1, 0);
                drawCube();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(offset, -fall, 0);
                glRotatef(-rot, 0, 1, 0);
                drawCube();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
                break;
            }
            case ProjectileRenderData::PYRAMID: {
                float offset = 0.6f + splitTime * 0.2f;
                float fall = splitTime * 0.2f;
                float rot = splitTime * 50.0f;

                glEnable(GL_LIGHTING);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
                glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(-offset, -fall, 0.0f);
                glRotatef(rot, 0, 0, 1);
                glScalef(0.5f, 1.0f, 1.0f);
                drawPyramid();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);
                glPushMatrix();
                glTranslatef(offset, -fall, 0.0f);
                glRotatef(-rot, 0, 0, 1);
                glScalef(0.5f, 1.0f, 1.0f);
                drawPyramid();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
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

            glEnable(GL_LIGHTING);
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

            // We can use timeToFrontOfZone here if needed
            // For debugging: qDebug() << "Projectile will hit zone between" << timeToBackOfZone << "and" << timeToFrontOfZone;
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
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Cylinder HIT at index" << index;
            }
        } else if (proj.type == ProjectileRenderData::CONE) {
            QVector3D coneStart = pos;
            QVector3D coneEnd = pos + QVector3D(0.0f, 0.0f, 2.0f);

            float dist = distanceBetweenSegments(handlePos, tipPos, coneStart, coneEnd);

            if (dist <= 0.6f) {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Cone HIT at index" << index;
            }
        } else if (proj.type == ProjectileRenderData::PYRAMID) {
            QVector3D baseCenter = pos;
            QVector3D tip = pos + QVector3D(0.0f, 1.6f, 0.0f);

            float dist = distanceBetweenSegments(handlePos, tipPos, baseCenter, tip);

            if (dist <= 1.0f) {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Pyramid HIT at index" << index;
            }
        } else {
            QVector3D toProj = pos - handlePos;
            float projLen = QVector3D::dotProduct(toProj, swordDir);
            projLen = std::clamp(projLen, 0.0f, swordLength);
            QVector3D closest = handlePos + swordDir * projLen;

            float distToSword = (closest - pos).length();

            if (distToSword <= projectileRadius) {
                emit projectileSlicedById(proj.id); // Emit signal with projectile ID
                splitProjectile(index);
                qDebug() << "Default HIT at index" << index;
            }
        }

        ++index;
    }
}

float GameWidget::getProjectileCollisionRadius(ProjectileRenderData::Type type) const {
    switch (type) {
        case ProjectileRenderData::Type::CYLINDER: return 1.0f;  // length is 2
        case ProjectileRenderData::Type::CONE:     return 0.7f;  // height = 2, base = 0.6
        case ProjectileRenderData::Type::PYRAMID:  return 1.2f;  // base 2, height 1.6
        // Add cases for other types
        default: return 0.5f;
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

void GameWidget::loadTextures()
{
    // Generate texture IDs
    glGenTextures(4, m_textures);
    
    // Load cube texture (index 0)
    QImage cubeImg(":/textures/textures/cube.png");
    if (cubeImg.isNull()) {
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
    if (pyramidImg.isNull()) {
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
    if (carrotImg.isNull()) {
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
    if (coneImg.isNull()) {
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
void GameWidget::createRandomProjectile() {
    // Debug output to check type distribution
    int randomType = QRandomGenerator::global()->bounded(4);
    Projectile::Type type;
    
    switch (randomType) {
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
    Projectile projectile(type); // Pass type to constructor instead of using setType
    projectile.setPosition(QVector3D(0.0f, 1.0f, 5.0f)); // Start position    
    launchProjectile(projectile);
}


