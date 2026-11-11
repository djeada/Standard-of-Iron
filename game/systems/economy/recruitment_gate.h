#pragma once

#include <QVector3D>

namespace Engine::Core {
class World;
class TransformComponent;
class ProductionComponent;
} // namespace Engine::Core

namespace Game::Systems {

struct RecruitmentGate {
  QVector3D position;
  QVector3D outward;
};

[[nodiscard]] auto
barracks_gate(const Engine::Core::TransformComponent& transform) -> RecruitmentGate;

[[nodiscard]] auto
barracks_default_rally(const Engine::Core::TransformComponent& transform) -> QVector3D;

[[nodiscard]] auto
barracks_assembly_point(const Engine::Core::TransformComponent& transform,
                        Engine::Core::ProductionComponent& production) -> QVector3D;

[[nodiscard]] auto barracks_assembly_slot(const QVector3D& assembly,
                                          const RecruitmentGate& gate,
                                          int index) -> QVector3D;

} // namespace Game::Systems
