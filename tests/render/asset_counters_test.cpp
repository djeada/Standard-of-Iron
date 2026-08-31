#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <string_view>

#include "game/core/nav_profile.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/profiling/asset_counters.h"
#include "render/profiling/performance_report.h"

namespace {

using Render::Profiling::AssetCounter;
using Render::Profiling::AssetCounters;

class AssetCountersTest : public ::testing::Test {
protected:
  void SetUp() override {
    Render::Creature::set_runtime_bake_forbidden(false);
    Render::Profiling::asset_counters().reset();
    Engine::Core::nav_profile().clear();
    Engine::Core::nav_profile().set_enabled(false);
  }

  void TearDown() override {
    Render::Creature::set_runtime_bake_forbidden(false);
    Render::Profiling::asset_counters().reset();
  }

  static auto counters() -> AssetCounters& {
    return Render::Profiling::asset_counters();
  }
};

TEST_F(AssetCountersTest, EveryCounterHasADistinctName) {
  std::set<std::string_view> names;
  for (std::size_t i = 0; i < AssetCounters::k_count; ++i) {
    const auto counter = static_cast<AssetCounter>(i);
    const std::string_view name = Render::Profiling::asset_counter_name(counter);
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, "unknown");
    EXPECT_TRUE(names.insert(name).second) << "duplicate counter name " << name;
  }
}

TEST_F(AssetCountersTest, TotalsAccumulateAndSurviveTheBarrier) {
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 3);
  counters().mark_load_barrier();
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 2);
  EXPECT_EQ(counters().total(AssetCounter::RiggedMeshBake), 5U);
  EXPECT_EQ(counters().since_barrier(AssetCounter::RiggedMeshBake), 2U);
}

TEST_F(AssetCountersTest, WorkBeforeTheBarrierIsNotAViolation) {
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 9);
  Render::Profiling::count_asset(AssetCounter::GlBufferCreated, 40);
  counters().mark_load_barrier();
  EXPECT_EQ(counters().post_barrier_asset_work(), 0U);
  EXPECT_TRUE(Render::Profiling::format_post_barrier_violations().empty());
}

TEST_F(AssetCountersTest, PostBarrierWorkIsNamedInTheViolationSummary) {
  counters().mark_load_barrier();
  Render::Profiling::count_asset(AssetCounter::SnapshotMeshBake, 4);
  Render::Profiling::count_asset(AssetCounter::ShaderCompiled, 1);
  EXPECT_EQ(counters().post_barrier_asset_work(), 5U);
  const std::string summary = Render::Profiling::format_post_barrier_violations();
  EXPECT_NE(summary.find("snapshot_mesh_bakes=4"), std::string::npos);
  EXPECT_NE(summary.find("shaders_compiled=1"), std::string::npos);
}

TEST_F(AssetCountersTest, CacheHitsAreNotCountedAsPostLoadWork) {
  counters().mark_load_barrier();
  Render::Profiling::count_asset(AssetCounter::RiggedCacheHit, 5000);
  Render::Profiling::count_asset(AssetCounter::SnapshotCacheHit, 5000);
  Render::Profiling::count_asset(AssetCounter::GlUploadBytes, 1U << 20U);
  EXPECT_EQ(counters().post_barrier_asset_work(), 0U);
}

TEST_F(AssetCountersTest, UnmarkedBarrierReportsNoPostLoadWork) {
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 7);
  EXPECT_FALSE(counters().load_barrier_marked());
  EXPECT_EQ(counters().post_barrier_asset_work(), 0U);
  EXPECT_TRUE(Render::Profiling::format_post_barrier_violations().empty());
}

TEST_F(AssetCountersTest, ForbiddingRuntimeBakesMarksTheLoadBarrier) {
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 6);
  EXPECT_FALSE(counters().load_barrier_marked());

  Render::Creature::set_runtime_bake_forbidden(true);
  EXPECT_TRUE(counters().load_barrier_marked());
  EXPECT_EQ(counters().since_barrier(AssetCounter::RiggedMeshBake), 0U);

  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 1);
  EXPECT_EQ(counters().since_barrier(AssetCounter::RiggedMeshBake), 1U);
}

