#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <QVector3D>
#include <QColor>

class Projectile
{
public:
    enum Type {
        CUBE = 0,        // Faces planes
        PYRAMID = 1,     // Faces planes
        CYLINDER = 2,    // Quadrique
        CONE = 3         // Quadrique
    };



    
    enum State {
        ACTIVE,    // Normal flying state
        SPLIT,     // Has been hit/sliced
        INACTIVE   // To be removed from game
    };
    
    Projectile(Type type = CYLINDER);
    
    // Update position based on elapsed time
    void update(float deltaTime);
    
    // Replace old collision method with one that uses sword geometry
    bool isColliding(const QVector3D& swordHandle, const QVector3D& swordTip) const;
    bool wasSliced() const { return m_wasSliced; }
    void markAsSliced() { m_wasSliced = true; }
    bool wasProcessed() const { return m_wasProcessed; }
    void markAsProcessed() { m_wasProcessed = true; }

private:
    bool m_wasSliced = false;
    bool m_wasProcessed = false;
    
public:
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
