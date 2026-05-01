

#include "render/equipment/weapons/bow_renderer.h"
#include "render/equipment/weapons/quiver_renderer.h"
#include "render/equipment/weapons/roman_scutum.h"
#include "render/equipment/weapons/shield_carthage.h"
#include "render/equipment/weapons/shield_renderer.h"
#include "render/equipment/weapons/shield_roman.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"

#include "render/entity/registry.h"
#include "render/entity/renderer_constants.h"
#include "render/equipment/equipment_submit.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/skeleton.h"
#include "render/palette.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <gtest/gtest.h>
#include <limits>
#include <utility>

namespace {

using Render::GL::BodyFrames;
using Render::GL::BowRenderConfig;
using Render::GL::BowRenderer;
using Render::GL::CarthageShieldConfig;
using Render::GL::CarthageShieldRenderer;
using Render::GL::DrawContext;
using Render::GL::EquipmentBatch;
using Render::GL::HumanoidAnimationContext;
using Render::GL::HumanoidPalette;
using Render::GL::ISubmitter;
using Render::GL::QuiverRenderConfig;
using Render::GL::QuiverRenderer;
using Render::GL::RomanScutumConfig;
using Render::GL::RomanScutumRenderer;
using Render::GL::RomanShieldConfig;
using Render::GL::RomanShieldRenderer;
using Render::GL::ShieldRenderConfig;
using Render::GL::ShieldRenderer;
using Render::GL::SpearRenderConfig;
using Render::GL::SpearRenderer;
using Render::GL::SwordRenderConfig;
using Render::GL::SwordRenderer;

auto make_frames() -> BodyFrames {
  BodyFrames f{};
  f.waist.origin = {0.00F, 0.72F, 0.00F};
  f.waist.right = {1.0F, 0.0F, 0.0F};
  f.waist.up = {0.0F, 1.0F, 0.0F};
  f.waist.forward = {0.0F, 0.0F, 1.0F};
  f.waist.radius = 0.28F;
  f.waist.depth = 0.22F;
  f.hand_r.origin = {0.30F, 1.10F, 0.20F};
  f.hand_r.right = {1.0F, 0.0F, 0.0F};
  f.hand_r.up = {0.0F, 1.0F, 0.0F};
  f.hand_r.forward = {0.0F, 0.0F, 1.0F};
  f.hand_l.origin = {-0.30F, 1.10F, 0.20F};
  f.hand_l.right = {-1.0F, 0.0F, 0.0F};
  f.hand_l.up = {0.0F, 1.0F, 0.0F};
  f.hand_l.forward = {0.0F, 0.0F, 1.0F};
  return f;
}

auto make_anim() -> HumanoidAnimationContext {
  HumanoidAnimationContext anim{};
  anim.inputs.time = 0.0F;
  anim.inputs.is_attacking = false;
  anim.inputs.is_melee = false;
  return anim;
}

auto make_palette() -> HumanoidPalette {
  HumanoidPalette p;
  return p;
}

auto make_ctx() -> DrawContext {
  DrawContext ctx{};
  ctx.model.setToIdentity();
  return ctx;
}

class CountingSubmitter : public ISubmitter {
public:
  void mesh(Render::GL::Mesh *, const QMatrix4x4 &, const QVector3D &,
            Render::GL::Texture * = nullptr, float = 1.0F, int = 0) override {
    ++draw_count;
  }

  void cylinder(const QVector3D &, const QVector3D &, float, const QVector3D &,
                float = 1.0F) override {
    ++draw_count;
  }

