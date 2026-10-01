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
                             "divide",
                             "join",
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
                             "roll_stones",
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

TEST(IconArtTest, TheQmlItemPaintsTheArtWithTheCallersPalette) {
  constexpr int k_size = 48;
  auto render = [](const QString& id, const QColor& accent) {
    IconArtItem item;
    item.setSize(QSizeF(k_size, k_size));
    item.set_icon_id(id);
    item.setProperty("tint", QColor(Qt::white));
    item.setProperty("accent", accent);
    QImage canvas(k_size, k_size, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    item.paint(&painter);
    painter.end();
    return canvas;
  };

  const QImage red = render(QStringLiteral("difficulty_hard"), QColor(Qt::red));
  const QImage blue = render(QStringLiteral("difficulty_hard"), QColor(Qt::blue));
  int painted = 0;
  bool accent_reached_the_art = false;
  for (int y = 0; y < k_size; ++y) {
    for (int x = 0; x < k_size; ++x) {
      painted += qAlpha(red.pixel(x, y)) > 24 ? 1 : 0;
      accent_reached_the_art =
          accent_reached_the_art || red.pixel(x, y) != blue.pixel(x, y);
    }
  }
  EXPECT_GT(painted, k_size * k_size / 20) << "the item painted nothing";
  EXPECT_TRUE(accent_reached_the_art) << "the accent colour never reached the drawing";

  IconArtItem unknown;
  unknown.set_icon_id(QStringLiteral("teleport"));
  EXPECT_FALSE(unknown.available());
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
