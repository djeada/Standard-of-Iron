#pragma once

#include <vector>
#include <variant>
#include <algorithm>
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL { class Mesh; class Texture; }

namespace Render::GL {

struct MeshCmd {
    Mesh* mesh = nullptr;
    Texture* texture = nullptr;
    QMatrix4x4 model;
    QVector3D color{1,1,1};
    float alpha = 1.0f;
};

struct GridCmd {
    // Placeholder for future grid overlay; keep minimal to avoid churn now
    QMatrix4x4 model;
    QVector3D color{0.2f,0.25f,0.2f};
    float cellSize = 1.0f;
    float thickness = 0.06f;
    float extent = 50.0f;
};

struct SelectionRingCmd {
    QMatrix4x4 model;
    QVector3D color{0,0,0};
    float alphaInner = 0.6f;
    float alphaOuter = 0.25f;
};

using DrawCmd = std::variant<MeshCmd, GridCmd, SelectionRingCmd>;

class DrawQueue {
public:
    void clear() { m_items.clear(); }
    void submit(const MeshCmd& c) { m_items.emplace_back(c); }
    void submit(const GridCmd& c) { m_items.emplace_back(c); }
    void submit(const SelectionRingCmd& c) { m_items.emplace_back(c); }
    bool empty() const { return m_items.empty(); }
    const std::vector<DrawCmd>& items() const { return m_items; }
    std::vector<DrawCmd>& items() { return m_items; }
    void sortForBatching() {
        // Conservative: keep submission order but group MeshCmd first for simple state changes
        auto it = std::stable_partition(m_items.begin(), m_items.end(), [](const DrawCmd& c){
            return std::holds_alternative<MeshCmd>(c);
        });
        (void)it;
    }
private:
    std::vector<DrawCmd> m_items;
};

} // namespace Render::GL
