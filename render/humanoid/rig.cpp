#include "rig.h"

#include "../material.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/map/terrain_service.h"
#include "../../game/systems/nation_id.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../entity/registry.h"
#include "../geom/affine_matrix.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../geom/parts.h"
#include "../gl/backend.h"
#include "../gl/camera.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/animation/gait.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../gl/resources.h"
#include "../palette.h"
#include "../pose_palette_cache.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "../template_cache.h"
#include "../visibility_budget.h"
#include "formation_calculator.h"
#include "humanoid_math.h"
#include "pose_controller.h"
#include "rig_stats_shim.h"
#include "clip_driver_cache.h"
#include "humanoid_spec.h"
#include "../creature/spec.h"
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <span>
#include <unordered_map>
#include <vector>

namespace Render::GL {

using namespace Render::GL::Geometry;
using Render::Geom::capsule_between;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::oriented_cylinder;
using Render::Geom::sphere_at;

namespace {

constexpr float k_shadow_size_infantry = 0.16F;
constexpr float k_shadow_size_mounted = 0.35F;

constexpr float k_run_extra_foot_lift = 0.08F;
constexpr float k_run_stride_enhancement = 0.15F;

struct CachedPoseEntry {
  HumanoidPose pose;
  VariationParams variation;
  uint32_t frame_number{0};
  bool was_moving{false};
};

using PoseCacheKey = uint64_t;
static std::unordered_map<PoseCacheKey, CachedPoseEntry> s_pose_cache;
static uint32_t s_current_frame = 0;
constexpr uint32_t k_pose_cache_max_age = 300;

struct SoldierLayout {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float vertical_jitter{0.0F};
  float yaw_offset{0.0F};
  float phase_offset{0.0F};
  std::uint32_t inst_seed{0};
};

struct UnitLayoutCacheEntry {
  std::vector<SoldierLayout> soldiers;
  FormationParams formation{};
  FormationCalculatorFactory::Nation nation{
      FormationCalculatorFactory::Nation::Roman};
  FormationCalculatorFactory::UnitCategory category{
      FormationCalculatorFactory::UnitCategory::Infantry};
  int rows{0};
  int cols{0};
  std::uint32_t seed{0};
  std::uint32_t frame_number{0};
};

static std::unordered_map<std::uintptr_t, UnitLayoutCacheEntry> s_layout_cache;
constexpr uint32_t k_layout_cache_max_age = 600;

inline auto make_pose_cache_key(uintptr_t entity_ptr,
                                int soldier_idx) -> PoseCacheKey {
  return (static_cast<uint64_t>(entity_ptr) << 16) |
         static_cast<uint64_t>(soldier_idx & 0xFFFF);
}

static HumanoidRenderStats s_render_stats;

constexpr float k_shadow_ground_offset = 0.02F;
constexpr float k_shadow_base_alpha = 0.24F;
constexpr QVector3D k_shadow_light_dir(0.4F, 1.0F, 0.25F);

constexpr float k_temporal_skip_distance_reduced = 35.0F;
constexpr float k_temporal_skip_distance_minimal = 45.0F;
constexpr uint32_t k_temporal_skip_period_reduced = 2;
constexpr uint32_t k_temporal_skip_period_minimal = 3;

inline auto should_render_temporal(uint32_t frame, uint32_t seed,
                                   uint32_t period) -> bool {
  if (period <= 1) {
    return true;
  }
  return ((frame + seed) % period) == 0U;
}

inline auto make_yaw_scale_model(float cos_yaw, float sin_yaw, float scale_x,
                                 float scale_y, float scale_z,
                                 const QVector3D &translation) -> QMatrix4x4 {
  QMatrix4x4 m;
  float *d = m.data();
  d[0] = cos_yaw * scale_x;
  d[1] = 0.0F;
  d[2] = -sin_yaw * scale_x;
  d[3] = 0.0F;

  d[4] = 0.0F;
  d[5] = scale_y;
  d[6] = 0.0F;
  d[7] = 0.0F;

  d[8] = sin_yaw * scale_z;
  d[9] = 0.0F;
  d[10] = cos_yaw * scale_z;
  d[11] = 0.0F;

  d[12] = translation.x();
  d[13] = translation.y();
  d[14] = translation.z();
  d[15] = 1.0F;
  return m;
}

inline auto
resolve_layout_seed(const Engine::Core::UnitComponent *unit_comp,
                    const Engine::Core::Entity *entity) -> std::uint32_t {
  std::uint32_t seed = 0U;
  if (unit_comp != nullptr) {
    seed ^= std::uint32_t(unit_comp->owner_id * 2654435761U);
  }
  if (entity != nullptr) {
    seed ^= std::uint32_t(reinterpret_cast<uintptr_t>(entity) & 0xFFFFFFFFU);
  }
  return seed;
}

inline auto resolve_variant_key_from_seed(std::uint32_t seed) -> std::uint8_t {
  std::uint32_t v = seed ^ (seed >> 16);
  return static_cast<std::uint8_t>(v % k_template_variant_count);
}

inline auto
resolve_attack_variant_from_seed(std::uint32_t seed) -> std::uint8_t {
  return static_cast<std::uint8_t>(seed % 3U);
}

inline auto resolve_variant_seed(const Engine::Core::UnitComponent *unit_comp,
                                 std::uint8_t variant_key) -> std::uint32_t {
  std::uint32_t seed = 0U;
  if (unit_comp != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit_comp->spawn_type) * 2654435761U;
    seed ^= static_cast<std::uint32_t>(unit_comp->owner_id) * 1013904223U;
  }
  seed ^= static_cast<std::uint32_t>(variant_key) * 2246822519U;
  return seed;
}

inline auto resolve_formation_category(
    const Engine::Core::UnitComponent *unit_comp,
    const AnimationInputs &anim) -> FormationCalculatorFactory::UnitCategory {
  using UnitCategory = FormationCalculatorFactory::UnitCategory;
  if ((unit_comp != nullptr) &&
      unit_comp->spawn_type == Game::Units::SpawnType::Builder &&
      anim.is_constructing) {
    return UnitCategory::BuilderConstruction;
  }
  bool is_mounted = false;
  if (unit_comp != nullptr) {
    using Game::Units::SpawnType;
    auto const st = unit_comp->spawn_type;
    is_mounted =
        (st == SpawnType::MountedKnight || st == SpawnType::HorseArcher ||
         st == SpawnType::HorseSpearman);
  }
  return is_mounted ? UnitCategory::Cavalry : UnitCategory::Infantry;
}

auto get_or_build_unit_layout(std::uintptr_t key,
                              const FormationParams &formation,
                              FormationCalculatorFactory::Nation nation,
                              FormationCalculatorFactory::UnitCategory category,
                              std::uint32_t seed, int rows,
                              int cols) -> UnitLayoutCacheEntry & {
  auto &entry = s_layout_cache[key];
  bool rebuild =
      entry.soldiers.empty() || entry.rows != rows || entry.cols != cols ||
      entry.seed != seed || entry.nation != nation ||
      entry.category != category ||
      std::abs(entry.formation.spacing - formation.spacing) > 1e-6F ||
      entry.formation.individuals_per_unit != formation.individuals_per_unit ||
      entry.formation.max_per_row != formation.max_per_row;

  if (rebuild) {
    entry.soldiers.clear();
    entry.soldiers.reserve(static_cast<std::size_t>(rows * cols));
    entry.rows = rows;
    entry.cols = cols;
    entry.seed = seed;
    entry.nation = nation;
    entry.category = category;
    entry.formation = formation;

    const IFormationCalculator *formation_calculator =
        FormationCalculatorFactory::get_calculator(nation, category);

    auto fast_random = [](uint32_t &state) -> float {
      state = state * 1664525U + 1013904223U;
      return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    for (int idx = 0; idx < rows * cols; ++idx) {
      int const r = idx / cols;
      int const c = idx % cols;

      FormationOffset const formation_offset =
          formation_calculator->calculate_offset(idx, r, c, rows, cols,
                                                 formation.spacing, seed);

      std::uint32_t const inst_seed = seed ^ std::uint32_t(idx * 9176U);
      std::uint32_t rng_state = inst_seed;

      SoldierLayout layout{};
      layout.offset_x = formation_offset.offset_x;
      layout.offset_z = formation_offset.offset_z;
      layout.vertical_jitter = (fast_random(rng_state) - 0.5F) * 0.03F;
      layout.yaw_offset = (fast_random(rng_state) - 0.5F) * 5.0F;
      layout.phase_offset = fast_random(rng_state) * 0.25F;
      layout.inst_seed = inst_seed;

      entry.soldiers.push_back(layout);
    }
  }

  entry.frame_number = s_current_frame;
  return entry;
}
} // namespace

void advance_pose_cache_frame() {
  ++s_current_frame;
  ::Render::Humanoid::ClipDriverCache::instance().advance_frame(s_current_frame);

  if ((s_current_frame & 0x1FF) == 0) {
    auto it = s_pose_cache.begin();
    while (it != s_pose_cache.end()) {
      if (s_current_frame - it->second.frame_number >
          k_pose_cache_max_age * 2) {
        it = s_pose_cache.erase(it);
      } else {
        ++it;
      }
    }

    auto layout_it = s_layout_cache.begin();
    while (layout_it != s_layout_cache.end()) {
      if (s_current_frame - layout_it->second.frame_number >
          k_layout_cache_max_age * 2) {
        layout_it = s_layout_cache.erase(layout_it);
      } else {
        ++layout_it;
      }
    }
  }
}

void clear_humanoid_caches() {
  s_pose_cache.clear();
  s_layout_cache.clear();
  s_current_frame = 0;
  ::Render::Humanoid::ClipDriverCache::instance().clear();
}

auto torso_mesh_without_bottom_cap() -> Mesh * {
  static std::unique_ptr<Mesh> s_mesh;
  if (s_mesh != nullptr) {
    return s_mesh.get();
  }

  Mesh *base = get_unit_torso();
  if (base == nullptr) {
    return nullptr;
  }

  auto filtered = base->clone_with_filtered_indices(
      [](unsigned int a, unsigned int b, unsigned int c,
         const std::vector<Vertex> &verts) -> bool {
        auto sample = [&](unsigned int idx) -> QVector3D {
          return {verts[idx].position[0], verts[idx].position[1],
                  verts[idx].position[2]};
        };
        QVector3D pa = sample(a);
        QVector3D pb = sample(b);
        QVector3D pc = sample(c);
        float min_y = std::min({pa.y(), pb.y(), pc.y()});
        float max_y = std::max({pa.y(), pb.y(), pc.y()});

        QVector3D n(
            verts[a].normal[0] + verts[b].normal[0] + verts[c].normal[0],
            verts[a].normal[1] + verts[b].normal[1] + verts[c].normal[1],
            verts[a].normal[2] + verts[b].normal[2] + verts[c].normal[2]);
        if (n.lengthSquared() > 0.0F) {
          n.normalize();
        }

        constexpr float k_band_height = 0.02F;
        constexpr float k_bottom_threshold = 0.45F;
        bool is_flat = (max_y - min_y) < k_band_height;
        bool is_at_bottom = min_y > k_bottom_threshold;
        bool facing_down = (n.y() > 0.35F);
        return is_flat && is_at_bottom && facing_down;
      });

  s_mesh =
      (filtered != nullptr) ? std::move(filtered) : std::unique_ptr<Mesh>(base);
  return s_mesh.get();
}

void align_torso_mesh_forward(QMatrix4x4 &model) noexcept {
  model.rotate(90.0F, 0.0F, 1.0F, 0.0F);
}

auto HumanoidRendererBase::frame_local_position(
    const AttachmentFrame &frame, const QVector3D &local) -> QVector3D {
  float const lx = local.x() * frame.radius;
  float const ly = local.y() * frame.radius;
  float const lz = local.z() * frame.radius;
  return frame.origin + frame.right * lx + frame.up * ly + frame.forward * lz;
}

auto HumanoidRendererBase::make_frame_local_transform(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  float scale = frame.radius * uniform_scale;
  if (scale == 0.0F) {
    scale = uniform_scale;
  }

  QVector3D const origin = frame_local_position(frame, local_offset);

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(frame.right * scale, 0.0F));
  local.setColumn(1, QVector4D(frame.up * scale, 0.0F));
  local.setColumn(2, QVector4D(frame.forward * scale, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

auto HumanoidRendererBase::head_local_position(
    const HeadFrame &frame, const QVector3D &local) -> QVector3D {
  return frame_local_position(frame, local);
}

auto HumanoidRendererBase::make_head_local_transform(
    const QMatrix4x4 &parent, const HeadFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  return make_frame_local_transform(parent, frame, local_offset, uniform_scale);
}

void HumanoidRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                       HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void HumanoidRendererBase::customize_pose(const DrawContext &,
                                          const HumanoidAnimationContext &,
                                          uint32_t, HumanoidPose &) const {}

void HumanoidRendererBase::add_attachments(const DrawContext &,
                                           const HumanoidVariant &,
                                           const HumanoidPose &,
                                           const HumanoidAnimationContext &,
                                           ISubmitter &) const {}

auto HumanoidRendererBase::resolve_entity_ground_offset(
    const DrawContext &, Engine::Core::UnitComponent *unit_comp,
    Engine::Core::TransformComponent *transform_comp) const -> float {
  (void)unit_comp;
  (void)transform_comp;

  return 0.0F;
}

auto HumanoidRendererBase::resolve_team_tint(const DrawContext &ctx)
    -> QVector3D {
  QVector3D tunic(0.8F, 0.9F, 1.0F);
  Engine::Core::UnitComponent *unit = nullptr;
  Engine::Core::RenderableComponent *rc = nullptr;

  if (ctx.entity != nullptr) {
    unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    rc = ctx.entity->get_component<Engine::Core::RenderableComponent>();
  }

  if ((unit != nullptr) && unit->owner_id > 0) {
    tunic = Game::Visuals::team_colorForOwner(unit->owner_id);
  } else if (rc != nullptr) {
    tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
  }

  return tunic;
}

auto HumanoidRendererBase::resolve_formation(const DrawContext &ctx)
    -> FormationParams {
  FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      params.individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->spawn_type);
      params.max_per_row =
          Game::Units::TroopConfig::instance().getMaxUnitsPerRow(
              unit->spawn_type);
      if (unit->spawn_type == Game::Units::SpawnType::MountedKnight) {
        params.spacing = 1.05F;
      }
    }
  }