  void selection_ring(const QMatrix4x4 &, float, float,
                      const QVector3D &) override {}
  void grid(const QMatrix4x4 &, const QVector3D &, float, float,
            float) override {}
  void selection_smoke(const QMatrix4x4 &, const QVector3D &, float) override {}
  void healing_beam(const QVector3D &, const QVector3D &, const QVector3D &,
                    float, float, float, float) override {}
  void healer_aura(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void combat_dust(const QVector3D &, const QVector3D &, float, float,
                   float) override {}
  void stone_impact(const QVector3D &, const QVector3D &, float, float,
                    float) override {}
  void mode_indicator(const QMatrix4x4 &, int, const QVector3D &,
                      float = 1.0F) override {}

  int draw_count{0};
};

struct AABB {
  QVector3D mn{std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity(),
               std::numeric_limits<float>::infinity()};
  QVector3D mx{-std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity(),
               -std::numeric_limits<float>::infinity()};

  void include(const QVector3D &p) {
    mn.setX(std::min(mn.x(), p.x()));
    mn.setY(std::min(mn.y(), p.y()));
    mn.setZ(std::min(mn.z(), p.z()));
    mx.setX(std::max(mx.x(), p.x()));
    mx.setY(std::max(mx.y(), p.y()));
    mx.setZ(std::max(mx.z(), p.z()));
  }
};

inline int draw_count_of(const EquipmentBatch &batch) {
  CountingSubmitter submitter;
  submit_equipment_batch(batch, submitter);
  return submitter.draw_count;
}

auto hash_batch(const EquipmentBatch &b) -> std::size_t {
  std::size_t h = 0;
  auto mix = [&h](std::size_t v) {
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6U) + (h >> 2U);
  };
  auto mix_f = [&mix](float v) {
    std::uint32_t u{};
    std::memcpy(&u, &v, sizeof(u));
    mix(u);
  };
  auto mix_v = [&mix_f](const QVector3D &v) {
    mix_f(v.x());
    mix_f(v.y());
    mix_f(v.z());
  };
  auto mix_m = [&mix_f](const QMatrix4x4 &m) {
    for (int i = 0; i < 16; ++i) {
      mix_f(m.constData()[i]);
    }
  };
  mix(b.meshes.size());
  for (const auto &p : b.meshes) {
    mix(reinterpret_cast<std::uintptr_t>(p.mesh));
    mix_m(p.model);
    mix_v(p.color);
    mix_f(p.alpha);
    mix(static_cast<std::size_t>(p.material_id));
  }
  mix(b.cylinders.size());
  for (const auto &c : b.cylinders) {
    mix_v(c.start);
    mix_v(c.end);
    mix_f(c.radius);
    mix_v(c.color);
    mix_f(c.alpha);
  }
  mix(b.archetypes.size());
  for (const auto &a : b.archetypes) {
    mix(reinterpret_cast<std::uintptr_t>(a.archetype));
    mix_m(a.world);
    mix(static_cast<std::size_t>(a.palette_count));
    for (std::size_t i = 0; i < a.palette_count; ++i) {
      mix_v(a.palette[i]);
    }
    mix_f(a.alpha_multiplier);
    mix(static_cast<std::size_t>(a.lod));
  }
  return h;
}

auto archetype_local_aabb(const Render::GL::RenderArchetype &archetype)
    -> AABB {
  AABB box;
  const auto &slice = archetype.lods[0];
  for (const auto &draw : slice.draws) {
    if (draw.mesh == nullptr) {
      continue;
    }
    for (const auto &v : draw.mesh->get_vertices()) {
      QVector3D const p{v.position[0], v.position[1], v.position[2]};
      box.include(draw.local_model.map(p));
    }
  }
  return box;
}

auto archetype_world_aabb(const Render::GL::EquipmentArchetypePrim &prim)
    -> AABB {
  AABB box;
  if (prim.archetype == nullptr) {
    return box;
  }

  const auto &slice = prim.archetype->lods[0];
  for (const auto &draw : slice.draws) {
    if (draw.mesh == nullptr) {
      continue;
    }
    QMatrix4x4 const model = prim.world * draw.local_model;
    for (const auto &v : draw.mesh->get_vertices()) {
      QVector3D const p{v.position[0], v.position[1], v.position[2]};
      box.include(model.map(p));
    }
  }
  return box;
}

auto project_aabb_on_axis(const AABB &box, const QVector3D &origin,
                          const QVector3D &axis) -> std::pair<float, float> {
  std::pair<float, float> range{std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity()};
  for (int xi = 0; xi < 2; ++xi) {
    for (int yi = 0; yi < 2; ++yi) {
      for (int zi = 0; zi < 2; ++zi) {
        QVector3D const corner{xi == 0 ? box.mn.x() : box.mx.x(),
                               yi == 0 ? box.mn.y() : box.mx.y(),
                               zi == 0 ? box.mn.z() : box.mx.z()};
        float const projection = QVector3D::dotProduct(corner - origin, axis);
        range.first = std::min(range.first, projection);
        range.second = std::max(range.second, projection);
      }
    }
  }
  return range;
}

template <class Renderer, class Cfg>
void run_stateless_battery(Renderer &renderer, const Cfg &cfg_a,
                           const Cfg &cfg_b) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch a;
  EquipmentBatch b;
  Renderer::submit(cfg_a, ctx, frames, palette, anim, a);
  Renderer::submit(cfg_b, ctx, frames, palette, anim, b);
  EXPECT_GT(draw_count_of(a), 0);
  EXPECT_GT(draw_count_of(b), 0);
  EXPECT_NE(hash_batch(a), hash_batch(b))
      << "two configs should yield distinct draw batches";

