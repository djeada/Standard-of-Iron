#include "mode_indicator.h"

#include <QVector2D>

#include <cmath>
#include <numbers>

namespace Render::Geom {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

constexpr float k_badge_outer_radius = 0.500F;
constexpr float k_badge_halo_inner_radius = 0.330F;
constexpr float k_badge_shadow_core_radius = 0.300F;
constexpr float k_badge_shadow_edge_radius = 0.470F;
constexpr float k_badge_contour_inner_radius = 0.360F;
constexpr float k_badge_contour_outer_radius = 0.404F;
constexpr float k_badge_rim_inner_radius = 0.300F;
constexpr float k_badge_rim_outer_radius = 0.382F;
constexpr float k_badge_face_radius = 0.318F;
constexpr int k_badge_segments = 44;

constexpr float k_badge_shadow_depth = -0.130F;
constexpr float k_badge_halo_depth = -0.100F;
constexpr float k_badge_contour_depth = -0.030F;
constexpr float k_badge_face_depth = 0.000F;
constexpr float k_badge_rim_crest_depth = 0.040F;
constexpr float k_badge_rim_lip_depth = -0.008F;

constexpr float k_glyph_fit_radius = 0.262F;
constexpr float k_glyph_face_depth = 0.088F;
constexpr float k_glyph_detail_depth = 0.100F;

constexpr GlyphBuilder::GlyphExtrusion k_glyph_extrusion{
    .glow_depth = 0.0F,
    .glow_width = 0.0F,
    .outline_depth = 0.052F,
    .outline_width = 0.046F,
    .back_depth = 0.056F,
    .face_depth = k_glyph_face_depth,
};

struct KindStyle {
  IndicatorKind kind;
  QVector3D color;
  bool has_glyph;
};

constexpr std::array<KindStyle, k_indicator_kind_count> k_kind_styles = {{
    {IndicatorKind::Idle, {0.62F, 0.64F, 0.68F}, false},
    {IndicatorKind::Move, {0.50F, 0.86F, 0.98F}, true},
    {IndicatorKind::Attack, {1.00F, 0.30F, 0.26F}, true},
    {IndicatorKind::Patrol, {0.98F, 0.90F, 0.56F}, true},
    {IndicatorKind::Guard, {0.34F, 0.56F, 1.00F}, true},
    {IndicatorKind::Hold, {1.00F, 0.60F, 0.16F}, true},
    {IndicatorKind::Construct, {0.92F, 0.72F, 0.40F}, true},
    {IndicatorKind::Repair, {0.30F, 0.78F, 0.72F}, true},
    {IndicatorKind::Dismantle, {0.78F, 0.46F, 0.30F}, true},
    {IndicatorKind::ChopWood, {0.66F, 0.90F, 0.26F}, true},
    {IndicatorKind::MineStone, {0.80F, 0.82F, 0.86F}, true},
    {IndicatorKind::MineIron, {0.56F, 0.66F, 0.92F}, true},
    {IndicatorKind::Deliver, {0.94F, 0.78F, 0.94F}, true},
    {IndicatorKind::Heal, {0.72F, 1.00F, 0.72F}, true},
    {IndicatorKind::Train, {0.80F, 0.58F, 1.00F}, true},
    {IndicatorKind::Blocked, {1.00F, 0.36F, 0.12F}, true},
}};

[[nodiscard]] auto style_for(IndicatorKind kind) noexcept -> const KindStyle& {
  auto const index = static_cast<std::size_t>(kind);
  if (index < k_kind_styles.size() && k_kind_styles[index].kind == kind) {
    return k_kind_styles[index];
  }
  return k_kind_styles.front();
}

void add_medallion(GlyphBuilder& builder) {
  builder.set_normal_style(GlyphNormal::Front);
  builder.reset_transform();

  builder.set_layer(GlyphLayer::Shadow);
  builder.set_depth(k_badge_shadow_depth);
  builder.disc({0.0F, 0.0F}, k_badge_shadow_core_radius, k_badge_segments);
  builder.gradient_ring(GlyphLayer::Shadow,
                        k_badge_shadow_core_radius,
                        k_badge_shadow_edge_radius,
                        k_badge_shadow_depth,
                        k_badge_shadow_core_radius / 0.5F,
                        1.30F,
                        k_badge_segments);

  builder.gradient_ring(GlyphLayer::Glow,
                        k_badge_halo_inner_radius,
                        k_badge_outer_radius,
                        k_badge_halo_depth,
                        0.0F,
                        1.0F,
                        k_badge_segments);

  builder.set_layer(GlyphLayer::Outline);
  builder.set_depth(k_badge_contour_depth);
  builder.ring({0.0F, 0.0F},
               k_badge_contour_inner_radius,
               k_badge_contour_outer_radius,
               k_badge_segments);

  builder.set_layer(GlyphLayer::Plaque);
  builder.set_depth(k_badge_face_depth);
  builder.disc({0.0F, 0.0F}, k_badge_face_radius, k_badge_segments);

  builder.set_layer(GlyphLayer::Rim);
  builder.revolved_band(k_badge_rim_inner_radius,
                        k_badge_rim_crest_depth,
                        k_badge_rim_outer_radius,
                        k_badge_rim_lip_depth,
                        k_badge_segments);

  builder.set_layer(GlyphLayer::Glyph);
  builder.set_depth(k_glyph_face_depth);
}

void add_sword(GlyphBuilder& builder, float rotation) {
  builder.set_transform({0.0F, -0.02F}, rotation);
  builder.bar({0.0F, -0.02F}, {0.0F, 0.26F}, 0.135F);
  builder.arrow_head({0.0F, 0.44F}, {0.0F, 1.0F}, 0.20F, 0.088F);
  builder.bar({-0.20F, -0.06F}, {0.20F, -0.06F}, 0.100F);
  builder.bar({0.0F, -0.04F}, {0.0F, -0.26F}, 0.092F);
  builder.disc({0.0F, -0.30F}, 0.072F, 12);
  builder.reset_transform();
}

void build_attack(GlyphBuilder& builder) {
  builder.begin_glyph();
  add_sword(builder, 0.62F);
  add_sword(builder, -0.62F);
  builder.end_glyph(k_glyph_extrusion);
}

void build_guard(GlyphBuilder& builder) {
  const std::array<QVector2D, 6> shield = {{
      {0.00F, 0.34F},
      {-0.27F, 0.21F},
      {-0.27F, -0.06F},
      {0.00F, -0.36F},
      {0.27F, -0.06F},
      {0.27F, 0.21F},
  }};
  builder.begin_glyph();
  builder.convex(shield);
  builder.end_glyph(k_glyph_extrusion);

  builder.set_layer(GlyphLayer::Outline);
  builder.set_depth(k_glyph_detail_depth);
  builder.ring({0.0F, 0.0F}, 0.070F, 0.130F, 16);
}

void build_hold(GlyphBuilder& builder) {
  const std::array<QVector2D, 3> pennant = {{
      {0.04F, 0.36F},
      {0.38F, 0.17F},
      {0.04F, -0.02F},
  }};
  builder.begin_glyph();
  builder.bar({-0.04F, 0.40F}, {-0.04F, -0.30F}, 0.115F);
  builder.convex(pennant);
  builder.bar({-0.30F, -0.34F}, {0.24F, -0.34F}, 0.110F);
  builder.end_glyph(k_glyph_extrusion);
}

void build_patrol(GlyphBuilder& builder) {
  constexpr float radius = 0.26F;
  constexpr float width = 0.115F;
  builder.begin_glyph();
  for (int side = 0; side < 2; ++side) {
    float const base = side == 0 ? 0.35F : 0.35F + k_pi;
    builder.ring(
        {0.0F, 0.0F}, radius - width * 0.5F, radius + width * 0.5F, 12, base, 2.05F);
    float const tip_angle = base + 2.05F;
    QVector2D const tip(std::cos(tip_angle) * radius, std::sin(tip_angle) * radius);
    QVector2D const tangent(-std::sin(tip_angle), std::cos(tip_angle));
    builder.arrow_head(tip + tangent * 0.19F, tangent, 0.21F, 0.15F);
  }
  builder.end_glyph(k_glyph_extrusion);
}

void build_move(GlyphBuilder& builder) {
  const std::array<QVector2D, 3> chevron = {{
      {-0.24F, -0.06F},
      {0.00F, 0.16F},
      {0.24F, -0.06F},
  }};
  const std::array<QVector2D, 3> chevron_low = {{
      {-0.24F, -0.26F},
      {0.00F, -0.04F},
      {0.24F, -0.26F},
  }};
  builder.begin_glyph();
  builder.polyline(chevron, 0.125F);
  builder.polyline(chevron_low, 0.125F);
  builder.end_glyph(k_glyph_extrusion);
}

void build_construct(GlyphBuilder& builder) {
  builder.begin_glyph();
  builder.set_transform({0.0F, 0.0F}, -0.35F);
  builder.rect({0.0F, 0.23F}, {0.24F, 0.125F});
  builder.rect({-0.15F, 0.34F}, {0.095F, 0.075F});
  builder.bar({0.0F, 0.20F}, {0.0F, -0.34F}, 0.105F);
  builder.reset_transform();
  builder.end_glyph(k_glyph_extrusion);
}

void build_repair(GlyphBuilder& builder) {
  builder.begin_glyph();
  builder.bar({-0.15F, -0.19F}, {0.13F, 0.13F}, 0.125F);
  builder.ring({0.19F, 0.20F}, 0.070F, 0.165F, 14, 1.15F, 4.55F);
  builder.ring({-0.21F, -0.24F}, 0.052F, 0.125F, 12, 1.15F + k_pi, 4.55F);
  builder.end_glyph(k_glyph_extrusion);
}

void build_dismantle(GlyphBuilder& builder) {
  builder.begin_glyph();
  builder.rect({-0.19F, -0.28F}, {0.15F, 0.090F});
  builder.rect({0.15F, -0.28F}, {0.15F, 0.090F});
  builder.rect({-0.28F, -0.07F}, {0.11F, 0.090F});
  builder.rect({-0.01F, -0.07F}, {0.14F, 0.090F});
  builder.rect({-0.18F, 0.14F}, {0.15F, 0.090F});
  builder.rect({0.21F, 0.24F}, {0.14F, 0.085F}, 0.62F);
  builder.end_glyph(k_glyph_extrusion);

  builder.set_layer(GlyphLayer::Accent);
  builder.set_depth(k_glyph_detail_depth);
  builder.disc({-0.02F, 0.32F}, 0.055F, 10);
  builder.disc({0.36F, 0.02F}, 0.042F, 10);
}

void build_chop_wood(GlyphBuilder& builder) {
  const std::array<QVector2D, 5> blade = {{
      {0.00F, 0.34F},
      {0.20F, 0.36F},
      {0.34F, 0.16F},
      {0.30F, -0.04F},
      {0.10F, 0.00F},
  }};
  builder.begin_glyph();
  builder.bar({-0.20F, -0.34F}, {0.08F, 0.26F}, 0.105F);
  builder.convex(blade);
  builder.end_glyph(k_glyph_extrusion);
}

void build_mine_stone(GlyphBuilder& builder) {
  const std::array<QVector2D, 5> head = {{
      {-0.28F, 0.10F},
      {-0.15F, 0.22F},
      {0.00F, 0.26F},
      {0.15F, 0.22F},
      {0.28F, 0.10F},
  }};
  builder.begin_glyph();

  builder.bar({0.0F, 0.36F}, {0.0F, -0.40F}, 0.115F);
  builder.polyline(head, 0.105F);
  builder.arrow_head({-0.44F, -0.06F}, {-0.62F, -0.79F}, 0.24F, 0.095F);
  builder.arrow_head({0.44F, -0.06F}, {0.62F, -0.79F}, 0.24F, 0.095F);
  builder.end_glyph(k_glyph_extrusion);
}

void build_mine_iron(GlyphBuilder& builder) {
  const std::array<QVector2D, 7> chunk = {{
      {-0.34F, -0.06F},
      {-0.20F, 0.20F},
      {0.05F, 0.30F},
      {0.28F, 0.20F},
      {0.34F, -0.06F},
      {0.17F, -0.29F},
      {-0.17F, -0.29F},
  }};
  builder.begin_glyph();
  builder.convex(chunk);
  builder.end_glyph(k_glyph_extrusion);

  builder.set_layer(GlyphLayer::Outline);
  builder.set_depth(k_glyph_detail_depth);
  builder.bar({-0.15F, 0.06F}, {0.02F, -0.14F}, 0.085F);
  builder.bar({0.10F, 0.16F}, {0.20F, 0.04F}, 0.070F);
}

void build_deliver(GlyphBuilder& builder) {
  builder.begin_glyph();
  builder.rect({0.0F, -0.22F}, {0.26F, 0.17F});
  builder.rect({0.0F, 0.00F}, {0.32F, 0.065F});
  builder.bar({0.0F, 0.42F}, {0.0F, 0.22F}, 0.115F);
  builder.arrow_head({0.0F, 0.11F}, {0.0F, -1.0F}, 0.16F, 0.155F);
  builder.end_glyph(k_glyph_extrusion);

  builder.set_layer(GlyphLayer::Outline);
  builder.set_depth(k_glyph_detail_depth);
  builder.bar({0.0F, -0.36F}, {0.0F, -0.10F}, 0.080F);
}

void build_heal(GlyphBuilder& builder) {
  builder.begin_glyph();
  builder.bar({0.0F, -0.28F}, {0.0F, 0.28F}, 0.155F);
  builder.bar({-0.28F, 0.0F}, {0.28F, 0.0F}, 0.155F);
  builder.end_glyph(k_glyph_extrusion);
}

void build_train(GlyphBuilder& builder) {
  const std::array<QVector2D, 3> upper = {{
      {-0.19F, 0.24F},
      {0.19F, 0.24F},
      {0.00F, 0.01F},
  }};
  const std::array<QVector2D, 3> lower = {{
      {-0.19F, -0.24F},
      {0.19F, -0.24F},
      {0.00F, -0.01F},
  }};
  builder.begin_glyph();
  builder.convex(upper);
  builder.convex(lower);
  builder.rect({0.0F, 0.30F}, {0.27F, 0.065F});
  builder.rect({0.0F, -0.30F}, {0.27F, 0.065F});
  builder.end_glyph(k_glyph_extrusion);

  builder.set_layer(GlyphLayer::Accent);
  builder.set_depth(k_glyph_detail_depth);
  builder.tri({-0.12F, -0.18F}, {0.12F, -0.18F}, {0.0F, -0.03F});
}

void build_blocked(GlyphBuilder& builder) {
  const std::array<QVector2D, 3> warning = {{
      {0.00F, 0.31F},
      {-0.30F, -0.24F},
      {0.30F, -0.24F},
  }};
  builder.begin_glyph();
  builder.convex(warning);
  builder.end_glyph(k_glyph_extrusion);

  builder.set_layer(GlyphLayer::Outline);
  builder.set_depth(k_glyph_detail_depth);
  builder.bar({0.0F, 0.15F}, {0.0F, -0.05F}, 0.098F);
  builder.disc({0.0F, -0.14F}, 0.062F, 10);
}

void build_idle(GlyphBuilder& builder) {
  builder.begin_glyph();
  builder.ring({0.0F, 0.0F}, 0.15F, 0.26F, 24);
  builder.disc({0.0F, 0.0F}, 0.07F, 14);
  builder.end_glyph(k_glyph_extrusion);
}

} // namespace