  return params;
}



namespace {
auto resolve_renderer_for_submitter(ISubmitter &out) -> Renderer * {
  if (auto *renderer = dynamic_cast<Renderer *>(&out)) {
    return renderer;
  }
  if (auto *batch = dynamic_cast<BatchingSubmitter *>(&out)) {
    return dynamic_cast<Renderer *>(batch->fallback_submitter());
  }
  return nullptr;
}

} // namespace

void HumanoidRendererBase::render(const DrawContext &ctx,
                                  ISubmitter &out) const {
  AnimationInputs anim = sample_anim_state(ctx);

  if (ctx.template_prewarm) {
    if (ctx.renderer_id.empty() || ctx.animation_override == nullptr ||
        !ctx.force_humanoid_lod || !ctx.has_variant_override) {
      return;
    }

    auto *unit_comp =
        ctx.entity ? ctx.entity->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
    std::uint32_t owner_id =
        (unit_comp != nullptr) ? static_cast<std::uint32_t>(unit_comp->owner_id)
                               : 0U;

    bool is_mounted_spawn = false;
    if (unit_comp != nullptr) {
      using Game::Units::SpawnType;
      auto const st = unit_comp->spawn_type;
      is_mounted_spawn =
          (st == SpawnType::MountedKnight || st == SpawnType::HorseArcher ||
           st == SpawnType::HorseSpearman);
    }

    std::uint8_t attack_variant = 0;
    if (ctx.has_attack_variant_override) {
      attack_variant = ctx.attack_variant_override;
    }

    AnimKey anim_key =
        make_anim_key(*ctx.animation_override, 0.0F, attack_variant);

    TemplateKey key;
    key.renderer_id = ctx.renderer_id;
    key.owner_id = owner_id;
    key.lod = static_cast<std::uint8_t>(ctx.forced_humanoid_lod);
    HorseLOD mount_lod = HorseLOD::Full;
    key.mount_lod = 0;
    if (is_mounted_spawn) {
      mount_lod = ctx.force_horse_lod
                      ? ctx.forced_horse_lod
                      : static_cast<HorseLOD>(ctx.forced_humanoid_lod);
      key.mount_lod = static_cast<std::uint8_t>(mount_lod);
    }
    key.variant = ctx.variant_override;
    key.attack_variant = anim_key.attack_variant;
    key.state = anim_key.state;
    key.combat_phase = anim_key.combat_phase;
    key.frame = anim_key.frame;

    const TemplateCache::DenseDomainHandle dense_domain =
        TemplateCache::instance().get_dense_domain_handle(
            key.renderer_id, key.owner_id, key.lod, key.mount_lod);
    const std::size_t dense_slot =
        TemplateCache::dense_slot_index(key.variant, anim_key);

    (void)TemplateCache::instance().get_or_build_dense(
        dense_domain, dense_slot, key, [&]() -> PoseTemplate {
          thread_local TemplateRecorder recorder;
          recorder.reset(320);
          recorder.set_current_shader(nullptr);
          recorder.set_current_material(
              Render::GL::MaterialRegistry::instance().is_initialised()
                  ? Render::GL::MaterialRegistry::instance().character()
                  : nullptr);

          if (auto *outer = dynamic_cast<Renderer *>(&out)) {
            recorder.set_current_shader(outer->get_current_shader());
          }

          Engine::Core::Entity template_entity(1);
          auto *templ_unit =
              template_entity.add_component<Engine::Core::UnitComponent>();
          if (unit_comp != nullptr) {
            *templ_unit = *unit_comp;
          }
          templ_unit->max_health =
              (templ_unit->max_health > 0) ? templ_unit->max_health : 1;
          templ_unit->health = templ_unit->max_health;

          auto *templ_transform =
              template_entity.add_component<Engine::Core::TransformComponent>();
          templ_transform->position = {0.0F, 0.0F, 0.0F};
          templ_transform->rotation = {0.0F, 0.0F, 0.0F};
          templ_transform->scale = {1.0F, 1.0F, 1.0F};

          if (auto *renderable =
                  ctx.entity
                      ? ctx.entity
                            ->get_component<Engine::Core::RenderableComponent>()
                      : nullptr) {
            auto *templ_renderable =
                template_entity
                    .add_component<Engine::Core::RenderableComponent>(
                        renderable->mesh_path, renderable->texture_path);
            templ_renderable->renderer_id = renderable->renderer_id;
            templ_renderable->mesh = renderable->mesh;
            templ_renderable->visible = true;
            templ_renderable->color = renderable->color;
          }

          DrawContext build_ctx = ctx;
          build_ctx.entity = &template_entity;
          build_ctx.world = nullptr;
          build_ctx.model = QMatrix4x4();
          build_ctx.camera = nullptr;
          build_ctx.selected = false;
          build_ctx.hovered = false;
          build_ctx.allow_template_cache = false;
          build_ctx.force_humanoid_lod = true;
          build_ctx.forced_humanoid_lod = ctx.forced_humanoid_lod;
          build_ctx.force_horse_lod = is_mounted_spawn;
          if (is_mounted_spawn) {
            build_ctx.forced_horse_lod = static_cast<HorseLOD>(key.mount_lod);
          }
          build_ctx.has_seed_override = true;
          build_ctx.seed_override =
              resolve_variant_seed(unit_comp, key.variant);
          build_ctx.has_attack_variant_override = true;
          build_ctx.attack_variant_override = key.attack_variant;
          build_ctx.force_single_soldier = true;
          build_ctx.skip_ground_offset = true;

          AnimationInputs build_anim = make_animation_inputs(anim_key);
          build_ctx.animation_override = &build_anim;

          render_procedural(build_ctx, build_anim, recorder);

          PoseTemplate built;
          built.commands = recorder.take_commands();
          return built;
        });
    return;
  }

  if (!ctx.allow_template_cache || ctx.renderer_id.empty() ||
      ctx.entity == nullptr) {
    render_procedural(ctx, anim, out);
    return;
  }

  auto *unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  auto *transform_comp =
      ctx.entity->get_component<Engine::Core::TransformComponent>();
  if (unit_comp == nullptr || transform_comp == nullptr) {
    render_procedural(ctx, anim, out);
    return;
  }

  FormationParams const formation = resolve_formation(ctx);
  const int rows =
      (formation.individuals_per_unit + formation.max_per_row - 1) /
      formation.max_per_row;
  int cols = formation.max_per_row;
  int effective_rows = rows;
  if (ctx.force_single_soldier) {
    cols = 1;
    effective_rows = 1;
  }

  int visible_count = rows * cols;
  int max_health = std::max(1, unit_comp->max_health);
  float ratio = std::clamp(unit_comp->health / float(max_health), 0.0F, 1.0F);
  visible_count = std::max(1, (int)std::ceil(ratio * float(rows * cols)));

  if (visible_count <= 0) {
    return;
  }

  s_render_stats.soldiers_total += visible_count;

  bool is_mounted_spawn = false;
  if (unit_comp != nullptr) {
    using Game::Units::SpawnType;
    auto const st = unit_comp->spawn_type;
    is_mounted_spawn =
        (st == SpawnType::MountedKnight || st == SpawnType::HorseArcher ||
         st == SpawnType::HorseSpearman);
  }

  FormationCalculatorFactory::Nation nation =
      FormationCalculatorFactory::Nation::Roman;
  if (unit_comp != nullptr &&
      unit_comp->nation_id == Game::Systems::NationID::Carthage) {
    nation = FormationCalculatorFactory::Nation::Carthage;
  }

  FormationCalculatorFactory::UnitCategory category =
      resolve_formation_category(unit_comp, anim);

  std::uint32_t layout_seed = resolve_layout_seed(unit_comp, ctx.entity);
  auto &layout_entry = get_or_build_unit_layout(
      reinterpret_cast<std::uintptr_t>(ctx.entity), formation, nation, category,
      layout_seed, rows, cols);

  float entity_ground_offset =
      resolve_entity_ground_offset(ctx, unit_comp, transform_comp);

  Renderer *renderer = resolve_renderer_for_submitter(out);
  Shader *last_shader = nullptr;
  const auto &gfx_settings = Render::GraphicsSettings::instance();

  const float base_pos_x = transform_comp->position.x;
  const float base_pos_y = transform_comp->position.y;
  const float base_pos_z = transform_comp->position.z;
  const float scale_x = transform_comp->scale.x;
  const float scale_y = transform_comp->scale.y;
  const float scale_z = transform_comp->scale.z;
  const float base_yaw = transform_comp->rotation.y;
  const float ground_offset_scaled = entity_ground_offset * scale_y;

  const bool has_camera = (ctx.camera != nullptr);
  const QVector3D camera_position =
      has_camera ? ctx.camera->get_position() : QVector3D();

  auto &terrain_service = Game::Map::TerrainService::instance();
  const bool terrain_initialized = terrain_service.is_initialized();
  QVector2D shadow_dir_for_use(0.0F, 1.0F);
  {
    QVector2D light_dir_xz(k_shadow_light_dir.x(), k_shadow_light_dir.z());
    if (light_dir_xz.lengthSquared() >= 1e-6F) {
      light_dir_xz.normalize();
      shadow_dir_for_use = -light_dir_xz;
      if (shadow_dir_for_use.lengthSquared() >= 1e-6F) {
        shadow_dir_for_use.normalize();
      } else {
        shadow_dir_for_use = QVector2D(0.0F, 1.0F);
      }
    }
  }

  Shader *shadow_shader = nullptr;
  Mesh *shadow_quad_mesh = nullptr;
  const bool can_render_shadow_mesh =
      gfx_settings.shadows_enabled() && ctx.backend != nullptr &&
      ctx.resources != nullptr && terrain_initialized;
  if (can_render_shadow_mesh) {
    shadow_shader = ctx.backend->shader(QStringLiteral("troop_shadow"));
    shadow_quad_mesh = ctx.resources->quad();
  }

  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  const std::size_t soldier_count = std::min<std::size_t>(
      layout_entry.soldiers.size(), static_cast<std::size_t>(visible_count));
  const std::uint32_t template_owner_id =
      static_cast<std::uint32_t>(unit_comp->owner_id);
  std::array<std::array<TemplateCache::DenseDomainHandle, 4>, 4>
      dense_domains{};
  std::array<std::array<bool, 4>, 4> dense_domains_ready{};

  auto dense_domain_for =
      [&](HumanoidLOD lod,
          HorseLOD mount_lod_val) -> TemplateCache::DenseDomainHandle {
    const std::size_t lod_idx =
        std::min<std::size_t>(static_cast<std::size_t>(lod), 3);
    const std::size_t mount_idx =
        std::min<std::size_t>(static_cast<std::size_t>(mount_lod_val), 3);

    if (!dense_domains_ready[lod_idx][mount_idx]) {
      const std::uint8_t lod_u8 = static_cast<std::uint8_t>(lod_idx);
      const std::uint8_t mount_u8 = static_cast<std::uint8_t>(mount_idx);
      dense_domains[lod_idx][mount_idx] =
          TemplateCache::instance().get_dense_domain_handle(
              ctx.renderer_id, template_owner_id, lod_u8, mount_u8);
      dense_domains_ready[lod_idx][mount_idx] = true;
    }

    return dense_domains[lod_idx][mount_idx];
  };

  for (std::size_t idx = 0; idx < soldier_count; ++idx) {
    const SoldierLayout &slot = layout_entry.soldiers[idx];

    float offset_x = slot.offset_x;
    float offset_z = slot.offset_z;
    float applied_vertical_jitter = slot.vertical_jitter;
    float applied_yaw_offset = slot.yaw_offset;
    float phase_offset = slot.phase_offset;

    uint32_t rng_state = slot.inst_seed;

    if (anim.is_attacking && anim.is_melee) {
      float const combat_jitter_x =
          (fast_random(rng_state) - 0.5F) * formation.spacing * 0.4F;
      float const combat_jitter_z =
          (fast_random(rng_state) - 0.5F) * formation.spacing * 0.3F;
      float const sway_time = anim.time + phase_offset * 2.0F;
      float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
      float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
      offset_x += combat_jitter_x + sway_x;
      offset_z += combat_jitter_z + sway_z;

      float const combat_yaw = (fast_random(rng_state) - 0.5F) * 15.0F;
      applied_yaw_offset += combat_yaw;
    }

    const float applied_yaw = base_yaw + applied_yaw_offset;
    const float yaw_rad = qDegreesToRadians(applied_yaw);
    const float cos_yaw = std::cos(yaw_rad);
    const float sin_yaw = std::sin(yaw_rad);
    const float local_x = offset_x * scale_x;
    const float local_z = offset_z * scale_z;

    QVector3D const soldier_world_pos(
        base_pos_x + (cos_yaw * local_x) + (sin_yaw * local_z),
        base_pos_y + (applied_vertical_jitter * scale_y) - ground_offset_scaled,
        base_pos_z - (sin_yaw * local_x) + (cos_yaw * local_z));

    constexpr float k_soldier_cull_radius = 0.6F;
    if (has_camera &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      continue;
    }

    HumanoidLOD soldier_lod = HumanoidLOD::Full;
    float soldier_distance = 0.0F;
    if (ctx.force_humanoid_lod) {
      soldier_lod = ctx.forced_humanoid_lod;
    } else if (has_camera) {
      soldier_distance = (soldier_world_pos - camera_position).length();
      soldier_lod = calculate_humanoid_lod(soldier_distance);

      if (soldier_lod == HumanoidLOD::Billboard) {
        ++s_render_stats.soldiers_skipped_lod;
        continue;
      }

      soldier_lod =
          Render::VisibilityBudgetTracker::instance().request_humanoid_lod(
              soldier_lod);
    }

    if (soldier_distance > 0.0F) {
      if (soldier_lod == HumanoidLOD::Reduced &&
          soldier_distance > k_temporal_skip_distance_reduced) {
        if (!should_render_temporal(s_current_frame, slot.inst_seed,
                                    k_temporal_skip_period_reduced)) {
          ++s_render_stats.soldiers_skipped_temporal;
          continue;
        }
      }
      if (soldier_lod == HumanoidLOD::Minimal &&
          soldier_distance > k_temporal_skip_distance_minimal) {
        if (!should_render_temporal(s_current_frame, slot.inst_seed,
                                    k_temporal_skip_period_minimal)) {
          ++s_render_stats.soldiers_skipped_temporal;
          continue;
        }
      }
    }

    ++s_render_stats.soldiers_rendered;
    switch (soldier_lod) {
    case HumanoidLOD::Full:
      ++s_render_stats.lod_full;
      break;
    case HumanoidLOD::Reduced:
      ++s_render_stats.lod_reduced;
      break;
    case HumanoidLOD::Minimal:
      ++s_render_stats.lod_minimal;
      break;
    case HumanoidLOD::Billboard:
      break;
    }

    QMatrix4x4 inst_model = make_yaw_scale_model(
        cos_yaw, sin_yaw, scale_x, scale_y, scale_z, soldier_world_pos);

    std::uint8_t variant_key = resolve_variant_key_from_seed(slot.inst_seed);
    std::uint8_t attack_variant =
        resolve_attack_variant_from_seed(slot.inst_seed);

    AnimKey anim_key = make_anim_key(anim, phase_offset, attack_variant);

    HorseLOD mount_lod = HorseLOD::Full;
    if (is_mounted_spawn) {
      if (ctx.force_horse_lod) {
        mount_lod = ctx.forced_horse_lod;
      } else {
        mount_lod = static_cast<HorseLOD>(soldier_lod);
      }
    }

    TemplateKey key;
    key.renderer_id = ctx.renderer_id;
    key.owner_id = template_owner_id;
    key.lod = static_cast<std::uint8_t>(soldier_lod);
    key.mount_lod = is_mounted_spawn ? static_cast<std::uint8_t>(mount_lod) : 0;
    key.variant = variant_key;
    key.attack_variant = anim_key.attack_variant;
    key.state = anim_key.state;
    key.combat_phase = anim_key.combat_phase;
    key.frame = anim_key.frame;

    const TemplateCache::DenseDomainHandle dense_domain = dense_domain_for(
        soldier_lod, is_mounted_spawn ? mount_lod : HorseLOD::Full);
    const std::size_t dense_slot =
        TemplateCache::dense_slot_index(variant_key, anim_key);

    const PoseTemplate *tpl = TemplateCache::instance().get_or_build_dense(
        dense_domain, dense_slot, key, [&]() -> PoseTemplate {
          thread_local TemplateRecorder recorder;
          recorder.reset(320);
          recorder.set_current_shader(nullptr);
          recorder.set_current_material(
              Render::GL::MaterialRegistry::instance().is_initialised()
                  ? Render::GL::MaterialRegistry::instance().character()
                  : nullptr);

          if (auto *outer = dynamic_cast<Renderer *>(&out)) {
            recorder.set_current_shader(outer->get_current_shader());
          }

          Engine::Core::Entity template_entity(1);
          auto *templ_unit =
              template_entity.add_component<Engine::Core::UnitComponent>();
          *templ_unit = *unit_comp;
          templ_unit->max_health =
              (templ_unit->max_health > 0) ? templ_unit->max_health : 1;
          templ_unit->health = templ_unit->max_health;

          auto *templ_transform =
              template_entity.add_component<Engine::Core::TransformComponent>();
          templ_transform->position = {0.0F, 0.0F, 0.0F};
          templ_transform->rotation = {0.0F, 0.0F, 0.0F};
          templ_transform->scale = {1.0F, 1.0F, 1.0F};

          if (auto *renderable =
                  ctx.entity
                      ->get_component<Engine::Core::RenderableComponent>()) {
            auto *templ_renderable =
                template_entity
                    .add_component<Engine::Core::RenderableComponent>(
                        renderable->mesh_path, renderable->texture_path);
            templ_renderable->renderer_id = renderable->renderer_id;
            templ_renderable->mesh = renderable->mesh;
            templ_renderable->visible = true;
            templ_renderable->color = renderable->color;
          }

          DrawContext build_ctx = ctx;
          build_ctx.entity = &template_entity;
          build_ctx.world = nullptr;
          build_ctx.model = QMatrix4x4();
          build_ctx.camera = nullptr;
          build_ctx.selected = false;
          build_ctx.hovered = false;
          build_ctx.allow_template_cache = false;
          build_ctx.force_humanoid_lod = true;
          build_ctx.forced_humanoid_lod = soldier_lod;
          build_ctx.force_horse_lod = is_mounted_spawn;
          if (is_mounted_spawn) {
            build_ctx.forced_horse_lod = mount_lod;
          }
          build_ctx.has_seed_override = true;
          build_ctx.seed_override =
              resolve_variant_seed(unit_comp, variant_key);
          build_ctx.has_attack_variant_override = true;
          build_ctx.attack_variant_override = anim_key.attack_variant;
          build_ctx.force_single_soldier = true;
          build_ctx.skip_ground_offset = true;

          AnimationInputs build_anim = make_animation_inputs(anim_key);
          build_ctx.animation_override = &build_anim;

          render_procedural(build_ctx, build_anim, recorder);

          PoseTemplate built;
          built.commands = recorder.take_commands();
          return built;
        });

    if (tpl == nullptr || tpl->commands.empty()) {
      render_procedural(ctx, anim, out);
      return;
    }

    for (const auto &cmd : tpl->commands) {
      if (renderer != nullptr && cmd.shader != last_shader) {
        renderer->set_current_shader(cmd.shader);
        last_shader = cmd.shader;
      }
      QMatrix4x4 world_model =
          Render::Geom::multiply_affine(inst_model, cmd.local_model);
      if (cmd.material != nullptr) {
        // Stage-5 path: Material-aware DrawPartCmd so the backend picks
        // the shader via material->resolve(GraphicsSettings quality).
        out.part(cmd.mesh, const_cast<Material *>(cmd.material), world_model,
                 cmd.color, cmd.texture, cmd.alpha, cmd.material_id);
      } else {
        out.mesh(cmd.mesh, world_model, cmd.color, cmd.texture, cmd.alpha,
                 cmd.material_id);
      }
    }

    const bool should_render_shadow =
        (shadow_shader != nullptr) && (shadow_quad_mesh != nullptr) &&
        (soldier_lod == HumanoidLOD::Full ||
         soldier_lod == HumanoidLOD::Reduced) &&
        soldier_distance < gfx_settings.shadow_max_distance();

    if (should_render_shadow) {
      float const shadow_size =
          is_mounted_spawn ? k_shadow_size_mounted : k_shadow_size_infantry;
      float depth_boost = 1.0F;
      float width_boost = 1.0F;
      using Game::Units::SpawnType;
      switch (unit_comp->spawn_type) {
      case SpawnType::Spearman:
        depth_boost = 1.8F;
        width_boost = 0.95F;
        break;
      case SpawnType::HorseSpearman:
        depth_boost = 2.1F;
        width_boost = 1.05F;
        break;
      case SpawnType::Archer:
      case SpawnType::HorseArcher:
        depth_boost = 1.2F;
        width_boost = 0.95F;
        break;
      default:
        break;
      }

      float const shadow_width =
          shadow_size * (is_mounted_spawn ? 1.05F : 1.0F) * width_boost;
      float const shadow_depth =
          shadow_size * (is_mounted_spawn ? 1.30F : 1.10F) * depth_boost;

      QVector3D const inst_pos = soldier_world_pos;
      float const shadow_y =
          terrain_service.get_terrain_height(inst_pos.x(), inst_pos.z());

      float const shadow_offset = shadow_depth * 1.25F;
      QVector2D const offset_2d = shadow_dir_for_use * shadow_offset;
      float const light_yaw_deg = qRadiansToDegrees(std::atan2(
          double(shadow_dir_for_use.x()), double(shadow_dir_for_use.y())));

      QMatrix4x4 shadow_model;
      shadow_model.translate(inst_pos.x() + offset_2d.x(),
                             shadow_y + k_shadow_ground_offset,
                             inst_pos.z() + offset_2d.y());
      shadow_model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
      shadow_model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
      shadow_model.scale(shadow_width, shadow_depth, 1.0F);

      if (renderer != nullptr) {
        Shader *previous_shader = renderer->get_current_shader();
        renderer->set_current_shader(shadow_shader);
        shadow_shader->set_uniform(QStringLiteral("u_lightDir"),
                                   shadow_dir_for_use);

        Render::Humanoid::HumanoidFullPrim const shadow_prim{
            shadow_quad_mesh, shadow_model, QVector3D(0.0F, 0.0F, 0.0F),
            k_shadow_base_alpha, 0};
        Render::Humanoid::submit_humanoid_full_prims(
            std::span<const Render::Humanoid::HumanoidFullPrim>(&shadow_prim,
                                                                1U),
            out);

        renderer->set_current_shader(previous_shader);
        last_shader = previous_shader;
      }
    }
  }

  if (renderer != nullptr) {
    renderer->set_current_shader(nullptr);
  }
}

