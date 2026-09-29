#include <QStringList>
#include <QThread>

#include <gtest/gtest.h>

#include "app/session/loading_overlay.h"

namespace {

using App::Session::LoadingOverlay;

auto ready() -> QStringList {
  return {};
}

auto blocked() -> QStringList {
  return {QStringLiteral("unit templates")};
}

TEST(LoadingOverlayTest, BeginRaisesTheOverlayWithoutWaitingForAFrame) {
  LoadingOverlay overlay;
  EXPECT_FALSE(overlay.active());
  overlay.begin();
  EXPECT_TRUE(overlay.active());
  EXPECT_FALSE(overlay.waiting_for_first_frame());
}

TEST(LoadingOverlayTest, AnUnreadyRendererKeepsRestartingTheCountdown) {
  LoadingOverlay overlay;
  overlay.begin();
  overlay.arm_after_load();
  EXPECT_TRUE(overlay.waiting_for_first_frame());

  for (int i = 0; i < 10; ++i) {
    const auto release = overlay.poll(false, ready);
    EXPECT_FALSE(release.released);
  }
  EXPECT_EQ(overlay.frames_remaining(), 5);
  EXPECT_TRUE(overlay.active());
}

TEST(LoadingOverlayTest, ReleaseNeedsFramesTimeAndStartupReadiness) {
  LoadingOverlay overlay;
  overlay.begin();
  overlay.arm_after_load();
  overlay.set_show_objectives_after_loading(true);

  for (int i = 0; i < 8; ++i) {
    EXPECT_FALSE(overlay.poll(true, ready).released)
        << "the minimum on-screen time has not passed";
  }
  EXPECT_EQ(overlay.frames_remaining(), 0);

  QThread::msleep(1050);
  EXPECT_FALSE(overlay.poll(true, blocked).released)
      << "a pending startup component holds the overlay";
  EXPECT_TRUE(overlay.active());

  const auto release = overlay.poll(true, ready);
  EXPECT_TRUE(release.released);
  EXPECT_TRUE(release.finalize_progress);
  EXPECT_TRUE(release.show_objectives);
  EXPECT_FALSE(overlay.active());
  EXPECT_FALSE(overlay.waiting_for_first_frame());

  overlay.begin();
  overlay.arm_after_load();
  QThread::msleep(1050);
  for (int i = 0; i < 6; ++i) {
    (void)overlay.poll(true, blocked);
  }
  const auto second = overlay.poll(true, ready);
  EXPECT_TRUE(second.released);
  EXPECT_FALSE(second.show_objectives) << "objective display is consumed by a release";
}

TEST(LoadingOverlayTest, AbortDropsEveryPendingFlag) {
  LoadingOverlay overlay;
  overlay.begin();
  overlay.arm_after_load();
  overlay.set_show_objectives_after_loading(true);
  overlay.abort();

  EXPECT_FALSE(overlay.active());
  EXPECT_FALSE(overlay.waiting_for_first_frame());
  overlay.arm_after_load();
  QThread::msleep(1050);
  for (int i = 0; i < 6; ++i) {
    (void)overlay.poll(true, ready);
  }
  EXPECT_FALSE(overlay.poll(true, ready).show_objectives);
}

TEST(LoadingOverlayTest, DescribeNamesTheStateForTheSelfTestReport) {
  LoadingOverlay overlay;
  overlay.begin();
  overlay.arm_after_load();
  const QString text = overlay.describe(true, false);
  EXPECT_TRUE(text.contains(QStringLiteral("renderer up")));
  EXPECT_TRUE(text.contains(QStringLiteral("gpu resources null")));
  EXPECT_TRUE(text.contains(QStringLiteral("waiting for first frame yes")));
  EXPECT_TRUE(text.contains(QStringLiteral("frames remaining 5")));
}

} // namespace
