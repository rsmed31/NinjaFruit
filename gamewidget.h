#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QTimer>
#include <QList>
#include <QRandomGenerator>

// Include the full Projectile class rather than forward declaring it
#include "projectile.h"

// Define ProjectileType enum
enum class ProjectileType {
    APPLE,
    ORANGE,
    BANANA,
    WATERMELON,
    CONE,
    CYLINDER,
    CUBE,
    PYRAMID
    // Add other types as needed
};

// Add this before the GameWidget class definition
struct ProjectileRenderData {
    enum Type {
        CONE,
        CYLINDER,
        CUBE,
        PYRAMID
        // Remove unused fruit types (APPLE, ORANGE, etc.)
    };
    
    enum State {
        INACTIVE,
        ACTIVE,
        SPLIT,
        DESTROYED
    };
    
    QVector3D position;
    QVector3D velocity;
    Type type;
    State state;
    bool active;
    float spawnTime;
    // Add other rendering properties as needed
};

class GameWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

    // Set the position of the virtual hand
    void setHandPosition(float x, float y);
    
    // Launch a new projectile (keep only the Projectile object version)
    void launchProjectile(const Projectile& projectile);
    
    // Clear all projectiles
    void clearProjectiles();
    
    // Split a projectile at the specified index
    void splitProjectile(int index);
    
    // Add this function declaration
    void createRandomProjectile();

signals:
    // Add this signal to notify when sword position changes
    void swordPositionUpdated(const QVector3D& handlePos, const QVector3D& tipPos);
    void scoreChanged(int points);  // Properly declare the signal
    void livesChanged(int lives);   // Signal for lives updates

protected:
    // OpenGL functions that must be implemented
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private slots:
    void updateScene();

private:
    // Hand position
    QVector3D m_handPosition;
    QVector3D m_lastValidHandPosition;  // Store the last valid hand position

    // Projectiles
    QList<ProjectileRenderData> m_projectiles;
    
    // Camera and view properties
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_cameraDistance;
    float m_cameraHeight;
    
    // Game area dimensions
    float m_floorSize;
    float m_handRangeRadius;
    float m_handRangeHeight;
    
    // Animation and timing
    QTimer* m_animationTimer;
    float m_elapsedTime;
    
    // Drawing helper functions
    void drawFloorGrid();
    void drawHandRange();
    void drawVirtualHand();
    void drawProjectiles();
    void drawDistanceIndicators();

    //ss
    void drawCone();
    void drawCylinder();
    void drawCube();
    void drawPyramid();
    void drawHalfCone(bool mirror);
//
    
    // Add missing declaration for getSwordEndpoints method
    void getSwordEndpoints(QVector3D& handlePos, QVector3D& tipPos);
    
    // OpenGL-specific helpers
    void createShaders();
    void createGeometry();
    
    // New functions for textures
    void loadTextures();
    GLuint m_textures[4]; // Array for texture IDs
    
    // Helper function to map Projectile::Type to ProjectileRenderData::Type
    ProjectileRenderData::Type mapProjectileType(Projectile::Type type);
    
    // Physics
    void updateProjectilePositions();
    QVector3D calculateProjectilePosition(const ProjectileRenderData& proj, float time);
    
    // OpenGL resources
    QOpenGLShaderProgram* m_program;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_indexBuffer;
    
    // Draw the hit cylinder
    void drawHitCylinder();
    
    // Spawn projectiles that will pass through the hit zone
    void configureProjectileTrajectory(ProjectileRenderData& projectile);
    
    // Check collision between hand and projectiles within hit zone
    void checkHitZoneCollisions();

    float getProjectileCollisionRadius(ProjectileRenderData::Type type) const;

};

#endif // GAMEWIDGET_H