TEST_F(AssetCountersTest, ReloadingContentClearsTheBarrier) {
  Render::Creature::set_runtime_bake_forbidden(true);
  ASSERT_TRUE(counters().load_barrier_marked());
  Render::Creature::set_runtime_bake_forbidden(false);
  EXPECT_FALSE(counters().load_barrier_marked());
}

TEST_F(AssetCountersTest, AnAllowScopeDoesNotMoveTheBarrier) {
  Render::Creature::set_runtime_bake_forbidden(true);
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 2);
  {
    const Render::Creature::RuntimeBakeAllowScope allow;
    EXPECT_FALSE(Render::Creature::runtime_bake_forbidden());
    EXPECT_TRUE(counters().load_barrier_marked());
    Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 1);
  }
  EXPECT_TRUE(Render::Creature::runtime_bake_forbidden());
  EXPECT_TRUE(counters().load_barrier_marked());
  EXPECT_EQ(counters().since_barrier(AssetCounter::RiggedMeshBake), 3U);
}

TEST_F(AssetCountersTest, JsonSeparatesTotalsFromPostLoadWork) {
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 4);
  counters().mark_load_barrier();
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 1);

  const QJsonObject json = Render::Profiling::asset_counters_json();
  EXPECT_TRUE(json.value(QStringLiteral("load_barrier_marked")).toBool());
  EXPECT_EQ(json.value(QStringLiteral("total"))
                .toObject()
                .value(QStringLiteral("rigged_mesh_bakes"))
                .toInt(),
            5);
  EXPECT_EQ(json.value(QStringLiteral("after_load_barrier"))
                .toObject()
                .value(QStringLiteral("rigged_mesh_bakes"))
                .toInt(),
            1);
  EXPECT_EQ(json.value(QStringLiteral("post_load_asset_work")).toInt(), 1);
}

TEST_F(AssetCountersTest, JsonOmitsPostLoadSectionBeforeTheBarrier) {
  const QJsonObject json = Render::Profiling::asset_counters_json();
  EXPECT_FALSE(json.value(QStringLiteral("load_barrier_marked")).toBool());
  EXPECT_FALSE(json.contains(QStringLiteral("after_load_barrier")));
  EXPECT_FALSE(json.contains(QStringLiteral("post_load_asset_work")));
}

auto passing_measurement() -> Render::Profiling::PerformanceMeasurement {
  Render::Profiling::PerformanceMeasurement measured;
  measured.frames = 1800;
  measured.frame_p50_ms = 8.0;
  measured.frame_p95_ms = 12.0;
  measured.frame_p99_ms = 15.0;
  measured.frame_max_ms = 24.0;
  measured.update_average_ms = 1.0;
  measured.update_p95_ms = 2.0;
  measured.render_submit_p95_ms = 2.0;
  measured.gpu_shadow_p95_ms = 4.0;
  measured.gpu_color_p95_ms = 6.0;
  measured.gpu_timed = true;
  measured.ultra_preset = true;
  measured.full_creature_lod = true;
  return measured;
}

TEST_F(AssetCountersTest, ACleanUltraRunPassesTheReleaseGate) {
  Render::Creature::set_runtime_bake_forbidden(true);
  const QJsonObject verdict = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), passing_measurement());
  EXPECT_TRUE(verdict.value(QStringLiteral("passed")).toBool())
      << QJsonDocument(verdict.value(QStringLiteral("failures")).toArray())
             .toJson()
             .toStdString();
  EXPECT_TRUE(verdict.value(QStringLiteral("failures")).toArray().isEmpty());
}

TEST_F(AssetCountersTest, ASlowFrameFailsAndNamesTheBudgetItMissed) {
  Render::Creature::set_runtime_bake_forbidden(true);
  auto measured = passing_measurement();
  measured.frame_p95_ms = 40.0;
  const QJsonObject verdict = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), measured);
  EXPECT_FALSE(verdict.value(QStringLiteral("passed")).toBool());
  const QJsonObject check = verdict.value(QStringLiteral("checks"))
                                .toObject()
                                .value(QStringLiteral("frame_p95_ms"))
                                .toObject();
  EXPECT_FALSE(check.value(QStringLiteral("passed")).toBool());
  EXPECT_DOUBLE_EQ(check.value(QStringLiteral("measured")).toDouble(), 40.0);
}

