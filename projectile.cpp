#include "projectile.h"

int Projectile::nextId = 0; // Initialize static counter

Projectile::Projectile(Type type)
    : m_type(type), m_state(ACTIVE), m_id(nextId++) // Assign unique ID
{
    switch (type) {
    case CONE:
        m_color = QColor(Qt::green);
        m_pointValue = 10;
        m_radius = 0.5f;
        break;
    case CYLINDER:
        m_color = QColor(Qt::yellow);
        m_pointValue = 10;
        m_radius = 0.4f; // base radius of cylinder
        break;
    case CUBE:
        m_color = QColor(Qt::red);
        m_pointValue = 10;
        m_radius = 0.5f * 1.414f; // diagonal from center to corner
        break;
    case PYRAMID:
        m_color = QColor(Qt::magenta);
        m_pointValue = 10;
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

    QVector3D swordVector = swordTip - swordHandle;
    float swordLength = swordVector.length();
    QVector3D swordDirection = swordVector / swordLength;

    QVector3D startToProjectile = m_position - swordHandle;
    float projection = QVector3D::dotProduct(startToProjectile, swordDirection);

    QVector3D closestPoint;
    if (projection <= 0) {
        closestPoint = swordHandle;
    } else if (projection >= swordLength) {
        closestPoint = swordTip;
    } else {
        closestPoint = swordHandle + swordDirection * projection;
    }

    float distance = (m_position - closestPoint).length();
    return distance <= m_radius;
}

void Projectile::split()
{
    if (m_state == ACTIVE) {
        m_state = SPLIT;
    }
}
