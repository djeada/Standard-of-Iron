#include <QString>

#include <gtest/gtest.h>

#include "app/persistence/save_load_coordinator.h"
#include "game/session/session_context.h"
#include "game/systems/match_snapshot.h"

namespace {

using App::Core::SaveLoadCoordinator;

struct RuntimeState {
  bool paused = false;
  float time_scale = 1.0F;
  int local_owner_id = 1;
  QString victory_state;
  CursorMode cursor_mode{CursorMode::Normal};
  int selected_player_id = 1;
  bool follow_selection = false;

  auto
  as_context(Game::Session::SessionContext& session) -> App::Core::ApplyRuntimeContext {
    return {.session = session,
            .paused = paused,
            .time_scale = time_scale,
            .local_owner_id = local_owner_id,
            .victory_state = victory_state,
            .cursor_mode = cursor_mode,
            .selected_player_id = selected_player_id,
            .follow_selection = follow_selection};
  }
};

TEST(SaveRuntimeRestoreTest, ALoadedMatchResumesEvenWhenTheSaveWasPaused) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);

  Game::Systems::RuntimeSnapshot snapshot;
  snapshot.paused = true;
  snapshot.time_scale = 2.0F;
  snapshot.local_owner_id = 3;

  RuntimeState runtime;
  const SaveLoadCoordinator coordinator;
  coordinator.apply_runtime_snapshot(snapshot, runtime.as_context(session));

  EXPECT_FALSE(runtime.paused) << "a restored battle must run";
  EXPECT_FLOAT_EQ(runtime.time_scale, 2.0F);
  EXPECT_EQ(runtime.local_owner_id, 3);
}

TEST(SaveRuntimeRestoreTest, ARunningSaveAlsoResumes) {
  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);

  Game::Systems::RuntimeSnapshot snapshot;
  snapshot.paused = false;

  RuntimeState runtime;
  runtime.paused = true;
  const SaveLoadCoordinator coordinator;
  coordinator.apply_runtime_snapshot(snapshot, runtime.as_context(session));

  EXPECT_FALSE(runtime.paused);
}

} // namespace
