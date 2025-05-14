#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <QVector3D>
#include <QColor>

class Projectile
{
public:
    enum Type {
        CONE,
        CYLINDER,
        CUBE,
        PYRAMID
    };


    
    enum State {
        ACTIVE,    // Normal flying state
        SPLIT,     // Has been hit/sliced
        INACTIVE   // To be removed from game
    };
    
    static int nextId; // Static counter for unique IDs

    Projectile(Type type = CYLINDER);
    
    // Update position based on elapsed time
    void update(float deltaTime);
    
    // Replace old collision method with one that uses sword geometry
    bool isColliding(const QVector3D& swordHandle, const QVector3D& swordTip) const;
    bool wasSliced() const { return m_wasSliced; }
    void markAsSliced() { m_wasSliced = true; }
    void markAsProcessed() { m_wasProcessed = true; }
    bool wasProcessed() const { return m_wasProcessed; }

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
    int getId() const { return m_id; } // Getter for ID
    
    // Setters
    void setPosition(const QVector3D& position) { m_position = position; }
    void setVelocity(const QVector3D& velocity) { m_velocity = velocity; }
    void setCreationTime(float time) { m_creationTime = time; }
    void setId(int id) { m_id = id; } // Setter for ID

private:
    QVector3D m_position;
    QVector3D m_velocity;
    QColor m_color;
    float m_radius;
    Type m_type;
    State m_state;
    int m_pointValue;
    float m_creationTime;
    int m_id; // Unique identifier for the projectile
};

#endif // PROJECTILE_H

