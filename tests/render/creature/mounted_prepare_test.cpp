

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <unordered_map>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/units/spawn_type.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/entity/horse_archer_renderer_base.h"
#include "render/entity/horse_spearman_renderer_base.h"
#include "render/entity/mounted_humanoid_renderer_base.h"
#include "render/entity/mounted_knight_renderer_base.h"
#include "render/entity/mounted_prepare.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/horse/armor/champion_renderer.h"
#include "render/equipment/horse/armor/crupper_renderer.h"
#include "render/equipment/horse/armor/scale_barding_renderer.h"
#include "render/equipment/horse/decorations/saddle_bag_renderer.h"
#include "render/equipment/horse/saddles/carthage_saddle_renderer.h"
#include "render/equipment/horse/saddles/roman_saddle_renderer.h"
#include "render/equipment/horse/tack/blanket_renderer.h"
#include "render/equipment/horse/tack/bridle_renderer.h"
#include "render/equipment/horse/tack/reins_renderer.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/cache_control.h"
#include "render/humanoid/pose_cache_components.h"
#include "render/humanoid/prepare.h"
#include "render/humanoid/skeleton.h"
#include "render/submitter.h"
#include "render/template_cache.h"
#include "tests/render/test_asset_paths.h"

namespace {

class NullSubmitter : public Render::GL::ISubmitter {
public:
  int rigged_calls{0};
  int meshes{0};
  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++meshes;
  }
  void rigged(const Render::GL::RiggedCreatureCmd&) override { ++rigged_calls; }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

class InspectableMountedKnightRenderer : public Render::GL::MountedKnightRendererBase {
public:
  using Render::GL::MountedHumanoidRendererBase::resolve_mount_render_state;
  using Render::GL::MountedKnightRendererBase::MountedKnightRendererBase;
};

auto test_equipment_role_only_attachment(std::uint8_t)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {};
}

auto rider_root_matrix(Render::Creature::ArchetypeId archetype_id,
                       Render::Creature::Pipeline::CreatureAssetId asset_id,
                       Render::Creature::AnimationStateId state,
                       float phase,
                       std::uint8_t clip_variant) -> std::optional<QMatrix4x4> {
  using Render::Creature::ArchetypeDescriptor;
  auto const base_clip =
      Render::Creature::ArchetypeRegistry::instance().bpat_clip(archetype_id, state);
  if (base_clip == ArchetypeDescriptor::k_unmapped_clip) {
    return std::nullopt;
  }

  auto const clip_id = static_cast<std::uint16_t>(base_clip + clip_variant);
  auto const* asset =
      Render::Creature::Pipeline::CreatureAssetRegistry::instance().get(asset_id);
  std::uint32_t const species_id = asset != nullptr
                                       ? asset->bpat_species_id
                                       : Render::Creature::Bpat::k_species_humanoid;
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  float normalized_phase = phase;
  if (!clip.loops && normalized_phase >= 1.0F) {
    normalized_phase = 1.0F;
  } else {
    normalized_phase -= std::floor(normalized_phase);
    if (normalized_phase < 0.0F) {
      normalized_phase += 1.0F;
    }
  }

  int frame_idx =
      (!clip.loops && normalized_phase >= 1.0F)
          ? static_cast<int>(clip.frame_count - 1U)
          : static_cast<int>(normalized_phase * static_cast<float>(clip.frame_count));
  frame_idx = std::clamp(frame_idx, 0, static_cast<int>(clip.frame_count) - 1);
  auto const palette = blob->frame_palette_view(clip.frame_offset +
                                                static_cast<std::uint32_t>(frame_idx));
  auto const root_index =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Root);
  if (palette.size() <= root_index) {
    return std::nullopt;
  }
  return palette[root_index];
}

