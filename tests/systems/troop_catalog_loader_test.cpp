#include <array>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

#include "game/systems/resource_types.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_catalog_loader.h"
#include "game/units/troop_config.h"

namespace {

constexpr std::array<Game::Units::TroopType, 21> k_all_troop_types{
    Game::Units::TroopType::Archer,
    Game::Units::TroopType::Swordsman,
    Game::Units::TroopType::Spearman,
    Game::Units::TroopType::MountedKnight,
    Game::Units::TroopType::HorseArcher,
    Game::Units::TroopType::HorseSpearman,
    Game::Units::TroopType::Healer,
    Game::Units::TroopType::SkeletonSwordsman,
    Game::Units::TroopType::SkeletonArcher,
    Game::Units::TroopType::GravePriest,
    Game::Units::TroopType::Catapult,
    Game::Units::TroopType::Ballista,
    Game::Units::TroopType::Elephant,
    Game::Units::TroopType::RomanLegionOrganizer,
    Game::Units::TroopType::RomanVeteranConsul,
    Game::Units::TroopType::RomanFieldCommander,
    Game::Units::TroopType::CarthageSpearCommander,
    Game::Units::TroopType::CarthageBowCommander,
    Game::Units::TroopType::CarthageSwordCommander,
    Game::Units::TroopType::Builder,
    Game::Units::TroopType::Civilian,
};

TEST(TroopCatalogLoader, ElephantKeepsGroundOffsetWithoutRingYOffset) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* elephant =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Elephant);
  ASSERT_NE(elephant, nullptr);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_size, 1.5F);
  EXPECT_FLOAT_EQ(elephant->visuals.selection_ring_ground_offset, 0.6F);
}

TEST(TroopCatalogLoader, CivilianStaysInLoadedCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* civilian =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Civilian);
  ASSERT_NE(civilian, nullptr);
  EXPECT_EQ(civilian->display_name, "Civilian");
  EXPECT_EQ(civilian->visuals.renderer_id, "troops/roman/civilian");
  EXPECT_EQ(civilian->production.cost, 8);
}

TEST(TroopCatalogLoader, FrontlineInfantryKeepConfiguredFormationSpacing) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* swordsman = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::Swordsman);
  auto const* spearman =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Spearman);
  ASSERT_NE(swordsman, nullptr);
  ASSERT_NE(spearman, nullptr);

  EXPECT_FLOAT_EQ(swordsman->visuals.formation_spacing, 1.05F);
  EXPECT_FLOAT_EQ(spearman->visuals.formation_spacing, 1.05F);
  EXPECT_FLOAT_EQ(Game::Units::TroopConfig::instance().get_formation_spacing(
                      Game::Units::TroopType::Swordsman),
                  1.05F);
  EXPECT_FLOAT_EQ(Game::Units::TroopConfig::instance().get_formation_spacing(
                      Game::Units::TroopType::Spearman),
                  1.05F);
}

TEST(TroopCatalogLoader, ProductionResourceCostsLoadForEconomyUnits) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* archer =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Archer);
  auto const* builder =
      Game::Units::TroopCatalog::instance().get_class(Game::Units::TroopType::Builder);
  ASSERT_NE(archer, nullptr);
  ASSERT_NE(builder, nullptr);

  EXPECT_EQ(archer->production.resource_costs.get(Game::Systems::ResourceType::Wood),
            6);
  EXPECT_EQ(archer->production.resource_costs.get(Game::Systems::ResourceType::Stone),
            0);
  EXPECT_EQ(builder->production.resource_costs.get(Game::Systems::ResourceType::Wood),
            0);
  EXPECT_TRUE(builder->production.resource_costs.empty());
}

TEST(TroopCatalogLoader, IronSepulcherTroopsLoadFromCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* skeleton_swordsman = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::SkeletonSwordsman);
  auto const* skeleton_archer = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::SkeletonArcher);
  auto const* grave_priest = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::GravePriest);
  ASSERT_NE(skeleton_swordsman, nullptr);
  ASSERT_NE(skeleton_archer, nullptr);
  ASSERT_NE(grave_priest, nullptr);

  EXPECT_EQ(skeleton_swordsman->production.cost, 0);
  EXPECT_FALSE(skeleton_swordsman->combat.can_ranged);
  EXPECT_EQ(skeleton_swordsman->visuals.renderer_id,
            "troops/iron_sepulcher/skeleton_swordsman");

  EXPECT_EQ(skeleton_archer->production.cost, 0);
  EXPECT_TRUE(skeleton_archer->combat.can_ranged);
  EXPECT_EQ(skeleton_archer->visuals.renderer_id,
            "troops/iron_sepulcher/skeleton_archer");

  EXPECT_EQ(grave_priest->production.cost, 0);
  EXPECT_TRUE(grave_priest->combat.can_ranged);
  EXPECT_EQ(grave_priest->visuals.renderer_id, "troops/iron_sepulcher/grave_priest");
}

