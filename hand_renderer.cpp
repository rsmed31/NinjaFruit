#include "hand_renderer.h"
#include <QtMath>
#include <QDebug>

HandRenderer::HandRenderer(QVector3D& handPosition, GLuint* textures) 
    : m_handPosition(handPosition), m_textures(textures) {
}

void HandRenderer::setHandPosition(const QVector3D& position) {
    m_handPosition = position;
}

void HandRenderer::drawVirtualHand() {
    glPushMatrix();

    // Map hand position with more responsive tracking
    float angle = m_handPosition.x() * (M_PI / 2.0f); // from -90° to +90°
    float radius = 6.0f;                              // ✅ match cylinder radius
    float x = m_handPosition.x();                     // Already normalized in [-7.5, 7.5]
    float z = -3.0f;                                  // Bring sword up onto the cylinder mesh
    float y = m_handPosition.y() * 0.6f;              // 🔺 broader vertical sweep

    // Position sword on the grid plane and at the cylinder's z position
    glTranslatef(x, y + 0.8f, z); // Updated to z=2.0f to align with cylinder

    // Adjust rotation for more natural sword orientation
    glRotatef(15.0f, 0.0f, 0.0f, 1.0f);  // Less tilt on Z axis
    glRotatef(-20.0f, 0.0f, 1.0f, 0.0f); // Less rotation on Y axis

    // Scale sword to be visible on grid
    float swordScale = 0.15f; // Larger scale for visibility
    glScalef(swordScale, swordScale, swordScale);

    glDisable(GL_LIGHTING);

    // Draw sword with bright colors for visibility
    // Handle
    glColor3f(0.8f, 0.6f, 0.4f); // Brighter brown
    glBegin(GL_QUADS);
    glVertex3f(-0.8f, -8.0f, 0.0f);
    glVertex3f(0.8f, -8.0f, 0.0f);
    glVertex3f(0.8f, -2.5f, 0.0f);
    glVertex3f(-0.8f, -2.5f, 0.0f);
    glEnd();

    // Crossguard with metallic appearance
    glColor3f(0.9f, 0.8f, 0.3f); // Brighter gold
    glBegin(GL_QUADS);
    glVertex3f(-3.0f, -2.5f, 0.0f);
    glVertex3f(3.0f, -2.5f, 0.0f);
    glVertex3f(3.0f, -1.5f, 0.0f);
    glVertex3f(-3.0f, -1.5f, 0.0f);
    glEnd();

    // Blade with metallic sheen
    glColor3f(0.95f, 0.95f, 1.0f); // Bright silver color
    glBegin(GL_QUADS);
    glVertex3f(-1.0f, -2.0f, 0.0f);
    glVertex3f(1.0f, -2.0f, 0.0f);
    glVertex3f(0.5f, 15.0f, 0.0f);
    glVertex3f(-0.5f, 15.0f, 0.0f);
    glEnd();

    // Blade tip
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.5f, 15.0f, 0.0f);
    glVertex3f(0.5f, 15.0f, 0.0f);
    glVertex3f(0.0f, 18.0f, 0.0f);
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
        glVertex3f(0.8f, y, 0.01f);
        glEnd();
    }

    // Pommel
    glColor3f(0.9f, 0.8f, 0.3f);
    glBegin(GL_QUADS);
    glVertex3f(-1.2f, -9.0f, 0.0f);
    glVertex3f(1.2f, -9.0f, 0.0f);
    glVertex3f(1.2f, -8.0f, 0.0f);
    glVertex3f(-1.2f, -8.0f, 0.0f);
    glEnd();

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void HandRenderer::getSwordEndpoints(QVector3D& handlePos, QVector3D& tipPos) {
    // Start with the hand position
    float handX = m_handPosition.x() * 0.25f; // Match scale in drawVirtualHand
    float handY = m_handPosition.y() * 0.25f;

    // Calculate handle position (base of sword) in world coordinates
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
}
