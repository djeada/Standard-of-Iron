#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "map_data.h"

// Town building for the editor: terraces of houses standing wall to wall,
// perimeter blocks of them around a court, and paved squares. Everything here
// returns ordinary structures and roads, laid on the navigation footprint of
// the building it places, so what the editor stamps is what any other tool or
// a hand edit could have placed one by one.
namespace MapEditor::TownBlock {

struct Lot {
  // Along the street, and from street to court, in map units.
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
  // Which way the fronts look, in degrees: 0 is +z, 90 is +x.
  float facing = 0.0F;
  QString type = QStringLiteral("home");
  QString style = QStringLiteral("tenement");
  QString id;
  // Lots left open, counted from `from`.
  QVector<int> skip;
  Owner owner;
};

// Houses along from -> to, centred on the run, one lot apart.
[[nodiscard]] auto terrace(const TerraceSpec& spec) -> QVector<StructureElement>;

struct BlockSpec {
  // The corner of the block's lots with the lowest x and z.
  QPointF origin;
  int lots_wide = 8;
  int lots_deep = 3;
  // Turn of the whole block about its origin, in degrees.
  float rotation = 0.0F;
  // Lot of the first terrace left open as the alley into the court; -1 for a
  // closed block.
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

// Four terraces round a court, fronts outward.
[[nodiscard]] auto block(const BlockSpec& spec) -> QVector<StructureElement>;

struct PavingSpec {
  QPointF min;
  QPointF max;
  QString style = QStringLiteral("stone");
  // Widest single road the square is laid in.
  float strip = 7.0F;
};

// A paved rectangle, as the parallel roads the terrain can carry and one
// across them, so the square is a single road network.
[[nodiscard]] auto paving(const PavingSpec& spec) -> QVector<LinearElement>;

struct WallSpec {
  // Corners of the circuit. Legs along x or z stand on the two-cell wall
  // lattice and can take gates; a leg at an angle is laid in free pieces.
  QVector<QPointF> points;
  bool closed = true;
  // Furthest two towers may stand apart along a leg; corners always get one.
  // Zero or less builds the wall with no towers.
  float tower_spacing = 44.0F;
  // "masonry" builds the curtain in dressed stone; unset, the nation's own
  // field wall (a palisade for Rome and Carthage).
  QString style;
  QString id;
  Owner owner;
};

struct WallPlan {
  QVector<LinearElement> runs;
  QVector<StructureElement> towers;
};

// A wall circuit with a tower on every corner and along every long leg; the
// runs stop two cells short of each tower, as the wall network expects.
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
