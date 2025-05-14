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
        INACTIVE,
        ACTIVE,
        SPLIT,
        DESTROYED
    };
    
    QVector3D position;   // Position of the projectile
    QVector3D velocity;   // Velocity of the projectile
    Type type;            // Type of projectile
    State state;          // Current state
    bool active;          // Whether the projectile is active
    float spawnTime;      // Time when the projectile was spawned
    int id;               // Unique identifier for the projectile
};

#endif // PROJECTILERENDERDATA_H