#include <QObject>
#include <QStringList>
#include <QVector3D>

#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <vector>

#include "app/core/client_context.h"
#include "app/orders/command_controller.h"
#include "app/viewmodels/placement_view_model.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/nav_grid.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Units::SpawnType;

class StubClientHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override {}

  auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return std::unique_lock<std::recursive_mutex>(m_frame_mutex);
  }

  void set_cursor_mode(CursorMode) override {}

private:
  std::recursive_mutex m_frame_mutex;
};

class FormationPanelWiringTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(64, 64);

    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    m_session = std::make_unique<Game::Session::SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    Game::Systems::initialize_default_content(m_session->nations());

    m_selection = &m_session->selection();
    ASSERT_NE(m_selection, nullptr);

    m_commands = std::make_unique<App::Controllers::CommandController>(
        &m_session->world(), m_selection, nullptr);

    m_context.world = &m_session->world();
    m_context.commands = m_commands.get();
    m_context.local_owner_id = 1;

    m_placement =
        std::make_unique<App::ViewModels::PlacementViewModel>(m_context, m_host);
    QObject::connect(m_placement.get(),
                     &App::ViewModels::PlacementViewModel::formation_options_changed,
                     m_placement.get(),
                     [this]() {
                       ++m_options_changed;

                       m_intents_the_panel_saw.append(
                           m_placement->formation_options().value("intent").toString());
                     });
  }

  void TearDown() override {
    m_placement.reset();
    m_commands.reset();
    m_scope.reset();
    m_session.reset();
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  auto spawn_and_select(float x, float z) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = 1;
    params.spawn_type = SpawnType::Swordsman;
    auto unit = m_factory->create(SpawnType::Swordsman, m_session->world(), params);
    if (!unit) {
      return 0;
    }
    m_selection->select_unit(unit->id());
    return unit->id();
  }

  void begin_placement() {
    for (int i = 0; i < 4; ++i) {
      ASSERT_NE(spawn_and_select(10.0F + static_cast<float>(i), 10.0F), 0U);
    }
    ASSERT_TRUE(
        m_commands->formation().begin_move_placement_at_position(QVector3D(20, 0, 20)));
    m_placement->publish_frame();
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  Game::Session::SelectionService* m_selection = nullptr;
  std::unique_ptr<App::Controllers::CommandController> m_commands;
  StubClientHost m_host;
  App::Core::ClientContext m_context;
  std::unique_ptr<App::ViewModels::PlacementViewModel> m_placement;
  int m_options_changed = 0;
  QStringList m_intents_the_panel_saw;
};

TEST_F(FormationPanelWiringTest, TheChosenFormationIsWhatThePanelReadsBack) {
  begin_placement();
  ASSERT_TRUE(m_placement->is_placing_formation());
  EXPECT_EQ(m_placement->formation_options().value("intent").toString(),
            QStringLiteral("faction_default"));

  m_placement->set_formation_intent(QStringLiteral("line"));

  m_placement->publish_frame();
  EXPECT_EQ(m_placement->formation_intent(), QStringLiteral("line"));
  EXPECT_EQ(m_placement->formation_options().value("intent").toString(),
            QStringLiteral("line"))
      << "the panel highlights whatever formation_options reports";
}

TEST_F(FormationPanelWiringTest, ThePanelIsToldToRefreshOnceTheChoiceIsPublished) {
  begin_placement();

  m_intents_the_panel_saw.clear();
  m_placement->set_formation_intent(QStringLiteral("column"));

  m_placement->publish_frame();
  EXPECT_TRUE(m_intents_the_panel_saw.contains(QStringLiteral("column")))
      << "the panel is only ever told to re-read while the published frame still "
         "holds the old choice, so the new formation never highlights; it saw "
      << m_intents_the_panel_saw.join(QStringLiteral(", ")).toStdString();
}

TEST_F(FormationPanelWiringTest, AChoiceMadeWithTheMouseStillOverThePanelStillLands) {

  begin_placement();
  m_placement->publish_frame();

  for (const char* intent : {"line", "column", "defensive", "faction_default"}) {
    m_intents_the_panel_saw.clear();
    m_placement->set_formation_intent(QString::fromLatin1(intent));
    m_placement->publish_frame();
    EXPECT_EQ(m_placement->formation_options().value("intent").toString(),
              QString::fromLatin1(intent));
    EXPECT_TRUE(m_intents_the_panel_saw.contains(QString::fromLatin1(intent)))
        << intent << " never reached the panel";
  }
}

TEST_F(FormationPanelWiringTest, AStillFrameDoesNotSpamThePanel) {
  begin_placement();
  m_placement->publish_frame();

  m_options_changed = 0;
  m_placement->publish_frame();
  m_placement->publish_frame();
  EXPECT_EQ(m_options_changed, 0)
      << "an unchanged frame must not force the panel to rebuild";
}

TEST_F(FormationPanelWiringTest, TheIntentListReachesThePanelWhenPlacementBegins) {
  begin_placement();
  const QStringList intents = m_placement->formation_intents();
  EXPECT_FALSE(intents.isEmpty());
  EXPECT_TRUE(intents.contains(QStringLiteral("line")));
}

} // namespace
