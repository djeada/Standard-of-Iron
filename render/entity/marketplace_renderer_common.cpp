#include "marketplace_renderer_common.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "../entity_appearance.h"
#include "civilian_actor.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"

namespace Render::GL {
namespace {

constexpr float k_cloth_far_metres = 95.0F;
constexpr int k_cloth_material = 3;
constexpr int k_produce_material = 4;
constexpr float k_tau = 2.0F * std::numbers::pi_v<float>;

auto unit_hash(std::uint32_t value) -> float {
  return static_cast<float>(ambient_hash(value)) * (1.0F / 4294967296.0F);
}

const QVector3D k_up{0.0F, 1.0F, 0.0F};

auto intact_goods() -> BuildingStateMask {
  return BuildingStateMask::Normal;
}

} // namespace

void add_market_paving(BuildingArchetypeDesc& desc, const MarketPaving& paving) {
  BuildingPartMaterial stone(desc, k_building_material_stone);
  const float span_x = paving.max_x - paving.min_x;
  const float span_z = paving.max_z - paving.min_z;
  if (span_x <= 0.0F || span_z <= 0.0F || paving.cell <= 0.0F) {
    return;
  }
  const int cols = std::max(1, static_cast<int>(std::lround(span_x / paving.cell)));
  const int rows = std::max(1, static_cast<int>(std::lround(span_z / paving.cell)));
  const float col_width = span_x / static_cast<float>(cols);
  const float row_depth = span_z / static_cast<float>(rows);
  const auto seed = static_cast<std::uint32_t>(paving.seed);

  for (int r = 0; r < rows; ++r) {
    const float z0 = paving.min_z + row_depth * static_cast<float>(r);
    const float offset = (r % 2 == 0) ? 0.0F : col_width * 0.5F;
    for (int k = -1; k < cols; ++k) {
      float x0 = paving.min_x + offset + col_width * static_cast<float>(k);
      float x1 = x0 + col_width;
      x0 = std::max(x0, paving.min_x);
      x1 = std::min(x1, paving.max_x);
      if (x1 - x0 < paving.gap * 4.0F) {
        continue;
      }
      const std::uint32_t h =
          ambient_hash(seed * 7919U + static_cast<std::uint32_t>(r) * 131U +
                       static_cast<std::uint32_t>(k + 1) * 17U);
      const float rise_t = unit_hash(h);
      const float tint = unit_hash(h ^ 0x9E3779B9U);
      const float rise = paving.min_rise + (paving.max_rise - paving.min_rise) * rise_t;
      const QVector3D tone = paving.tones[h % 3U] * (0.94F + 0.10F * tint);
      desc.add_box(QVector3D((x0 + x1) * 0.5F,
                             paving.base_y + rise * 0.5F,
                             z0 + row_depth * 0.5F),
                   QVector3D((x1 - x0) * 0.5F - paving.gap * 0.5F,
                             rise * 0.5F,
                             row_depth * 0.5F - paving.gap * 0.5F),
                   tone);
    }
  }
}

void add_market_fruit(BuildingArchetypeDesc& desc,
                      const QVector3D& centre,
                      float radius,
                      const QVector3D& colour) {
  desc.add_cone(
      centre, centre + k_up * (radius * 0.95F), radius, colour, intact_goods());
  desc.add_cone(
      centre, centre - k_up * (radius * 0.85F), radius, colour * 0.84F, intact_goods());
}

void add_market_amphora(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        const QVector3D& clay,
                        const QVector3D& band,
                        float scale,
                        bool slender) {
  BuildingPartMaterial ceramic(desc, k_building_material_ceramic);
  auto p = [&](float x, float y, float z) {
    return base + QVector3D(x, y, z) * scale;
  };
  const QVector3D dark = clay * 0.62F;
  const QVector3D neck = clay * 0.92F;
  const BuildingStateMask mask = intact_goods();

  if (!slender) {
    desc.add_cylinder(
        p(0.0F, 0.0F, 0.0F), p(0.0F, 0.028F, 0.0F), 0.012F * scale, dark, mask);
    desc.add_cone(
        p(0.0F, 0.116F, 0.0F), p(0.0F, 0.016F, 0.0F), 0.059F * scale, clay, mask);
    desc.add_cylinder(
        p(0.0F, 0.112F, 0.0F), p(0.0F, 0.160F, 0.0F), 0.063F * scale, clay, mask);
    desc.add_cylinder(
        p(0.0F, 0.134F, 0.0F), p(0.0F, 0.145F, 0.0F), 0.0655F * scale, band, mask);
    desc.add_cone(
        p(0.0F, 0.157F, 0.0F), p(0.0F, 0.234F, 0.0F), 0.064F * scale, clay, mask);
    desc.add_cylinder(
        p(0.0F, 0.198F, 0.0F), p(0.0F, 0.290F, 0.0F), 0.020F * scale, neck, mask);
    desc.add_cylinder(
        p(0.0F, 0.279F, 0.0F), p(0.0F, 0.298F, 0.0F), 0.028F * scale, dark, mask);
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_cylinder(p(side * 0.016F, 0.272F, 0.0F),
                        p(side * 0.047F, 0.268F, 0.0F),
                        0.0075F * scale,
                        neck,
                        mask);
      desc.add_cylinder(p(side * 0.047F, 0.268F, 0.0F),
                        p(side * 0.052F, 0.196F, 0.0F),
                        0.0075F * scale,
                        neck,
                        mask);
    }
    return;
  }

