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
        case BOMB:
            m_color = QColor(25, 25, 25);    // Almost Black
            m_radius = 0.5f;
            m_pointValue = -30; // Reduced penalty from -50 to -30
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

bool Projectile::isColliding(const QVector3D& point, float collisionDistance) const
{
    if (m_state != ACTIVE) return false;
    
    // Calculate distance between projectile center and point (sword position)
    float distance = (m_position - point).length();
    
    // More generous collision detection - slightly larger than the sum of radii
    // This makes it easier to hit projectiles and feels more satisfying
    return distance < (m_radius + collisionDistance) * 1.1f;
}

void Projectile::split()
{
    if (m_state == ACTIVE) {
        m_state = SPLIT;
    }
}
