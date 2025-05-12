#include "projectile.h"

Projectile::Projectile(Type type)
    : m_type(type), m_state(ACTIVE)
{
    switch (type) {
    case CONE:
        m_color = QColor(Qt::green);
        m_pointValue = 10;
        m_radius = 0.5f;
        break;
    case CYLINDER:
        m_color = QColor(Qt::yellow);
        m_pointValue = 15;
        m_radius = 0.4f; // base radius of cylinder
        break;
    case CUBE:
        m_color = QColor(Qt::red);
        m_pointValue = 20;
        m_radius = 0.5f * 1.414f; // diagonal from center to corner
        break;
    case PYRAMID:
        m_color = QColor(Qt::magenta);
        m_pointValue = 25;
        m_radius = 0.5f * 1.2f; // approximate
        break;
    }
}



void Projectile::update(float deltaTime)
{
    if (m_state == INACTIVE) return;
    
    // Apply gravity to velocity
    QVector3D gravity(0.0f, -9.8f, 0.0f);
    m_velocity += gravity * deltaTime;
    
    // Update position
    m_position += m_velocity * deltaTime;
    
    // If projectile has hit the ground or passed far plane (z >= 10.0), make it inactive
    if (m_position.y() < -0.1f && m_velocity.y() < 0) {
        m_state = INACTIVE;
    }
}

// Replace old point-based collision with line segment collision
bool Projectile::isColliding(const QVector3D& swordHandle, const QVector3D& swordTip) const
{
    if (m_state != ACTIVE) return false;
    
    // Vector from start to end of sword
    QVector3D swordVector = swordTip - swordHandle;
    float swordLength = swordVector.length();
    
    // Normalize the sword vector
    QVector3D swordDirection = swordVector / swordLength;
    
    // Vector from sword start to projectile center
    QVector3D startToProjectile = m_position - swordHandle;
    
    // Project this vector onto the sword line
    float projection = QVector3D::dotProduct(startToProjectile, swordDirection);
    
    // Find the closest point on the sword line segment to the projectile
    QVector3D closestPoint;
    
    if (projection <= 0) {
        // Closest to the sword handle
        closestPoint = swordHandle;
    } 
    else if (projection >= swordLength) {
        // Closest to the sword tip
        closestPoint = swordTip;
    } 
    else {
        // Closest to a point on the sword blade
        closestPoint = swordHandle + swordDirection * projection;
    }
    
    // Calculate the distance from the closest point to the projectile center
    float distance = (m_position - closestPoint).length();
    
    // A hit occurs only if the distance is less than the projectile radius
    return distance <= m_radius;
}

void Projectile::split()
{
    if (m_state == ACTIVE) {
        m_state = SPLIT;
    }
}
