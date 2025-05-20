#include "physicsutils.h"
#include "projectile.h" // For ProjectileRenderData
#include <QtMath>
#include <QRandomGenerator>

QVector3D calculateProjectilePosition(const ProjectileRenderData& proj, float time)
{
    const QVector3D gravity(0.0f, -9.8f, 0.0f);
    return proj.position + proj.velocity * time + 0.5f * gravity * time * time;
}

void configureProjectileTrajectory(ProjectileRenderData& projectile)
{
    bool willHitZone = false;

    if (projectile.velocity.z() < 0) {
        float timeToBack = (0.0f - projectile.position.z()) / projectile.velocity.z();
        float timeToFront = (5.0f - projectile.position.z()) / projectile.velocity.z();
        if (timeToBack > 0.0f && timeToFront > timeToBack) {
            willHitZone = true;
        }
    }

    if (!willHitZone) {
        float targetZ = 3.0f;
        float targetX = QRandomGenerator::global()->bounded(6) - 3;
        float targetY = 1.0f + QRandomGenerator::global()->bounded(3);
        const float desiredTime = 2.0f;

        projectile.velocity.setZ((targetZ - projectile.position.z()) / desiredTime);
        projectile.velocity.setX((targetX - projectile.position.x()) / desiredTime);
        projectile.velocity.setY((targetY - projectile.position.y()) / desiredTime +
                                 0.5f * 9.8f * desiredTime);
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

    if (a <= 1e-6f && e <= 1e-6f)
        return (p1 - p2).length();

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
            s = (denom != 0.0f) ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = std::clamp((b * s + f) / e, 0.0f, 1.0f);
        }
    }

    QVector3D c1 = p1 + d1 * s;
    QVector3D c2 = p2 + d2 * t;
    return (c1 - c2).length();
}


float getProjectileCollisionRadius(Projectile::Type type)
{
    switch (type)
    {
    case Projectile::CYLINDER: return 1.0f;   // length = 2
    case Projectile::CONE:     return 0.7f;   // height = 2, base = 0.6
    case Projectile::CUBE:     return 1.3f;   // side = 2
    case Projectile::PYRAMID:  return 1.2f;   // base 2, height 1.6
    default:                   return 0.6f;
    }
}