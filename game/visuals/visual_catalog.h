#pragma once

#include <QString>
#include <QVector3D>
#include <unordered_map>
#include <string>

namespace Engine { namespace Core { class RenderableComponent; } }

namespace Game::Visuals {

struct VisualDef {
    // Mirrors Engine::Core::RenderableComponent::MeshKind by name
    enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };
    MeshKind mesh = MeshKind::Cube;
    QVector3D color{1.0f, 1.0f, 1.0f};
    QString texture; // optional
};

class VisualCatalog {
public:
    bool loadFromJsonFile(const QString& path, QString* outError = nullptr);
    bool lookup(const std::string& unitType, VisualDef& out) const;
private:
    std::unordered_map<std::string, VisualDef> m_units;
};

// Utility to map string to MeshKind
VisualDef::MeshKind meshKindFromString(const QString& s);

// Apply a VisualDef to an Engine::Core::RenderableComponent
void applyToRenderable(const VisualDef& def, Engine::Core::RenderableComponent& r);

} // namespace Game::Visuals
