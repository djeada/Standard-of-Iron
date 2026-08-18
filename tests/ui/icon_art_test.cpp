#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QSet>

#include <algorithm>
#include <gtest/gtest.h>

#include "game/systems/unit_activity.h"
#include "ui/icon_art.h"

namespace {

auto activity_ids() -> QStringList {
  QStringList ids;
  for (const auto kind : {Game::Systems::ActivityKind::Idle,
                          Game::Systems::ActivityKind::Move,
                          Game::Systems::ActivityKind::Attack,
                          Game::Systems::ActivityKind::Patrol,
                          Game::Systems::ActivityKind::Guard,
                          Game::Systems::ActivityKind::Hold,
                          Game::Systems::ActivityKind::Construct,
                          Game::Systems::ActivityKind::Repair,
                          Game::Systems::ActivityKind::Dismantle,
                          Game::Systems::ActivityKind::ChopWood,
                          Game::Systems::ActivityKind::MineStone,
                          Game::Systems::ActivityKind::MineIron,
                          Game::Systems::ActivityKind::AutoGather,
                          Game::Systems::ActivityKind::Deliver,
                          Game::Systems::ActivityKind::Heal,
                          Game::Systems::ActivityKind::Train,
                          Game::Systems::ActivityKind::Blocked}) {
    const auto id = Game::Systems::activity_kind_id(kind);
    ids.append(QString::fromUtf8(id.data(), static_cast<int>(id.size())));
  }
  return ids;
}

auto ink_coverage(const QString& id, int size) -> double {
  QImage canvas(size, size, QImage::Format_ARGB32);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  Ui::IconArt::paint(
      painter, id, QRectF(0, 0, size, size), Ui::IconArt::default_palette());
  painter.end();

  int painted = 0;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (qAlpha(canvas.pixel(x, y)) > 24) {
        ++painted;
      }
    }
  }
  return static_cast<double>(painted) / static_cast<double>(size * size);
}

TEST(IconArtTest, EveryUnitActivityHasADrawing) {
  for (const QString& id : activity_ids()) {
    EXPECT_NE(Ui::IconArt::find(id), nullptr)
        << "activity has no icon: " << id.toStdString();
  }
}

TEST(IconArtTest, EveryHudOrderHasADrawing) {
  for (const char* action : {"attack",
                             "guard",
                             "hold",
                             "patrol",
                             "formation",
                             "build",
                             "repair",
                             "heal",
                             "collect",
                             "auto_gather",
                             "rally",
                             "deliver",
                             "aura",
                             "gate",
                             "stop",
                             "run"}) {
    EXPECT_NE(Ui::IconArt::find(QString::fromLatin1(action)), nullptr)
        << "HUD order has no icon: " << action;
  }
}

TEST(IconArtTest, SimulationJobNamesResolveThroughAliases) {
  EXPECT_EQ(Ui::IconArt::resolve_id(QStringLiteral("cut_tree")),
            QStringLiteral("chop_wood"));
  EXPECT_EQ(Ui::IconArt::resolve_id(QStringLiteral("collect_stone")),
            QStringLiteral("mine_stone"));
  EXPECT_EQ(Ui::IconArt::resolve_id(QStringLiteral("collect_iron_ore")),
            QStringLiteral("mine_iron"));
  EXPECT_EQ(Ui::IconArt::resolve_id(QStringLiteral("build")),
            QStringLiteral("construct"));
}

TEST(IconArtTest, AnUnknownIdDrawsNothingRatherThanCrashing) {
  EXPECT_EQ(Ui::IconArt::find(QStringLiteral("teleport")), nullptr);

  QImage canvas(32, 32, QImage::Format_ARGB32);
  canvas.fill(Qt::transparent);
  QPainter painter(&canvas);
  Ui::IconArt::paint(painter,
                     QStringLiteral("teleport"),
                     QRectF(0, 0, 32, 32),
                     Ui::IconArt::default_palette());
  painter.end();
  EXPECT_EQ(ink_coverage(QStringLiteral("teleport"), 32), 0.0);
}