  desc.add_cone(p(0.0F, 0.062F, 0.0F), p(0.0F, 0.0F, 0.0F), 0.032F * scale, dark, mask);
  desc.add_cone(
      p(0.0F, 0.090F, 0.0F), p(0.0F, 0.030F, 0.0F), 0.056F * scale, clay, mask);
  desc.add_cylinder(
      p(0.0F, 0.088F, 0.0F), p(0.0F, 0.226F, 0.0F), 0.057F * scale, clay, mask);
  desc.add_cylinder(
      p(0.0F, 0.186F, 0.0F), p(0.0F, 0.197F, 0.0F), 0.0595F * scale, band, mask);
  desc.add_cylinder(
      p(0.0F, 0.118F, 0.0F), p(0.0F, 0.124F, 0.0F), 0.0592F * scale, band * 0.8F, mask);
  desc.add_cone(
      p(0.0F, 0.224F, 0.0F), p(0.0F, 0.272F, 0.0F), 0.058F * scale, clay, mask);
  desc.add_cylinder(
      p(0.0F, 0.254F, 0.0F), p(0.0F, 0.276F, 0.0F), 0.024F * scale, dark, mask);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(p(side * 0.040F, 0.238F, 0.0F),
                      p(side * 0.060F, 0.214F, 0.0F),
                      0.0080F * scale,
                      neck,
                      mask);
  }
}

