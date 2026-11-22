#include <QPoint>

#include <cstdlib>
#include <gtest/gtest.h>

#include "app/commander/commander_look.h"
#include "app/commander/commander_pointer_capture.h"

namespace {

using App::Core::CommanderLook;
using App::Core::CommanderPointerCapture;
using App::Core::PointerDevice;

class FakePointer final : public PointerDevice {
public:
  [[nodiscard]] auto position() const -> QPoint override { return m_position; }

  void warp(const QPoint& global_position) override {
    ++warps;
    if (refuse_warps) {
      return;
    }
    m_position = global_position;
    if (clamp_x_max >= 0 && m_position.x() > clamp_x_max) {
      m_position.setX(clamp_x_max);
    }
    m_position += race_offset;
  }

  void move_by(int dx, int dy) { m_position += QPoint(dx, dy); }
  void place(const QPoint& position) { m_position = position; }
  [[nodiscard]] auto where() const -> QPoint { return m_position; }

  int warps = 0;
  bool refuse_warps = false;
  int clamp_x_max = -1;
  QPoint race_offset;

private:
  QPoint m_position{200, 200};
};

const QPoint k_centre(960, 540);

TEST(CommanderPointerCaptureTest, AnUncapturedPointerIsLeftAlone) {
  FakePointer pointer;
  CommanderPointerCapture capture;

  pointer.move_by(40, 0);
  EXPECT_TRUE(capture.poll(pointer, k_centre).isNull());
  pointer.move_by(40, 0);
  EXPECT_TRUE(capture.poll(pointer, k_centre).isNull());
  EXPECT_EQ(pointer.warps, 0) << "the pointer belongs to the interface until captured";
}

TEST(CommanderPointerCaptureTest, TakingThePointerNeverCountsTheJumpToTheCentre) {
  FakePointer pointer;
  CommanderPointerCapture capture;
  capture.capture();

  EXPECT_TRUE(capture.poll(pointer, k_centre).isNull());
  EXPECT_EQ(pointer.where(), k_centre);

  pointer.move_by(12, -5);
  EXPECT_EQ(capture.poll(pointer, k_centre), QPoint(12, -5));
  EXPECT_EQ(pointer.where(), k_centre) << "every move is followed by a recentre";
}

TEST(CommanderPointerCaptureTest, AHandMovingDuringTheWarpDoesNotEndRecentring) {
  FakePointer pointer;
  CommanderPointerCapture capture;
  capture.capture();
  static_cast<void>(capture.poll(pointer, k_centre));

  pointer.race_offset = QPoint(9, 4);
  pointer.move_by(300, 0);
  EXPECT_EQ(capture.poll(pointer, k_centre), QPoint(300, 0));
  EXPECT_TRUE(capture.warp_supported())
      << "landing a few pixels off the centre is a race with the hand, not a refusal";

  pointer.race_offset = QPoint();
  int travelled = 0;
  for (int frame = 0; frame < 200; ++frame) {
    pointer.move_by(300, 0);
    travelled += capture.poll(pointer, k_centre).x();
    ASSERT_LT(std::abs(pointer.where().x() - k_centre.x()), 320)
        << "the pointer ran away from the centre at frame " << frame;
  }
  EXPECT_EQ(travelled, 200 * 300)
      << "a long turn must keep turning: the pointer never reaches a screen edge";
}

TEST(CommanderPointerCaptureTest, MotionAfterARacedWarpIsMeasuredFromWhereItLanded) {
  FakePointer pointer;
  CommanderPointerCapture capture;
  capture.capture();
  static_cast<void>(capture.poll(pointer, k_centre));

  pointer.race_offset = QPoint(7, 0);
  pointer.move_by(50, 0);
  EXPECT_EQ(capture.poll(pointer, k_centre), QPoint(50, 0));
  pointer.race_offset = QPoint();

  EXPECT_TRUE(capture.poll(pointer, k_centre).isNull())
      << "the offset left by the race is not a look delta";
  pointer.move_by(3, 0);
  EXPECT_EQ(capture.poll(pointer, k_centre), QPoint(3, 0));
}

TEST(CommanderPointerCaptureTest, AResetKeepsThePointerAndDropsOnlyTheNextDelta) {
  FakePointer pointer;
  CommanderPointerCapture capture;
  capture.capture();
  static_cast<void>(capture.poll(pointer, k_centre));

  capture.suspend();
  pointer.move_by(500, 200);
  EXPECT_TRUE(capture.poll(pointer, k_centre).isNull())
      << "motion across a suspension is not a look delta";
  EXPECT_EQ(pointer.where(), k_centre) << "and the pointer is taken back at once";

  pointer.move_by(-20, 6);
  EXPECT_EQ(capture.poll(pointer, k_centre), QPoint(-20, 6));
}

TEST(CommanderPointerCaptureTest, ARefusedWarpStillTurnsAndIsRetriedSparingly) {
  FakePointer pointer;
  pointer.refuse_warps = true;
  CommanderPointerCapture capture;
  capture.capture();
  static_cast<void>(capture.poll(pointer, k_centre));

  int travelled = 0;
  for (int frame = 0; frame < 120; ++frame) {
    pointer.move_by(2, 0);
    travelled += capture.poll(pointer, k_centre).x();
  }
  EXPECT_EQ(travelled, 240) << "deltas are measured from where the pointer really is";
  EXPECT_FALSE(capture.warp_supported());
  EXPECT_LT(pointer.warps, 12)
      << "a platform that refuses the warp is not asked each frame";

  pointer.refuse_warps = false;
  for (int frame = 0; frame < 40; ++frame) {
    pointer.move_by(2, 0);
    static_cast<void>(capture.poll(pointer, k_centre));
  }
  EXPECT_TRUE(capture.warp_supported()) << "a warp that starts working is noticed";
  EXPECT_LT(std::abs(pointer.where().x() - k_centre.x()), 4);
}

TEST(CommanderPointerCaptureTest, AClampedWarpIsNotMistakenForARefusal) {
  FakePointer pointer;
  pointer.clamp_x_max = 900;
  CommanderPointerCapture capture;
  capture.capture();
  static_cast<void>(capture.poll(pointer, k_centre));
  EXPECT_EQ(pointer.where(), QPoint(900, k_centre.y()));

  for (int frame = 0; frame < 10; ++frame) {
    EXPECT_TRUE(capture.poll(pointer, k_centre).isNull())
        << "resting where the warp leaves the pointer is not a look delta";
  }
  EXPECT_TRUE(capture.warp_supported());

  pointer.move_by(-30, 10);
  EXPECT_EQ(capture.poll(pointer, k_centre), QPoint(-30, 10));
  EXPECT_EQ(pointer.where(), QPoint(900, k_centre.y()));
}

TEST(CommanderPointerCaptureTest, ReleasingHandsThePointerBack) {
  FakePointer pointer;
  CommanderPointerCapture capture;
  capture.capture();
  static_cast<void>(capture.poll(pointer, k_centre));
  capture.release();

  int const warps = pointer.warps;
  pointer.move_by(80, 80);
  EXPECT_TRUE(capture.poll(pointer, k_centre).isNull());
  EXPECT_EQ(pointer.warps, warps);
  EXPECT_EQ(pointer.where(), k_centre + QPoint(80, 80));
}

TEST(CommanderLookPointerTest, ALongFastTurnKeepsTurningTheView) {
  FakePointer pointer;
  CommanderLook look;
  look.center_mouse(960.0, 540.0, nullptr);
  look.poll_pointer(pointer, k_centre, 1.0F);
  float const start = look.yaw();

  float turned = 0.0F;
  float previous = start;
  for (int frame = 0; frame < 300; ++frame) {
    pointer.race_offset = (frame % 3 == 0) ? QPoint(6, 1) : QPoint();
    pointer.move_by(40, 0);
    look.poll_pointer(pointer, k_centre, 1.0F);
    float step = previous - look.yaw();
    if (step < -180.0F) {
      step += 360.0F;
    }
    EXPECT_GT(step, 0.0F) << "the view stopped turning at frame " << frame;
    turned += step;
    previous = look.yaw();
  }
  EXPECT_GT(turned, 1800.0F)
      << "five full turns of the mouse are five turns of the view";
}

TEST(CommanderLookPointerTest, DroppingHeldInputDoesNotGiveThePointerAway) {
  FakePointer pointer;
  CommanderLook look;
  look.center_mouse(960.0, 540.0, nullptr);
  look.poll_pointer(pointer, k_centre, 1.0F);
  ASSERT_TRUE(look.mouse_captured());

  look.suspend_mouse();
  look.reset();
  EXPECT_TRUE(look.mouse_captured())
      << "a rally, a menu or an input reset must not leave the pointer loose";

  pointer.move_by(25, 0);
  look.poll_pointer(pointer, k_centre, 1.0F);
  float const before = look.yaw();
  pointer.move_by(25, 0);
  look.poll_pointer(pointer, k_centre, 1.0F);
  EXPECT_NE(look.yaw(), before);
  EXPECT_EQ(pointer.where(), k_centre);

  look.release_mouse();
  EXPECT_FALSE(look.mouse_captured());
}

} // namespace
