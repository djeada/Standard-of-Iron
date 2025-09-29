#pragma once

#include "entity.h"

namespace Engine::Core {

// Transform Component
class TransformComponent : public Component {
public:
    TransformComponent(float x = 0.0f, float y = 0.0f, float z = 0.0f, 
                      float rotX = 0.0f, float rotY = 0.0f, float rotZ = 0.0f,
                      float scaleX = 1.0f, float scaleY = 1.0f, float scaleZ = 1.0f)
        : position{x, y, z}, rotation{rotX, rotY, rotZ}, scale{scaleX, scaleY, scaleZ} {}

    struct Vec3 { float x, y, z; };
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
};

// Renderable Component
class RenderableComponent : public Component {
public:
    enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };

    RenderableComponent(const std::string& meshPath, const std::string& texturePath)
        : meshPath(meshPath), texturePath(texturePath), visible(true), mesh(MeshKind::Cube) {
        color[0] = color[1] = color[2] = 1.0f;
    }

    std::string meshPath;
    std::string texturePath;
    bool visible;
    MeshKind mesh;
    float color[3]; // RGB 0..1
};

// Unit Component (for RTS units)
class UnitComponent : public Component {
public:
    UnitComponent(int health = 100, int maxHealth = 100, float speed = 1.0f)
        : health(health), maxHealth(maxHealth), speed(speed), selected(false) {}

    int health;
    int maxHealth;
    float speed;
    bool selected;
    std::string unitType;
};

// Movement Component
class MovementComponent : public Component {
public:
    MovementComponent() : hasTarget(false), targetX(0.0f), targetY(0.0f), vx(0.0f), vz(0.0f) {}

    bool hasTarget;
    float targetX, targetY;
    // velocity in XZ plane for smoothing
    float vx, vz;
    std::vector<std::pair<float, float>> path;
};

} // namespace Engine::Core