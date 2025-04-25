#include "projectile.h"

Projectile::Projectile(Type type)
    : m_position(0.0f, 0.0f, 0.0f)
    , m_velocity(0.0f, 0.0f, 0.0f)
    , m_type(type)
    , m_state(ACTIVE)
    , m_creationTime(0.0f)
{
    // Set properties based on type with more vibrant colors
    switch (type) {
        case APPLE:
            m_color = QColor(255, 0, 0);  // Bright Red
            m_radius = 0.4f; // Slightly larger
            m_pointValue = 10;
            break;
        case ORANGE:
            m_color = QColor(255, 140, 0); // Brighter Orange
            m_radius = 0.45f;
            m_pointValue = 15;
            break;
        case BANANA:
            m_color = QColor(255, 255, 0); // Bright Yellow
            m_radius = 0.35f;
            m_pointValue = 20;
            break;
        case WATERMELON:
            m_color = QColor(50, 205, 50);  // Lime Green
            m_radius = 0.6f; // Larger
            m_pointValue = 30;
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
    
    // If projectile has hit the ground, make it inactive
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
