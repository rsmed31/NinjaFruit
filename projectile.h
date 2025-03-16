#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <QVector3D>
#include <QColor>

class Projectile
{
public:
    enum Type {
        APPLE,
        ORANGE,
        BANANA,
        WATERMELON,
        BOMB // special projectile that reduces lives when hit
    };
    
    enum State {
        ACTIVE,    // Normal flying state
        SPLIT,     // Has been hit/sliced
        INACTIVE   // To be removed from game
    };
    
    Projectile(Type type = APPLE);
    
    // Update position based on elapsed time
    void update(float deltaTime);
    
    // Check if projectile is within the specified distance from a point
    bool isColliding(const QVector3D& point, float collisionDistance) const;
    
    // Split the projectile (when hit by player)
    void split();
    
    // Getters
    QVector3D getPosition() const { return m_position; }
    QVector3D getVelocity() const { return m_velocity; }
    QColor getColor() const { return m_color; }
    float getRadius() const { return m_radius; }
    Type getType() const { return m_type; }
    State getState() const { return m_state; }
    int getPointValue() const { return m_pointValue; }
    float getCreationTime() const { return m_creationTime; }
    
    // Setters
    void setPosition(const QVector3D& position) { m_position = position; }
    void setVelocity(const QVector3D& velocity) { m_velocity = velocity; }
    void setCreationTime(float time) { m_creationTime = time; }
    
private:
    QVector3D m_position;
    QVector3D m_velocity;
    QColor m_color;
    float m_radius;
    Type m_type;
    State m_state;
    int m_pointValue;
    float m_creationTime;
};

#endif // PROJECTILE_H