TEST(MountedPrepare, ProducesHorseMountAndHumanoidRiderRows) {
  using namespace Render::Creature::Pipeline;

  MountedSpec mounted{};
  mounted.mount.kind = CreatureKind::Horse;
  mounted.mount.debug_name = "test/horse";
  mounted.rider.kind = CreatureKind::Humanoid;
  mounted.rider.debug_name = "test/rider";

  Render::Horse::HorseSpecPose const mount_pose{};
  Render::GL::HorseVariant const mount_variant{};
  Render::GL::HumanoidPose const rider_pose{};
  Render::GL::HumanoidVariant const rider_variant{};
  Render::GL::HumanoidAnimationContext const rider_anim{};
  QMatrix4x4 const mount_world;
  QMatrix4x4 const rider_world;

  auto set = Render::GL::prepare_mounted_rows(mounted,
                                              mount_world,
                                              rider_world,
                                              mount_pose,
                                              mount_variant,
                                              rider_pose,
                                              rider_variant,
                                              rider_anim,
                                              99,
                                              Render::Creature::CreatureLOD::Full,
                                              RenderPassIntent::Shadow);

  EXPECT_EQ(set.mount_row.spec.kind, CreatureKind::Horse);
  EXPECT_EQ(set.rider_row.spec.kind, CreatureKind::Humanoid);
  EXPECT_EQ(set.mount_row.pass, RenderPassIntent::Shadow);
  EXPECT_EQ(set.rider_row.pass, RenderPassIntent::Shadow);
  EXPECT_EQ(set.mount_row.seed, 99U);
  EXPECT_EQ(set.rider_row.seed, 99U);
}

TEST(MountedPrepare, ShadowPairProducesNoDrawCalls) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  const auto stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 0U);
  EXPECT_EQ(sink.rigged_calls, 0);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(MountedPrepare, MainPairProducesTwoEntitySubmissions) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  const auto stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 2U);
}

TEST(MountedPrepare, HorseMountArchetypeUsesHandleBackedEquipmentLoadout) {
  Render::GL::register_built_in_equipment();

  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/horse_swordsman");
  ASSERT_TRUE(loadout.found);

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;
  cfg.horse_saddle_handle = loadout.horse_saddle_handle;
  cfg.horse_bridle_handle = loadout.horse_bridle_handle;
  cfg.horse_reins_handle = loadout.horse_reins_handle;
  cfg.horse_blanket_handle = loadout.horse_blanket_handle;
  cfg.horse_barding_handle = loadout.horse_barding_handle;
  cfg.horse_crupper_handle = loadout.horse_crupper_handle;
  cfg.horse_decoration_handle = loadout.horse_decoration_handle;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  const auto mount_archetype_id = renderer.mounted_visual_spec().mount.archetype_id;
  ASSERT_NE(mount_archetype_id, Render::Creature::k_invalid_archetype);

  const auto* desc =
      Render::Creature::ArchetypeRegistry::instance().get(mount_archetype_id);
  ASSERT_NE(desc, nullptr);
  EXPECT_EQ(desc->species, Render::Creature::Pipeline::CreatureKind::Horse);
  EXPECT_EQ(desc->bake_attachment_count, 8U);
  EXPECT_EQ(desc->attachments_view()[0].archetype,
            &Render::GL::roman_saddle_archetype());
  EXPECT_EQ(desc->attachments_view()[1].archetype, &Render::GL::bridle_archetype());
  EXPECT_EQ(desc->attachments_view()[2].archetype, &Render::GL::reins_archetype());
  EXPECT_EQ(desc->attachments_view()[3].archetype, &Render::GL::blanket_archetype());
  EXPECT_EQ(desc->attachments_view()[4].archetype,
            &Render::GL::scale_barding_chest_archetype());
  EXPECT_EQ(desc->attachments_view()[5].archetype,
            &Render::GL::scale_barding_barrel_archetype());
  EXPECT_EQ(desc->attachments_view()[6].archetype,
            &Render::GL::scale_barding_neck_archetype());
  EXPECT_EQ(desc->attachments_view()[7].archetype, &Render::GL::crupper_archetype());
  EXPECT_EQ(desc->extra_role_color_fn_count, 6U);
  EXPECT_EQ(
      desc->role_count,
      Render::Creature::Pipeline::CreatureAssetRegistry::instance()
              .for_species(Render::Creature::Pipeline::CreatureKind::Horse)
              ->role_count +
          Render::GL::k_roman_saddle_role_count + Render::GL::k_bridle_role_count +
          Render::GL::k_reins_role_count + Render::GL::k_blanket_role_count +
          Render::GL::k_scale_barding_role_count + Render::GL::k_crupper_role_count);
}

