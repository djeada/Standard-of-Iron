// Phase E — Mounted base loadout migration parity tests.
//
// Asserts that each mounted humanoid base class
// (HorseArcherRendererBase / HorseSpearmanRendererBase /
// MountedKnightRendererBase) populates its rider UnitVisualSpec with
// a non-empty equipment loadout when handed a config that names every
// equipment slot, and that the spec gates the legacy Helmet + Armor
// virtuals via owned_legacy_slots so equipment is not double-emitted.

#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/horse_archer_renderer_base.h"
#include "render/entity/horse_spearman_renderer_base.h"
#include "render/entity/mounted_knight_renderer_base.h"

#include <gtest/gtest.h>

namespace {

using Render::Creature::Pipeline::CreatureKind;
using Render::Creature::Pipeline::LegacySlotMask;
using Render::Creature::Pipeline::owns_slot;

TEST(MountedBaseLoadoutMigration, MountedKnightLoadoutIsPopulated) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.sword_equipment_id = "sword_carthage";
  cfg.shield_equipment_id = "shield_carthage_cavalry";
  cfg.helmet_equipment_id = "carthage_heavy";
  cfg.armor_equipment_id = "armor_heavy_carthage";
  cfg.shoulder_equipment_id = "carthage_shoulder_cover_cavalry";
  cfg.has_shoulder = true;

  Render::GL::MountedKnightRendererBase const renderer(cfg);
  const auto &spec = renderer.visual_spec();

  EXPECT_EQ(spec.kind, CreatureKind::Humanoid);
  EXPECT_FALSE(spec.equipment.empty());
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Helmet));
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Armor));
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Attachments));

  const auto &mspec = renderer.mounted_visual_spec();
  EXPECT_FALSE(mspec.rider.equipment.empty());
}

TEST(MountedBaseLoadoutMigration, HorseSpearmanLoadoutIsPopulated) {
  Render::GL::HorseSpearmanRendererConfig cfg;
  cfg.spear_equipment_id = "spear_carthage";
  cfg.shield_equipment_id = "shield_carthage";
  cfg.helmet_equipment_id = "carthage_light";
  cfg.armor_equipment_id = "armor_light_carthage";
  cfg.shoulder_equipment_id = "carthage_shoulder_cover";
  cfg.has_shield = true;
  cfg.has_shoulder = true;

  Render::GL::HorseSpearmanRendererBase const renderer(cfg);
  const auto &spec = renderer.visual_spec();

  EXPECT_EQ(spec.kind, CreatureKind::Humanoid);
  EXPECT_FALSE(spec.equipment.empty());
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Helmet));
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Armor));
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Attachments));

  const auto &mspec = renderer.mounted_visual_spec();
  EXPECT_FALSE(mspec.rider.equipment.empty());
}

TEST(MountedBaseLoadoutMigration, HorseArcherLoadoutIsPopulated) {
  Render::GL::HorseArcherRendererConfig cfg;
  cfg.bow_equipment_id = "bow_roman";
  cfg.quiver_equipment_id = "quiver";
  cfg.helmet_equipment_id = "roman_light";
  cfg.armor_equipment_id = "roman_light_armor";
  cfg.cloak_equipment_id = "cloak_carthage";
  cfg.has_cloak = true;

  Render::GL::HorseArcherRendererBase const renderer(cfg);
  const auto &spec = renderer.visual_spec();

  EXPECT_EQ(spec.kind, CreatureKind::Humanoid);
  EXPECT_FALSE(spec.equipment.empty());
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Helmet));
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Armor));
  EXPECT_TRUE(owns_slot(spec.owned_legacy_slots, LegacySlotMask::Attachments));

  const auto &mspec = renderer.mounted_visual_spec();
  EXPECT_FALSE(mspec.rider.equipment.empty());
}

// Empty config still yields a well-formed (possibly empty) loadout.
TEST(MountedBaseLoadoutMigration, EmptyConfigsProduceEmptyLoadout) {
  Render::GL::MountedKnightRendererConfig cfg;
  cfg.has_sword = false;
  cfg.has_cavalry_shield = false;
  // No equipment ids set.
  Render::GL::MountedKnightRendererBase const renderer(cfg);
  const auto &spec = renderer.visual_spec();
  EXPECT_TRUE(spec.equipment.empty());
}

} // namespace
