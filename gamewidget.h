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

// Forward declaration - class should be forward declared but not used directly
class Projectile;

class GameWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

    // Set the position of the virtual hand
    void setHandPosition(float x, float y);
    
    // Launch a new projectile
    void launchProjectile(const QVector3D& position, const QVector3D& velocity);
    
    // Clear all projectiles
    void clearProjectiles();
    
    // Split a projectile at the specified index
    void splitProjectile(int index);

signals:
    // Add this signal to notify when sword position changes
    void swordPositionUpdated(const QVector3D& handlePos, const QVector3D& tipPos);

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
    
    // Projectiles
    struct ProjectileRenderData {
        QVector3D position;
        QVector3D velocity;
        float spawnTime;
        bool active;
        enum State { ACTIVE, SPLIT, INACTIVE } state;
    };
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
    
    // Add missing declaration for getSwordEndpoints method
    void getSwordEndpoints(QVector3D& handlePos, QVector3D& tipPos);
    
    // OpenGL-specific helpers
    void createShaders();
    void createGeometry();
    
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
};

#endif // GAMEWIDGET_H