  EquipmentBatch a2;
  Renderer::submit(cfg_a, ctx, frames, palette, anim, a2);
  EXPECT_EQ(hash_batch(a), hash_batch(a2))
      << "submit() must not retain any per-call state";

  Renderer legacy{cfg_a};
  EquipmentBatch legacy_batch;
  legacy.render(ctx, frames, palette, anim, legacy_batch);
  EXPECT_EQ(hash_batch(a), hash_batch(legacy_batch))
      << "legacy render() must forward to submit() without divergence";
}

} // namespace

TEST(StatelessWeaponRenderers, SwordSubmitIsStateless) {
  SwordRenderer renderer{};
  SwordRenderConfig a;
  a.metal_color = {0.7F, 0.7F, 0.8F};
  a.sword_length = 0.80F;
  SwordRenderConfig b;
  b.metal_color = {0.3F, 0.6F, 0.4F};
  b.sword_length = 1.10F;
  b.guard_half_width = 0.18F;
  run_stateless_battery<SwordRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, SwordUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  SwordRenderer::submit(SwordRenderConfig{}, ctx, frames, palette, anim,
                        via_submit);
  SwordRenderer renderer;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(via_submit), 15);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, SwordTrailUsesArchetypePath) {
  const auto frames = make_frames();
  auto anim = make_anim();
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = true;
  anim.inputs.time = Render::GL::KNIGHT_ATTACK_CYCLE_TIME * 0.40F;
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  SwordRenderer::submit(SwordRenderConfig{}, ctx, frames, palette, anim,
                        via_submit);
  SwordRenderer renderer;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 2U);
  EXPECT_EQ(draw_count_of(via_submit), 16);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, ShieldSubmitIsStateless) {
  ShieldRenderer renderer{};
  ShieldRenderConfig a;
  a.shield_color = {0.7F, 0.3F, 0.2F};
  a.shield_radius = 0.18F;
  ShieldRenderConfig b;
  b.shield_color = {0.2F, 0.4F, 0.6F};
  b.shield_radius = 0.24F;
  b.shield_aspect = 1.4F;
  b.has_cross_decal = true;
  run_stateless_battery<ShieldRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, ShieldUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch base_submit;
  EquipmentBatch cross_submit;
  EquipmentBatch via_render;
  ShieldRenderConfig cross_config;
  cross_config.has_cross_decal = true;

  ShieldRenderer::submit(ShieldRenderConfig{}, ctx, frames, palette, anim,
                         base_submit);
  ShieldRenderer::submit(cross_config, ctx, frames, palette, anim,
                         cross_submit);
  ShieldRenderer renderer;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(base_submit.meshes.empty());
  EXPECT_EQ(base_submit.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(base_submit), 40);
  EXPECT_EQ(draw_count_of(cross_submit), 42);
  EXPECT_EQ(hash_batch(base_submit), hash_batch(via_render));
  EXPECT_NE(hash_batch(base_submit), hash_batch(cross_submit));
}

