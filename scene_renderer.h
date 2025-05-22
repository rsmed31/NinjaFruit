#ifndef SCENE_RENDERER_H
#define SCENE_RENDERER_H

#include <QVector3D>
#include <QMatrix4x4>
#include <GL/gl.h>

class SceneRenderer {
public:
    SceneRenderer(float floorSize, float cameraDistance, float cameraHeight);
    
    void drawDistanceIndicators();
    void drawHitCylinder();
    void setupLights();
    void setupCamera(int width, int height); 
    void drawArenaWalls(GLuint wallTexture, GLuint archTexture, GLuint portalTexture);
    void setFloorTexture(GLuint textureId);

    

private:
    float m_floorSize;
    float m_cameraDistance;
    float m_cameraHeight;
    GLuint m_floorTexture = 0;

};

#endif // SCENE_RENDERER_H