TEST(MountedPrepare, RomanAndCarthageHorseMountArchetypesStayDistinct) {
  Render::GL::register_built_in_equipment();

  const auto roman_loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/roman/horse_archer");
  const auto carthage_loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_archer");
  ASSERT_TRUE(roman_loadout.found);
  ASSERT_TRUE(carthage_loadout.found);

  Render::GL::HorseArcherRendererConfig roman_cfg;
  roman_cfg.has_bow = false;
  roman_cfg.has_quiver = false;
  roman_cfg.has_cloak = false;
  roman_cfg.horse_saddle_handle = roman_loadout.horse_saddle_handle;
  roman_cfg.horse_bridle_handle = roman_loadout.horse_bridle_handle;
  roman_cfg.horse_reins_handle = roman_loadout.horse_reins_handle;
  roman_cfg.horse_blanket_handle = roman_loadout.horse_blanket_handle;
  roman_cfg.horse_barding_handle = roman_loadout.horse_barding_handle;
  roman_cfg.horse_crupper_handle = roman_loadout.horse_crupper_handle;
  roman_cfg.horse_decoration_handle = roman_loadout.horse_decoration_handle;

  Render::GL::HorseArcherRendererConfig carthage_cfg = roman_cfg;
  carthage_cfg.horse_saddle_handle = carthage_loadout.horse_saddle_handle;
  carthage_cfg.horse_bridle_handle = carthage_loadout.horse_bridle_handle;
  carthage_cfg.horse_reins_handle = carthage_loadout.horse_reins_handle;
  carthage_cfg.horse_blanket_handle = carthage_loadout.horse_blanket_handle;
  carthage_cfg.horse_barding_handle = carthage_loadout.horse_barding_handle;
  carthage_cfg.horse_crupper_handle = carthage_loadout.horse_crupper_handle;
  carthage_cfg.horse_decoration_handle = carthage_loadout.horse_decoration_handle;

  Render::GL::HorseArcherRendererBase const roman_renderer(roman_cfg);
  Render::GL::HorseArcherRendererBase const carthage_renderer(carthage_cfg);

  const auto roman_mount = roman_renderer.mounted_visual_spec().mount.archetype_id;
  const auto carthage_mount =
      carthage_renderer.mounted_visual_spec().mount.archetype_id;
  ASSERT_NE(roman_mount, Render::Creature::k_invalid_archetype);
  ASSERT_NE(carthage_mount, Render::Creature::k_invalid_archetype);
  EXPECT_NE(roman_mount, carthage_mount);

  const auto* roman_desc =
      Render::Creature::ArchetypeRegistry::instance().get(roman_mount);
  const auto* carthage_desc =
      Render::Creature::ArchetypeRegistry::instance().get(carthage_mount);
  ASSERT_NE(roman_desc, nullptr);
  ASSERT_NE(carthage_desc, nullptr);
  ASSERT_FALSE(roman_desc->attachments_view().empty());
  ASSERT_FALSE(carthage_desc->attachments_view().empty());
  EXPECT_EQ(roman_desc->attachments_view()[0].archetype,
            &Render::GL::roman_saddle_archetype());
  EXPECT_EQ(carthage_desc->attachments_view()[0].archetype,
            &Render::GL::carthage_saddle_archetype());
  EXPECT_EQ(roman_desc->attachments_view().back().archetype,
            &Render::GL::saddle_bag_archetype());
  EXPECT_EQ(carthage_desc->attachments_view().back().archetype,
            &Render::GL::saddle_bag_archetype());
}