TEST(StatelessWeaponRenderers, ShieldArchetypeSitsHigherAndCoversMoreLine) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  ShieldRenderConfig config;
  config.shield_color = {0.20F, 0.46F, 0.62F};
  config.trim_color = {0.76F, 0.68F, 0.42F};
  config.metal_color = {0.70F, 0.68F, 0.52F};
  config.shield_radius = 0.18F * 0.9F;
  config.shield_aspect = 1.0F;

  EquipmentBatch batch;
  ShieldRenderer::submit(config, ctx, frames, palette, anim, batch);

  ASSERT_EQ(batch.archetypes.size(), 1U);
  ASSERT_NE(batch.archetypes[0].archetype, nullptr);

  const AABB box = archetype_local_aabb(*batch.archetypes[0].archetype);
  const float center_y = 0.5F * (box.mn.y() + box.mx.y());
  const float width = box.mx.x() - box.mn.x();

  EXPECT_GT(center_y, -0.12F);
  EXPECT_GT(width, 0.50F);
}

TEST(StatelessWeaponRenderers, SpearSubmitIsStateless) {
  SpearRenderer renderer{};
  SpearRenderConfig a;
  a.spear_length = 1.10F;
  a.shaft_radius = 0.020F;
  SpearRenderConfig b;
  b.spear_length = 1.45F;
  b.shaft_radius = 0.024F;
  b.spearhead_color = {0.9F, 0.85F, 0.7F};
  run_stateless_battery<SpearRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, SpearUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  SpearRenderer::submit(SpearRenderConfig{}, ctx, frames, palette, anim,
                        via_submit);
  SpearRenderer renderer;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 4U);
  EXPECT_EQ(draw_count_of(via_submit), 4);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, BowSubmitIsStateless) {
  BowRenderer renderer{};
  BowRenderConfig a;
  a.bow_top_y = 1.4F;
  a.bow_bot_y = 0.6F;
  a.bow_depth = 0.22F;
  BowRenderConfig b;
  b.bow_top_y = 1.6F;
  b.bow_bot_y = 0.4F;
  b.bow_depth = 0.30F;
  b.bow_curve_factor = 1.3F;
  b.fletching_color = {0.9F, 0.2F, 0.1F};
  run_stateless_battery<BowRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, BowBodyUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  BowRenderConfig config;
  config.bow_top_y = 1.4F;
  config.bow_bot_y = 0.6F;
  config.bow_depth = 0.22F;

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  BowRenderer::submit(config, ctx, frames, palette, anim, via_submit);
  BowRenderer renderer{config};
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 3U);
  EXPECT_EQ(draw_count_of(via_submit), 25);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, BowArrowUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  BowRenderConfig config;
  config.bow_top_y = 1.4F;
  config.bow_bot_y = 0.6F;
  config.bow_depth = 0.22F;
  config.arrow_visibility = Render::GL::ArrowVisibility::IdleAndAttackCycle;

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  BowRenderer::submit(config, ctx, frames, palette, anim, via_submit);
  BowRenderer renderer{config};
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 7U);
  EXPECT_EQ(draw_count_of(via_submit), 29);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, BowAttackStringUsesArchetypePath) {
  const auto frames = make_frames();
  auto anim = make_anim();
  anim.inputs.is_attacking = true;
  anim.inputs.is_melee = false;
  anim.inputs.time = Render::GL::ARCHER_ATTACK_CYCLE_TIME * 0.25F;
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  BowRenderConfig config;
  config.bow_top_y = 1.4F;
  config.bow_bot_y = 0.6F;
  config.bow_depth = 0.22F;
  config.arrow_visibility = Render::GL::ArrowVisibility::Hidden;

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  BowRenderer::submit(config, ctx, frames, palette, anim, via_submit);
  BowRenderer renderer{config};
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 4U);
  EXPECT_EQ(draw_count_of(via_submit), 26);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, QuiverSubmitIsStateless) {
  QuiverRenderer renderer{};
  QuiverRenderConfig a;
  a.quiver_radius = 0.08F;
  a.num_arrows = 1;
  QuiverRenderConfig b;
  b.quiver_radius = 0.10F;
  b.num_arrows = 2;
  b.fletching_color = {0.2F, 0.7F, 0.3F};
  run_stateless_battery<QuiverRenderer>(renderer, a, b);
}

