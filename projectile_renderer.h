#ifndef PROJECTILE_RENDERER_H
#define PROJECTILE_RENDERER_H

#include <QVector>
#include <QVector3D>
#include "projectile.h"
#include "projectilerenderdata.h"
#include <GL/gl.h>

// Use forward declaration for GLUquadric
typedef struct GLUquadric GLUquadric;

class ProjectileRenderer
{
public:
    ProjectileRenderer(QVector<ProjectileRenderData>& projectiles, 
                      GLuint* textures, 
                      float& elapsedTime);
    
    void drawProjectiles();
    
private:
    // Shape rendering methods
    void drawCone();
    void drawCylinder();
    void drawCube();
    void drawPyramid();
    void drawHalfCone(bool mirror);
    
    // Physics and collision detection
    float getProjectileCollisionRadius(Projectile::Type type);
    
    // References to game data
    QVector<ProjectileRenderData>& m_projectiles;
    GLuint* m_textures;
    float& m_elapsedTime;
};

#endif // PROJECTILE_RENDERER_H
