#include "scene_renderer.h"
#include <QtMath>
#include <QDebug>
#include <GL/glu.h>

SceneRenderer::SceneRenderer(float floorSize, float cameraDistance, float cameraHeight)
    : m_floorSize(floorSize), m_cameraDistance(cameraDistance), m_cameraHeight(cameraHeight)
{
}

void SceneRenderer::drawDistanceIndicators()
{
    // Disable lighting for the ground plane
    glDisable(GL_LIGHTING);

    // Draw a ground plane with fading colors to indicate distance
    glBegin(GL_QUADS);
    // Near zone - red (danger zone)
    glColor4f(0.7f, 0.0f, 0.0f, 0.3f);
    glVertex3f(-m_floorSize, 0.0f, 10.0f);
    glVertex3f(m_floorSize, 0.0f, 10.0f);
    // Middle zone - yellow (warning zone)
    glColor4f(0.7f, 0.7f, 0.0f, 0.3f);
    glVertex3f(m_floorSize, 0.0f, 0.0f);
    glVertex3f(-m_floorSize, 0.0f, 0.0f);
    glEnd();

    // Middle to far zone - green and blue gradient
    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.7f, 0.0f, 0.3f);
    glVertex3f(-m_floorSize, 0.0f, 0.0f);
    glVertex3f(m_floorSize, 0.0f, 0.0f);
    glColor4f(0.0f, 0.0f, 0.7f, 0.3f);
    glVertex3f(m_floorSize, 0.0f, -m_floorSize);
    glVertex3f(-m_floorSize, 0.0f, -m_floorSize);
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
}

void SceneRenderer::drawHitCylinder()
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();

    const float radius = 6.0f;
    const float z = 2.0f;
    const float height = 10.0f;
    const float yCenter = height / 2.0f;
    const int segments = 64;
    const int rings = 14;

    glTranslatef(0.0f, yCenter, z);
    glColor4f(0.5f, 1.0f, 1.0f, 0.4f);
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
    {
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

void SceneRenderer::setupLights()
{
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
    GLfloat light1_pos[] = {-5.0f, 5.0f, 5.0f, 1.0f};
    GLfloat light1_diffuse[] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat light1_specular[] = {0.2f, 0.2f, 0.2f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);

    // Enable third light source
    glEnable(GL_LIGHT2);
    GLfloat light2_pos[] = {3.0f, -2.0f, 2.0f, 1.0f};
    GLfloat light2_diffuse[] = {0.2f, 0.2f, 0.5f, 1.0f};
    GLfloat light2_specular[] = {0.1f, 0.1f, 0.3f, 1.0f};
    glLightfv(GL_LIGHT2, GL_POSITION, light2_pos);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
}

void SceneRenderer::setupCamera(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(95.0, width / static_cast<float>(height), 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        0.0, 1.8, 6.0,
        0.0, 1.0, -30.0,
        0.0, 1.0, 10.0);
}

void SceneRenderer::drawArenaWalls(GLuint wallTexture, GLuint archTexture, GLuint portalTexture)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    // Consistent constants for geometry - ensure perfect symmetry by using exact same value
    const float wallX = 7.5f;
    const float wallHeight = 10.0f;
    const float archRadius = wallX;
    const float archBaseY = wallHeight;
    const float wallZMin = -30.0f;
    const float wallZMax = 10.0f;
    const float portalZ = -25.0f;

    // Important: Set material properties for texturing to work correctly
    GLfloat matDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLfloat matAmbient[] = {0.5f, 0.5f, 0.5f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);

    // Left wall
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBegin(GL_QUADS);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-wallX, 0.0f, wallZMin);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-wallX, wallHeight, wallZMin);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-wallX, wallHeight, wallZMax);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-wallX, 0.0f, wallZMax);
    glEnd();

    // Right wall - use wall texture for consistent theme
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE); // Prevent right wall being culled

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);
    glDisable(GL_LIGHTING);          
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glNormal3f(-1.0f, 0.0f, 0.0f); // Inward facing
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(+wallX, 0.0f, wallZMin); // bottom-left
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(+wallX, wallHeight, wallZMin); // top-left
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(+wallX, wallHeight, wallZMax); // top-right
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(+wallX, 0.0f, wallZMax); // bottom-right
    glEnd();

    glEnable(GL_CULL_FACE); // Restore
    glEnable(GL_LIGHTING);
    
    // Draw top arch
    glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, wallTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // Changed from GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Changed from GL_CLAMP_TO_EDGE
    glColor4f(1.0f, 1.0f, 1.0f, 0.6f);

    const int archSegments = 20;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= archSegments; i++)
    {
        float angle = M_PI * i / archSegments;
        float x = archRadius * cos(angle);
        float y = archBaseY + archRadius * sin(angle);

        glNormal3f(cos(angle), sin(angle), 0.0f);
        glTexCoord2f(i / (float)archSegments, 0.0f);
        glVertex3f(x, y, wallZMin);
        glTexCoord2f(i / (float)archSegments, 1.0f);
        glVertex3f(x, y, wallZMax);
    }
    glEnd();

    glEnable(GL_LIGHTING);

    // Portal drawing - completely rewritten with proper texture setup
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    
    // Remove the unsupported glActiveTexture call
    // GL_TEXTURE0 and glActiveTexture require OpenGL extensions
    
    // 1. Explicitly enable texturing
    glEnable(GL_TEXTURE_2D);
    
    // 2. Set texture environment mode to modulate with color
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    
    // 3. Set bright white color for full intensity
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    // 4. Bind the portal texture
    glBindTexture(GL_TEXTURE_2D, portalTexture);
    
    // 5. Set texture parameters explicitly
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // 6. Get and verify current texture binding
    GLint currentTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexture);
    
    // Portal dimensions
    const float portalWidth = 6.0f;
    const float portalHeight = 10.0f;
    const float portalY = 1.0f;
    
    // Draw portal quad with clockwise winding (facing the camera)
    glBegin(GL_QUADS);
      // Bottom-left
      glTexCoord2f(0.0f, 0.0f);
      glVertex3f(-portalWidth/2, portalY, portalZ);
      
      // Top-left
      glTexCoord2f(0.0f, 1.0f);
      glVertex3f(-portalWidth/2, portalY + portalHeight, portalZ);
      
      // Top-right
      glTexCoord2f(1.0f, 1.0f);
      glVertex3f(portalWidth/2, portalY + portalHeight, portalZ);
      
      // Bottom-right
      glTexCoord2f(1.0f, 0.0f);
      glVertex3f(portalWidth/2, portalY, portalZ);
    glEnd();
    
    // Restore rendering states
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
}
