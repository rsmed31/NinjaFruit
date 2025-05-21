#ifndef HAND_RENDERER_H
#define HAND_RENDERER_H

#include <QVector3D>
#include <QObject>
#include <GL/gl.h>

class HandRenderer {
public:
    HandRenderer(QVector3D& handPosition, GLuint* textures);
    
    void drawVirtualHand();
    void getSwordEndpoints(QVector3D& handlePos, QVector3D& tipPos);
    void setHandPosition(const QVector3D& position);

private:
    QVector3D& m_handPosition;
    GLuint* m_textures;
};

#endif // HAND_RENDERER_H
