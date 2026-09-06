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
      source.find("\nvoid GameEngine::finish_save_request", capture);
  ASSERT_NE(capture_end, std::string::npos);
  const std::string capture_body = source.substr(capture, capture_end - capture);

  ASSERT_NE(capture_body.find("to_runtime_snapshot()"), std::string::npos);
  ASSERT_NE(capture_body.find("begin_save_to_slot"), std::string::npos);

  const std::size_t begin_save = source.find("void GameEngine::begin_save(");
  ASSERT_NE(begin_save, std::string::npos);
  const std::size_t begin_save_end =
      source.find("\nauto GameEngine::pending_save_capture_queued", begin_save);
  ASSERT_NE(begin_save_end, std::string::npos);
  const std::string begin_save_body =
      source.substr(begin_save, begin_save_end - begin_save);

  const std::size_t lock = begin_save_body.find("lock_frame()");
  const std::size_t inline_capture = begin_save_body.find("capture_save_to_slot(");
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
  const std::string source = read_text(root / "app" / "core" / "game_engine.cpp");
  ASSERT_FALSE(source.empty());

  const std::size_t loop = source.find("void GameEngine::run_simulation_thread() {");
  ASSERT_NE(loop, std::string::npos);
  const std::size_t loop_end = source.find("\nvoid GameEngine::simulate(", loop);
  ASSERT_NE(loop_end, std::string::npos);
  const std::string body = source.substr(loop, loop_end - loop);

  const std::size_t frame_lock = body.find("frame_lock(m_frame_mutex)");
  const std::size_t drain = body.find("drain_pending_save_capture();");
  ASSERT_NE(frame_lock, std::string::npos);
  ASSERT_NE(drain, std::string::npos)
      << "a queued save must be captured by the simulation thread, not by the GUI "
         "thread that asked for it";
  EXPECT_LT(frame_lock, drain);
}
