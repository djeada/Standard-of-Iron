#include <QSettings>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <string>

#include "app/core/economy_overview.h"
#include "app/core/user_settings.h"
#include "app/viewmodels/economy_view_model.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/default_content.h"
#include "game/systems/harvest_yields.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_queries.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Systems::ResourceType;

constexpr int k_owner = 1;
constexpr int k_population_cap = 600;

class EconomyOverviewTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    Game::Systems::initialize_default_content(nations);
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().initialize();
  }

  void TearDown() override {
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::TroopCountRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  [[nodiscard]] auto request() const -> App::Core::EconomyOverviewRequest {
    return {.world = const_cast<Engine::Core::World*>(&world),
            .nations = &Game::Systems::NationRegistry::instance(),
            .resources = &Game::Systems::PlayerResourceRegistry::instance(),
            .owner_id = k_owner,
            .nation_id = Game::Systems::NationID::RomanRepublic,
            .population_cap = k_population_cap};
  }

  auto add_unit(Game::Units::SpawnType spawn_type,
                int owner_id = k_owner) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(0.0F, 0.0F, 0.0F);
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 1.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = spawn_type;
    unit->nation_id = Game::Systems::NationID::RomanRepublic;
    return entity;
  }

  static auto entry_for(const QVariantList& entries,
                        const QString& key) -> QVariantMap {
    for (const auto& value : entries) {
      const auto map = value.toMap();
      if (map.value(QStringLiteral("key")).toString() == key) {
        return map;
      }
    }
    return {};
  }

  static auto item_for(const QVariantList& entries,
                       const QString& field,
                       const QString& key) -> QVariantMap {
    for (const auto& value : entries) {
      const auto map = value.toMap();
      if (map.value(field).toString() == key) {
        return map;
      }
    }
    return {};
  }

  Engine::Core::World world;
};

TEST_F(EconomyOverviewTest, EveryResourceTheEngineTracksIsReported) {
  const QVariantList entries = App::Core::build_resource_overview(request());
  ASSERT_EQ(entries.size(),
            static_cast<qsizetype>(Game::Systems::k_resource_type_count));
  for (const auto type : Game::Systems::k_all_resource_types) {
    const QString key = QLatin1String(Game::Systems::resource_type_key(type));
    EXPECT_FALSE(entry_for(entries, key).isEmpty()) << key.toStdString();
  }
}

TEST_F(EconomyOverviewTest, FoodIsReportedAndStaysRelevantWhileTheMissionHoldsIt) {
  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, ResourceType::Food, 380);

  const QVariantMap food =
      entry_for(App::Core::build_resource_overview(request()), QStringLiteral("food"));
  ASSERT_FALSE(food.isEmpty());
  EXPECT_EQ(food.value(QStringLiteral("amount")).toInt(), 380);
  EXPECT_TRUE(food.value(QStringLiteral("relevant")).toBool());
  EXPECT_FALSE(food.value(QStringLiteral("gatherable")).toBool());
}

TEST_F(EconomyOverviewTest, AResourceAMissionObjectiveAsksForIsNeverHidden) {
  App::Core::EconomyOverviewRequest asking = request();
  asking.objective_resources.set(ResourceType::Food, 400);

  const QVariantMap food =
      entry_for(App::Core::build_resource_overview(asking), QStringLiteral("food"));
  ASSERT_FALSE(food.isEmpty());
  EXPECT_EQ(food.value(QStringLiteral("amount")).toInt(), 0);
  EXPECT_EQ(food.value(QStringLiteral("objective_target")).toInt(), 400);
  EXPECT_TRUE(food.value(QStringLiteral("relevant")).toBool())
      << "an objective the player has no stock of must still be on the bar";

  const QVariantMap unused =
      entry_for(App::Core::build_resource_overview(request()), QStringLiteral("food"));
  EXPECT_EQ(unused.value(QStringLiteral("objective_target")).toInt(), 0);
}

