#include <QElapsedTimer>
#include <QEventLoop>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <chrono>
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

TEST(LoadingOverlayTest, AbortedPreparationCannotCompleteTheNextLoad) {
  LoadingOverlay overlay;
  const quint64 abandoned_load = overlay.begin();
  EXPECT_EQ(overlay.current_load(), abandoned_load);
  overlay.abort();
  EXPECT_EQ(overlay.current_load(), 0U);

  const quint64 next_load = overlay.begin();
  EXPECT_NE(next_load, abandoned_load);
  EXPECT_EQ(overlay.current_load(), next_load);
  overlay.arm_after_load();
  EXPECT_EQ(overlay.armed_load(), next_load);
}

TEST(LoadingOverlayTest, FramesFromAnEarlierLoadAreIgnored) {
  LoadingOverlay overlay;
  overlay.begin();
  overlay.arm_after_load();
  const auto previous_load = overlay.armed_load();

  overlay.begin();
  EXPECT_FALSE(overlay.waiting_for_first_frame());
  overlay.arm_after_load();
  EXPECT_NE(previous_load, overlay.armed_load());
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(overlay.poll(previous_load, ready).released);
  }
  EXPECT_EQ(overlay.frames_remaining(), 5);
  EXPECT_TRUE(overlay.active());
}

TEST(LoadingOverlayTest, MissingFramesTimeOutWithoutRenderCallbacks) {
  LoadingOverlay overlay(std::chrono::milliseconds(20));
  QEventLoop event_loop;
  bool timed_out = false;
  QObject::connect(
      &overlay, &LoadingOverlay::first_frames_timed_out, &event_loop, [&]() {
        timed_out = true;
        event_loop.quit();
      });
  QTimer::singleShot(std::chrono::seconds(1), &event_loop, &QEventLoop::quit);

  overlay.begin();
  overlay.arm_after_load();
  const auto stalled_load = overlay.armed_load();
  event_loop.exec();

  EXPECT_TRUE(timed_out);
  EXPECT_FALSE(overlay.active());
  EXPECT_FALSE(overlay.waiting_for_first_frame());
  EXPECT_FALSE(overlay.poll(stalled_load, ready).released);
}

TEST(LoadingOverlayTest, RepeatedUnreadyPollsDoNotPostponeTheDeadline) {
  LoadingOverlay overlay(std::chrono::milliseconds(80));
  QEventLoop event_loop;
  bool timed_out = false;
  QObject::connect(
      &overlay, &LoadingOverlay::first_frames_timed_out, &event_loop, [&]() {
        timed_out = true;
        event_loop.quit();
      });
  QTimer::singleShot(std::chrono::seconds(2), &event_loop, &QEventLoop::quit);

  overlay.begin();
  overlay.arm_after_load();
  // A renderer that never reaches the load keeps reporting load 0 (the engine
  // passes the armed load only once gameplay rendering runs).
  QTimer unready_polls;
  unready_polls.setInterval(5);
  QObject::connect(&unready_polls, &QTimer::timeout, &event_loop, [&]() {
    EXPECT_FALSE(overlay.poll(0, ready).released);
  });
  unready_polls.start();
  event_loop.exec();

  EXPECT_TRUE(timed_out);
  EXPECT_FALSE(overlay.active());
}

TEST(LoadingOverlayTest, BlockedStartupComponentsStillTimeOutWithoutFrames) {
  LoadingOverlay overlay(std::chrono::milliseconds(80));
  QEventLoop event_loop;
  bool timed_out = false;
  QObject::connect(
      &overlay, &LoadingOverlay::first_frames_timed_out, &event_loop, [&]() {
        timed_out = true;
        event_loop.quit();
      });
  QTimer::singleShot(std::chrono::seconds(2), &event_loop, &QEventLoop::quit);

  overlay.begin();
  overlay.arm_after_load();
  const auto load = overlay.armed_load();
  // Frames arrive but startup readiness never completes, then they stop.
  int frames = 0;
  QTimer frame_timer;
  frame_timer.setInterval(5);
  QObject::connect(&frame_timer, &QTimer::timeout, &event_loop, [&]() {
    if (++frames <= 3) {
      EXPECT_FALSE(overlay.poll(load, blocked).released);
    }
  });
  frame_timer.start();
  event_loop.exec();

  EXPECT_TRUE(timed_out);
  EXPECT_FALSE(overlay.active());
}

