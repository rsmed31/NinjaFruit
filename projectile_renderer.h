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
    void drawCylinder(bool leftCone = true, bool rightCone = true);
    void drawCube();
    void drawPyramid();
    void drawHalfCone(bool mirror);
    
    // Physics and collision detection
    QVector3D calculateProjectilePosition(const ProjectileRenderData& proj, float time);
    float getProjectileCollisionRadius(Projectile::Type type);
    
    // References to game data
    QVector<ProjectileRenderData>& m_projectiles;
    GLuint* m_textures;
    float& m_elapsedTime;
};

#endif // PROJECTILE_RENDERER_H