TEST(StatelessWeaponRenderers, RomanShieldUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  RomanShieldRenderer::submit(RomanShieldConfig{}, ctx, frames, palette, anim,
                              via_submit);
  RomanShieldRenderer renderer;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(via_submit), 5);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, RomanShieldOffsetsFromHandOrigin) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch batch;
  RomanShieldRenderer::submit(RomanShieldConfig{}, ctx, frames, palette, anim,
                              batch);

  ASSERT_EQ(batch.archetypes.size(), 1U);
  QVector3D const actual =
      batch.archetypes.front().world.column(3).toVector3D();
  EXPECT_LT(actual.x(), frames.hand_l.origin.x() - 0.05F);
  EXPECT_GT(std::abs(actual.z() - frames.hand_l.origin.z()), 0.05F);
}

TEST(StatelessWeaponRenderers, CarthageShieldUsesArchetypePath) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch base_submit;
  EquipmentBatch cavalry_submit;
  CarthageShieldRenderer::submit(CarthageShieldConfig{1.0F}, ctx, frames,
                                 palette, anim, base_submit);
  CarthageShieldRenderer::submit(CarthageShieldConfig{0.84F}, ctx, frames,
                                 palette, anim, cavalry_submit);

  CarthageShieldRenderer renderer;
  EquipmentBatch via_render;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(base_submit.meshes.empty());
  EXPECT_EQ(base_submit.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(base_submit), 31);
  EXPECT_EQ(hash_batch(base_submit), hash_batch(via_render));
  EXPECT_NE(hash_batch(base_submit), hash_batch(cavalry_submit));
}

TEST(StatelessWeaponRenderers, CarthageShieldOffsetsFromHandOrigin) {
  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch batch;
  CarthageShieldRenderer::submit(CarthageShieldConfig{1.0F}, ctx, frames,
                                 palette, anim, batch);

  ASSERT_EQ(batch.archetypes.size(), 1U);
  ASSERT_NE(batch.archetypes.front().archetype, nullptr);
  const auto grip = Render::Humanoid::socket_attachment_frame(
      frames.hand_l, Render::Humanoid::HumanoidSocket::GripL);
  const AABB world_box = archetype_world_aabb(batch.archetypes.front());
  QVector3D const world_center = (world_box.mn + world_box.mx) * 0.5F;
  const float side_offset = QVector3D::dotProduct(world_center - grip.origin,
                                                  grip.right.normalized());
  EXPECT_GT(std::abs(side_offset), 0.08F);
  EXPECT_GT(std::abs(world_center.z() - frames.hand_l.origin.z()), 0.02F);

  const AABB box = archetype_local_aabb(*batch.archetypes.front().archetype);
  const float width = box.mx.x() - box.mn.x();
  EXPECT_GT(width, 1.08F);
  EXPECT_LT(width, 1.15F);
}

