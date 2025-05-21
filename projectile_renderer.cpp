#include "projectile_renderer.h"
#include "projectilerenderdata.h"
#include <QtMath>
#include <QOpenGLFunctions>
#include "physicsutils.h"
// Properly include GLU
#include <GL/gl.h>
extern "C" {
#include <GL/glu.h>
}

ProjectileRenderer::ProjectileRenderer(QVector<ProjectileRenderData>& projectiles, 
                                     GLuint* textures, 
                                     float& elapsedTime)
    : m_projectiles(projectiles),
      m_textures(textures),
      m_elapsedTime(elapsedTime)
{
}

void ProjectileRenderer::drawProjectiles()
{
    glEnable(GL_LIGHTING);

    for (const ProjectileRenderData &proj : m_projectiles)
    {
        if (!proj.active)
            continue;

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

        if (proj.state == ProjectileRenderData::ACTIVE)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, m_textures[proj.type]);

            switch (proj.type)
            {
            case ProjectileRenderData::CONE:
                drawCone();
                break;
            case ProjectileRenderData::CYLINDER:
                drawCylinder();
                break;
            case ProjectileRenderData::CUBE:
                drawCube();
                break;
            case ProjectileRenderData::PYRAMID:
                drawPyramid();
                break;
            }
            glDisable(GL_TEXTURE_2D);
        }
        else if (proj.state == ProjectileRenderData::SPLIT)
        {
            float splitTime = m_elapsedTime - proj.spawnTime - 0.1f;

            glDisable(GL_LIGHTING);
            glColor3f(1.0f, 1.0f, 0.0f); // Only for POP effect (not textured)
            glPointSize(5.0f);
            glBegin(GL_POINTS);
            for (int i = 0; i < 20; i++)
            {
                float angle = i * 18.0f;
                float radius = 0.2f + splitTime * 0.7f;
                float x = radius * cos(angle);
                float y = radius * sin(angle);
                glVertex3f(x, y, 0);
            }
            glEnd();

            // For split halves, set material and avoid glColor3f() before textured draw
            switch (proj.type)
            {
            case ProjectileRenderData::CYLINDER:
            {
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
            case ProjectileRenderData::CONE:
            {
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
            case ProjectileRenderData::CUBE:
            {
                float offset = 0.6f + splitTime * 0.15f;
                float fall = splitTime * 0.15f;
                float rot = splitTime * 45.0f;

                glEnable(GL_LIGHTING);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
                glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[0]); // Use index 0 for cube texture
                glPushMatrix();
                glTranslatef(-offset, -fall, 0);
                glRotatef(rot, 0, 1, 0);
                drawCube();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[0]); // Use index 0 for cube texture
                glPushMatrix();
                glTranslatef(offset, -fall, 0);
                glRotatef(-rot, 0, 1, 0);
                drawCube();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
                break;
            }
            case ProjectileRenderData::PYRAMID:
            {
                float offset = 0.6f + splitTime * 0.2f;
                float fall = splitTime * 0.2f;
                float rot = splitTime * 50.0f;

                glEnable(GL_LIGHTING);
                glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
                glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[1]); // Use index 1 for pyramid texture
                glPushMatrix();
                glTranslatef(-offset, -fall, 0.0f);
                glRotatef(rot, 0, 0, 1);
                glScalef(0.5f, 1.0f, 1.0f);
                drawPyramid();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, m_textures[1]); // Use index 1 for pyramid texture
                glPushMatrix();
                glTranslatef(offset, -fall, 0.0f);
                glRotatef(-rot, 0, 0, 1);
                glScalef(0.5f, 1.0f, 1.0f);
                drawPyramid();
                glPopMatrix();
                glDisable(GL_TEXTURE_2D);
                break;
            }
            }

            glEnable(GL_LIGHTING);
        }

        glPopMatrix();
    }
}

void ProjectileRenderer::drawCone()
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[3]); // Cone texture

    GLUquadric *quad = gluNewQuadric();
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

