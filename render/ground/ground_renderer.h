#pragma once
#include <QMatrix4x4>
#include <QVector3D>

namespace Render { namespace GL {
class Renderer; class ResourceManager; class Mesh; class Texture;

class GroundRenderer {
public:
    void configure(float tileSize, int width, int height) {
        m_tileSize = tileSize; m_width = width; m_height = height; recomputeModel();
    }
    void configureExtent(float extent) { m_extent = extent; recomputeModel(); }
    void setColor(const QVector3D& c) { m_color = c; }
    void submit(Renderer& renderer, ResourceManager& resources);
private:
    void recomputeModel();
    float m_tileSize = 1.0f;
    int m_width = 50;
    int m_height = 50;
    float m_extent = 50.0f; // alternative config
    QVector3D m_color{0.15f,0.18f,0.15f};
    QMatrix4x4 m_model;
};

} }