TEST(MountedPrepare, TemplatePrewarmRenderWarmsMountedSnapshotCache) {
  auto const root = TestAssets::find_creature_assets_dir("horse.bpat");
  ASSERT_FALSE(root.empty());
  auto& snapshots = Render::Creature::Snapshot::SnapshotMeshRegistry::instance();
  ASSERT_TRUE(snapshots.load_species(Render::Creature::Bpat::k_species_horse,
                                     Render::Creature::CreatureLOD::Minimal,
                                     root + "/horse_minimal.bpsm"))
      << snapshots.last_error();
  Render::GL::clear_humanoid_caches();
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->max_health = 100;
  unit->health = 100;
  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->visible = true;

  Render::GL::DrawContext ctx{};
  ctx.entity = &entity;
  ctx.template_prewarm = true;
  ctx.force_single_soldier = true;

  Render::GL::TemplateRecorder recorder;
  recorder.snapshot_mesh_cache().clear();
  renderer.render(ctx, recorder);

  EXPECT_GE(recorder.snapshot_mesh_cache().size(), 2U);
  EXPECT_TRUE(recorder.commands().empty());
}

TEST(MountedPrepare, MountedHumanoidPreparationQueuesRiderAndHorseBodies) {
  using namespace Render::Creature::Pipeline;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  ASSERT_EQ(prep.bodies.size(), 2U);
  int rider_requests = 0;
  int horse_requests = 0;
  float rider_world_y = 0.0F;
  float horse_world_y = 0.0F;
  for (const auto& req : prep.bodies.requests()) {
    auto const species =
        Render::Creature::ArchetypeRegistry::instance().species(req.archetype);
    if (species == CreatureKind::Humanoid) {
      ++rider_requests;
      rider_world_y = req.world.column(3).y();
      EXPECT_TRUE(req.world_already_grounded);
    }
    if (species == CreatureKind::Horse) {
      ++horse_requests;
      horse_world_y = req.world.column(3).y();
    }
  }

  EXPECT_EQ(rider_requests, 1);
  EXPECT_EQ(horse_requests, 1);
  EXPECT_GT(std::abs(rider_world_y - horse_world_y), 0.01F);
  EXPECT_EQ(prep.bodies.requests().size(), 2U);
  EXPECT_TRUE(owns_slot(renderer.visual_spec().owned_legacy_slots,
                        LegacySlotMask::Attachments));
}

TEST(MountedPrepare, MountedRiderUsesMountedChargeStateForMeleeAttack) {
  Render::GL::HorseSpearmanRendererConfig cfg;
  cfg.has_spear = false;
  cfg.has_shield = false;

  Render::GL::HorseSpearmanRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::AnimationInputs anim{};
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Spear;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(rider_req, requests.end());
  EXPECT_EQ(rider_req->state, Render::Creature::AnimationStateId::RidingCharge);
}

TEST(MountedPrepare, MountedSwordAttackUsesMountedSwordStateWhileMoving) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::AnimationInputs anim{};
  anim.movement_state = Render::Creature::MovementAnimationState::Run;
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Sword;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(rider_req, requests.end());
  EXPECT_EQ(rider_req->state, Render::Creature::AnimationStateId::AttackSword);
  EXPECT_EQ(rider_req->clip_variant, 0U);
}

