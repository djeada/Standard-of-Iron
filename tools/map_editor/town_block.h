#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "map_data.h"

namespace MapEditor::TownBlock {

struct Lot {

  float width = 4.3F;
  float depth = 4.4F;
};

[[nodiscard]] auto lot_of(const QString& building_type) -> Lot;

struct Owner {
  int player_id = 1;
  QString nation;
  int max_population = 150;
};

struct TerraceSpec {
  QPointF from;
  QPointF to;

  float facing = 0.0F;
  QString type = QStringLiteral("home");
  QString style = QStringLiteral("tenement");
  QString id;

  QVector<int> skip;
  Owner owner;
};

[[nodiscard]] auto terrace(const TerraceSpec& spec) -> QVector<StructureElement>;

struct BlockSpec {

  QPointF origin;
  int lots_wide = 8;
  int lots_deep = 3;

  float rotation = 0.0F;

  int alley_lot = -1;
  QString type = QStringLiteral("home");
  QString style = QStringLiteral("tenement");
  QString id;
  Owner owner;
};

struct BlockExtent {
  float width = 0.0F;
  float depth = 0.0F;
};

[[nodiscard]] auto block_extent(const BlockSpec& spec) -> BlockExtent;

[[nodiscard]] auto block(const BlockSpec& spec) -> QVector<StructureElement>;

struct PavingSpec {
  QPointF min;
  QPointF max;
  QString style = QStringLiteral("stone");

  float strip = 7.0F;
};

[[nodiscard]] auto paving(const PavingSpec& spec) -> QVector<LinearElement>;

struct WallSpec {

  QVector<QPointF> points;
  bool closed = true;

  float tower_spacing = 44.0F;

  QString style;
  QString id;
  Owner owner;
};

struct WallPlan {
  QVector<LinearElement> runs;
  QVector<StructureElement> towers;
};

[[nodiscard]] auto wall(const WallSpec& spec, QString& error) -> WallPlan;

[[nodiscard]] auto
wall_from_json(const QJsonObject& op, WallSpec& out, QString& error) -> bool;
[[nodiscard]] auto
terrace_from_json(const QJsonObject& op, TerraceSpec& out, QString& error) -> bool;
[[nodiscard]] auto
block_from_json(const QJsonObject& op, BlockSpec& out, QString& error) -> bool;
[[nodiscard]] auto
paving_from_json(const QJsonObject& op, PavingSpec& out, QString& error) -> bool;

} // namespace MapEditor::TownBlock