void ProjectileRenderer::drawCylinder()
{
    // Apply material properties for better 3D appearance
    GLfloat matAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat matDiffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat matSpecular[] = {0.5f, 0.5f, 0.5f, 1.0f};
    GLfloat matShininess = 30.0f;
    
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[2]); // Cylinder texture

    GLUquadric *quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricOrientation(quad, GLU_OUTSIDE); // Ensure normals point outward

    glPushMatrix();
    
    // Orient cylinder along X axis (horizontal) instead of Z axis
    glRotatef(90, 0, 1, 0); // Rotate around Y to point along X axis

    float radius = 0.5f;
    float height = 2.0f;

    // Draw cylinder
    gluCylinder(quad, radius, radius, height, 32, 8); // Increased segments for smoother appearance

    // Draw caps with proper normals
    glPushMatrix();
    glRotatef(180, 1, 0, 0); // Flip normal for the base cap
    gluDisk(quad, 0.0f, radius, 32, 4); // Bottom cap
    glPopMatrix();
    
    glTranslatef(0.0f, 0.0f, height); // Move to top
    gluDisk(quad, 0.0f, radius, 32, 4); // Top cap

    glPopMatrix();

    gluDeleteQuadric(quad);
    glDisable(GL_TEXTURE_2D);
}

void ProjectileRenderer::drawCube()
{
    // Apply material properties for better 3D appearance
    GLfloat matAmbient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat matDiffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat matSpecular[] = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat matShininess = 50.0f;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT, GL_SHININESS, matShininess);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[0]); // Use index 0 for cube texture

    // Cube vertices
    const float size = 0.6f; // Slightly larger for better visibility
    const float vertices[][3] = {
        {-size, -size, -size}, {size, -size, -size}, {size, size, -size}, {-size, size, -size},
        {-size, -size, size}, {size, -size, size}, {size, size, size}, {-size, size, size}
    };

    // Normal vectors for each face
    const float normals[][3] = {
        {0.0f, 0.0f, -1.0f}, // Back face (-Z)
        {1.0f, 0.0f, 0.0f},  // Right face (+X)
        {0.0f, 0.0f, 1.0f},  // Front face (+Z)
        {-1.0f, 0.0f, 0.0f}, // Left face (-X)
        {0.0f, 1.0f, 0.0f},  // Top face (+Y)
        {0.0f, -1.0f, 0.0f}  // Bottom face (-Y)
    };

    // Cube faces (vertex indices)
    const int faces[][4] = {
        {0,3,2,1}, // Back
        {1,2,6,5}, // Right
        {5,6,7,4}, // Front
        {4,7,3,0}, // Left
        {3,7,6,2}, // Top
        {0,1,5,4}  // Bottom
    };

    // Texture coordinates
    const float texCoords[][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    // Draw each face with proper normals
    glBegin(GL_QUADS);
    for (int i = 0; i < 6; i++) {
        // Set normal for entire face
        glNormal3fv(normals[i]);
        
        for (int j = 0; j < 4; j++) {
            glTexCoord2fv(texCoords[j]);
            glVertex3fv(vertices[faces[i][j]]);
        }
    }
    glEnd();

    // Draw edges for better definition
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(0.0f, 0.0f, 0.0f);
    glLineWidth(1.0f);
    
    for (int i = 0; i < 6; i++) {
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j < 4; j++) {
            glVertex3fv(vertices[faces[i][j]]);
        }
        glEnd();
    }
    
    glEnable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
}

