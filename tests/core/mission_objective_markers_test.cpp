#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>

#include "app/core/client_context.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/mission_view_model.h"

namespace {

class SilentHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override {}
  void set_cursor_mode(CursorMode) override {}
};

auto settlement_stage(const QString& title,
                      float nx,
                      bool complete,
                      bool held_by_player) -> QVariantMap {
  QVariantMap stage;
  stage["title"] = title;
  stage["type"] = QStringLiteral("capture_structures");
  stage["complete"] = complete;
  stage["has_target"] = true;
  stage["nx"] = nx;
  stage["ny"] = 0.5F;
  stage["target_structure_present"] = true;
  stage["target_structure_is_local"] = held_by_player;
  return stage;
}

auto plain_target_stage(const QString& title, bool complete) -> QVariantMap {
  QVariantMap stage;
  stage["title"] = title;
  stage["type"] = QStringLiteral("reach_position");
  stage["complete"] = complete;
  stage["has_target"] = true;
  stage["nx"] = 0.1F;
  stage["ny"] = 0.1F;
  stage["target_structure_present"] = false;
  stage["target_structure_is_local"] = false;
  return stage;
}

class MissionMarkersTest : public ::testing::Test {
protected:
  [[nodiscard]] auto markers_for(const QVariantList& stages) -> QVariantList {
    App::ViewModels::MissionViewModel mission(m_context, m_host, m_camera);
    mission.set_stages(stages);
    return mission.markers();
  }

  [[nodiscard]] static auto titles_of(const QVariantList& markers) -> QStringList {
    QStringList titles;
    for (const auto& marker : markers) {
      titles.append(marker.toMap().value(QStringLiteral("title")).toString());
    }
    return titles;
  }

  App::Core::ClientContext m_context;
  SilentHost m_host;
  App::ViewModels::CameraViewModel m_camera{m_context, m_host};
};

TEST_F(MissionMarkersTest, EveryEnemySettlementLeftToTakeIsPinned) {
  const QVariantList stages{
      settlement_stage(QStringLiteral("Take the hill fort"), 0.2F, false, false),
      settlement_stage(QStringLiteral("Take the river town"), 0.8F, false, false)};

  const auto markers = markers_for(stages);
  ASSERT_EQ(markers.size(), 2)
      << "both villages the mission asks for belong on the minimap";
  EXPECT_EQ(titles_of(markers),
            QStringList({QStringLiteral("Take the hill fort"),
                         QStringLiteral("Take the river town")}));
  EXPECT_TRUE(markers.at(0).toMap().value(QStringLiteral("active")).toBool());
  EXPECT_FALSE(markers.at(1).toMap().value(QStringLiteral("active")).toBool());
}

TEST_F(MissionMarkersTest, ASettlementThePlayerHoldsLosesItsPin) {
  const QVariantList stages{
      settlement_stage(QStringLiteral("Take the hill fort"), 0.2F, true, true),
      settlement_stage(QStringLiteral("Take the river town"), 0.8F, false, false)};

  const auto markers = markers_for(stages);
  ASSERT_EQ(markers.size(), 1);
  EXPECT_EQ(titles_of(markers), QStringList({QStringLiteral("Take the river town")}));
}

TEST_F(MissionMarkersTest, ObjectivesWithoutASettlementStayOnTheActiveStage) {
  const QVariantList stages{
      plain_target_stage(QStringLiteral("Reach the ford"), false),
      plain_target_stage(QStringLiteral("Reach the pass"), false)};

  const auto markers = markers_for(stages);
  ASSERT_EQ(markers.size(), 1)
      << "a walk-here objective should not pin every later waypoint";
  EXPECT_EQ(titles_of(markers), QStringList({QStringLiteral("Reach the ford")}));
}

TEST_F(MissionMarkersTest, FinishedCaptureWorkClearsThePins) {
  const QVariantList stages{
      settlement_stage(QStringLiteral("Take the hill fort"), 0.2F, true, true),
      settlement_stage(QStringLiteral("Take the river town"), 0.8F, true, true)};

  EXPECT_TRUE(markers_for(stages).isEmpty());
}

TEST_F(MissionMarkersTest, ASettlementRazedRatherThanTakenIsNotPinnedAhead) {
  QVariantList stages{
      plain_target_stage(QStringLiteral("Reach the ford"), false),
      settlement_stage(QStringLiteral("Take the river town"), 0.8F, false, false)};
  QVariantMap razed = stages.at(1).toMap();
  razed["target_structure_present"] = false;
  stages[1] = razed;

  const auto markers = markers_for(stages);
  ASSERT_EQ(markers.size(), 1);
  EXPECT_EQ(titles_of(markers), QStringList({QStringLiteral("Reach the ford")}));
}

} // namespace