auto ModeIndicator::meshes() -> MeshCache& {
  static auto* storage = new MeshCache{};
  return *storage;
}

auto indicator_has_glyph(IndicatorKind kind) noexcept -> bool {
  return style_for(kind).has_glyph;
}

auto indicator_base_color(IndicatorKind kind) noexcept -> QVector3D {
  return style_for(kind).color;
}

auto indicator_color(IndicatorKind kind, IndicatorState state) noexcept -> QVector3D {
  QVector3D const base = indicator_base_color(kind);
  switch (state) {
  case IndicatorState::Active:
    return base;
  case IndicatorState::Queued:
    return base * 0.68F + QVector3D(0.20F, 0.21F, 0.24F);
  case IndicatorState::Unavailable:
    return base * 0.35F + QVector3D(1.0F, 0.30F, 0.22F) * 0.65F;
  case IndicatorState::Interrupted:
    return base * 0.40F + QVector3D(1.0F, 0.72F, 0.24F) * 0.60F;
  }
  return base;
}

auto indicator_state_alpha(IndicatorState state) noexcept -> float {
  switch (state) {
  case IndicatorState::Active:
    return k_indicator_alpha;
  case IndicatorState::Queued:
    return k_indicator_alpha * 0.78F;
  case IndicatorState::Unavailable:
  case IndicatorState::Interrupted:
    return k_indicator_alpha;
  }
  return k_indicator_alpha;
}

