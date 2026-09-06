#include <QAbstractItemModel>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "app/core/client_context.h"
#include "app/models/selected_units_model.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/render_bridge/selection_controller.h"
#include "game/systems/selection_system.h"

namespace {

struct SelectionFixture {
  Engine::Core::World world;
  Game::Systems::SelectionSystem* selection_system = nullptr;
  std::unique_ptr<Game::Systems::SelectionController> controller;
  App::Core::ClientContext context;
  std::unique_ptr<SelectedUnitsModel> model;

  SelectionFixture() {
    world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
    selection_system = world.get_system<Game::Systems::SelectionSystem>();
    controller = std::make_unique<Game::Systems::SelectionController>(
        &world, selection_system, nullptr);
    context.world = &world;
    context.selection = controller.get();
    model = std::make_unique<SelectedUnitsModel>(context);
  }

  auto add_soldier(Game::Units::SpawnType type) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = type;
    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    entity->add_component<Engine::Core::TransformComponent>();
    selection_system->select_unit(entity->get_id());
    return entity;
  }

  auto add_building() -> Engine::Core::Entity* {
    auto* entity = add_soldier(Game::Units::SpawnType::Barracks);
    entity->add_component<Engine::Core::BuildingComponent>();
    return entity;
  }

  [[nodiscard]] auto role_of(int row, int role) const -> QVariant {
    return model->data(model->index(row, 0), role);
  }

  [[nodiscard]] auto counted_in_groups() const -> int {
    int total = 0;
    for (const auto& group : model->grouped_by_type()) {
      total += group.toMap().value(QStringLiteral("count")).toInt();
    }
    return total;
  }
};

TEST(SelectedUnitsModel, DropsSoldiersThatDieWhileSelected) {
  SelectionFixture fixture;
  fixture.add_soldier(Game::Units::SpawnType::Archer);
  auto* doomed = fixture.add_soldier(Game::Units::SpawnType::Archer);
  fixture.model->refresh();
  ASSERT_EQ(fixture.model->rowCount(), 2);

  doomed->get_component<Engine::Core::UnitComponent>()->health = 0;
  fixture.model->refresh();
  EXPECT_EQ(fixture.model->rowCount(), 1)
      << "a soldier killed while selected must leave the selection panel";
  EXPECT_EQ(fixture.counted_in_groups(), fixture.model->rowCount());
}

TEST(SelectedUnitsModel, DropsSoldiersWhoseBodyLeavesTheWorld) {
  SelectionFixture fixture;
  auto* survivor = fixture.add_soldier(Game::Units::SpawnType::Spearman);
  auto* doomed = fixture.add_soldier(Game::Units::SpawnType::Spearman);
  fixture.model->refresh();
  ASSERT_EQ(fixture.model->rowCount(), 2);

  fixture.world.destroy_entity(doomed->get_id());
  fixture.model->refresh();
  EXPECT_EQ(fixture.model->rowCount(), 1);
  EXPECT_EQ(fixture.counted_in_groups(), 1)
      << "the roster count and the group cards must agree";

  fixture.world.destroy_entity(survivor->get_id());
  fixture.model->refresh();
  EXPECT_EQ(fixture.model->rowCount(), 0)
      << "an emptied selection must fall back to the empty-selection panel";
  EXPECT_EQ(fixture.counted_in_groups(), 0);
}

TEST(SelectedUnitsModel, KeepsARowPerSoldierWhenASelectedBuildingIsFilteredOut) {
  SelectionFixture fixture;
  fixture.add_soldier(Game::Units::SpawnType::Archer);
  fixture.add_building();
  fixture.model->refresh();
  ASSERT_EQ(fixture.model->rowCount(), 1);

  int resets = 0;
  QObject::connect(
      fixture.model.get(), &QAbstractItemModel::modelReset, [&resets]() { ++resets; });
  fixture.model->refresh();
  fixture.model->refresh();
  fixture.model->refresh();
  EXPECT_EQ(resets, 0) << "an unchanged selection must not rebuild the unit chips";
  EXPECT_EQ(fixture.model->rowCount(), 1);
}

TEST(SelectedUnitsModel, ReportsTheMenStillStandingInAFormationUnit) {
  SelectionFixture fixture;
  auto* archers = fixture.add_soldier(Game::Units::SpawnType::Archer);
  fixture.model->refresh();
  ASSERT_EQ(fixture.model->rowCount(), 1);

  const int roster = fixture.role_of(0, SelectedUnitsModel::MaxSoldiersRole).toInt();
  ASSERT_GT(roster, 1) << "archers fight as a formation, not as one body";
  EXPECT_EQ(fixture.role_of(0, SelectedUnitsModel::SoldiersRole).toInt(), roster)
      << "an undamaged unit must show a full roster";

  archers->get_component<Engine::Core::UnitComponent>()->health = 50;
  fixture.model->refresh();
  const int survivors = fixture.role_of(0, SelectedUnitsModel::SoldiersRole).toInt();
  EXPECT_LT(survivors, roster) << "half a unit's health is half its men";
  EXPECT_GT(survivors, 0);
  EXPECT_EQ(fixture.role_of(0, SelectedUnitsModel::MaxSoldiersRole).toInt(), roster)
      << "casualties must not shrink the denominator";
}

TEST(SelectedUnitsModel, GroupsCarryTheSoldierCountsTheHudPrints) {
  SelectionFixture fixture;
  fixture.add_soldier(Game::Units::SpawnType::Archer);
  fixture.add_soldier(Game::Units::SpawnType::Archer);
  fixture.model->refresh();

  const auto groups = fixture.model->grouped_by_type();
  ASSERT_EQ(groups.size(), 1);
  const QVariantMap group = groups.first().toMap();
  const int roster = fixture.role_of(0, SelectedUnitsModel::MaxSoldiersRole).toInt();
  EXPECT_EQ(group.value(QStringLiteral("maxSoldiers")).toInt(), roster * 2);
  EXPECT_EQ(group.value(QStringLiteral("soldiers")).toInt(), roster * 2);
}

} // namespace