TEST(StatelessWeaponRenderers, RomanScutumUsesArchetypePath) {
  auto frames = make_frames();
  frames.hand_l.radius = 0.05F;
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  RomanScutumRenderer::submit(RomanScutumConfig{}, ctx, frames, palette, anim,
                              via_submit);
  RomanScutumRenderer renderer;
  renderer.render(ctx, frames, palette, anim, via_render);

  EXPECT_TRUE(via_submit.meshes.empty());
  EXPECT_EQ(via_submit.archetypes.size(), 1U);
  EXPECT_EQ(draw_count_of(via_submit), 13);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, RomanScutumKeepsModerateStandOffFromGrip) {
  auto frames = make_frames();
  frames.hand_l.radius = 0.05F;
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch batch;
  RomanScutumRenderer::submit(RomanScutumConfig{}, ctx, frames, palette, anim,
                              batch);

  ASSERT_EQ(batch.archetypes.size(), 1U);
  ASSERT_NE(batch.archetypes.front().archetype, nullptr);
  const auto grip = Render::Humanoid::socket_attachment_frame(
      frames.hand_l, Render::Humanoid::HumanoidSocket::GripL);
  const AABB world_box = archetype_world_aabb(batch.archetypes.front());
  QVector3D const side_axis = grip.right.normalized();
  const auto [min_side, max_side] =
      project_aabb_on_axis(world_box, grip.origin, side_axis);
  const float nearest_edge_offset =
      (min_side <= 0.0F && max_side >= 0.0F)
          ? 0.0F
          : std::min(std::abs(min_side), std::abs(max_side));
  EXPECT_GT(nearest_edge_offset, 0.24F);
  EXPECT_LT(nearest_edge_offset, 0.34F);
}

TEST(StatelessWeaponRenderers, BaseConfigPreservedAcrossSubmitPaths) {
  ShieldRenderConfig base;
  base.shield_color = {0.4F, 0.5F, 0.7F};
  base.shield_radius = 0.21F;
  base.has_cross_decal = true;
  ShieldRenderer renderer{base};

  EXPECT_EQ(renderer.base_config().shield_radius, base.shield_radius);

  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch via_submit;
  EquipmentBatch via_render;
  ShieldRenderer::submit(renderer.base_config(), ctx, frames, palette, anim,
                         via_submit);
  renderer.render(ctx, frames, palette, anim, via_render);
  EXPECT_EQ(hash_batch(via_submit), hash_batch(via_render));
}

TEST(StatelessWeaponRenderers, BowDefaultsHonouredViaBaseConfig) {
  BowRenderConfig roman_defaults;
  roman_defaults.bow_depth = 0.22F;
  roman_defaults.bow_curve_factor = 1.0F;
  roman_defaults.bow_height_scale = 1.0F;
  BowRenderer roman{roman_defaults};

  BowRenderConfig carthage_defaults;
  carthage_defaults.bow_depth = 0.28F;
  carthage_defaults.bow_curve_factor = 1.2F;
  carthage_defaults.bow_height_scale = 0.95F;
  BowRenderer carthage{carthage_defaults};

  const auto frames = make_frames();
  const auto anim = make_anim();
  const auto palette = make_palette();
  const auto ctx = make_ctx();

  EquipmentBatch r_legacy;
  EquipmentBatch c_legacy;
  roman.render(ctx, frames, palette, anim, r_legacy);
  carthage.render(ctx, frames, palette, anim, c_legacy);

  EquipmentBatch r_submit;
  EquipmentBatch c_submit;
  BowRenderer::submit(roman.base_config(), ctx, frames, palette, anim,
                      r_submit);
  BowRenderer::submit(carthage.base_config(), ctx, frames, palette, anim,
                      c_submit);

  EXPECT_EQ(hash_batch(r_legacy), hash_batch(r_submit));
  EXPECT_EQ(hash_batch(c_legacy), hash_batch(c_submit));
  EXPECT_NE(hash_batch(r_submit), hash_batch(c_submit))
      << "different ctor defaults must yield different draw batches";
}