TEST(MountedPrepare, MountedSwordAttackRecoveryStaysOnOutgoingClipBeforeIdle) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  auto* persistent =
      entity.add_component<Render::Creature::HumanoidAnimationStateComponent>();
  ASSERT_NE(persistent, nullptr);
  ctx.entity = &entity;

  auto find_rider_request = [](const auto& requests) {
    return std::find_if(requests.begin(), requests.end(), [](const auto& req) {
      return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
             Render::Creature::Pipeline::CreatureKind::Humanoid;
    });
  };

  Render::GL::AnimationInputs attack_anim{};
  attack_anim.time = 2.0F;
  attack_anim.movement_state = Render::Creature::MovementAnimationState::Run;
  attack_anim.is_attacking = true;
  attack_anim.is_melee = true;
  attack_anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  attack_anim.combat_phase = Render::GL::CombatAnimPhase::Recover;
  attack_anim.combat_phase_progress = 0.30F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, attack_anim, 0, prep);

  auto const attack_req = find_rider_request(prep.bodies.requests());
  ASSERT_NE(attack_req, prep.bodies.requests().end());
  EXPECT_EQ(attack_req->state, Render::Creature::AnimationStateId::AttackSword);
  float const attack_phase = attack_req->phase;

  Render::GL::AnimationInputs recover_anim = attack_anim;
  recover_anim.time += 0.06F;
  recover_anim.is_attacking = false;
  recover_anim.combat_phase = Render::GL::CombatAnimPhase::Idle;
  recover_anim.combat_phase_progress = 0.0F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, recover_anim, 1, prep);

  auto const recover_req = find_rider_request(prep.bodies.requests());
  ASSERT_NE(recover_req, prep.bodies.requests().end());
  EXPECT_EQ(recover_req->state, Render::Creature::AnimationStateId::AttackSword);
  EXPECT_GT(recover_req->phase, attack_phase);

  Render::GL::AnimationInputs idle_anim = recover_anim;
  idle_anim.time += 0.28F;
  idle_anim.movement_state = Render::Creature::MovementAnimationState::Idle;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, idle_anim, 2, prep);

  auto const idle_req = find_rider_request(prep.bodies.requests());
  ASSERT_NE(idle_req, prep.bodies.requests().end());
  EXPECT_EQ(idle_req->state, Render::Creature::AnimationStateId::AttackSword);
  EXPECT_TRUE(persistent->combat_visual.active);

  Render::GL::AnimationInputs settled_anim = idle_anim;
  settled_anim.time += 0.20F;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, settled_anim, 3, prep);

  auto const settled_req = find_rider_request(prep.bodies.requests());
  ASSERT_NE(settled_req, prep.bodies.requests().end());
  EXPECT_NE(settled_req->state, Render::Creature::AnimationStateId::AttackSword);
}

TEST(MountedPrepare, HorseSpearmanShieldBuildsIntoRiderArchetype) {
  auto& equipment = Render::GL::EquipmentRegistry::instance();
  equipment.register_placeholder_equipment(Render::GL::EquipmentCategory::Weapon,
                                           "test_horse_spearman_shield");
  auto const shield_handle = equipment.resolve_handle(
      Render::GL::EquipmentCategory::Weapon, "test_horse_spearman_shield");
  ASSERT_NE(shield_handle, Render::GL::k_invalid_equipment_handle);
  Render::GL::register_humanoid_equipment_contribution(
      shield_handle,
      {.build_attachments = &test_equipment_role_only_attachment, .role_count = 1U});

  Render::GL::HorseSpearmanRendererConfig cfg;
  cfg.has_spear = false;
  cfg.has_shield = true;
  cfg.shield_handle = shield_handle;

  Render::GL::HorseSpearmanRendererBase const renderer(cfg);

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const* base_desc =
      registry.get(Render::Creature::ArchetypeRegistry::k_rider_base);
  auto const* rider_desc = registry.get(renderer.visual_spec().archetype_id);
  ASSERT_NE(base_desc, nullptr);
  ASSERT_NE(rider_desc, nullptr);
  EXPECT_GT(rider_desc->role_count, base_desc->role_count);
}

TEST(MountedPrepare, SubmitPreparationDrawsRiderFromPreparedPose) {
  using namespace Render::Creature::Pipeline;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_GT(sink.rigged_calls, 0);
}

TEST(MountedPrepare, MountedRiderRequestUsesAbsoluteSeatWorld) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  Render::GL::AnimationInputs const anim{};

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  ASSERT_EQ(requests.size(), 2U);

  auto const horse_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Horse;
      });
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(horse_req, requests.end());
  ASSERT_NE(rider_req, requests.end());
  bool invertible = false;
  QMatrix4x4 const rider_from_horse =
      horse_req->world.inverted(&invertible) * rider_req->world;
  ASSERT_TRUE(invertible);
  EXPECT_GT(rider_from_horse.column(3).toVector3D().length(), 0.0F);
}

