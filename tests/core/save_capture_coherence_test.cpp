#include <QJsonDocument>
#include <QJsonObject>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/save/serialization.h"
#include "game/session/deterministic_rng.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"

namespace {

using Engine::Core::CaptureStamp;
using Engine::Core::Serialization;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

void advance_one_tick(SessionContext& session) {
  session.clock().restore(session.clock().tick() + 1);
  (void)session.rng().next_u64();
}

auto find_repo_root() -> std::filesystem::path {
  auto has_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "app" / "core" / "game_engine.cpp");
  };
  auto walk_up = [&](std::filesystem::path path) -> std::filesystem::path {
    while (!path.empty()) {
      if (has_markers(path)) {
        return path;
      }
      const auto parent = path.parent_path();
      if (parent == path) {
        break;
      }
      path = parent;
    }
    return {};
  };
  if (auto from_file = walk_up(std::filesystem::path(__FILE__).parent_path());
      !from_file.empty()) {
    return from_file;
  }
  return walk_up(std::filesystem::current_path());
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

class SaveCaptureCoherenceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

} // namespace

TEST_F(SaveCaptureCoherenceTest, SerializedWorldCarriesTheSessionTickAndDrawCount) {
  SessionContext session;
  const ScopedSession scope(session);

  for (int i = 0; i < 7; ++i) {
    advance_one_tick(session);
  }

  const QJsonDocument doc = Serialization::serialize_world(&session.world());
  const CaptureStamp stamp = Serialization::read_capture_stamp(doc);

  EXPECT_TRUE(stamp.present);
  EXPECT_EQ(stamp.tick, session.clock().tick());
  EXPECT_EQ(stamp.rng_draws, session.rng().draw_count());
  EXPECT_TRUE(stamp.matches(session.clock().tick(), session.rng().draw_count()));
}

TEST_F(SaveCaptureCoherenceTest, StampFollowsTheSessionAsItAdvances) {
  SessionContext session;
  const ScopedSession scope(session);

  const CaptureStamp first = Serialization::read_capture_stamp(
      Serialization::serialize_world(&session.world()));
  advance_one_tick(session);
  const CaptureStamp second = Serialization::read_capture_stamp(
      Serialization::serialize_world(&session.world()));

  EXPECT_EQ(second.tick, first.tick + 1);
  EXPECT_EQ(second.rng_draws, first.rng_draws + 1);
  EXPECT_FALSE(second.matches(first.tick, first.rng_draws));
}

TEST_F(SaveCaptureCoherenceTest, AbsentStampIsTreatedAsCompatible) {
  QJsonObject legacy;
  legacy["schemaVersion"] = 2;
  const CaptureStamp stamp =
      Serialization::read_capture_stamp(QJsonDocument(std::move(legacy)));

  EXPECT_FALSE(stamp.present);
  EXPECT_TRUE(stamp.matches(1234, 5678));
}

TEST_F(SaveCaptureCoherenceTest, MetadataReadOutsideTheCaptureIsDetectedAsTorn) {
  SessionContext session;
  const ScopedSession scope(session);

  std::mutex capture_mutex;
  std::atomic<bool> stop{false};
  std::thread simulation([&]() {
    const ScopedSession thread_scope(session);
    while (!stop.load(std::memory_order_acquire)) {
      const std::lock_guard<std::mutex> tick_lock(capture_mutex);
      advance_one_tick(session);
    }
  });

  int torn = 0;
  for (int attempt = 0; attempt < 200 && torn == 0; ++attempt) {
    const std::uint64_t metadata_tick = session.clock().tick();
    const std::uint64_t metadata_draws = session.rng().draw_count();
    std::this_thread::yield();
    const CaptureStamp stamp = Serialization::read_capture_stamp(
        Serialization::serialize_world(&session.world()));
    if (!stamp.matches(metadata_tick, metadata_draws)) {
      ++torn;
    }
  }

  stop.store(true, std::memory_order_release);
  simulation.join();

  EXPECT_GT(torn, 0)
      << "reading session metadata outside the capture lock must be detectable";
}

TEST_F(SaveCaptureCoherenceTest, MetadataReadInsideTheCaptureLockIsNeverTorn) {
  SessionContext session;
  const ScopedSession scope(session);

  std::mutex capture_mutex;
  std::atomic<bool> stop{false};
  std::thread simulation([&]() {
    const ScopedSession thread_scope(session);
    while (!stop.load(std::memory_order_acquire)) {
      const std::lock_guard<std::mutex> tick_lock(capture_mutex);
      advance_one_tick(session);
    }
  });

  int torn = 0;
  for (int attempt = 0; attempt < 200; ++attempt) {
    const std::lock_guard<std::mutex> capture_lock(capture_mutex);
    const std::uint64_t metadata_tick = session.clock().tick();
    const std::uint64_t metadata_draws = session.rng().draw_count();
    const CaptureStamp stamp = Serialization::read_capture_stamp(
        Serialization::serialize_world(&session.world()));
    if (!stamp.matches(metadata_tick, metadata_draws)) {
      ++torn;
    }
  }

  stop.store(true, std::memory_order_release);
  simulation.join();

  EXPECT_EQ(torn, 0);
}

