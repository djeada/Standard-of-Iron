#pragma once

#include "../gl/mesh.h"
#include <QVector3D>
#include <memory>

namespace Render::Geom {

constexpr int k_mode_type_attack = 0;
constexpr int k_mode_type_guard = 1;
constexpr int k_mode_type_hold = 2;
constexpr int k_mode_type_patrol = 3;

constexpr float k_indicator_height_base = 2.5F;
constexpr float k_indicator_size = 0.4F;
constexpr float k_indicator_alpha = 0.85F;
constexpr float k_indicator_height_multiplier = 2.0F;
constexpr float k_frustum_cull_margin = 1.5F;

const QVector3D k_attack_mode_color(1.0F, 0.3F, 0.3F);
const QVector3D k_guard_mode_color(0.3F, 0.5F, 1.0F);
const QVector3D k_hold_mode_color(1.0F, 0.6F, 0.2F);
const QVector3D k_patrol_mode_color(0.5F, 0.5F, 0.5F);

class ModeIndicator {
public:
  static auto get_attack_mode_mesh() -> Render::GL::Mesh *;
  static auto get_guard_mode_mesh() -> Render::GL::Mesh *;
  static auto get_hold_mode_mesh() -> Render::GL::Mesh *;
  static auto get_patrol_mode_mesh() -> Render::GL::Mesh *;

private:
  static auto create_attack_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;
  static auto create_guard_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;
  static auto create_hold_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;
  static auto create_patrol_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;

  static std::unique_ptr<Render::GL::Mesh> s_attack_mesh;
  static std::unique_ptr<Render::GL::Mesh> s_guard_mesh;
  static std::unique_ptr<Render::GL::Mesh> s_hold_mesh;
  static std::unique_ptr<Render::GL::Mesh> s_patrol_mesh;
};

} // namespace Render::Geom
