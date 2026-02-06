#pragma once

#include <QMatrix4x4>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Core {
class Entity;
class TransformComponent;
class UnitComponent;
class RenderableComponent;
class MovementComponent;
} // namespace Engine::Core

namespace Render {

struct CachedUnitData {
  std::uint32_t entity_id{0};
  Engine::Core::Entity *entity{nullptr};

  Engine::Core::TransformComponent *transform{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  Engine::Core::RenderableComponent *renderable{nullptr};
  Engine::Core::MovementComponent *movement{nullptr};

  std::string renderer_key;
  QMatrix4x4 model_matrix;
  float distance_sq{0.0F};
  bool moving{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool has_attack{false};
  bool has_guard_mode{false};
  bool has_hold_mode{false};
  bool has_patrol{false};

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

  std::uint32_t last_seen_frame{0};
};

class UnitRenderCache {
public:
  auto get_or_create(std::uint32_t entity_id, Engine::Core::Entity *entity,
                     std::uint32_t frame) -> CachedUnitData &;

  void prune(std::uint32_t current_frame, std::uint32_t max_age = 120);

  void clear() { m_cache.clear(); }

  [[nodiscard]] auto size() const -> std::size_t { return m_cache.size(); }

  static auto update_model_matrix(CachedUnitData &data) -> bool;

private:
  std::unordered_map<std::uint32_t, CachedUnitData> m_cache;
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
  auto get_or_create(std::uint32_t entity_id,
                     Engine::Core::TransformComponent *transform,
                     std::uint32_t frame) -> const QMatrix4x4 &;

  void prune(std::uint32_t current_frame, std::uint32_t max_age = 120);
  void clear() { m_cache.clear(); }

private:
  std::unordered_map<std::uint32_t, CachedModelMatrix> m_cache;
};

} // namespace Render
