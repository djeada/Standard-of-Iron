#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QVariantMap>
#include <QtQml/qqmlextensionplugin.h>

#include <algorithm>
#include <gtest/gtest.h>

#include "tools/arena/promo_rpg_hud.h"

Q_IMPORT_QML_PLUGIN(StandardOfIron_CorePlugin)

namespace {

auto commander_status() -> QVariantMap {
  return {
      {QStringLiteral("has_commander"), true},
      {QStringLiteral("alive"), true},
      {QStringLiteral("name"), QStringLiteral("Publius Cornelius Scipio")},
      {QStringLiteral("health"), 3300},
      {QStringLiteral("max_health"), 3300},
      {QStringLiteral("health_ratio"), 1.0},
      {QStringLiteral("stamina_ratio"), 0.6},
      {QStringLiteral("weapon_stance"), QStringLiteral("melee")},
  };
}

auto changed_pixels(const QImage& before,
                    const QImage& after,
                    const QRect& region) -> int {
  int changed = 0;
  for (int y = region.top(); y <= region.bottom(); ++y) {
    for (int x = region.left(); x <= region.right(); ++x) {
      if (before.pixel(x, y) != after.pixel(x, y)) {
        ++changed;
      }
    }
  }
  return changed;
}

class ArenaRpgHudTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { Arena::Promo::RpgHud::select_software_scene_graph(); }
};

TEST_F(ArenaRpgHudTest, PaintsTheGamesRpgOverlayOntoACapturedFrame) {
  Arena::Promo::RpgHud hud;
  ASSERT_TRUE(hud.ready()) << hud.error().toStdString();

  QImage frame(1280, 720, QImage::Format_ARGB32);
  frame.fill(QColor(40, 70, 40));
  const QImage untouched = frame;

  hud.paint(frame, commander_status(), 0.5, 1.0F / 60.0F);

  const QRect lower_left(0, frame.height() / 2, frame.width() / 3, frame.height() / 2);
  const QRect centre(frame.width() / 2 - 40, frame.height() / 2 - 40, 80, 80);
  EXPECT_GT(changed_pixels(untouched, frame, lower_left), 2000);
  EXPECT_GT(changed_pixels(untouched, frame, centre), 20);

  const QRect open_sky(frame.width() / 3, 40, frame.width() / 3, frame.height() / 4);
  EXPECT_EQ(changed_pixels(untouched, frame, open_sky), 0);
}

TEST_F(ArenaRpgHudTest, NoCommanderStatusLeavesTheFrameAlone) {
  Arena::Promo::RpgHud hud;
  ASSERT_TRUE(hud.ready()) << hud.error().toStdString();

  QImage frame(640, 360, QImage::Format_ARGB32);
  frame.fill(QColor(40, 70, 40));
  const QImage untouched = frame;

  hud.paint(frame, QVariantMap{}, 0.0, 1.0F / 60.0F);

  EXPECT_EQ(frame, untouched);
}

TEST_F(ArenaRpgHudTest, ACommanderHitRisesAsTheGamesDamageBurst) {
  Arena::Promo::RpgHud hud;
  ASSERT_TRUE(hud.ready()) << hud.error().toStdString();

  hud.set_projection([](const QVector3D&, QPointF& out) {
    out = QPointF(320.0, 600.0);
    return true;
  });

  App::Core::WorldFeedbackTick tick;
  tick.anchor = 7U;
  tick.kind = App::Core::FeedbackKind::Damage;
  tick.style = App::Core::FeedbackStyle::Burst;
  tick.amount = 66;
  tick.outgoing = true;
  tick.severity = 0.6F;
  hud.push_feedback(tick);

  const QRect above_anchor(120, 240, 400, 320);
  int most_changed = 0;
  for (int frame_index = 0; frame_index < 60; ++frame_index) {

    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QImage frame(1280, 720, QImage::Format_ARGB32);
    frame.fill(QColor(40, 70, 40));
    const QImage untouched = frame;
    hud.paint(frame, commander_status(), frame_index / 60.0, 1.0F / 60.0F);
    most_changed =
        std::max(most_changed, changed_pixels(untouched, frame, above_anchor));
  }
  EXPECT_GT(most_changed, 400);
}

} // namespace