TEST(TroopCatalogLoader, CommandersLoadFromCatalog) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  auto const* fabius = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::RomanLegionOrganizer);
  auto const* scipio = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::RomanVeteranConsul);
  auto const* marcellus = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::RomanFieldCommander);
  auto const* hanno = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::CarthageSpearCommander);
  auto const* hasdrubal = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::CarthageBowCommander);
  auto const* hannibal = Game::Units::TroopCatalog::instance().get_class(
      Game::Units::TroopType::CarthageSwordCommander);
  ASSERT_NE(fabius, nullptr);
  ASSERT_NE(scipio, nullptr);
  ASSERT_NE(marcellus, nullptr);
  ASSERT_NE(hanno, nullptr);
  ASSERT_NE(hasdrubal, nullptr);
  ASSERT_NE(hannibal, nullptr);

  for (auto const* commander :
       {fabius, scipio, marcellus, hanno, hasdrubal, hannibal}) {
    EXPECT_GT(commander->visuals.render_scale, 0.60F);
    EXPECT_EQ(commander->individuals_per_unit, 1);
    EXPECT_EQ(commander->max_units_per_row, 1);
  }

  EXPECT_EQ(fabius->visuals.renderer_id, "troops/roman/commanders/fabius_maximus");
  EXPECT_EQ(scipio->visuals.renderer_id, "troops/roman/commanders/scipio_africanus");
  EXPECT_EQ(marcellus->visuals.renderer_id, "troops/roman/commanders/marcellus");
  EXPECT_EQ(hanno->visuals.renderer_id, "troops/carthage/commanders/hanno_the_great");
  EXPECT_EQ(hasdrubal->visuals.renderer_id,
            "troops/carthage/commanders/hasdrubal_barca");
  EXPECT_EQ(hannibal->visuals.renderer_id, "troops/carthage/commanders/hannibal_barca");
}

TEST(TroopCatalogLoader, CompiledDefaultsMatchTheShippedTroopData) {
  auto& catalog = Game::Units::TroopCatalog::instance();

  catalog.reset_to_defaults();
  std::unordered_map<Game::Units::TroopType, Game::Units::TroopClass> compiled;
  for (auto const type : k_all_troop_types) {
    auto const* troop_class = catalog.get_class(type);
    ASSERT_NE(troop_class, nullptr)
        << "compiled defaults are missing " << Game::Units::troop_typeToString(type);
    compiled.emplace(type, *troop_class);
  }

  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());

  for (auto const type : k_all_troop_types) {
    auto const* loaded = catalog.get_class(type);
    ASSERT_NE(loaded, nullptr);
    auto const& fallback = compiled.at(type);
    const std::string name = Game::Units::troop_typeToString(type);

    EXPECT_EQ(fallback.display_name, loaded->display_name) << name;
    EXPECT_EQ(fallback.production.cost, loaded->production.cost) << name;
    EXPECT_EQ(fallback.production.is_melee, loaded->production.is_melee) << name;
    EXPECT_FLOAT_EQ(fallback.production.build_time, loaded->production.build_time)
        << name;
    EXPECT_EQ(fallback.production.priority, loaded->production.priority) << name;
    for (auto const resource : Game::Systems::k_all_resource_types) {
      EXPECT_EQ(fallback.production.resource_costs.get(resource),
                loaded->production.resource_costs.get(resource))
          << name << " " << Game::Systems::resource_type_key(resource);
    }

    EXPECT_EQ(fallback.combat.health, loaded->combat.health) << name;
    EXPECT_EQ(fallback.combat.max_health, loaded->combat.max_health) << name;
    EXPECT_FLOAT_EQ(fallback.combat.speed, loaded->combat.speed) << name;
    EXPECT_FLOAT_EQ(fallback.combat.vision_range, loaded->combat.vision_range) << name;
    EXPECT_FLOAT_EQ(fallback.combat.ranged_range, loaded->combat.ranged_range) << name;
    EXPECT_EQ(fallback.combat.ranged_damage, loaded->combat.ranged_damage) << name;
    EXPECT_FLOAT_EQ(fallback.combat.ranged_cooldown, loaded->combat.ranged_cooldown)
        << name;
    EXPECT_FLOAT_EQ(fallback.combat.melee_range, loaded->combat.melee_range) << name;
    EXPECT_EQ(fallback.combat.melee_damage, loaded->combat.melee_damage) << name;
    EXPECT_FLOAT_EQ(fallback.combat.melee_cooldown, loaded->combat.melee_cooldown)
        << name;
    EXPECT_EQ(fallback.combat.can_ranged, loaded->combat.can_ranged) << name;
    EXPECT_EQ(fallback.combat.can_melee, loaded->combat.can_melee) << name;

    EXPECT_FLOAT_EQ(fallback.visuals.render_scale, loaded->visuals.render_scale)
        << name;
    EXPECT_FLOAT_EQ(fallback.visuals.selection_ring_size,
                    loaded->visuals.selection_ring_size)
        << name;
    EXPECT_FLOAT_EQ(fallback.visuals.selection_ring_ground_offset,
                    loaded->visuals.selection_ring_ground_offset)
        << name;
    EXPECT_FLOAT_EQ(fallback.visuals.formation_spacing,
                    loaded->visuals.formation_spacing)
        << name;
    EXPECT_EQ(fallback.visuals.renderer_id, loaded->visuals.renderer_id) << name;

    EXPECT_EQ(fallback.individuals_per_unit, loaded->individuals_per_unit) << name;
    EXPECT_EQ(fallback.max_units_per_row, loaded->max_units_per_row) << name;
  }
}

} // namespace