TEST_F(EconomyOverviewTest, AGatherableResourceCarriesItsYieldAndStorageCap) {
  const QVariantMap wood =
      entry_for(App::Core::build_resource_overview(request()), QStringLiteral("wood"));
  ASSERT_FALSE(wood.isEmpty());
  EXPECT_TRUE(wood.value(QStringLiteral("gatherable")).toBool());
  EXPECT_EQ(wood.value(QStringLiteral("yield_per_trip")).toInt(),
            Game::Systems::k_cut_tree_wood_reward);
  EXPECT_GT(wood.value(QStringLiteral("display_cap")).toInt(), 0);
}

TEST_F(EconomyOverviewTest, EveryResourceSaysWhatItIsSpentOn) {
  const QVariantList entries = App::Core::build_resource_overview(request());
  for (const char* key : {"wood", "stone", "iron", "gold"}) {
    const QVariantMap entry = entry_for(entries, QLatin1String(key));
    EXPECT_FALSE(entry.value(QStringLiteral("used_by")).toStringList().isEmpty())
        << key << " is spent on nothing the player can see";
  }
}

TEST_F(EconomyOverviewTest, AShortfallNamesTheCheapestItemItBlocks) {
  const QVariantMap wood =
      entry_for(App::Core::build_resource_overview(request()), QStringLiteral("wood"));
  ASSERT_FALSE(wood.isEmpty());
  EXPECT_GT(wood.value(QStringLiteral("shortfall")).toInt(), 0);
  EXPECT_FALSE(wood.value(QStringLiteral("shortfall_item")).toString().isEmpty());

  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, ResourceType::Wood, 9999);
  const QVariantMap rich =
      entry_for(App::Core::build_resource_overview(request()), QStringLiteral("wood"));
  EXPECT_EQ(rich.value(QStringLiteral("shortfall")).toInt(), 0);
}

TEST_F(EconomyOverviewTest, GatheringAndHaulingShowUpAgainstTheResourceBeingGathered) {
  auto* builder = add_unit(Game::Units::SpawnType::Builder);
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = std::string(Game::Systems::k_builder_product_cut_tree);
  production->in_progress = true;
  production->has_task_target = true;

  auto* hauler = add_unit(Game::Units::SpawnType::Builder);
  auto* carry = hauler->add_component<Engine::Core::ResourceCarryComponent>();
  carry->amounts.set(ResourceType::Stone, 35);

  const QVariantList entries = App::Core::build_resource_overview(request());
  EXPECT_EQ(entry_for(entries, QStringLiteral("wood"))
                .value(QStringLiteral("gathering_workers"))
                .toInt(),
            1);
  EXPECT_EQ(entry_for(entries, QStringLiteral("stone"))
                .value(QStringLiteral("carrying"))
                .toInt(),
            35);
}

TEST_F(EconomyOverviewTest, TheHelpViewCostsEveryBuildingAndUnitTheNationCanRaise) {
  const QVariantMap help = App::Core::build_production_help(request());
  const auto buildings = help.value(QStringLiteral("buildings")).toList();
  const auto units = help.value(QStringLiteral("units")).toList();
  ASSERT_FALSE(buildings.isEmpty());
  ASSERT_FALSE(units.isEmpty());

  const QVariantMap home =
      item_for(buildings, QStringLiteral("item_type"), QStringLiteral("home"));
  ASSERT_FALSE(home.isEmpty());
  EXPECT_GT(home.value(QStringLiteral("build_time")).toDouble(), 0.0);
  EXPECT_FALSE(home.value(QStringLiteral("resource_costs")).toMap().isEmpty());

  const QVariantMap archer =
      item_for(units, QStringLiteral("unit_type"), QStringLiteral("archer"));
  ASSERT_FALSE(archer.isEmpty());
  EXPECT_GT(archer.value(QStringLiteral("population_cost")).toInt(), 0);
  EXPECT_FALSE(archer.value(QStringLiteral("display_name")).toString().isEmpty());
}

TEST_F(EconomyOverviewTest, AnUnaffordableItemReportsWhatIsMissingAndByHowMuch) {
  const QVariantMap help = App::Core::build_production_help(request());
  const QVariantMap home = item_for(help.value(QStringLiteral("buildings")).toList(),
                                    QStringLiteral("item_type"),
                                    QStringLiteral("home"));
  ASSERT_FALSE(home.isEmpty());
  EXPECT_FALSE(home.value(QStringLiteral("affordable")).toBool());
  const QVariantMap missing = home.value(QStringLiteral("missing")).toMap();
  ASSERT_FALSE(missing.isEmpty());
  EXPECT_GT(missing.value(QStringLiteral("wood")).toInt(), 0);
}

