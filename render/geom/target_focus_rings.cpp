#include "target_focus_rings.h"

#include <QMatrix4x4>
#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "game/accessibility/team_identity.h"
#include "game/map/terrain_service.h"
#include "mode_indicator.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

const QVector3D k_hostile_lock_color(1.0F, 0.40F, 0.20F);
const QVector3D k_hostile_lock_core(1.0F, 0.72F, 0.42F);
const QVector3D k_friendly_focus_color(1.0F, 0.84F, 0.32F);
const QVector3D k_neutral_focus_color(0.86F, 0.86F, 0.80F);
const QVector3D k_incoming_color(0.96F, 0.16F, 0.12F);

constexpr float k_lock_thickness = 0.16F;
constexpr float k_lock_alpha = 0.95F;
constexpr float k_lock_scale = 1.22F;
constexpr float k_incoming_thickness = 0.11F;
constexpr float k_incoming_alpha = 0.85F;
constexpr float k_incoming_scale = 1.10F;
constexpr float k_inspect_thickness = 0.14F;
constexpr float k_inspect_alpha = 0.90F;
constexpr float k_inspect_scale = 1.30F;
constexpr float k_ring_lift = 0.03F;

struct BillboardBasis {
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
};

auto billboard_basis(const Camera& camera) -> BillboardBasis {
  const QMatrix4x4 view = camera.get_view_matrix();
  BillboardBasis basis;
  basis.right = QVector3D(view(0, 0), view(0, 1), view(0, 2));
  QVector3D const view_up(view(1, 0), view(1, 1), view(1, 2));
  QVector3D const view_forward =
      QVector3D::crossProduct(basis.right, view_up).normalized();
  float const tilt_sin = std::sin(Render::Geom::k_indicator_tilt_radians);
  float const tilt_cos = std::cos(Render::Geom::k_indicator_tilt_radians);
  basis.up = (view_up * tilt_cos - view_forward * tilt_sin).normalized();
  basis.forward = QVector3D::crossProduct(basis.right, basis.up).normalized();
  return basis;
}

void draw_glyph(Renderer* renderer,
                const BillboardBasis& basis,
                const QVector3D& ground_position,
                float radius,
                Render::Geom::IndicatorKind kind,
                const QVector3D& color) {
  if (!Render::Geom::indicator_has_glyph(kind)) {
    return;
  }
  float const scale = Render::Geom::indicator_world_size();
  float const height = Render::Geom::indicator_height_for_unit(radius, 1.0F);
  QVector3D const anchor(
      ground_position.x(), ground_position.y() + height, ground_position.z());
  QMatrix4x4 model;
  model.setColumn(0, QVector4D(basis.right * scale, 0.0F));
  model.setColumn(1, QVector4D(basis.up * scale, 0.0F));
  model.setColumn(2, QVector4D(basis.forward * scale, 0.0F));
  model.setColumn(3, QVector4D(anchor, 1.0F));
  renderer->mode_indicator(
      model, static_cast<int>(kind), color, Render::Geom::k_indicator_alpha);
}

} // namespace

void render_target_focus_rings(Renderer* renderer,
                               ResourceManager* resources,
                               std::span<const TargetFocusVisual> visuals,
                               const TargetFocusStyle& style) {
  if ((renderer == nullptr) || (resources == nullptr) || visuals.empty()) {
    return;
  }

  const Camera* camera = renderer->camera();
  BillboardBasis const basis =
      camera != nullptr ? billboard_basis(*camera) : BillboardBasis{};
  const auto& terrain = renderer->world_view().terrain_or_empty();
  float const time = renderer->get_animation_time();
  float const pulse =
      style.reduced_motion ? 1.0F : 0.90F + 0.10F * std::sin(time * 5.2F);
  float const chevron_spin = style.reduced_motion ? 0.0F : std::fmod(time * 0.9F, 1.0F);

  for (const auto& visual : visuals) {
    QVector3D const grounded = terrain.resolve_surface_world_position(
        visual.position.x(), visual.position.z(), 0.0F, visual.position.y());
    GroundMarkerCmd ring;
    ring.center = QVector3D(grounded.x(), grounded.y() + k_ring_lift, grounded.z());

    switch (visual.role) {
    case TargetFocusVisualRole::Inspected: {
      QVector3D const color = visual.hostile       ? k_hostile_lock_color
                              : visual.is_building ? k_friendly_focus_color
                                                   : k_neutral_focus_color;
      ring.outer_radius = visual.radius * k_inspect_scale;
      ring.thickness = k_inspect_thickness;
      ring.color = color;
      ring.alpha = k_inspect_alpha;
      ring.pattern = Game::Accessibility::TeamPattern::Solid;
      ring.focused = true;
      renderer->ground_marker(ring);
      break;
    }
    case TargetFocusVisualRole::LockedTarget: {
      ring.outer_radius = visual.radius * k_lock_scale * pulse;
      ring.thickness = k_lock_thickness;
      ring.color = visual.hostile ? k_hostile_lock_color : k_neutral_focus_color;
      ring.alpha = k_lock_alpha;
      ring.pattern = Game::Accessibility::TeamPattern::DoubleRing;
      ring.focused = true;
      renderer->ground_marker(ring);

      GroundMarkerCmd core = ring;
      core.outer_radius = visual.radius * 0.55F;
      core.thickness = 0.06F;
      core.color = k_hostile_lock_core;
      core.alpha = 0.55F * (visual.hostile ? 1.0F : 0.6F);
      core.pattern = Game::Accessibility::TeamPattern::Solid;
      core.focused = false;
      renderer->ground_marker(core);

      draw_glyph(renderer,
                 basis,
                 grounded,
                 visual.radius,
                 Render::Geom::IndicatorKind::Attack,
                 ring.color);
      break;
    }
    case TargetFocusVisualRole::IncomingAttacker: {
      ring.outer_radius = visual.radius * k_incoming_scale;
      ring.thickness = k_incoming_thickness;
      ring.color = k_incoming_color;
      ring.alpha = k_incoming_alpha;
      ring.pattern = Game::Accessibility::TeamPattern::Chevron;
      ring.phase = chevron_spin;
      ring.focused = false;
      renderer->ground_marker(ring);
      break;
    }
    }
  }
}

} // namespace Render::GL
