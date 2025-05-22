#ifndef PROJECTILERENDERDATA_H
#define PROJECTILERENDERDATA_H

#include <QVector3D>

// Structure to hold the rendering data of a projectile
struct ProjectileRenderData {
    // Type enum
    enum Type {
        CONE,
        CYLINDER,
        CUBE,
        PYRAMID
    };
    
    // State enum
    enum State {
        ACTIVE,
        SPLIT,
        REMOVED
    };
    
    QVector3D position;   // Position of the projectile
    QVector3D velocity;   // Velocity of the projectile
    Type type;            // Type of projectile
    float spawnTime;      // Time when the projectile was spawned
    bool active;          // Whether the projectile is active
    State state;          // Current state
    int id;               // Unique identifier for the projectile
    
    // Physics data
    QVector3D initialPosition;
    QVector3D initialVelocity;
    float gravity;
};

// Collision utility function - moved from projectile_renderer.cpp
float getProjectileCollisionRadius(ProjectileRenderData::Type type);

#endif // PROJECTILERENDERDATA_H