#include "interaction_target_markers.h"

#include <QMatrix4x4>
#include <QVector4D>

#include <cmath>

#include "game/accessibility/team_identity.h"
#include "game/map/terrain_service.h"
#include "mode_indicator.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

constexpr float k_idle_alpha = 0.45F;
constexpr float k_hovered_alpha = 0.85F;
constexpr float k_idle_thickness = 0.10F;
constexpr float k_hovered_thickness = 0.14F;
constexpr float k_hovered_ring_scale = 1.2F;
constexpr float k_glyph_alpha = Render::Geom::k_indicator_alpha;

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

void draw_action_glyph(Renderer* renderer,
                       const BillboardBasis& basis,
                       const QVector3D& ground_position,
                       float radius,
                       Render::Geom::IndicatorKind action) {
  if (!Render::Geom::indicator_has_glyph(action)) {
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

  renderer->mode_indicator(model,
                           static_cast<int>(action),
                           Render::Geom::indicator_base_color(action),
                           k_glyph_alpha);
}

} // namespace

void render_interaction_target_markers(
    Renderer* renderer,
    ResourceManager* resources,
    std::span<const InteractionTargetMarkerVisual> markers) {
  if ((renderer == nullptr) || (resources == nullptr) || markers.empty()) {
    return;
  }

  const Camera* camera = renderer->camera();
  BillboardBasis const basis =
      camera != nullptr ? billboard_basis(*camera) : BillboardBasis{};

  const auto& terrain_service = renderer->world_view().terrain_or_empty();
  for (const auto& marker : markers) {
    GroundMarkerCmd ring;
    ring.center =
        QVector3D(marker.position.x(), marker.position.y(), marker.position.z());
    ring.outer_radius =
        marker.hovered ? marker.radius * k_hovered_ring_scale : marker.radius;
    ring.thickness = marker.hovered ? k_hovered_thickness : k_idle_thickness;
    ring.focused = marker.hovered;
    ring.color = Render::Geom::indicator_base_color(marker.action);
    ring.alpha = marker.hovered ? k_hovered_alpha : k_idle_alpha;
    ring.pattern = marker.hovered ? Game::Accessibility::TeamPattern::DoubleRing
                                  : Game::Accessibility::TeamPattern::Dotted;
    renderer->ground_marker(ring);

    if (!marker.hovered) {
      continue;
    }

    QVector3D const grounded = terrain_service.resolve_surface_world_position(
        marker.position.x(), marker.position.z(), 0.0F, marker.position.y());
    draw_action_glyph(renderer, basis, grounded, marker.radius, marker.action);
  }
}

} // namespace Render::GL
