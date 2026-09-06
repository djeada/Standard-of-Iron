#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "game/core/system_access_recorder.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"

namespace {

constexpr int k_map_size = 32;

} // namespace

TEST(SystemAccessVerificationTest, RecordingIsOffUntilAScopeInstallsIt) {
  Engine::Core::SystemAccessRecorder recorder;
  EXPECT_EQ(Engine::Core::Detail::active_access_recorder(), nullptr);
  {
    const Engine::Core::ScopedAccessRecording recording(&recorder);
    EXPECT_EQ(Engine::Core::Detail::active_access_recorder(), &recorder);
  }
  EXPECT_EQ(Engine::Core::Detail::active_access_recorder(), nullptr);
}

TEST(SystemAccessVerificationTest, TouchedComponentTypesAreRecordedByKind) {
  Engine::Core::SystemAccessRecorder recorder;
  recorder.note(3, true);
  recorder.note(5, false);

  EXPECT_TRUE(recorder.touched(3, true));
  EXPECT_FALSE(recorder.touched(3, false));
  EXPECT_TRUE(recorder.touched(5, false));
  EXPECT_FALSE(recorder.touched(9, true));

  recorder.clear();
  EXPECT_FALSE(recorder.touched(3, true));
}

TEST(SystemAccessVerificationTest, EveryDeclaredFootprintCoversWhatItsSystemTouches) {
  if (SOI_VERIFY_SYSTEM_ACCESS == 0) {
    GTEST_SKIP() << "component access recording is compiled out of this build";
  }

  Game::Session::SessionContext session;
  const Game::Session::ScopedSession scope(session);
  session.owners().register_owner_with_id(
      1, Game::Systems::OwnerType::Player, "verifier");
  session.owners().set_local_player_id(1);
  Game::Systems::NavGrid::initialize(k_map_size, k_map_size);
  Game::Systems::register_runtime_systems(session.world());

  session.world().set_access_verification(true);
  session.world().update(1.0F / 60.0F);
  session.world().set_access_verification(false);

  std::vector<std::string> reported;
  for (const auto& violation : session.world().access_violations()) {
    reported.push_back(std::string(violation.system_name) +
                       (violation.write ? " writes " : " reads ") +
                       std::to_string(violation.type_id) + " without declaring it");
  }

  std::string message;
  for (const auto& line : reported) {
    message += "\n  " + line;
  }
  EXPECT_TRUE(reported.empty())
      << "a system reached a component its access() does not declare, so the phase "
         "schedule cannot be trusted to batch it:"
      << message;
}
