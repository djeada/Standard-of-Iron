#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "render/world_view.h"

namespace Engine::Core {
class Entity;
class TransformComponent;
class UnitComponent;
class RenderableComponent;
class MovementComponent;
class CommanderPresentationSampleComponent;
} // namespace Engine::Core

namespace Engine::Core {
using EntityID = std::uint64_t;
}

namespace Render {

[[nodiscard]] auto resolve_profile_unit_renderer_key(
    const WorldView& world, const Engine::Core::UnitComponent& unit) -> std::string;

[[nodiscard]] auto resolve_unit_renderer_key(
    const WorldView& world,
    const Engine::Core::UnitComponent& unit,
    const Engine::Core::RenderableComponent* renderable) -> std::string;

struct CachedUnitData {
  Engine::Core::EntityID entity_id{0};
  Engine::Core::Entity* entity{nullptr};

  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  const Engine::Core::CommanderPresentationSampleComponent* presentation{nullptr};

  std::string renderer_key;
  std::uint32_t renderer_handle{0};
  bool has_renderer_handle{false};
  QMatrix4x4 model_matrix;
  float distance_sq{0.0F};
  bool moving{false};
  bool in_frustum{true};
  bool fog_visible{true};
  float indicator_height{0.0F};

  float last_pos_x{0.0F};
  float last_pos_y{0.0F};
  float last_pos_z{0.0F};
  float last_rot_x{0.0F};
  float last_rot_y{0.0F};
  float last_rot_z{0.0F};
  float last_scale_x{0.0F};
  float last_scale_y{0.0F};
  float last_scale_z{0.0F};
  bool model_matrix_valid{false};
  std::uint32_t presentation_seen_sequence{0};
  float presentation_age{0.0F};
  bool renderer_key_valid{false};
  bool last_is_building{false};
  Game::Units::SpawnType last_spawn_type{Game::Units::SpawnType::Archer};
  Game::Systems::NationID last_nation_id{Game::Systems::NationID::RomanRepublic};
  std::string last_renderable_renderer_id;

  std::uint32_t last_seen_frame{0};
};

class UnitRenderCache {
public:
  auto get_or_create(const WorldView& world,
                     Engine::Core::EntityID entity_id,
                     Engine::Core::Entity* entity,
                     std::uint32_t frame) -> CachedUnitData&;

  void prune(std::uint32_t current_frame, std::uint32_t max_age = 120);

  void clear() { m_cache.clear(); }

  [[nodiscard]] auto size() const -> std::size_t { return m_cache.size(); }

  static auto update_model_matrix(CachedUnitData& data,
                                  float frame_delta_seconds) -> bool;

private:
  std::unordered_map<Engine::Core::EntityID, CachedUnitData> m_cache;
};

struct CachedModelMatrix {
  QMatrix4x4 matrix;
  float last_pos_x{0.0F};
  float last_pos_y{0.0F};
  float last_pos_z{0.0F};
  float last_rot_x{0.0F};
  float last_rot_y{0.0F};
  float last_rot_z{0.0F};
  float last_scale_x{0.0F};
  float last_scale_y{0.0F};
  float last_scale_z{0.0F};
  bool valid{false};
  std::uint32_t last_seen_frame{0};
};

class ModelMatrixCache {
public:
  auto get_or_create(Engine::Core::EntityID entity_id,
                     Engine::Core::TransformComponent* transform,
                     std::uint32_t frame) -> const QMatrix4x4&;

  void prune(std::uint32_t current_frame, std::uint32_t max_age = 120);
  void clear() { m_cache.clear(); }

private:
  std::unordered_map<Engine::Core::EntityID, CachedModelMatrix> m_cache;
};

} // namespace Render
