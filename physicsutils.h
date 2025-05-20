#ifndef PHYSICSUTILS_H
#define PHYSICSUTILS_H

#include <QVector3D>
#include "projectile.h"
#include "projectilerenderdata.h" // Include the header for ProjectileRenderData

QVector3D calculateProjectilePosition(const ProjectileRenderData& proj, float time);
void configureProjectileTrajectory(ProjectileRenderData& projectile);
float distanceBetweenSegments(
    const QVector3D& p1, const QVector3D& q1,
    const QVector3D& p2, const QVector3D& q2);

float getProjectileCollisionRadius(Projectile::Type type);


#endif // PHYSICSUTILS_H