void add_market_basket(BuildingArchetypeDesc& desc,
                       const QVector3D& base,
                       float radius,
                       const MarketGoodsPalette& palette,
                       const QVector3D& goods_a,
                       const QVector3D& goods_b) {
  const float h = radius * 0.85F;
  const BuildingStateMask mask = intact_goods();
  {
    BuildingPartMaterial wicker(desc, k_building_material_wood);
    desc.add_cylinder(
        base, base + k_up * (h * 0.42F), radius * 0.86F, palette.wicker, mask);
    desc.add_cylinder(base + k_up * (h * 0.40F),
                      base + k_up * h,
                      radius * 0.95F,
                      palette.wicker * 1.06F,
                      mask);
    desc.add_cylinder(base + k_up * (h * 0.16F),
                      base + k_up * (h * 0.16F + 0.008F),
                      radius * 0.86F + 0.003F,
                      palette.wicker_dark,
                      mask);
    desc.add_cylinder(base + k_up * (h * 0.64F),
                      base + k_up * (h * 0.64F + 0.008F),
                      radius * 0.95F + 0.003F,
                      palette.wicker_dark,
                      mask);
    desc.add_cylinder(base + k_up * (h - 0.010F),
                      base + k_up * (h + 0.006F),
                      radius * 0.95F + 0.009F,
                      palette.wicker_dark,
                      mask);
  }
  BuildingPartMaterial produce(desc, k_building_material_leather);
  desc.add_cone(base + k_up * (h - 0.004F),
                base + k_up * (h + radius * 0.34F),
                radius * 0.92F,
                goods_a * 0.80F,
                mask);
  const float fruit = radius * 0.27F;
  const float seed_angle = (base.x() * 7.3F + base.z() * 3.1F);
  for (int i = 0; i < 5; ++i) {
    const float angle = seed_angle + static_cast<float>(i) * (k_tau / 5.0F);
    const QVector3D centre = base + QVector3D(std::cos(angle) * radius * 0.54F,
                                              h + fruit * 0.85F,
                                              std::sin(angle) * radius * 0.54F);
    const QVector3D colour = ((i % 2) == 0 ? goods_a : goods_b) *
                             (0.92F + 0.04F * static_cast<float>(i % 3));
    add_market_fruit(desc, centre, fruit, colour);
  }
  add_market_fruit(
      desc, base + k_up * (h + radius * 0.34F + fruit * 0.35F), fruit, goods_b);
}

void add_market_sack(BuildingArchetypeDesc& desc,
                     const QVector3D& base,
                     float radius,
                     float height,
                     const MarketGoodsPalette& palette,
                     const QVector3D& content,
                     bool open) {
  const BuildingStateMask mask = intact_goods();
  const QVector3D dark = palette.burlap * 0.78F;
  BuildingPartMaterial cloth(desc, k_building_material_cloth);
  desc.add_cylinder(base, base + k_up * (height * 0.16F), radius * 1.07F, dark, mask);
  desc.add_cylinder(base + k_up * (height * 0.14F),
                    base + k_up * (height * 0.72F),
                    radius,
                    palette.burlap,
                    mask);
  if (open) {
    desc.add_cylinder(base + k_up * (height * 0.66F),
                      base + k_up * (height * 0.80F),
                      radius * 1.08F,
                      dark,
                      mask);
    BuildingPartMaterial goods(desc, k_building_material_leather);
    desc.add_cone(base + k_up * (height * 0.78F),
                  base + k_up * (height * 0.78F + radius * 0.62F),
                  radius * 0.98F,
                  content,
                  mask);
    return;
  }
  desc.add_cone(base + k_up * (height * 0.70F),
                base + k_up * (height * 1.02F),
                radius,
                palette.burlap,
                mask);
  desc.add_cylinder(base + k_up * (height * 0.88F),
                    base + k_up * (height * 0.94F),
                    radius * 0.26F,
                    palette.rope,
                    mask);
  desc.add_cone(base + k_up * (height * 0.93F),
                base + k_up * (height * 1.12F),
                radius * 0.30F,
                dark,
                mask);
}

