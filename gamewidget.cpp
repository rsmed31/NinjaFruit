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
    // Invert Y again if needed by subtracting (the mapping in MainWindow now produces gameY accordingly)
    // Here we assume gameY is already correct; if further inversion is needed, use: -y
    m_handPosition = QVector3D(x, y, 10.0f);  // fixed Z value for 3D placement
    update();
}

void GameWidget::initializeGL()
{
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
        0.0, 1.0, 0.0       // Up vector
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

// Replace drawHandRange with a proper 3D cylindrical hit zone
void GameWidget::drawHitCylinder()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Position the hit cylinder directly in front of the camera
    // This is the region where sword slicing is detected
    glPushMatrix();
    
    // Move the cylinder to a position that makes sense for gameplay
    // Center at player's position, extending forward
    glTranslatef(0.0f, 1.0f, 3.0f);
    
    // Parameters for the hit cylinder
    const float cylinderRadius = 4.0f;
    const float cylinderHeight = 3.0f;
    const int cylinderSegments = 20;
    
    // Draw a semi-cylindrical hit zone (wireframe with transparency)
    glColor4f(0.3f, 0.8f, 1.0f, 0.25f);  // Light blue, mostly transparent
    
    // Draw the curved surface of the semi-cylinder
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= cylinderSegments; i++) {
        float angle = M_PI * (1.0f - (float)i / cylinderSegments);
        float x = cylinderRadius * cos(angle);
        float z = cylinderRadius * sin(angle);
        
        // Bottom vertex
        glVertex3f(x, 0.0f, z);
        // Top vertex
        glVertex3f(x, cylinderHeight, z);
    }
    glEnd();
    
    // Draw wireframe overlay for better visibility
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor4f(0.5f, 1.0f, 1.0f, 0.8f);  // Brighter blue, more opaque
    glLineWidth(1.5f);
    
    // Draw curved surface wireframe
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= cylinderSegments; i++) {
        float angle = M_PI * (1.0f - (float)i / cylinderSegments);
        float x = cylinderRadius * cos(angle);
        float z = cylinderRadius * sin(angle);
        glVertex3f(x, 0.0f, z);
    }
    glEnd();
    
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= cylinderSegments; i++) {
        float angle = M_PI * (1.0f - (float)i / cylinderSegments);
        float x = cylinderRadius * cos(angle);
        float z = cylinderRadius * sin(angle);
        glVertex3f(x, cylinderHeight, z);
    }
    glEnd();
    
    // Draw vertical lines
    for (int i = 0; i <= cylinderSegments; i += 4) {
        float angle = M_PI * (1.0f - (float)i / cylinderSegments);
        float x = cylinderRadius * cos(angle);
        float z = cylinderRadius * sin(angle);
        
        glBegin(GL_LINES);
        glVertex3f(x, 0.0f, z);
        glVertex3f(x, cylinderHeight, z);
        glEnd();
    }
    
    // Add a "HIT ZONE" label using simple lines
    glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, cylinderHeight / 2.0f, cylinderRadius - 0.1f);
    glScalef(0.5f, 0.5f, 0.5f);
    // Draw a simple marker
    glBegin(GL_LINES);
    glVertex3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glEnd();
    glPopMatrix();
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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
    handlePos = QVector3D(handX, handY + 0.5f, 3.0f);
    
    // Rotation angles from drawVirtualHand (should match exactly)
    float rotZ = 15.0f * M_PI / 180.0f;  // 15° in radians
    float rotY = -20.0f * M_PI / 180.0f; // -20° in radians
    
    // Sword length in world units (scaled from model units)
    float swordScale = 0.045f;  // Match the scale in drawVirtualHand
    float swordLength = 18.0f * swordScale; // blade length * scale factor
    
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
    float handX = m_handPosition.x() * 0.25f;  // Scale factor increased for better movement
    float handY = m_handPosition.y() * 0.25f;
    
    // Position sword closer to camera for better first-person feel
    // Move z value closer to camera (from 0.75f to 3.0f)
    glTranslatef(
        handX,           
        handY + 0.5f,    // Lower position to see more of the sword
        3.0f             // Much closer to camera for first-person feel
    );
    
    // Calculate and emit sword endpoints for collision detection
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);
    
    // Adjust rotation for more natural sword orientation
    glRotatef(15.0f, 0.0f, 0.0f, 1.0f);  // Less tilt on Z axis
    glRotatef(-20.0f, 0.0f, 1.0f, 0.0f); // Less rotation on Y axis
    
    // Increase scale for bigger sword appearance
    float swordScale = 0.045f;  // Increased from 0.035f
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
            // Draw a more visible sphere for active projectiles
            glColor3f(0.0f, 0.8f, 0.2f); // Bright green
            
            GLUquadric* quad = gluNewQuadric();
            gluQuadricNormals(quad, GLU_SMOOTH);
            gluSphere(quad, 0.5f, 16, 16);  // Larger radius (0.5)
            gluDeleteQuadric(quad);
            
            // Draw an outline to improve visibility
            glDisable(GL_LIGHTING);
            glColor3f(1.0f, 1.0f, 1.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            
            GLUquadric* outlineQuad = gluNewQuadric();
            gluQuadricDrawStyle(outlineQuad, GLU_LINE);
            gluSphere(outlineQuad, 0.52f, 8, 8);
            gluDeleteQuadric(outlineQuad);
            
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_LIGHTING);
        }
        else if (proj.state == ProjectileRenderData::SPLIT) {
            // Split projectile visualization (keeping the existing code)
            float splitTime = m_elapsedTime - proj.spawnTime - 0.1f;
            
            glDisable(GL_LIGHTING);
            glColor3f(1.0f, 1.0f, 0.0f);
            
            glPointSize(5.0f);  // Larger points
            glBegin(GL_POINTS);
            for (int i = 0; i < 20; i++) {
                float angle = i * 18.0f;
                float radius = 0.2f + splitTime * 0.7f;
                float x = radius * cos(angle);
                float y = radius * sin(angle);
                glVertex3f(x, y, 0);
            }
            glEnd();
            
            // Draw two halves
            glColor3f(0.0f, 0.9f, 0.2f);
            
            // First half - adjust positioning for better visibility
            glPushMatrix();
            glTranslatef(-0.3f - splitTime * 0.7f, -splitTime * 1.0f, 0);
            glRotatef(splitTime * 240.0f, 0, 0, 1);
            
            GLUquadric* quad1 = gluNewQuadric();
            gluQuadricNormals(quad1, GLU_SMOOTH);
            gluSphere(quad1, 0.4f, 16, 8);
            gluDeleteQuadric(quad1);
            glPopMatrix();
            
            // Second half
            glPushMatrix();
            glTranslatef(0.3f + splitTime * 0.7f, -splitTime * 1.0f, 0);
            glRotatef(-splitTime * 240.0f, 0, 0, 1);
            
            GLUquadric* quad2 = gluNewQuadric();
            gluQuadricNormals(quad2, GLU_SMOOTH);
            gluSphere(quad2, 0.4f, 16, 8);
            gluDeleteQuadric(quad2);
            glPopMatrix();
            
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
    // For this minimal example, we're using immediate mode (glBegin/glEnd)
    // rather than creating proper vertex buffers
    // In a real application, you should use modern OpenGL with VBOs and VAOs
    
    // Example of setting up vertex arrays
    // m_vao.create();
    // m_vao.bind();
    //
    // m_vertexBuffer.create();
    // m_vertexBuffer.bind();
    // m_vertexBuffer.allocate(cylinderVertices, sizeof(cylinderVertices));
    //
    // m_indexBuffer.create();
    // m_indexBuffer.bind();
    // m_indexBuffer.allocate(cylinderIndices, sizeof(cylinderIndices));
    //
    // m_program->enableAttributeArray("position");
    // m_program->setAttributeBuffer("position", GL_FLOAT, 0, 3, sizeof(GLfloat) * 8);
    //
    // m_vao.release();
}