TEST(LoadingOverlayTest, SlowSuccessfulLoadReleasesBeforeTheDeadline) {
  LoadingOverlay overlay(std::chrono::seconds(30));
  overlay.begin();
  overlay.arm_after_load();
  const auto load = overlay.armed_load();
  QElapsedTimer waited;
  waited.start();
  bool released = false;
  bool components_ready = false;
  while (!released && waited.elapsed() < 5000) {
    QThread::msleep(20);
    components_ready = waited.elapsed() > 1200;
    released = overlay
                   .poll(load,
                         [&]() {
                           return components_ready
                                      ? QStringList{}
                                      : QStringList{QStringLiteral("unit templates")};
                         })
                   .released;
  }
  EXPECT_TRUE(released);
  EXPECT_TRUE(components_ready);
  EXPECT_FALSE(overlay.active());
}

TEST(LoadingOverlayTest, AbortCancelsTheFirstFrameDeadline) {
  LoadingOverlay overlay(std::chrono::milliseconds(20));
  QEventLoop event_loop;
  QObject::connect(
      &overlay, &LoadingOverlay::first_frames_timed_out, &event_loop, []() {
        ADD_FAILURE() << "An aborted load must not issue a delayed timeout";
      });
  QTimer::singleShot(std::chrono::milliseconds(60), &event_loop, &QEventLoop::quit);

  overlay.begin();
  overlay.arm_after_load();
  overlay.abort();
  event_loop.exec();
}

TEST(LoadingOverlayTest, FirstFrameDeadlineWaitsForAMinimizedWindow) {
  LoadingOverlay overlay(std::chrono::milliseconds(20));
  QEventLoop event_loop;
  bool timed_out = false;
  QObject::connect(
      &overlay, &LoadingOverlay::first_frames_timed_out, &event_loop, [&]() {
        timed_out = true;
        event_loop.quit();
      });
  overlay.begin();
  overlay.arm_after_load();
  overlay.set_window_minimized(true);
  QTimer::singleShot(std::chrono::milliseconds(60), &event_loop, &QEventLoop::quit);
  event_loop.exec();
  EXPECT_FALSE(timed_out);
  EXPECT_TRUE(overlay.active());

  overlay.set_window_minimized(false);
  QTimer::singleShot(std::chrono::seconds(1), &event_loop, &QEventLoop::quit);
  event_loop.exec();
  EXPECT_TRUE(timed_out);
  EXPECT_FALSE(overlay.active());
}

TEST(LoadingOverlayTest, ReleaseNeedsFramesTimeAndStartupReadiness) {
  LoadingOverlay overlay;
  overlay.begin();
  overlay.arm_after_load();
  overlay.set_show_objectives_after_loading(true);
  const auto first_load = overlay.armed_load();

  for (int i = 0; i < 8; ++i) {
    EXPECT_FALSE(overlay.poll(first_load, ready).released)
        << "the minimum on-screen time has not passed";
  }
  EXPECT_EQ(overlay.frames_remaining(), 0);

  QThread::msleep(1050);
  EXPECT_FALSE(overlay.poll(first_load, blocked).released)
      << "a pending startup component holds the overlay";
  EXPECT_TRUE(overlay.active());

  const auto release = overlay.poll(first_load, ready);
  EXPECT_TRUE(release.released);
  EXPECT_TRUE(release.finalize_progress);
  EXPECT_TRUE(release.show_objectives);
  EXPECT_FALSE(overlay.active());
  EXPECT_FALSE(overlay.waiting_for_first_frame());

  overlay.begin();
  overlay.arm_after_load();
  const auto second_load = overlay.armed_load();
  QThread::msleep(1050);
  for (int i = 0; i < 6; ++i) {
    (void)overlay.poll(second_load, blocked);
  }
  const auto second = overlay.poll(second_load, ready);
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
  overlay.begin();
  overlay.arm_after_load();
  const auto next_load = overlay.armed_load();
  QThread::msleep(1050);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FALSE(overlay.poll(next_load, ready).released);
  }
  const auto release = overlay.poll(next_load, ready);
  EXPECT_TRUE(release.released);
  EXPECT_FALSE(release.show_objectives);
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