auto build_indicator_glyph(IndicatorKind kind) -> GlyphBuilder {
  GlyphBuilder builder;
  add_medallion(builder);
  std::size_t const glyph_start = builder.vertex_mark();

  switch (kind) {
  case IndicatorKind::Idle:
    build_idle(builder);
    break;
  case IndicatorKind::Move:
    build_move(builder);
    break;
  case IndicatorKind::Attack:
    build_attack(builder);
    break;
  case IndicatorKind::Patrol:
    build_patrol(builder);
    break;
  case IndicatorKind::Guard:
    build_guard(builder);
    break;
  case IndicatorKind::Hold:
    build_hold(builder);
    break;
  case IndicatorKind::Construct:
    build_construct(builder);
    break;
  case IndicatorKind::Repair:
    build_repair(builder);
    break;
  case IndicatorKind::Dismantle:
    build_dismantle(builder);
    break;
  case IndicatorKind::ChopWood:
    build_chop_wood(builder);
    break;
  case IndicatorKind::MineStone:
    build_mine_stone(builder);
    break;
  case IndicatorKind::MineIron:
    build_mine_iron(builder);
    break;
  case IndicatorKind::Deliver:
    build_deliver(builder);
    break;
  case IndicatorKind::Heal:
    build_heal(builder);
    break;
  case IndicatorKind::Train:
    build_train(builder);
    break;
  case IndicatorKind::Blocked:
    build_blocked(builder);
    break;
  }

  builder.fit_since(glyph_start, k_glyph_fit_radius);
  builder.normalize_extent(0.5F);
  return builder;
}

auto ModeIndicator::mesh_for(IndicatorKind kind) -> Render::GL::Mesh* {
  auto const index = static_cast<std::size_t>(kind);
  if (index >= meshes().size()) {
    return nullptr;
  }
  if (!meshes()[index]) {
    meshes()[index] = build_indicator_glyph(kind).build();
  }
  return meshes()[index].get();
}

void ModeIndicator::release_meshes() {
  for (auto& mesh : meshes()) {
    mesh.reset();
  }
}

} // namespace Render::Geom