TEST_F(EconomyOverviewTest, PrerequisitesAreReportedSeparatelyFromCost) {
  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, ResourceType::Wood, 9999);
  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, ResourceType::Stone, 9999);

  const QVariantMap without_builder = App::Core::build_production_help(request());
  const QVariantMap home_blocked =
      item_for(without_builder.value(QStringLiteral("buildings")).toList(),
               QStringLiteral("item_type"),
               QStringLiteral("home"));
  EXPECT_TRUE(home_blocked.value(QStringLiteral("affordable")).toBool());
  EXPECT_FALSE(home_blocked.value(QStringLiteral("prerequisite_met")).toBool());

  add_unit(Game::Units::SpawnType::Builder)
      ->add_component<Engine::Core::BuilderProductionComponent>();
  const QVariantMap with_builder = App::Core::build_production_help(request());
  const QVariantMap home_ready =
      item_for(with_builder.value(QStringLiteral("buildings")).toList(),
               QStringLiteral("item_type"),
               QStringLiteral("home"));
  EXPECT_TRUE(home_ready.value(QStringLiteral("prerequisite_met")).toBool());
  EXPECT_EQ(with_builder.value(QStringLiteral("builder_count")).toInt(), 1);
}

TEST_F(EconomyOverviewTest, RecruitingReportsBarracksManpowerSeparatelyFromResources) {
  auto* barracks = add_unit(Game::Units::SpawnType::Barracks);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->manpower_available = 0;

  QVariantMap help = App::Core::build_production_help(request());
  QVariantMap archer = item_for(help.value(QStringLiteral("units")).toList(),
                                QStringLiteral("unit_type"),
                                QStringLiteral("archer"));
  ASSERT_FALSE(archer.isEmpty());
  EXPECT_TRUE(archer.value(QStringLiteral("prerequisite_met")).toBool());
  EXPECT_FALSE(archer.value(QStringLiteral("manpower_met")).toBool());

  production->manpower_available = 500;
  help = App::Core::build_production_help(request());
  archer = item_for(help.value(QStringLiteral("units")).toList(),
                    QStringLiteral("unit_type"),
                    QStringLiteral("archer"));
  EXPECT_TRUE(archer.value(QStringLiteral("manpower_met")).toBool());
}

TEST_F(EconomyOverviewTest, TheCoachWalksGatherThenBuildThenRecruitThenArmy) {
  const auto baseline = App::Core::capture_economy_coach_baseline(request());
  ASSERT_TRUE(baseline.captured);

  QVariantMap coach = App::Core::build_economy_coach_state(request(), baseline);
  EXPECT_EQ(coach.value(QStringLiteral("step")).toString(), QStringLiteral("gather"));
  EXPECT_FALSE(coach.value(QStringLiteral("complete")).toBool());

  Game::Systems::PlayerResourceRegistry::instance().add_harvested(
      k_owner, ResourceType::Wood, Game::Systems::k_cut_tree_wood_reward);
  coach = App::Core::build_economy_coach_state(request(), baseline);
  EXPECT_EQ(coach.value(QStringLiteral("step")).toString(), QStringLiteral("build"));

  add_unit(Game::Units::SpawnType::Home);
  coach = App::Core::build_economy_coach_state(request(), baseline);
  EXPECT_EQ(coach.value(QStringLiteral("step")).toString(), QStringLiteral("recruit"));

  auto* barracks = add_unit(Game::Units::SpawnType::Barracks);
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  production->in_progress = true;
  coach = App::Core::build_economy_coach_state(request(), baseline);
  EXPECT_EQ(coach.value(QStringLiteral("step")).toString(), QStringLiteral("army"));
  EXPECT_EQ(coach.value(QStringLiteral("steps")).toList().size(), 4);
}