TEST(MountedPrepare, MountedUnitGroupsRiderAndHorseBySharedWorldKey) {
  using Render::Creature::Pipeline::CreatureKind;

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  struct GroupCounts {
    std::size_t horse{0};
    std::size_t rider{0};
  };
  std::unordered_map<Render::Creature::WorldKey, GroupCounts> groups;
  std::size_t horse_count = 0;
  std::size_t rider_count = 0;

  for (const auto& req : requests) {
    auto& group = groups[req.world_key];
    if (registry.species(req.archetype) == CreatureKind::Horse) {
      ++horse_count;
      ++group.horse;
    } else if (registry.species(req.archetype) == CreatureKind::Humanoid) {
      ++rider_count;
      ++group.rider;
    }
  }

  EXPECT_GT(horse_count, 1U);
  EXPECT_EQ(rider_count, horse_count);
  EXPECT_EQ(groups.size(), horse_count);
  for (const auto& [key, counts] : groups) {
    EXPECT_NE(key, 0U);
    EXPECT_EQ(counts.horse, 1U);
    EXPECT_EQ(counts.rider, 1U);
  }
}

TEST(MountedPrepare, MountedRiderRootAttachesToHorseSeatFrame) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid,
                                root + "/humanoid.bpat"));
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid_sword,
                                root + "/humanoid_sword.bpat"));

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;
  cfg.rider_creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;

  InspectableMountedKnightRenderer const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;
  ctx.force_single_soldier = true;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  auto const horse_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Horse;
      });
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(horse_req, requests.end());
  ASSERT_NE(rider_req, requests.end());

  Render::GL::HumanoidVariant variant{};
  std::uint32_t const seed =
      static_cast<std::uint32_t>(unit->owner_id * 2654435761U) ^
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&entity) &
                                 0xFFFFFFFFU);
  renderer.get_variant(ctx, seed, variant);

  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.inputs = anim;
  Render::GL::HorseProfile profile{};
  Render::GL::HorseDimensions dims{};
  Render::GL::MountedAttachmentFrame mount{};
  Render::GL::HorseMotionSample motion{};
  renderer.resolve_mount_render_state(
      ctx, seed, variant, anim_ctx, true, profile, dims, mount, motion);

  auto const root_matrix = rider_root_matrix(rider_req->archetype,
                                             rider_req->creature_asset_id,
                                             rider_req->state,
                                             rider_req->phase,
                                             rider_req->clip_variant);
  ASSERT_TRUE(root_matrix.has_value());

  bool invertible = false;
  QMatrix4x4 const rider_from_horse =
      horse_req->world.inverted(&invertible) * rider_req->world;
  ASSERT_TRUE(invertible);
  QMatrix4x4 const anchored_root = rider_from_horse * *root_matrix;
  QMatrix4x4 expected_seat;
  expected_seat.setColumn(0, QVector4D(mount.seat_right, 0.0F));
  expected_seat.setColumn(1, QVector4D(mount.seat_up, 0.0F));
  expected_seat.setColumn(2, QVector4D(mount.seat_forward, 0.0F));
  expected_seat.setColumn(3, QVector4D(mount.seat_position, 1.0F));

  for (int column = 0; column < 4; ++column) {
    auto const actual = anchored_root.column(column);
    auto const expected = expected_seat.column(column);
    EXPECT_NEAR(actual.x(), expected.x(), 1.0e-4F);
    EXPECT_NEAR(actual.y(), expected.y(), 1.0e-4F);
    EXPECT_NEAR(actual.z(), expected.z(), 1.0e-4F);
    EXPECT_NEAR(actual.w(), expected.w(), 1.0e-4F);
  }
}