void add_market_crate(BuildingArchetypeDesc& desc,
                      const QVector3D& center,
                      const QVector3D& half,
                      const MarketGoodsPalette& palette,
                      bool lidded) {
  BuildingPartMaterial wood(desc, k_building_material_wood);
  const BuildingStateMask mask = BuildingStateMask::Normal | BuildingStateMask::Damaged;
  constexpr float k_proud = 0.006F;
  constexpr float k_post = 0.022F;
  desc.add_box(center, half, palette.timber * 0.82F, mask);
  for (float const sy : {-0.48F, 0.48F}) {
    for (float const s : {-1.0F, 1.0F}) {
      desc.add_box(center +
                       QVector3D(s * (half.x() + k_proud * 0.5F), sy * half.y(), 0.0F),
                   QVector3D(k_proud * 0.5F, half.y() * 0.28F, half.z() - k_post),
                   palette.timber,
                   mask);
      desc.add_box(center +
                       QVector3D(0.0F, sy * half.y(), s * (half.z() + k_proud * 0.5F)),
                   QVector3D(half.x() - k_post, half.y() * 0.28F, k_proud * 0.5F),
                   palette.timber * 0.95F,
                   mask);
    }
  }
  for (float const sx : {-1.0F, 1.0F}) {
    for (float const sz : {-1.0F, 1.0F}) {
      desc.add_box(center + QVector3D(sx * (half.x() - 0.005F),
                                      0.002F,
                                      sz * (half.z() - 0.005F)),
                   QVector3D(0.011F, half.y() + 0.002F, 0.011F),
                   palette.timber_dark,
                   mask);
    }
  }
  if (lidded) {
    desc.add_box(center + QVector3D(0.0F, half.y() + 0.004F, 0.0F),
                 QVector3D(half.x() - 0.004F, 0.004F, half.z() - 0.004F),
                 palette.timber * 1.05F,
                 mask);
  }
}

void add_market_scale(BuildingArchetypeDesc& desc,
                      const QVector3D& base,
                      const MarketGoodsPalette& palette) {
  BuildingPartMaterial metal(desc, k_building_material_metal);
  const BuildingStateMask mask = intact_goods();
  desc.add_cylinder(base, base + k_up * 0.012F, 0.034F, palette.metal * 0.78F, mask);
  desc.add_cylinder(base, base + k_up * 0.232F, 0.0075F, palette.metal, mask);
  desc.add_cone(
      base + k_up * 0.230F, base + k_up * 0.256F, 0.012F, palette.metal, mask);
  const QVector3D end_a = base + QVector3D(0.0F, 0.222F, -0.118F);
  const QVector3D end_b = base + QVector3D(0.0F, 0.206F, 0.118F);
  desc.add_cylinder(end_a, end_b, 0.0048F, palette.metal * 1.08F, mask);
  for (const QVector3D& end : {end_a, end_b}) {
    const float pan_y = end.y() - 0.115F;
    for (float const dx : {-0.030F, 0.030F}) {
      desc.add_cylinder(end,
                        QVector3D(end.x() + dx, pan_y + 0.008F, end.z()),
                        0.0018F,
                        palette.metal * 0.7F,
                        mask);
    }
    desc.add_cylinder(QVector3D(end.x(), pan_y, end.z()),
                      QVector3D(end.x(), pan_y + 0.008F, end.z()),
                      0.044F,
                      palette.metal,
                      mask);
  }
}

