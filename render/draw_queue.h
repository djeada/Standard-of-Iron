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
        // Stable partition to group MeshCmds first while preserving relative order of non-mesh cmds
        std::stable_sort(m_items.begin(), m_items.end(), [](const DrawCmd& a, const DrawCmd& b){
            bool am = std::holds_alternative<MeshCmd>(a);
            bool bm = std::holds_alternative<MeshCmd>(b);
            if (am != bm) return am && !bm; // MeshCmds come first
            if (!am && !bm) return false;   // preserve order among non-mesh
            // Both are MeshCmds: sort by texture then mesh ptr for fewer state changes
            const auto& ma = std::get<MeshCmd>(a);
            const auto& mb = std::get<MeshCmd>(b);
            if (ma.texture != mb.texture) return ma.texture < mb.texture;
            return ma.mesh < mb.mesh;
        });
    }
private:
    std::vector<DrawCmd> m_items;
};

} // namespace Render::GL