void GameWidget::updateProjectilePositions()
{
    // Update all projectiles, removing inactive ones
    QMutableListIterator<ProjectileRenderData> i(m_projectiles);
    while (i.hasNext()) {
        ProjectileRenderData &proj = i.next();
        
        // Check if projectile has been active for too long or hit the ground
        float timeActive = m_elapsedTime - proj.spawnTime;
        QVector3D currentPos = calculateProjectilePosition(proj, timeActive);
        
        if (timeActive > 10.0f || currentPos.y() < 0.0f) {
            i.remove();
        }
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
    float timeToHitZone = 0.0f;
    bool willHitZone = false;
    
    // Solve for time when z coordinate will be in hit zone range
    // z = z₀ + v₀t + 0.5at²
    // For z axis, we have: z = z₀ + v_z*t (no gravity in z direction)
    
    // Time to reach back of hit zone (z = 0)
    if (projectile.velocity.z() < 0) {  // Only if moving toward screen
        float timeToBackOfZone = (0.0f - projectile.position.z()) / projectile.velocity.z();
        float timeToFrontOfZone = (5.0f - projectile.position.z()) / projectile.velocity.z();
        
        // Projectile passes through hit zone if timeToBackOfZone > 0
        if (timeToBackOfZone > 0.0f) {
            // Choose the time in the middle of the zone
            timeToHitZone = (timeToBackOfZone + timeToFrontOfZone) / 2.0f;
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

// Add this method to detect collisions between sword and projectiles in the hit zone
void GameWidget::checkHitZoneCollisions()
{
    // Get current sword position
    QVector3D handlePos, tipPos;
    getSwordEndpoints(handlePos, tipPos);
    
    // Define the hit cylinder parameters (match drawHitCylinder)
    const float cylinderRadius = 4.0f;
    const float cylinderHeight = 3.0f;
    const QVector3D cylinderCenter(0.0f, 1.0f, 3.0f); // Match the translation in drawHitCylinder
    
    // Check each projectile
    QMutableListIterator<ProjectileRenderData> i(m_projectiles);
    int index = 0;
    
    while (i.hasNext()) {
        ProjectileRenderData &proj = i.next();
        
        if (proj.state != ProjectileRenderData::ACTIVE) {
            index++;
            continue;
        }
        
        // Calculate current projectile position
        float timeActive = m_elapsedTime - proj.spawnTime;
        QVector3D projPos = calculateProjectilePosition(proj, timeActive);
        
        // 1. First check if projectile is in hit cylinder (semi-cylindrical zone)
        // Distance from cylinder center axis (x and z only)
        float dx = projPos.x() - cylinderCenter.x();
        float dz = projPos.z() - cylinderCenter.z();
        float distanceFromAxis = sqrt(dx*dx + dz*dz);
        
        // Check if projectile is within cylinder radius and height and in front half
        bool inCylinderRadius = distanceFromAxis <= cylinderRadius;
        bool inCylinderHeight = projPos.y() >= cylinderCenter.y() && 
                               projPos.y() <= cylinderCenter.y() + cylinderHeight;
        bool inFrontHalf = projPos.z() >= cylinderCenter.z();
        
        // If within hit cylinder, check for sword collision
        if (inCylinderRadius && inCylinderHeight && inFrontHalf) {
            // 2. Check if sword intersects with projectile
            // Simplified sword collision using line-sphere intersection
            // Treat projectile as a sphere
            const float projectileRadius = 0.5f;
            
            // Calculate closest point on sword line segment to projectile center
            QVector3D swordVector = tipPos - handlePos;
            float swordLength = swordVector.length();
            QVector3D swordDirection = swordVector / swordLength;
            
            // Vector from handle to projectile
            QVector3D handleToProj = projPos - handlePos;
            
            // Project handleToProj onto swordDirection
            float projectionLength = QVector3D::dotProduct(handleToProj, swordDirection);
            
            // Clamp projection to sword segment
            projectionLength = qMax(0.0f, qMin(projectionLength, swordLength));
            
            // Closest point on sword line to projectile
            QVector3D closestPointOnSword = handlePos + swordDirection * projectionLength;
            
            // Distance from closest point to projectile center
            float distance = (closestPointOnSword - projPos).length();
            
            // If distance is less than projectile radius, we have a hit
            if (distance <= projectileRadius) {
                // Split the projectile
                splitProjectile(index);
            }
        }
        
        index++;
    }
}