void add_market_stall(BuildingArchetypeDesc& desc,
                      const MarketStall& s,
                      const MarketGoodsPalette& palette) {
  const float g = s.ground_y;
  const float top = s.counter_top;
  const float zb = s.cz + s.side * 0.22F;
  const float zf = s.cz - s.side * 0.22F;
  const float front = s.cz - s.side * 0.14F;
  const BuildingStateMask counter_mask =
      BuildingStateMask::Normal | BuildingStateMask::Damaged;

  BuildingPartMaterial wood(desc, k_building_material_wood);
  desc.add_box(QVector3D(s.cx, top - 0.012F, s.cz),
               QVector3D(0.27F, 0.012F, 0.14F),
               s.board,
               counter_mask);
  desc.add_box(QVector3D(s.cx, top - 0.005F, front - s.side * 0.004F),
               QVector3D(0.275F, 0.009F, 0.006F),
               palette.timber_dark,
               counter_mask);
  desc.add_box(QVector3D(s.cx, top - 0.10F, front + s.side * 0.004F),
               QVector3D(0.255F, 0.07F, 0.006F),
               s.apron,
               counter_mask);
  {
    BuildingPartMaterial paint(desc, k_building_material_cloth);
    for (float const px : {-0.125F, 0.125F}) {
      desc.add_box(QVector3D(s.cx + px, top - 0.10F, front - s.side * 0.004F),
                   QVector3D(0.105F, 0.052F, 0.002F),
                   s.apron_panel,
                   BuildingStateMask::Normal);
    }
  }
  desc.add_box(QVector3D(s.cx, g + 0.100F, s.cz + s.side * 0.02F),
               QVector3D(0.20F, 0.007F, 0.085F),
               palette.timber,
               counter_mask);

  for (float const lx : {-0.22F, 0.22F}) {
    desc.add_cylinder(QVector3D(s.cx + lx, g, s.cz - 0.10F),
                      QVector3D(s.cx + lx, top - 0.024F, s.cz + 0.10F),
                      0.012F,
                      palette.timber_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(s.cx + lx, g, s.cz + 0.10F),
                      QVector3D(s.cx + lx, top - 0.024F, s.cz - 0.10F),
                      0.012F,
                      palette.timber_dark,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(QVector3D(s.cx - 0.23F, g + 0.090F, s.cz),
                    QVector3D(s.cx + 0.23F, g + 0.090F, s.cz),
                    0.010F,
                    palette.timber_dark,
                    k_building_state_mask_intact);

  for (float const lx : {-0.26F, 0.26F}) {
    desc.add_cylinder(QVector3D(s.cx + lx, g, zb),
                      QVector3D(s.cx + lx, s.back_post_top, zb),
                      0.017F,
                      palette.timber_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(s.cx + lx, g, zf),
                      QVector3D(s.cx + lx, s.front_post_top, zf),
                      0.016F,
                      palette.timber_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(s.cx + lx, g, zb),
                      QVector3D(s.cx + lx, g + 0.020F, zb),
                      0.028F,
                      palette.clay_dark,
                      BuildingStateMask::All);
    desc.add_cylinder(QVector3D(s.cx + lx, g, zf),
                      QVector3D(s.cx + lx, g + 0.020F, zf),
                      0.027F,
                      palette.clay_dark,
                      BuildingStateMask::All);
    desc.add_cylinder(QVector3D(s.cx + lx, s.back_post_top - 0.020F, zb),
                      QVector3D(s.cx + lx, s.front_post_top - 0.020F, zf),
                      0.010F,
                      palette.timber,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(s.cx + lx, s.back_post_top - 0.15F, zb),
                      QVector3D(s.cx + lx * 0.60F, s.back_post_top - 0.012F, zb),
                      0.0075F,
                      palette.timber_dark,
                      k_building_state_mask_intact);
    desc.add_cylinder(QVector3D(s.cx + lx, s.front_post_top - 0.13F, zf),
                      QVector3D(s.cx + lx * 0.60F, s.front_post_top - 0.012F, zf),
                      0.0075F,
                      palette.timber_dark,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(QVector3D(s.cx - 0.29F, s.back_post_top - 0.012F, zb),
                    QVector3D(s.cx + 0.29F, s.back_post_top - 0.012F, zb),
                    0.013F,
                    palette.timber,
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(s.cx - 0.29F, s.front_post_top - 0.012F, zf),
                    QVector3D(s.cx + 0.29F, s.front_post_top - 0.012F, zf),
                    0.013F,
                    palette.timber,
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(s.cx - 0.24F, s.front_post_top - 0.024F, zf),
                    QVector3D(s.cx + 0.24F, s.front_post_top - 0.024F, zf),
                    0.0035F,
                    palette.rope,
                    k_building_state_mask_intact);
}

void submit_market_awnings(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const MarketAwning> awnings,
                           std::span<const MarketHanging> hangings) {
  if ((awnings.empty() && hangings.empty()) || ctx.template_prewarm) {
    return;
  }
  if (ctx.distance_sq > k_cloth_far_metres * k_cloth_far_metres) {
    return;
  }
  const BuildingState state = resolve_building_state(ctx);
  if (state == BuildingState::Destroyed) {
    return;
  }
  const bool tattered = state == BuildingState::Damaged;
  const float time = ctx.animation_time;
  const auto owner =
      ctx.entity != nullptr ? static_cast<std::uint32_t>(ctx.entity->get_id()) : 0U;
  const float gust_phase = unit_hash(owner * 131U + 7U) * k_tau;
  const float gust = 0.55F + 0.45F * std::sin(time * 0.37F + gust_phase) *
                                 std::sin(time * 0.11F + gust_phase * 0.5F);
  Mesh* const cube = get_unit_cube();
  constexpr int k_segments = 6;

  std::uint32_t index = 0;
  for (const MarketAwning& awning : awnings) {
    ++index;
    const float phase = unit_hash(owner * 977U + index) * k_tau;
    const QVector3D run = awning.front - awning.back;
    const float length = run.length();
    if (length < 1.0e-4F) {
      continue;
    }
    const QVector3D along = run / length;
    const QVector3D across = awning.width_axis.normalized();
    QVector3D normal = QVector3D::crossProduct(across, along);
    if (normal.y() < 0.0F) {
      normal = -normal;
    }
    const int stripes = std::max(1, awning.stripes);
    const float stripe_w = awning.half_width * 2.0F / static_cast<float>(stripes);
    const float seg_len = length / static_cast<float>(k_segments);
    const float width_sag = 0.045F * awning.half_width * (0.8F + 0.4F * gust);
    for (int s = 0; s < stripes; ++s) {
      if (tattered && (s + static_cast<int>(index)) % 3 == 0) {
        continue;
      }
      const float offset =
          -awning.half_width + stripe_w * (static_cast<float>(s) + 0.5F);
      const float across_t = offset / std::max(awning.half_width, 1.0e-4F);
      const float sag = width_sag * (1.0F - across_t * across_t);
      const float sag_slope = -2.0F * width_sag * offset /
                              std::max(awning.half_width * awning.half_width, 1.0e-6F);
      const QVector3D x_axis = (across - normal * sag_slope).normalized();
      const QVector3D& color = (s % 2 == 0) ? awning.stripe_a : awning.stripe_b;
      const float weather = 0.93F + 0.07F * unit_hash(owner * 389U + index * 31U +
                                                      static_cast<std::uint32_t>(s));
      QVector3D previous = awning.back + across * offset - normal * sag;
      for (int k = 1; k <= k_segments; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(k_segments);
        const float billow =
            std::sin(t * std::numbers::pi_v<float>) * 0.035F * length * (0.6F + gust);
        const float ripple_phase = time * 2.3F - t * 4.2F + phase + offset * 3.0F;
        const float ripple = std::sin(ripple_phase) * 0.012F * t * (0.4F + gust);
        const QVector3D point = awning.back + across * offset + along * (t * length) -
                                normal * (billow - ripple + sag * (1.0F - 0.35F * t));
        const QVector3D mid = (previous + point) * 0.5F;
        const QVector3D dir = point - previous;
        QMatrix4x4 local;
        local.translate(mid);
        QVector3D z_axis = dir.normalized();
        QVector3D y_axis = QVector3D::crossProduct(z_axis, x_axis).normalized();
        QMatrix4x4 basis(x_axis.x(),
                         y_axis.x(),
                         z_axis.x(),
                         0.0F,
                         x_axis.y(),
                         y_axis.y(),
                         z_axis.y(),
                         0.0F,
                         x_axis.z(),
                         y_axis.z(),
                         z_axis.z(),
                         0.0F,
                         0.0F,
                         0.0F,
                         0.0F,
                         1.0F);
        local *= basis;
        local.scale(stripe_w * 0.51F, 0.006F, std::max(dir.length(), seg_len) * 0.52F);
        const float fold =
            0.94F + 0.06F * std::sin(ripple_phase + 1.2F) - 0.05F * (billow / length);
        out.mesh(cube,
                 ctx.model * local,
                 color * (fold * weather),
                 nullptr,
                 1.0F,
                 k_cloth_material);
        previous = point;
      }
      const float flap = std::sin(time * 3.1F + phase + static_cast<float>(s) * 1.3F) *
                         (0.20F + 0.25F * gust);
      const QVector3D hang_dir =
          (QVector3D(0.0F, -1.0F, 0.0F) + along * flap).normalized();
      const QVector3D z_axis = QVector3D::crossProduct(across, hang_dir).normalized();
      QMatrix4x4 const drop_basis(across.x(),
                                  hang_dir.x(),
                                  z_axis.x(),
                                  0.0F,
                                  across.y(),
                                  hang_dir.y(),
                                  z_axis.y(),
                                  0.0F,
                                  across.z(),
                                  hang_dir.z(),
                                  z_axis.z(),
                                  0.0F,
                                  0.0F,
                                  0.0F,
                                  0.0F,
                                  1.0F);
      const QVector3D& valance = (s % 2 == 0) ? awning.stripe_b : awning.stripe_a;
      const QVector3D tip = previous + hang_dir * 0.050F;
      QMatrix4x4 drop;
      drop.translate((previous + tip) * 0.5F);
      drop *= drop_basis;
      drop.scale(stripe_w * 0.49F, 0.025F, 0.005F);
      out.mesh(
          cube, ctx.model * drop, valance * weather, nullptr, 1.0F, k_cloth_material);

      const QVector3D point_tip = tip + hang_dir * 0.022F;
      QMatrix4x4 scallop;
      scallop.translate((tip + point_tip) * 0.5F);
      scallop *= drop_basis;
      scallop.rotate(45.0F, 0.0F, 0.0F, 1.0F);
      scallop.scale(stripe_w * 0.26F, stripe_w * 0.26F, 0.0045F);
      out.mesh(cube,
               ctx.model * scallop,
               valance * (weather * 0.96F),
               nullptr,
               1.0F,
               k_cloth_material);
    }
  }

  Mesh* const cylinder = get_unit_cylinder(6);
  Mesh* const sphere = get_unit_sphere(6, 8);
  for (const MarketHanging& hanging : hangings) {
    ++index;
    const float phase = unit_hash(owner * 613U + index) * k_tau;
    const float swing = std::sin(time * 1.7F + phase) * 0.16F * (0.35F + gust);
    const float twist = std::cos(time * 1.3F + phase * 1.7F) * 0.08F * (0.35F + gust);
    const QVector3D dir = QVector3D(swing, -1.0F, twist).normalized();
    const QVector3D end = hanging.pivot + dir * hanging.length;
    out.mesh(cylinder,
             ctx.model * Render::Geom::cylinder_between(hanging.pivot, end, 0.004F),
             QVector3D(0.42F, 0.34F, 0.22F),
             nullptr,
             1.0F,
             k_cloth_material);
    for (int b = 0; b < std::max(1, hanging.beads); ++b) {
      const float t = 1.0F - static_cast<float>(b) * 0.28F;
      QMatrix4x4 bead;
      bead.translate(hanging.pivot + dir * (hanging.length * t));
      bead.scale(hanging.radius * (1.0F - 0.12F * static_cast<float>(b)));
      out.mesh(sphere,
               ctx.model * bead,
               hanging.color * (1.0F - 0.06F * static_cast<float>(b)),
               nullptr,
               1.0F,
               k_produce_material);
    }
  }
}

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "marketplace",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        const BuildingState state = resolve_building_state(ctx);
        if (config.palette_slots != nullptr) {
          const QVector3D team = Render::entity_color(*ctx.entity);
          const auto palette_slots = config.palette_slots(team);
          submit_building_instance(out, ctx, config.archetype(state), palette_slots);
        } else {
          submit_building_instance(out, ctx, config.archetype(state));
        }
        submit_building_torches(ctx, out, config.torches);
        submit_market_awnings(ctx, out, config.awnings, config.hangings);
        submit_ambient_people(ctx,
                              out,
                              config.nation_slug == "carthage",
                              config.people,
                              config.walk_surfaces);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