TEST_F(SaveCaptureCoherenceTest, TheSaveCaptureAlwaysRunsUnderTheFrameLock) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const std::string source = read_text(root / "app" / "core" / "game_engine.cpp");
  ASSERT_FALSE(source.empty());

  const std::size_t capture = source.find("auto GameEngine::capture_save_to_slot(");
  ASSERT_NE(capture, std::string::npos);
  const std::size_t capture_end =
      source.find("\nauto GameEngine::consume_screenshot_request", capture);
  ASSERT_NE(capture_end, std::string::npos);
  const std::string capture_body = source.substr(capture, capture_end - capture);

  ASSERT_NE(capture_body.find("to_runtime_snapshot()"), std::string::npos);
  ASSERT_NE(capture_body.find("begin_save_to_slot"), std::string::npos);

  const std::string controller =
      read_text(root / "app" / "persistence" / "save_slot_controller.cpp");
  ASSERT_FALSE(controller.empty());
  const std::size_t begin_save =
      controller.find("void SaveSlotController::begin_save(");
  ASSERT_NE(begin_save, std::string::npos);
  const std::size_t begin_save_end =
      controller.find("\nauto SaveSlotController::queue_capture", begin_save);
  ASSERT_NE(begin_save_end, std::string::npos);
  const std::string begin_save_body =
      controller.substr(begin_save, begin_save_end - begin_save);

  const std::size_t lock = begin_save_body.find("m_hooks.lock_frame()");
  const std::size_t inline_capture = begin_save_body.find("m_hooks.capture(");
  ASSERT_NE(lock, std::string::npos)
      << "the fallback capture in begin_save must hold the frame lock so the world, "
         "the clock, the RNG and the mission state all come from one tick";
  ASSERT_NE(inline_capture, std::string::npos);
  EXPECT_LT(lock, inline_capture);
  EXPECT_EQ(begin_save_body.find("to_runtime_snapshot()"), std::string::npos)
      << "begin_save must not capture on the GUI thread itself";
}

TEST_F(SaveCaptureCoherenceTest, TheSimulationThreadCapturesQueuedSavesUnderTheLock) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const std::string lifecycle =
      read_text(root / "app" / "core" / "simulation_lifecycle.cpp");
  ASSERT_FALSE(lifecycle.empty());

  const std::size_t loop = lifecycle.find("void SimulationLifecycle::run() {");
  ASSERT_NE(loop, std::string::npos);
  const std::size_t loop_end = lifecycle.find("\n}\n", loop);
  ASSERT_NE(loop_end, std::string::npos);
  const std::string loop_body = lifecycle.substr(loop, loop_end - loop);

  const std::size_t frame_lock = loop_body.find("frame_lock(m_frame_mutex)");
  const std::size_t tick_body = loop_body.find("m_body(dt);");
  ASSERT_NE(frame_lock, std::string::npos);
  ASSERT_NE(tick_body, std::string::npos);
  EXPECT_LT(frame_lock, tick_body)
      << "every simulation tick, including the save capture inside it, runs under "
         "the frame lock";

  const std::string source = read_text(root / "app" / "core" / "game_engine.cpp");
  ASSERT_FALSE(source.empty());
  const std::size_t tick =
      source.find("void GameEngine::run_simulation_tick(float dt) {");
  ASSERT_NE(tick, std::string::npos);
  const std::size_t tick_end = source.find("\n}\n", tick);
  ASSERT_NE(tick_end, std::string::npos);
  const std::string body = source.substr(tick, tick_end - tick);

  const std::size_t presentation = body.find("update_presentation(dt);");
  const std::size_t drain = body.find("m_saves->drain_pending_capture();");
  ASSERT_NE(presentation, std::string::npos);
  ASSERT_NE(drain, std::string::npos)
      << "a queued save must be captured by the simulation thread, not by the GUI "
         "thread that asked for it";
  EXPECT_LT(presentation, drain);
}

TEST_F(SaveCaptureCoherenceTest,
       ALoadRestoresTheCameraAfterTheEnvironmentFramesTheMap) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty());
  const std::string source =
      read_text(root / "app" / "persistence" / "save_load_coordinator.cpp");
  ASSERT_FALSE(source.empty());

  const std::size_t load = source.find("SaveLoadCoordinator::load_from_slot(");
  ASSERT_NE(load, std::string::npos);
  const std::size_t environment =
      source.find("restore_environment_from_metadata(", load);
  const std::size_t camera = source.find("restore_camera_from_metadata(", load);
  ASSERT_NE(environment, std::string::npos);
  ASSERT_NE(camera, std::string::npos);
  EXPECT_GT(camera, environment)
      << "Environment::apply frames the map's opening view; a camera restored before "
         "it is overwritten and every load opens on that view";
}
