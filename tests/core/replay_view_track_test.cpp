#include <QTemporaryDir>
#include <QVector3D>

#include <gtest/gtest.h>

#include "app/session/replay_view_track.h"
#include "game/session/selection_service.h"
#include "scene/camera.h"

namespace {

using App::Session::ReplayViewPointer;
using App::Session::ReplayViewTrack;

void aim(Render::GL::Camera& camera, float x) {
  camera.look_at(QVector3D(x, 40.0F, 30.0F),
                 QVector3D(x, 0.0F, 0.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));
}

TEST(ReplayViewTrackTest, RecordedViewPlaysBackAtTheSameTicks) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = ReplayViewTrack::sidecar_path(dir.filePath("match.replay"));
  Render::GL::Camera camera;
  {
    ReplayViewTrack track;
    ASSERT_TRUE(track.begin_recording(path));
    aim(camera, 0.0F);
    track.note_pointer(QPointF(0.25, 0.5), 0);
    track.record(0, camera, {});
    aim(camera, 10.0F);
    track.note_pointer(QPointF(0.75, 0.5), ReplayViewPointer::k_right);
    track.record(10, camera, {7, 9});
    track.finish();
  }

  ReplayViewTrack track;
  ASSERT_TRUE(track.load(path));
  const auto halfway = track.sample_at(5);
  ASSERT_TRUE(halfway.has_value());
  EXPECT_NEAR(halfway->target.x(), 5.0F, 1e-4F);
  EXPECT_NEAR(halfway->position.x(), 5.0F, 1e-4F);
  ASSERT_TRUE(halfway->pointer.has_value());
  EXPECT_NEAR(halfway->pointer->position.x(), 0.5, 1e-6);
  EXPECT_EQ(halfway->pointer->buttons, 0);
  const auto last = track.sample_at(40);
  ASSERT_TRUE(last.has_value());
  EXPECT_NEAR(last->target.x(), 10.0F, 1e-4F);
  EXPECT_EQ(last->pointer->buttons, ReplayViewPointer::k_right);

  Game::Session::SelectionService selection;
  Render::GL::Camera replayed;
  track.apply(3, replayed, selection);
  EXPECT_TRUE(selection.get_selected_units().empty());
  EXPECT_NEAR(replayed.get_target().x(), 3.0F, 1e-3F);
  track.apply(12, replayed, selection);
  EXPECT_EQ(selection.get_selected_units(),
            (std::vector<Engine::Core::EntityID>{7, 9}));
}

TEST(ReplayViewTrackTest, AMissingSidecarLoadsNothing) {
  QTemporaryDir dir;
  ReplayViewTrack track;
  EXPECT_FALSE(track.load(dir.filePath("absent.view.jsonl")));
  EXPECT_FALSE(track.sample_at(0).has_value());
}

} // namespace