void ProjectileRenderer::drawPyramid()
{
    // Apply material properties for better 3D appearance
    GLfloat matAmbient[] = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat matDiffuse[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat matSpecular[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat matShininess = 60.0f;
    
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);
    
    // Temporarily disable face culling for debugging
    glDisable(GL_CULL_FACE);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[1]); // Use index 1 for pyramid texture

    const float size = 0.7f; // Size of base
    const float height = 1.5f; // Height of pyramid

    // Vertices: 4 base corners and 1 apex
    const float vertices[5][3] = {
        {-size, -size/2, -size}, // Base vertices (bottom face)
        {size, -size/2, -size},
        {size, -size/2, size},
        {-size, -size/2, size},
        {0.0f, height, 0.0f}     // Apex (top vertex)
    };

    // Calculate face normals with proper orientation
    QVector3D normals[5];
    
    // Base face normal (pointing down)
    normals[0] = QVector3D(0.0f, -1.0f, 0.0f);

    // Calculate side face normals
    for (int i = 0; i < 4; i++) {
        // Get three points forming this triangular face: 
        // current base vertex, next base vertex, and apex
        int v1 = i;
        int v2 = (i + 1) % 4;
        
        // Create vectors for two edges of the triangle
        QVector3D edge1(
            vertices[v2][0] - vertices[v1][0],
            vertices[v2][1] - vertices[v1][1],
            vertices[v2][2] - vertices[v1][2]
        );
        
        QVector3D edge2(
            vertices[4][0] - vertices[v1][0],
            vertices[4][1] - vertices[v1][1],
            vertices[4][2] - vertices[v1][2]
        );
        
        // Cross product to calculate normal
        normals[i+1] = QVector3D::crossProduct(edge1, edge2).normalized();
    }

    // Texture coordinates
    const float baseTexCoords[][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    const float sideTexCoords[][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}
    };

    // Draw base (square) - Ensuring counterclockwise winding
    glBegin(GL_QUADS);
    glNormal3f(normals[0].x(), normals[0].y(), normals[0].z());
    for (int i = 0; i < 4; i++) {
        int idx = 3 - i; // Reversed to get proper winding
        glTexCoord2fv(baseTexCoords[i]);
        glVertex3fv(vertices[idx]);
    }
    glEnd();

    // Draw sides (triangles)
    for (int i = 0; i < 4; i++) {
        int v1 = i;
        int v2 = (i + 1) % 4;
        
        glBegin(GL_TRIANGLES);
        glNormal3f(normals[i+1].x(), normals[i+1].y(), normals[i+1].z());
        
        // Ensure proper counterclockwise winding (v1->v2->apex)
        glTexCoord2fv(sideTexCoords[0]);
        glVertex3fv(vertices[v1]);
        
        glTexCoord2fv(sideTexCoords[1]);
        glVertex3fv(vertices[v2]);
        
        glTexCoord2fv(sideTexCoords[2]);
        glVertex3fv(vertices[4]); // Apex
        glEnd();
    }

    // Finish textured faces
    glDisable(GL_TEXTURE_2D);

    // Re-enable face culling for correct back-face hiding
    glEnable(GL_CULL_FACE);
}

void ProjectileRenderer::drawHalfCone(bool mirror)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_textures[3]); // Cone texture

    GLUquadric *quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);

    glPushMatrix();

    // Flip direction so base faces viewer (Z- direction)
    glRotatef(90, 1, 0, 0); // Cone points toward -Z

    float baseRadius = 0.6f;
    float height = 2.0f;
    int slices = 16;

    if (mirror) {
        glScalef(-1.0f, 1.0f, 1.0f); // Mirror on X axis
    }

    // Draw half cone
    glBegin(GL_TRIANGLE_FAN);
    // Apex
    glTexCoord2f(0.5f, 1.0f);
    glVertex3f(0.0f, 0.0f, height);
    
    // Half of base vertices
    for (int i = 0; i <= slices/2; i++) {
        float angle = (mirror ? -1 : 1) * (i * M_PI / (slices/2));
        float x = baseRadius * cos(angle);
        float y = baseRadius * sin(angle);
        glTexCoord2f((cos(angle)+1.0f)/2.0f, 0.0f);
        glVertex3f(x, y, 0.0f);
    }
    glEnd();

    // Draw half base
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.5f, 0.5f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= slices/2; i++) {
        float angle = (mirror ? -1 : 1) * (i * M_PI / (slices/2));
        float x = baseRadius * cos(angle);
        float y = baseRadius * sin(angle);
        glTexCoord2f((cos(angle)+1.0f)/2.0f, (sin(angle)+1.0f)/2.0f);
        glVertex3f(x, y, 0.0f);
    }
    glEnd();

    glPopMatrix();

    gluDeleteQuadric(quad);
    glDisable(GL_TEXTURE_2D);
}

float getProjectileCollisionRadius(Projectile::Type type)
{
    switch (type) {
    case Projectile::Type::CYLINDER:
        // Cylinder - slightly reduced from 1.12f
        return 1.0f; 
    case Projectile::Type::CONE:
        // Cone - keep the same
        return std::sqrt(0.6f*0.6f + 1.0f*1.0f); // ≈ 1.17f 
    case Projectile::Type::CUBE:
        // Cube - slightly reduced from 1.04f
        return 0.95f;
    case Projectile::Type::PYRAMID:
        // Pyramid - slightly reduced from 1.82f
        return 1.6f;
    default:
        return 1.0f;
    }
}