TEST(IconArtTest, EveryIconStillReadsAtSixteenPixels) {

  for (const QString& id : Ui::IconArt::ids()) {
    const double coverage = ink_coverage(id, 16);
    EXPECT_GT(coverage, 0.08) << id.toStdString() << " nearly vanishes at 16px";
    EXPECT_LT(coverage, 0.85) << id.toStdString() << " fills the whole tile at 16px";
  }
}

TEST(IconArtTest, GeometryIsIndependentOfTheSizeItIsDrawnAt) {

  for (const QString& id : Ui::IconArt::ids()) {
    const double small = ink_coverage(id, 24);
    const double large = ink_coverage(id, 96);
    EXPECT_NEAR(small, large, 0.12) << id.toStdString() << " changes shape with size";
  }
}

TEST(IconArtTest, ShapesStayInsideTheirTile) {
  constexpr int k_size = 64;
  for (const QString& id : Ui::IconArt::ids()) {
    QImage canvas(k_size, k_size, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    Ui::IconArt::paint(
        painter, id, QRectF(0, 0, k_size, k_size), Ui::IconArt::default_palette());
    painter.end();

    bool touches_edge = false;
    for (int i = 0; i < k_size; ++i) {
      touches_edge = touches_edge || qAlpha(canvas.pixel(i, 0)) > 24 ||
                     qAlpha(canvas.pixel(i, k_size - 1)) > 24 ||
                     qAlpha(canvas.pixel(0, i)) > 24 ||
                     qAlpha(canvas.pixel(k_size - 1, i)) > 24;
    }
    EXPECT_FALSE(touches_edge) << id.toStdString() << " is clipped by its own tile";
  }
}

TEST(IconArtTest, TheQmlFacadeHandsBackNormalisedPolylines) {
  const QVariantList strokes = IconArtLibrary::strokes(QStringLiteral("guard"));
  ASSERT_FALSE(strokes.isEmpty());

  QSet<QString> tones;
  for (const QVariant& entry : strokes) {
    const QVariantMap stroke = entry.toMap();
    tones.insert(stroke.value(QStringLiteral("tone")).toString());
    const QVariantList subpaths = stroke.value(QStringLiteral("subpaths")).toList();
    ASSERT_FALSE(subpaths.isEmpty());
    for (const QVariant& subpath : subpaths) {
      const QVariantList points = subpath.toList();
      ASSERT_GE(points.size(), 4);
      ASSERT_EQ(points.size() % 2, 0);
      for (const QVariant& value : points) {
        const double coordinate = value.toDouble();
        EXPECT_GE(coordinate, -0.05);
        EXPECT_LE(coordinate, 1.05);
      }
    }
  }
  EXPECT_TRUE(tones.contains(QStringLiteral("metal")));
}

TEST(IconArtTest, ClosedOutlinesComeBackClosed) {

  const QVariantList strokes = IconArtLibrary::strokes(QStringLiteral("blocked"));
  ASSERT_FALSE(strokes.isEmpty());
  const QVariantList points = strokes.front()
                                  .toMap()
                                  .value(QStringLiteral("subpaths"))
                                  .toList()
                                  .front()
                                  .toList();
  ASSERT_GE(points.size(), 6);
  EXPECT_NEAR(points.front().toDouble(), points[points.size() - 2].toDouble(), 1e-4);
  EXPECT_NEAR(points[1].toDouble(), points.back().toDouble(), 1e-4);
}

TEST(IconArtTest, GatheringIconsCarryTheirResourceTone) {
  struct Expectation {
    const char* id;
    Ui::IconArt::Tone tone;
  };
  for (const auto& expectation : {Expectation{"chop_wood", Ui::IconArt::Tone::Timber},
                                  Expectation{"mine_stone", Ui::IconArt::Tone::Stone},
                                  Expectation{"mine_iron", Ui::IconArt::Tone::Iron}}) {
    const auto* art = Ui::IconArt::find(QString::fromLatin1(expectation.id));
    ASSERT_NE(art, nullptr);
    const bool carries_tone =
        std::any_of(art->strokes.begin(),
                    art->strokes.end(),
                    [&expectation](const Ui::IconArt::Stroke& stroke) {
                      return stroke.tone == expectation.tone;
                    });
    EXPECT_TRUE(carries_tone) << expectation.id
                              << " does not identify the resource it yields";
  }
}

} // namespace