TEST(MountedPrepare, AttackingMountedRiderRootAttachesToHorseSeatFrame) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid,
                                root + "/humanoid.bpat"));
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid_sword,
                                root + "/humanoid_sword.bpat"));

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;
  cfg.rider_creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;

  InspectableMountedKnightRenderer const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;
  ctx.force_single_soldier = true;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.35F;
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.attack_variant = 1U;
  anim.has_attack_offset = true;
  anim.attack_offset = 0.0F;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  auto const horse_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Horse;
      });
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(horse_req, requests.end());
  ASSERT_NE(rider_req, requests.end());
  ASSERT_EQ(rider_req->state, Render::Creature::AnimationStateId::AttackSword);
  EXPECT_TRUE(rider_req->world_already_grounded);

  Render::GL::HumanoidVariant variant{};
  std::uint32_t const seed =
      static_cast<std::uint32_t>(unit->owner_id * 2654435761U) ^
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&entity) &
                                 0xFFFFFFFFU);
  renderer.get_variant(ctx, seed, variant);

  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.inputs = anim;
  Render::GL::HorseProfile profile{};
  Render::GL::HorseDimensions dims{};
  Render::GL::MountedAttachmentFrame mount{};
  Render::GL::HorseMotionSample motion{};
  renderer.resolve_mount_render_state(
      ctx, seed, variant, anim_ctx, true, profile, dims, mount, motion);

  auto const root_matrix = rider_root_matrix(rider_req->archetype,
                                             rider_req->creature_asset_id,
                                             rider_req->state,
                                             rider_req->phase,
                                             rider_req->clip_variant);
  ASSERT_TRUE(root_matrix.has_value());

  bool invertible = false;
  QMatrix4x4 const rider_from_horse =
      horse_req->world.inverted(&invertible) * rider_req->world;
  ASSERT_TRUE(invertible);
  QMatrix4x4 const anchored_root = rider_from_horse * *root_matrix;
  QMatrix4x4 expected_seat;
  expected_seat.setColumn(0, QVector4D(mount.seat_right, 0.0F));
  expected_seat.setColumn(1, QVector4D(mount.seat_up, 0.0F));
  expected_seat.setColumn(2, QVector4D(mount.seat_forward, 0.0F));
  expected_seat.setColumn(3, QVector4D(mount.seat_position, 1.0F));

  for (int column = 0; column < 4; ++column) {
    auto const actual = anchored_root.column(column);
    auto const expected = expected_seat.column(column);
    EXPECT_NEAR(actual.x(), expected.x(), 1.0e-4F);
    EXPECT_NEAR(actual.y(), expected.y(), 1.0e-4F);
    EXPECT_NEAR(actual.z(), expected.z(), 1.0e-4F);
    EXPECT_NEAR(actual.w(), expected.w(), 1.0e-4F);
  }
}

TEST(MountedPrepare, MovingMountedRiderRootAttachesToHorseSeatFrame) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  if (root.empty()) {
    GTEST_SKIP() << "baked .bpat assets not found";
  }
  auto& bpat = Render::Creature::Bpat::BpatRegistry::instance();
  ASSERT_TRUE(bpat.load_species(Render::Creature::Bpat::k_species_humanoid,
                                root + "/humanoid.bpat"));

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  InspectableMountedKnightRenderer const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;
  ctx.force_single_soldier = true;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs anim{};
  anim.movement_state = Render::Creature::MovementAnimationState::Run;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto const& requests = prep.bodies.requests();
  auto const horse_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Horse;
      });
  auto const rider_req =
      std::find_if(requests.begin(), requests.end(), [](const auto& req) {
        return Render::Creature::ArchetypeRegistry::instance().species(req.archetype) ==
               Render::Creature::Pipeline::CreatureKind::Humanoid;
      });
  ASSERT_NE(horse_req, requests.end());
  ASSERT_NE(rider_req, requests.end());

  Render::GL::HumanoidVariant variant{};
  std::uint32_t const seed =
      static_cast<std::uint32_t>(unit->owner_id * 2654435761U) ^
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&entity) &
                                 0xFFFFFFFFU);
  renderer.get_variant(ctx, seed, variant);

  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.inputs = anim;
  Render::GL::HorseProfile profile{};
  Render::GL::HorseDimensions dims{};
  Render::GL::MountedAttachmentFrame mount{};
  Render::GL::HorseMotionSample motion{};
  renderer.resolve_mount_render_state(
      ctx, seed, variant, anim_ctx, true, profile, dims, mount, motion);

  auto const root_matrix = rider_root_matrix(rider_req->archetype,
                                             rider_req->creature_asset_id,
                                             rider_req->state,
                                             rider_req->phase,
                                             rider_req->clip_variant);
  ASSERT_TRUE(root_matrix.has_value());

  bool invertible = false;
  QMatrix4x4 const rider_from_horse =
      horse_req->world.inverted(&invertible) * rider_req->world;
  ASSERT_TRUE(invertible);
  QMatrix4x4 const anchored_root = rider_from_horse * *root_matrix;
  QMatrix4x4 expected_seat;
  expected_seat.setColumn(0, QVector4D(mount.seat_right, 0.0F));
  expected_seat.setColumn(1, QVector4D(mount.seat_up, 0.0F));
  expected_seat.setColumn(2, QVector4D(mount.seat_forward, 0.0F));
  expected_seat.setColumn(3, QVector4D(mount.seat_position, 1.0F));

  for (int column = 0; column < 4; ++column) {
    auto const actual = anchored_root.column(column);
    auto const expected = expected_seat.column(column);
    EXPECT_NEAR(actual.x(), expected.x(), 1.0e-4F);
    EXPECT_NEAR(actual.y(), expected.y(), 1.0e-4F);
    EXPECT_NEAR(actual.z(), expected.z(), 1.0e-4F);
    EXPECT_NEAR(actual.w(), expected.w(), 1.0e-4F);
  }
}