void HumanoidRendererBase::render_procedural(const DrawContext &ctx,
                                             const AnimationInputs &anim,
                                             ISubmitter &out) const {
  FormationParams const formation = resolve_formation(ctx);

  Engine::Core::UnitComponent *unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }

  Engine::Core::MovementComponent *movement_comp = nullptr;
  Engine::Core::TransformComponent *transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    movement_comp =
        ctx.entity->get_component<Engine::Core::MovementComponent>();
    transform_comp =
        ctx.entity->get_component<Engine::Core::TransformComponent>();
  }

  float entity_ground_offset =
      resolve_entity_ground_offset(ctx, unit_comp, transform_comp);

  uint32_t seed = 0U;
  if (ctx.has_seed_override) {
    seed = ctx.seed_override;
  } else {
    if (unit_comp != nullptr) {
      seed ^= uint32_t(unit_comp->owner_id * 2654435761U);
    }
    if (ctx.entity != nullptr) {
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }
  }

  const int rows =
      (formation.individuals_per_unit + formation.max_per_row - 1) /
      formation.max_per_row;
  int cols = formation.max_per_row;
  int effective_rows = rows;
  if (ctx.force_single_soldier) {
    cols = 1;
    effective_rows = 1;
  }

  bool is_mounted_spawn = false;
  if (unit_comp != nullptr) {
    using Game::Units::SpawnType;
    auto const st = unit_comp->spawn_type;
    is_mounted_spawn =
        (st == SpawnType::MountedKnight || st == SpawnType::HorseArcher ||
         st == SpawnType::HorseSpearman);
  }

  int visible_count = effective_rows * cols;
  if (!ctx.force_single_soldier && unit_comp != nullptr) {
    int const mh = std::max(1, unit_comp->max_health);
    float const ratio = std::clamp(unit_comp->health / float(mh), 0.0F, 1.0F);
    visible_count = std::max(1, (int)std::ceil(ratio * float(rows * cols)));
  }

  HumanoidVariant variant;
  get_variant(ctx, seed, variant);

  if (!m_proportion_scale_cached) {
    m_cached_proportion_scale = get_proportion_scaling();
    m_proportion_scale_cached = true;
  }
  const QVector3D prop_scale = m_cached_proportion_scale;
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  const QMatrix4x4 k_identity_matrix;

  const IFormationCalculator *formation_calculator = nullptr;
  {
    using Nation = FormationCalculatorFactory::Nation;
    using UnitCategory = FormationCalculatorFactory::UnitCategory;

    Nation nation = Nation::Roman;
    if (unit_comp != nullptr) {
      if (unit_comp->nation_id == Game::Systems::NationID::Carthage) {
        nation = Nation::Carthage;
      }
    }

    UnitCategory category =
        is_mounted_spawn ? UnitCategory::Cavalry : UnitCategory::Infantry;

    if (unit_comp != nullptr &&
        unit_comp->spawn_type == Game::Units::SpawnType::Builder &&
        anim.is_constructing) {
      category = UnitCategory::BuilderConstruction;
    }

    formation_calculator =
        FormationCalculatorFactory::get_calculator(nation, category);
  }

  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  s_render_stats.soldiers_total += visible_count;

  for (int idx = 0; idx < visible_count; ++idx) {
    int const r = idx / cols;
    int const c = idx % cols;

    float offset_x = 0.0F;
    float offset_z = 0.0F;

    uint32_t inst_seed = seed ^ uint32_t(idx * 9176U);

    uint32_t rng_state = inst_seed;

    float vertical_jitter = 0.0F;
    float yaw_offset = 0.0F;
    float phase_offset = 0.0F;

    if (!ctx.force_single_soldier) {
      FormationOffset const formation_offset =
          formation_calculator->calculate_offset(idx, r, c, rows, cols,
                                                 formation.spacing, seed);

      offset_x = formation_offset.offset_x;
      offset_z = formation_offset.offset_z;

      vertical_jitter = (fast_random(rng_state) - 0.5F) * 0.03F;
      yaw_offset = (fast_random(rng_state) - 0.5F) * 5.0F;
      phase_offset = fast_random(rng_state) * 0.25F;
    }

    if (anim.is_attacking && anim.is_melee) {
      float const combat_jitter_x =
          (fast_random(rng_state) - 0.5F) * formation.spacing * 0.4F;
      float const combat_jitter_z =
          (fast_random(rng_state) - 0.5F) * formation.spacing * 0.3F;
      float const sway_time = anim.time + phase_offset * 2.0F;
      float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
      float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
      offset_x += combat_jitter_x + sway_x;
      offset_z += combat_jitter_z + sway_z;
    }

    float applied_vertical_jitter = vertical_jitter;
    float applied_yaw_offset = yaw_offset;

    if (anim.is_attacking && anim.is_melee) {
      float const combat_yaw = (fast_random(rng_state) - 0.5F) * 15.0F;
      applied_yaw_offset += combat_yaw;
    }

    QMatrix4x4 inst_model;
    float applied_yaw = applied_yaw_offset;

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x, transform_comp->position.y,
                  transform_comp->position.z);
      m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      m.scale(transform_comp->scale.x, transform_comp->scale.y,
              transform_comp->scale.z);
      m.translate(offset_x, applied_vertical_jitter, offset_z);
      if (!ctx.skip_ground_offset && entity_ground_offset != 0.0F) {
        m.translate(0.0F, -entity_ground_offset, 0.0F);
      }
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, applied_vertical_jitter, offset_z);
      if (!ctx.skip_ground_offset && entity_ground_offset != 0.0F) {
        inst_model.translate(0.0F, -entity_ground_offset, 0.0F);
      }
    }

    QVector3D const soldier_world_pos =
        inst_model.map(QVector3D(0.0F, 0.0F, 0.0F));

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      continue;
    }

    HumanoidLOD soldier_lod = HumanoidLOD::Full;
    float soldier_distance = 0.0F;
    if (ctx.force_humanoid_lod) {
      soldier_lod = ctx.forced_humanoid_lod;
    } else if (ctx.camera != nullptr) {
      soldier_distance =
          (soldier_world_pos - ctx.camera->get_position()).length();
      soldier_lod = calculate_humanoid_lod(soldier_distance);

      if (soldier_lod == HumanoidLOD::Billboard) {
        ++s_render_stats.soldiers_skipped_lod;
        continue;
      }

      soldier_lod =
          Render::VisibilityBudgetTracker::instance().request_humanoid_lod(
              soldier_lod);
    }

    if (soldier_distance > 0.0F) {
      if (soldier_lod == HumanoidLOD::Reduced &&
          soldier_distance > k_temporal_skip_distance_reduced) {
        if (!should_render_temporal(s_current_frame, inst_seed,
                                    k_temporal_skip_period_reduced)) {
          ++s_render_stats.soldiers_skipped_temporal;
          continue;
        }
      }
      if (soldier_lod == HumanoidLOD::Minimal &&
          soldier_distance > k_temporal_skip_distance_minimal) {
        if (!should_render_temporal(s_current_frame, inst_seed,
                                    k_temporal_skip_period_minimal)) {
          ++s_render_stats.soldiers_skipped_temporal;
          continue;
        }
      }
    }

    ++s_render_stats.soldiers_rendered;

    DrawContext inst_ctx{ctx.resources, ctx.entity, ctx.world, inst_model};
    inst_ctx.selected = ctx.selected;
    inst_ctx.hovered = ctx.hovered;
    inst_ctx.animation_time = ctx.animation_time;
    inst_ctx.renderer_id = ctx.renderer_id;
    inst_ctx.backend = ctx.backend;
    inst_ctx.camera = ctx.camera;

    VariationParams variation = VariationParams::fromSeed(inst_seed);
    adjust_variation(inst_ctx, inst_seed, variation);

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling ||
        std::abs(variation.height_scale - 1.0F) > 0.001F) {
      QMatrix4x4 scale_matrix;
      scale_matrix.scale(variation.bulk_scale, combined_height_scale, 1.0F);
      inst_ctx.model = inst_ctx.model * scale_matrix;
    }

    HumanoidPose pose;
    bool used_cached_pose = false;

    PoseCacheKey cache_key =
        make_pose_cache_key(reinterpret_cast<uintptr_t>(ctx.entity), idx);

    auto cache_it = s_pose_cache.find(cache_key);
    const bool allow_cached_pose = (!anim.is_moving) || ctx.animation_throttled;
    if (allow_cached_pose && cache_it != s_pose_cache.end()) {
      CachedPoseEntry &cached = cache_it->second;
      if ((ctx.animation_throttled || !cached.was_moving) &&
          s_current_frame - cached.frame_number < k_pose_cache_max_age) {
        pose = cached.pose;
        variation = cached.variation;
        cached.frame_number = s_current_frame;
        used_cached_pose = true;
        ++s_render_stats.poses_cached;
      }
    }

    if (!used_cached_pose) {

      bool used_palette = false;
      if (!anim.is_moving && !anim.is_attacking &&
          PosePaletteCache::instance().is_generated() &&
          (soldier_lod == HumanoidLOD::Reduced ||
           soldier_lod == HumanoidLOD::Minimal)) {
        PosePaletteKey palette_key;
        palette_key.state = AnimState::Idle;
        palette_key.frame = 0;
        palette_key.is_moving = false;
        const auto *palette_entry =
            PosePaletteCache::instance().get(palette_key);
        if (palette_entry != nullptr) {
          pose = palette_entry->pose;
          used_palette = true;
          ++s_render_stats.poses_cached;
        }
      }

      if (!used_palette) {
        compute_locomotion_pose(inst_seed, anim.time + phase_offset,
                                anim.is_moving, variation, pose);
        ++s_render_stats.poses_computed;
      }

      CachedPoseEntry &entry = s_pose_cache[cache_key];
      entry.pose = pose;
      entry.variation = variation;
      entry.frame_number = s_current_frame;
      entry.was_moving = anim.is_moving;
    }

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = anim;
    anim_ctx.variation = variation;
    anim_ctx.formation = formation;
    anim_ctx.jitter_seed = phase_offset;
    anim_ctx.instance_position =
        inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    float yaw_rad = qDegreesToRadians(applied_yaw);
    QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    if (forward.lengthSquared() > 1e-8F) {
      forward.normalize();
    } else {
      forward = QVector3D(0.0F, 0.0F, 1.0F);
    }
    QVector3D up(0.0F, 1.0F, 0.0F);
    QVector3D right = QVector3D::crossProduct(up, forward);
    if (right.lengthSquared() > 1e-8F) {
      right.normalize();
    } else {
      right = QVector3D(1.0F, 0.0F, 0.0F);
    }

    anim_ctx.entity_forward = forward;
    anim_ctx.entity_right = right;
    anim_ctx.entity_up = up;
    anim_ctx.locomotion_direction = forward;
    anim_ctx.yaw_degrees = applied_yaw;
    anim_ctx.yaw_radians = yaw_rad;
    anim_ctx.has_movement_target = false;
    anim_ctx.move_speed = 0.0F;
    anim_ctx.movement_target = QVector3D(0.0F, 0.0F, 0.0F);

    if (movement_comp != nullptr) {
      QVector3D velocity(movement_comp->vx, 0.0F, movement_comp->vz);
      float speed = velocity.length();
      anim_ctx.move_speed = speed;
      if (speed > 1e-4F) {
        anim_ctx.locomotion_direction = velocity.normalized();
      }
      anim_ctx.has_movement_target = movement_comp->has_target;
      anim_ctx.movement_target =
          QVector3D(movement_comp->target_x, 0.0F, movement_comp->target_y);
    }

    anim_ctx.locomotion_velocity =
        anim_ctx.locomotion_direction * anim_ctx.move_speed;
    anim_ctx.motion_state = classify_motion_state(anim, anim_ctx.move_speed);
    anim_ctx.gait.state = anim_ctx.motion_state;
    anim_ctx.gait.speed = anim_ctx.move_speed;
    anim_ctx.gait.velocity = anim_ctx.locomotion_velocity;
    anim_ctx.gait.has_target = anim_ctx.has_movement_target;
    anim_ctx.gait.is_airborne = false;

    float reference_speed = (anim_ctx.gait.state == HumanoidMotionState::Run)
                                ? k_reference_run_speed
                                : k_reference_walk_speed;
    if (reference_speed > 0.0001F) {
      anim_ctx.gait.normalized_speed =
          std::clamp(anim_ctx.gait.speed / reference_speed, 0.0F, 1.0F);
    } else {
      anim_ctx.gait.normalized_speed = 0.0F;
    }
    if (!anim.is_moving) {
      anim_ctx.gait.normalized_speed = 0.0F;
    }

    if (anim.is_moving) {
      float stride_base = 0.8F;
      if (anim_ctx.motion_state == HumanoidMotionState::Run) {
        stride_base = 0.45F;
      }
      float const base_cycle =
          stride_base / std::max(0.1F, variation.walk_speed_mult);
      anim_ctx.locomotion_cycle_time = base_cycle;
      anim_ctx.locomotion_phase = std::fmod(
          (anim.time + phase_offset) / std::max(0.001F, base_cycle), 1.0F);
      anim_ctx.gait.cycle_time = anim_ctx.locomotion_cycle_time;
      anim_ctx.gait.cycle_phase = anim_ctx.locomotion_phase;
      anim_ctx.gait.stride_distance =
          anim_ctx.gait.speed * anim_ctx.gait.cycle_time;
    } else {
      anim_ctx.locomotion_cycle_time = 0.0F;
      anim_ctx.locomotion_phase = 0.0F;
      anim_ctx.gait.cycle_time = 0.0F;
      anim_ctx.gait.cycle_phase = 0.0F;
      anim_ctx.gait.stride_distance = 0.0F;
    }
    if (anim.is_attacking) {
      float const attack_offset = phase_offset * 1.5F;
      anim_ctx.attack_phase = std::fmod(anim.time + attack_offset, 1.0F);
      if (ctx.has_attack_variant_override) {
        anim_ctx.inputs.attack_variant = ctx.attack_variant_override;
      } else {
        anim_ctx.inputs.attack_variant =
            static_cast<std::uint8_t>(inst_seed % 3);
      }
    }

    customize_pose(inst_ctx, anim_ctx, inst_seed, pose);

    // Stage 15 — apply clip-driven idle overlays (sway, breathing) via
    // the per-entity state machine. This is the live consumer of the
    // Stage 12 Clip/StateMachine infrastructure. Overlays are applied
    // after customize_pose so rig-specific overrides win, and before
    // IK so downstream pose_controller passes see the adjusted torso.
    if (ctx.entity != nullptr) {
      auto const entity_id =
          static_cast<std::uint32_t>(ctx.entity->get_id());
      auto &cache_entry =
          ::Render::Humanoid::ClipDriverCache::instance().get_or_create(
              entity_id);
      float const now = anim.time + phase_offset;
      float dt = 0.0F;
      if (cache_entry.initialised) {
        dt = now - cache_entry.last_time;
        if (dt < 0.0F || dt > 1.0F) {
          dt = 0.0F; // clip skips / rewinds — don't advance blend
        }
      } else {
        cache_entry.initialised = true;
      }
      cache_entry.last_time = now;
      cache_entry.driver.tick(dt, anim);
      auto const overlays = cache_entry.driver.sample(now);
      ::Render::Humanoid::apply_overlays_to_pose(pose, overlays,
                                                  anim_ctx.entity_right);
    }

    if (!anim.is_moving && !anim.is_attacking) {
      HumanoidPoseController pose_ctrl(pose, anim_ctx);

      pose_ctrl.apply_micro_idle(anim.time + phase_offset, inst_seed);

      if (visible_count > 0) {
        auto hash_u32 = [](std::uint32_t x) -> std::uint32_t {
          x ^= x >> 16;
          x *= 0x7feb352dU;
          x ^= x >> 15;
          x *= 0x846ca68bU;
          x ^= x >> 16;
          return x;
        };

        constexpr float k_ambient_anim_duration = 6.0F;
        constexpr float k_unit_cycle_base = 15.0F;
        constexpr float k_unit_cycle_range = 10.0F;

        float const unit_cycle_period =
            k_unit_cycle_base +
            static_cast<float>(seed % 1000U) / (1000.0F / k_unit_cycle_range);
        float const unit_time_in_cycle =
            std::fmod(anim.time, std::max(0.001F, unit_cycle_period));
        std::uint32_t const unit_cycle_number = static_cast<std::uint32_t>(
            anim.time / std::max(0.001F, unit_cycle_period));

        int const max_slots = std::min(2, visible_count);
        std::uint32_t const cycle_rng =
            hash_u32(seed ^ (unit_cycle_number * 0x9e3779b9U));
        int const slot_count =
            (max_slots <= 0)
                ? 0
                : (1 + static_cast<int>(cycle_rng %
                                        static_cast<std::uint32_t>(max_slots)));

        int const idx_a =
            static_cast<int>(hash_u32(cycle_rng ^ 0xA341316CU) %
                             static_cast<std::uint32_t>(visible_count));
        int idx_b = static_cast<int>(hash_u32(cycle_rng ^ 0xC8013EA4U) %
                                     static_cast<std::uint32_t>(visible_count));
        if (visible_count > 1 && idx_b == idx_a) {
          idx_b = (idx_a + 1 +
                   static_cast<int>(cycle_rng % static_cast<std::uint32_t>(
                                                    visible_count - 1))) %
                  visible_count;
        }

        auto pick_idle_type = [&](std::uint32_t v) -> AmbientIdleType {
          std::uint32_t const roll = v % 100U;
          if (roll < 18U) {
            return AmbientIdleType::SitDown;
          }
          if (roll < 36U) {
            return AmbientIdleType::ShiftWeight;
          }
          if (roll < 52U) {
            return AmbientIdleType::ShuffleFeet;
          }
          if (roll < 66U) {
            return AmbientIdleType::TapFoot;
          }
          if (roll < 78U) {
            return AmbientIdleType::StepInPlace;
          }
          if (roll < 90U) {
            return AmbientIdleType::BendKnee;
          }
          if (roll < 98U) {
            return AmbientIdleType::RaiseWeapon;
          }
          return AmbientIdleType::Jump;
        };

        AmbientIdleType const unit_idle_type =
            pick_idle_type(hash_u32(cycle_rng ^ 0x3C6EF372U));

        if (slot_count > 0 && unit_time_in_cycle <= k_ambient_anim_duration) {
          bool const is_active =
              (idx == idx_a) || (slot_count > 1 && idx == idx_b);
          if (is_active) {
            float const phase = unit_time_in_cycle / k_ambient_anim_duration;
            pose_ctrl.apply_ambient_idle_explicit(unit_idle_type, phase);
          }
        }
      }
    }

    if (anim_ctx.motion_state == HumanoidMotionState::Run) {

      float const run_lean = 0.10F;
      pose.pelvis_pos.setZ(pose.pelvis_pos.z() - 0.05F);
      pose.shoulder_l.setZ(pose.shoulder_l.z() + run_lean);
      pose.shoulder_r.setZ(pose.shoulder_r.z() + run_lean);
      pose.neck_base.setZ(pose.neck_base.z() + run_lean * 0.7F);
      pose.head_pos.setZ(pose.head_pos.z() + run_lean * 0.5F);

      pose.pelvis_pos.setY(pose.pelvis_pos.y() - 0.03F);
      pose.shoulder_l.setY(pose.shoulder_l.y() - 0.04F);
      pose.shoulder_r.setY(pose.shoulder_r.y() - 0.04F);

      float const run_stride_mult = 1.5F;
      float const phase = anim_ctx.locomotion_phase;
      float const left_phase = phase;
      float const right_phase = std::fmod(phase + 0.5F, 1.0F);

      auto enhance_run_foot = [&](QVector3D &foot, float foot_phase) {
        float const lift_raw =
            std::sin(foot_phase * 2.0F * std::numbers::pi_v<float>);
        if (lift_raw > 0.0F) {

          float const extra_lift = lift_raw * k_run_extra_foot_lift;
          foot.setY(foot.y() + extra_lift);

          float const stride_enhance = std::sin((foot_phase - 0.25F) * 2.0F *
                                                std::numbers::pi_v<float>) *
                                       k_run_stride_enhancement;
          foot.setZ(foot.z() + stride_enhance);
        }
      };

      enhance_run_foot(pose.foot_l, left_phase);
      enhance_run_foot(pose.foot_r, right_phase);

      float const run_arm_swing = 0.06F;
      constexpr float max_run_arm_displacement = 0.08F;
      float const left_swing_raw =
          std::sin((left_phase + 0.1F) * 2.0F * std::numbers::pi_v<float>);
      float const left_arm_phase =
          std::clamp(left_swing_raw * run_arm_swing, -max_run_arm_displacement,
                     max_run_arm_displacement);
      float const right_swing_raw =
          std::sin((right_phase + 0.1F) * 2.0F * std::numbers::pi_v<float>);
      float const right_arm_phase =
          std::clamp(right_swing_raw * run_arm_swing, -max_run_arm_displacement,
                     max_run_arm_displacement);

      pose.hand_l.setZ(pose.hand_l.z() - left_arm_phase);
      pose.hand_r.setZ(pose.hand_r.z() - right_arm_phase);

      pose.hand_l.setY(pose.hand_l.y() + 0.02F);
      pose.hand_r.setY(pose.hand_r.y() + 0.02F);

      {
        using HP = HumanProportions;
        float const h_scale = variation.height_scale;
        float const max_reach =
            (HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN) * h_scale * 0.98F;

        auto clamp_hand = [&](const QVector3D &shoulder, QVector3D &hand) {
          QVector3D diff = hand - shoulder;
          float const len = diff.length();
          if (len > max_reach && len > 1e-6F) {
            hand = shoulder + diff * (max_reach / len);
          }
        };
        clamp_hand(pose.shoulder_l, pose.hand_l);
        clamp_hand(pose.shoulder_r, pose.hand_r);

        QVector3D right_axis = pose.shoulder_r - pose.shoulder_l;
        right_axis.setY(0.0F);
        if (right_axis.lengthSquared() < 1e-8F) {
          right_axis = QVector3D(1.0F, 0.0F, 0.0F);
        }
        right_axis.normalize();

        if (right_axis.x() < 0.0F) {
          right_axis = -right_axis;
        }
        QVector3D const outward_l = -right_axis;
        QVector3D const outward_r = right_axis;

        pose.elbow_l = elbow_bend_torso(pose.shoulder_l, pose.hand_l, outward_l,
                                        0.45F, 0.15F, -0.08F, +1.0F);
        pose.elbow_r = elbow_bend_torso(pose.shoulder_r, pose.hand_r, outward_r,
                                        0.48F, 0.12F, 0.02F, +1.0F);
      }

      float const hip_rotation_raw =
          std::sin(phase * 2.0F * std::numbers::pi_v<float>);
      float const hip_rotation = hip_rotation_raw * 0.003F;
      pose.pelvis_pos.setX(pose.pelvis_pos.x() + hip_rotation);

      float const torso_sway_z = 0.0F;
      pose.shoulder_l.setZ(pose.shoulder_l.z() + torso_sway_z);
      pose.shoulder_r.setZ(pose.shoulder_r.z() + torso_sway_z);
      pose.neck_base.setZ(pose.neck_base.z() + torso_sway_z * 0.7F);
      pose.head_pos.setZ(pose.head_pos.z() + torso_sway_z * 0.5F);

      if (pose.head_frame.radius > 0.001F) {
        QVector3D head_up = pose.head_pos - pose.neck_base;
        if (head_up.lengthSquared() < 1e-8F) {
          head_up = pose.head_frame.up;
        } else {
          head_up.normalize();
        }

        QVector3D head_right =
            pose.head_frame.right -
            head_up * QVector3D::dotProduct(pose.head_frame.right, head_up);
        if (head_right.lengthSquared() < 1e-8F) {
          head_right =
              QVector3D::crossProduct(head_up, anim_ctx.entity_forward);
          if (head_right.lengthSquared() < 1e-8F) {
            head_right = QVector3D(1.0F, 0.0F, 0.0F);
          }
        }
        head_right.normalize();
        QVector3D head_forward =
            QVector3D::crossProduct(head_right, head_up).normalized();

        pose.head_frame.origin = pose.head_pos;
        pose.head_frame.up = head_up;
        pose.head_frame.right = head_right;
        pose.head_frame.forward = head_forward;
        pose.body_frames.head = pose.head_frame;
      }
    }

    const auto &gfx_settings = Render::GraphicsSettings::instance();
    const bool should_render_shadow =
        ctx.allow_template_cache && gfx_settings.shadows_enabled() &&
        (soldier_lod == HumanoidLOD::Full ||
         soldier_lod == HumanoidLOD::Reduced) &&
        soldier_distance < gfx_settings.shadow_max_distance();

    if (should_render_shadow && inst_ctx.backend != nullptr &&
        inst_ctx.resources != nullptr) {
      auto *shadow_shader =
          inst_ctx.backend->shader(QStringLiteral("troop_shadow"));
      auto *quad_mesh = inst_ctx.resources->quad();

      if (shadow_shader != nullptr && quad_mesh != nullptr) {

        float const shadow_size =
            is_mounted_spawn ? k_shadow_size_mounted : k_shadow_size_infantry;
        float depth_boost = 1.0F;
        float width_boost = 1.0F;
        if (unit_comp != nullptr) {
          using Game::Units::SpawnType;
          switch (unit_comp->spawn_type) {
          case SpawnType::Spearman:
            depth_boost = 1.8F;
            width_boost = 0.95F;
            break;
          case SpawnType::HorseSpearman:
            depth_boost = 2.1F;
            width_boost = 1.05F;
            break;
          case SpawnType::Archer:
          case SpawnType::HorseArcher:
            depth_boost = 1.2F;
            width_boost = 0.95F;
            break;
          default:
            break;
          }
        }

        float const shadow_width =
            shadow_size * (is_mounted_spawn ? 1.05F : 1.0F) * width_boost;
        float const shadow_depth =
            shadow_size * (is_mounted_spawn ? 1.30F : 1.10F) * depth_boost;

        auto &terrain_service = Game::Map::TerrainService::instance();

        if (terrain_service.is_initialized()) {

          QVector3D const inst_pos =
              inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
          float const shadow_y =
              terrain_service.get_terrain_height(inst_pos.x(), inst_pos.z());

          QVector3D light_dir = k_shadow_light_dir.normalized();
          QVector2D light_dir_xz(light_dir.x(), light_dir.z());
          if (light_dir_xz.lengthSquared() < 1e-6F) {
            light_dir_xz = QVector2D(0.0F, 1.0F);
          } else {
            light_dir_xz.normalize();
          }
          QVector2D const shadow_dir = -light_dir_xz;
          QVector2D dir_for_use = shadow_dir;
          if (dir_for_use.lengthSquared() < 1e-6F) {
            dir_for_use = QVector2D(0.0F, 1.0F);
          } else {
            dir_for_use.normalize();
          }
          float const shadow_offset = shadow_depth * 1.25F;
          QVector2D const offset_2d = dir_for_use * shadow_offset;
          float const light_yaw_deg = qRadiansToDegrees(
              std::atan2(double(dir_for_use.x()), double(dir_for_use.y())));

          QMatrix4x4 shadow_model;
          shadow_model.translate(inst_pos.x() + offset_2d.x(),
                                 shadow_y + k_shadow_ground_offset,
                                 inst_pos.z() + offset_2d.y());
          shadow_model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
          shadow_model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
          shadow_model.scale(shadow_width, shadow_depth, 1.0F);

          if (auto *renderer = dynamic_cast<Renderer *>(&out)) {
            Shader *previous_shader = renderer->get_current_shader();
            renderer->set_current_shader(shadow_shader);
            shadow_shader->set_uniform(QStringLiteral("u_lightDir"),
                                       dir_for_use);

            Render::Humanoid::HumanoidFullPrim const shadow_prim{
                quad_mesh, shadow_model, QVector3D(0.0F, 0.0F, 0.0F),
                k_shadow_base_alpha, 0};
            Render::Humanoid::submit_humanoid_full_prims(
                std::span<const Render::Humanoid::HumanoidFullPrim>(
                    &shadow_prim, 1U),
                out);

            renderer->set_current_shader(previous_shader);
          }
        }
      }
    }

    switch (soldier_lod) {
    case HumanoidLOD::Full: {

      ++s_render_stats.lod_full;

      // Compute the per-entity body metrics + frames that armor and
      // attachment renderers read out of pose.body_frames. The metrics
      // encode proportion_scaling / torso_scale so attachments still
      // align with the soldier's silhouette even though the baked Full
      // mesh was authored at the static baseline (Stage 15.5d deferral).
      Render::Humanoid::HumanoidBodyMetrics metrics{};
      Render::Humanoid::compute_humanoid_body_metrics(
          pose, get_proportion_scaling(), get_torso_scale(), metrics);
      Render::Humanoid::compute_humanoid_head_frame(pose, metrics);
      Render::Humanoid::compute_humanoid_body_frames(pose, metrics);

      Render::Humanoid::submit_humanoid_lod(
          pose, variant, Render::Creature::CreatureLOD::Full, inst_ctx.model,
          out);

      // Overridable helpers — nations plug armor overlay, shoulder
      // decorations and helmets in here. Base impls are empty; these
      // all route through DrawPartCmd when a nation overrides them.
      draw_armor_overlay(inst_ctx, variant, pose, metrics.y_top_cover,
                         metrics.torso_r, metrics.shoulder_half_span,
                         metrics.upper_arm_r, metrics.right_axis, out);
      draw_shoulder_decorations(inst_ctx, variant, pose, metrics.y_top_cover,
                                pose.neck_base.y(), metrics.right_axis, out);
      draw_helmet(inst_ctx, variant, pose, out);

      // Facial hair stays imperative — RNG-procedural per entity seed;
      // baking it would require keying the RiggedMeshCache by a seed
      // bucket (the cache key has `variant_bucket` room for it; see
      // Stage 15.5e+).
      draw_facial_hair(inst_ctx, variant, pose, out);
      draw_armor(inst_ctx, variant, pose, anim_ctx, out);
      add_attachments(inst_ctx, variant, pose, anim_ctx, out);
      break;
    }

    case HumanoidLOD::Reduced:

      ++s_render_stats.lod_reduced;
      Render::Humanoid::submit_humanoid_lod(
          pose, variant, Render::Creature::CreatureLOD::Reduced,
          inst_ctx.model, out);
      draw_armor(inst_ctx, variant, pose, anim_ctx, out);
      add_attachments(inst_ctx, variant, pose, anim_ctx, out);
      break;

    case HumanoidLOD::Minimal:

      ++s_render_stats.lod_minimal;
      Render::Humanoid::submit_humanoid_lod(
          pose, variant, Render::Creature::CreatureLOD::Minimal,
          inst_ctx.model, out);
      break;

    case HumanoidLOD::Billboard:

      break;
    }
  }
}

auto get_humanoid_render_stats() -> const HumanoidRenderStats & {
  return s_render_stats;
}

void reset_humanoid_render_stats() { s_render_stats.reset(); }

namespace detail {
void increment_facial_hair_skipped_distance() {
  ++s_render_stats.facial_hair_skipped_distance;
}
} // namespace detail

} // namespace Render::GL