TEST_F(AssetCountersTest, AnUntimedGpuCannotClaimTheGpuBudget) {
  Render::Creature::set_runtime_bake_forbidden(true);
  auto measured = passing_measurement();
  measured.gpu_timed = false;
  const QJsonObject verdict = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), measured);
  EXPECT_FALSE(verdict.value(QStringLiteral("passed")).toBool());
  EXPECT_FALSE(verdict.value(QStringLiteral("checks"))
                   .toObject()
                   .contains(QStringLiteral("gpu_total_p95_ms")));
}

TEST_F(AssetCountersTest, ANonUltraRunCannotPass) {
  Render::Creature::set_runtime_bake_forbidden(true);
  auto measured = passing_measurement();
  measured.ultra_preset = false;
  const QJsonObject verdict = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), measured);
  EXPECT_FALSE(verdict.value(QStringLiteral("passed")).toBool());
}

TEST_F(AssetCountersTest, PostLoadBakesFailTheGate) {
  Render::Creature::set_runtime_bake_forbidden(true);
  Render::Profiling::count_asset(AssetCounter::RiggedMeshBake, 1);
  const QJsonObject verdict = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), passing_measurement());
  EXPECT_FALSE(verdict.value(QStringLiteral("passed")).toBool());
  EXPECT_FALSE(verdict.value(QStringLiteral("checks"))
                   .toObject()
                   .value(QStringLiteral("post_load_asset_work"))
                   .toObject()
                   .value(QStringLiteral("passed"))
                   .toBool());
}

TEST_F(AssetCountersTest, ARunThatNeverReachedTheBarrierCannotPass) {
  const QJsonObject verdict = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), passing_measurement());
  EXPECT_FALSE(verdict.value(QStringLiteral("passed")).toBool());
}

TEST_F(AssetCountersTest, NavigationBudgetsOnlyApplyOnceTicksWereObserved) {
  Render::Creature::set_runtime_bake_forbidden(true);
  const QJsonObject before = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), passing_measurement());
  EXPECT_FALSE(before.value(QStringLiteral("checks"))
                   .toObject()
                   .contains(QStringLiteral("navigation_p95_ms")));

  Engine::Core::nav_profile().set_enabled(true);
  {
    const Engine::Core::NavTickScope tick;
    Engine::Core::count_nav(Engine::Core::NavCounter::ElapsedUs, 9000);
  }
  const QJsonObject after = Render::Profiling::budget_verdict_json(
      Render::Profiling::PerformanceBudget::release_gate(), passing_measurement());
  Engine::Core::nav_profile().set_enabled(false);
  EXPECT_FALSE(after.value(QStringLiteral("checks"))
                   .toObject()
                   .value(QStringLiteral("navigation_p95_ms"))
                   .toObject()
                   .value(QStringLiteral("passed"))
                   .toBool());
  EXPECT_FALSE(after.value(QStringLiteral("passed")).toBool());
}

TEST_F(AssetCountersTest, ScaleGateRelaxesTheFrameBudgetOnly) {
  const auto scale = Render::Profiling::PerformanceBudget::scale_gate(33.3);
  EXPECT_DOUBLE_EQ(scale.frame_p95_ms, 33.3);
  EXPECT_GT(scale.frame_p99_ms, scale.frame_p95_ms);
  EXPECT_GT(scale.frame_max_ms, scale.frame_p99_ms);
  EXPECT_DOUBLE_EQ(
      scale.update_average_ms,
      Render::Profiling::PerformanceBudget::release_gate().update_average_ms);
}

TEST_F(AssetCountersTest, NavigationJsonReportsEveryCounter) {
  const QJsonObject json = Render::Profiling::navigation_counters_json();
  const QJsonObject per_tick =
      json.value(QStringLiteral("per_tick_average")).toObject();
  EXPECT_EQ(per_tick.size(), static_cast<int>(Engine::Core::NavProfile::k_count));
  EXPECT_TRUE(per_tick.contains(QStringLiteral("standability_tests")));
}

} // namespace