TEST(MountedPrepare, ShieldedMountedKnightMovementUsesRiggedSubmissionsOnly) {
  Render::GL::register_built_in_equipment();

  Render::GL::MountedKnightRendererConfig cfg;
  cfg.sword_equipment_id = "sword_roman";
  cfg.shield_equipment_id = "roman_scutum";
  cfg.helmet_equipment_id = "roman_heavy";
  cfg.armor_equipment_id = "roman_heavy_armor";
  cfg.shoulder_equipment_id = "roman_shoulder_cover_cavalry";
  cfg.has_shoulder = true;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.force_single_soldier = true;
  ctx.allow_template_cache = false;

  Render::GL::AnimationInputs anim{};
  anim.movement_state = Render::Creature::MovementAnimationState::Run;

  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  NullSubmitter sink;
  auto const stats = Render::Creature::Pipeline::submit_preparation(prep, sink);

  EXPECT_EQ(stats.entities_submitted, 2U);
  EXPECT_EQ(sink.rigged_calls, 2);
  EXPECT_EQ(sink.meshes, 0);
}

TEST(MountedPrepare, MountedKnightKeepsConfiguredRiderArchetype) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;
  cfg.rider_archetype_id = 77U;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  auto const& mounted = renderer.mounted_visual_spec();

  EXPECT_EQ(mounted.rider.archetype_id, 77U);
}

TEST(MountedPrepare, HorseSpearmanKeepsConfiguredRiderArchetype) {
  Render::GL::HorseSpearmanRendererConfig cfg;
  cfg.has_spear = false;
  cfg.has_shield = false;
  cfg.rider_archetype_id = 91U;

  Render::GL::HorseSpearmanRendererBase const renderer(cfg);
  auto const& mounted = renderer.mounted_visual_spec();

  EXPECT_EQ(mounted.rider.archetype_id, 91U);
}

TEST(MountedPrepare, StaleLayoutCacheVersionForcesMountedFormationRefresh) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  Render::GL::DrawContext ctx{};
  ctx.allow_template_cache = false;

  Engine::Core::Entity entity(1);
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  ASSERT_NE(unit, nullptr);
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  ctx.entity = &entity;

  Render::GL::AnimationInputs const anim{};
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 0, prep);

  auto* layout_cache =
      entity.get_component<Render::Humanoid::HumanoidLayoutCacheComponent>();
  ASSERT_NE(layout_cache, nullptr);
  ASSERT_GT(layout_cache->soldiers.size(), 1U);

  for (auto& soldier : layout_cache->soldiers) {
    soldier.offset_x = 0.0F;
    soldier.offset_z = 0.0F;
    soldier.yaw_offset = 0.0F;
  }
  layout_cache->layout_version = 0U;
  layout_cache->valid = true;

  prep.clear();
  Render::Humanoid::prepare_humanoid_instances(renderer, ctx, anim, 1, prep);

  ASSERT_NE(layout_cache->layout_version, 0U);
  bool found_non_grid_offset = false;
  for (auto const& soldier : layout_cache->soldiers) {
    if (std::abs(soldier.offset_x) > 0.001F || std::abs(soldier.offset_z) > 0.001F ||
        std::abs(soldier.yaw_offset) > 0.001F) {
      found_non_grid_offset = true;
      break;
    }
  }
  EXPECT_TRUE(found_non_grid_offset);
}

} // namespace
