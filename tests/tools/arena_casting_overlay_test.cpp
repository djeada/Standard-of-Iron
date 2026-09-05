#include <QColor>
#include <QImage>

#include <gtest/gtest.h>

#include "tools/arena/arena_casting.h"
#include "tools/arena/promo_casting_overlay.h"

namespace {

auto side(const char* label, int owner, const QVector3D& color, int units, int soldiers)
    -> Arena::ArenaCastingSide {
  Arena::ArenaCastingSide result;
  result.census.label = QString::fromLatin1(label);
  result.census.owner_id = owner;
  result.census.living_units = units;
  result.census.living_soldiers = soldiers;
  result.census.living_buildings = 9;
  result.census.strategy = QStringLiteral("aggressive");
  result.census.posture = QStringLiteral("field");
  result.census.ai_state = QStringLiteral("attacking");
  result.color = color;
  result.gold = 1200;
  result.food = 340;
  result.wood = 210;
  return result;
}

auto dominant_hue_matches(const QImage& frame,
                          int x,
                          int y,
                          const QColor& expected) -> bool {
  const QColor actual = frame.pixelColor(x, y);
  return std::abs(actual.red() - expected.red()) < 40 &&
         std::abs(actual.green() - expected.green()) < 40 &&
         std::abs(actual.blue() - expected.blue()) < 40;
}

auto count_bright_pixels(const QImage& frame, int y0, int y1) -> int {
  int bright = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = 0; x < frame.width(); ++x) {
      if (frame.pixelColor(x, y).lightness() > 180) {
        ++bright;
      }
    }
  }
  return bright;
}

} // namespace

TEST(ArenaCastingOverlayTest, PaintsEachSidesColourBarAndTheStrengthBarInProportion) {
  QImage frame(1920, 1080, QImage::Format_ARGB32);
  frame.fill(QColor(90, 130, 70));

  Arena::ArenaCastingSnapshot snapshot;
  snapshot.valid = true;
  snapshot.elapsed_seconds = 754.0F;
  snapshot.sides = {side("scipio", 2, QVector3D(1.0F, 0.3F, 0.3F), 12, 96),
                    side("hannibal", 3, QVector3D(0.2F, 0.8F, 0.4F), 4, 32)};

  Arena::Promo::paint_casting_overlay(frame, snapshot);

  const int strip = Arena::Promo::casting_overlay_height(frame.height());
  EXPECT_GT(strip, 60) << "the strip is a readable band on a 1080-high frame";

  EXPECT_TRUE(dominant_hue_matches(frame, 300, 1, QColor(255, 77, 77)));
  EXPECT_TRUE(dominant_hue_matches(frame, 1620, 1, QColor(51, 204, 102)));

  EXPECT_GT(count_bright_pixels(frame, 0, strip), 2000);
  EXPECT_TRUE(dominant_hue_matches(frame, 960, 700, QColor(90, 130, 70)));

  int bar_y = -1;
  for (int y = strip / 2; y < strip + strip / 2 && bar_y < 0; ++y) {
    if (dominant_hue_matches(frame, 960 - 80, y, QColor(255, 77, 77)) &&
        dominant_hue_matches(frame, 960 + 130, y, QColor(51, 204, 102))) {
      bar_y = y;
    }
  }
  EXPECT_GE(bar_y, 0) << "no row of the strip carries a red-then-green strength bar";
}

TEST(ArenaCastingOverlayTest, LeavesTheFrameAloneWithoutTwoSides) {
  QImage frame(640, 360, QImage::Format_ARGB32);
  frame.fill(QColor(10, 20, 30));
  const QImage before = frame;

  Arena::ArenaCastingSnapshot snapshot;
  snapshot.valid = true;
  snapshot.sides = {side("alone", 2, QVector3D(1.0F, 0.0F, 0.0F), 3, 20)};
  Arena::Promo::paint_casting_overlay(frame, snapshot);
  EXPECT_EQ(frame, before);

  Arena::ArenaCastingSnapshot invalid;
  Arena::Promo::paint_casting_overlay(frame, invalid);
  EXPECT_EQ(frame, before);
}