TEST_F(EconomyOverviewTest, TheCoachFinishesOnceAnArmyHasBeenRaised) {
  const auto baseline = App::Core::capture_economy_coach_baseline(request());
  Game::Systems::PlayerResourceRegistry::instance().add_harvested(
      k_owner, ResourceType::Wood, 40);
  add_unit(Game::Units::SpawnType::Home);

  for (int i = 0; i < 4; ++i) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitSpawnedEvent(static_cast<Engine::Core::EntityID>(100 + i),
                                       k_owner,
                                       Game::Units::SpawnType::Archer,
                                       false));
  }
  ASSERT_GE(Game::Systems::troop_count_for(k_owner),
            App::Core::k_economy_coach_army_population);

  const QVariantMap coach = App::Core::build_economy_coach_state(request(), baseline);
  EXPECT_TRUE(coach.value(QStringLiteral("complete")).toBool())
      << coach.value(QStringLiteral("step")).toString().toStdString();
}

TEST_F(EconomyOverviewTest, AnotherOwnersWorkForceIsNotCounted) {
  auto* builder = add_unit(Game::Units::SpawnType::Builder, 2);
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = std::string(Game::Systems::k_builder_product_cut_tree);
  production->in_progress = true;
  production->has_task_target = true;

  const QVariantList entries = App::Core::build_resource_overview(request());
  EXPECT_EQ(entry_for(entries, QStringLiteral("wood"))
                .value(QStringLiteral("gathering_workers"))
                .toInt(),
            0);
  EXPECT_EQ(App::Core::build_production_help(request())
                .value(QStringLiteral("builder_count"))
                .toInt(),
            0);
}

class EconomyViewModelTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(settings_dir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
    App::Core::UserSettings::clear();
  }

  void TearDown() override { App::Core::UserSettings::clear(); }

  QTemporaryDir settings_dir;
};

TEST_F(EconomyViewModelTest, AResourceIsFoundByItsKey) {
  App::ViewModels::EconomyViewModel view_model;
  QVariantMap wood;
  wood[QStringLiteral("key")] = QStringLiteral("wood");
  wood[QStringLiteral("amount")] = 120;
  view_model.set_resources({wood});

  EXPECT_EQ(view_model.resource(QStringLiteral("wood"))
                .value(QStringLiteral("amount"))
                .toInt(),
            120);
  EXPECT_TRUE(view_model.resource(QStringLiteral("food")).isEmpty());
}

TEST_F(EconomyViewModelTest, ThePromptsAreShownUntilTheyAreDismissed) {
  App::ViewModels::EconomyViewModel view_model;
  EXPECT_TRUE(view_model.coach_enabled());
  EXPECT_FALSE(view_model.coach_visible()) << "nothing to show before a mission";

  QVariantMap coach;
  coach[QStringLiteral("step")] = QStringLiteral("gather");
  view_model.set_coach(coach);
  view_model.set_coach_available(true);
  EXPECT_TRUE(view_model.coach_visible());

  view_model.dismiss_coach();
  EXPECT_FALSE(view_model.coach_visible());
  EXPECT_FALSE(App::Core::UserSettings::load_ui_economy_coach())
      << "a dismissal must outlive the mission";
}

TEST_F(EconomyViewModelTest, AMissionWithoutACoachNeverShowsThePrompts) {
  App::ViewModels::EconomyViewModel view_model;
  QVariantMap coach;
  coach[QStringLiteral("step")] = QStringLiteral("gather");
  view_model.set_coach(coach);
  EXPECT_FALSE(view_model.coach_visible()) << "a spectated match has no coach";
}

TEST_F(EconomyViewModelTest, ClearingDropsEverythingTheLastMissionPushed) {
  App::ViewModels::EconomyViewModel view_model;
  QVariantMap entry;
  entry[QStringLiteral("key")] = QStringLiteral("wood");
  view_model.set_resources({entry});
  view_model.set_help(entry);
  view_model.set_coach(entry);
  view_model.set_coach_available(true);
  ASSERT_TRUE(view_model.coach_visible());

  view_model.clear();
  EXPECT_TRUE(view_model.resources().isEmpty());
  EXPECT_TRUE(view_model.help().isEmpty());
  EXPECT_TRUE(view_model.coach().isEmpty());
  EXPECT_FALSE(view_model.coach_visible());
}

} // namespace
