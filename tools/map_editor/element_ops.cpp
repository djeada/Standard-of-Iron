#include "element_ops.h"

#include <algorithm>
#include <cmath>

#include "map_json_keys.h"

namespace MapEditor::ElementOps {

namespace {

template <typename... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

auto round_cell(float value) -> float {
  return std::round(value);
}

auto translated_entrances(const QJsonArray& entrances,
                          float dx,
                          float dz) -> QJsonArray {
  QJsonArray moved;
  for (const QJsonValue& value : entrances) {
    if (!value.isObject()) {
      moved.append(value);
      continue;
    }
    QJsonObject entrance = value.toObject();
    if (entrance.contains(MapJsonKeys::x)) {
      entrance[MapJsonKeys::x] =
          entrance.value(MapJsonKeys::x).toDouble() + static_cast<double>(dx);
    }
    if (entrance.contains(MapJsonKeys::z)) {
      entrance[MapJsonKeys::z] =
          entrance.value(MapJsonKeys::z).toDouble() + static_cast<double>(dz);
    }
    moved.append(entrance);
  }
  return moved;
}

} // namespace

auto is_valid_kind(int kind) -> bool {
  return kind >= 0 && kind < k_element_kind_count;
}

auto kind_of(const ElementSnapshot& snap) -> int {
  return static_cast<int>(snap.index()) - 1;
}

auto category_label(int kind) -> QString {
  switch (static_cast<ElementKind>(kind)) {
  case ElementKind::Terrain:
    return QStringLiteral("Terrain");
  case ElementKind::WorldProp:
    return QStringLiteral("Prop");
  case ElementKind::Linear:
    return QStringLiteral("Path");
  case ElementKind::Structure:
    return QStringLiteral("Structure");
  case ElementKind::TroopSpawn:
    return QStringLiteral("Troop");
  case ElementKind::UndeadZone:
    return QStringLiteral("Undead zone");
  }
  return {};
}

auto count(const MapData& data, int kind) -> int {
  switch (static_cast<ElementKind>(kind)) {
  case ElementKind::Terrain:
    return static_cast<int>(data.terrain_elements().size());
  case ElementKind::WorldProp:
    return static_cast<int>(data.world_props().size());
  case ElementKind::Linear:
    return static_cast<int>(data.linear_elements().size());
  case ElementKind::Structure:
    return static_cast<int>(data.structures().size());
  case ElementKind::TroopSpawn:
    return static_cast<int>(data.troop_spawns().size());
  case ElementKind::UndeadZone:
    return static_cast<int>(data.undead_zones().size());
  }
  return 0;
}

auto index_valid(const MapData& data, int kind, int index) -> bool {
  return is_valid_kind(kind) && index >= 0 && index < count(data, kind);
}

auto snapshot(const MapData& data, int kind, int index) -> ElementSnapshot {
  if (!index_valid(data, kind, index)) {
    return {};
  }
  switch (static_cast<ElementKind>(kind)) {
  case ElementKind::Terrain:
    return data.terrain_elements()[index];
  case ElementKind::WorldProp:
    return data.world_props()[index];
  case ElementKind::Linear:
    return data.linear_elements()[index];
  case ElementKind::Structure:
    return data.structures()[index];
  case ElementKind::TroopSpawn:
    return data.troop_spawns()[index];
  case ElementKind::UndeadZone:
    return data.undead_zones()[index];
  }
  return {};
}

auto type_name(const ElementSnapshot& snap) -> QString {
  return std::visit(Overloaded{
                        [](std::monostate) { return QString{}; },
                        [](const UndeadZoneElement& e) { return e.anchor_type; },
                        [](const auto& e) { return e.type; },
                    },
                    snap);
}

auto display_name(const ElementSnapshot& snap) -> QString {
  if (const auto* zone = std::get_if<UndeadZoneElement>(&snap)) {
    return zone->id.isEmpty() ? QStringLiteral("undead zone") : zone->id;
  }
  return type_name(snap);
}

auto position(const ElementSnapshot& snap) -> std::optional<QPointF> {
  return std::visit(
      Overloaded{
          [](std::monostate) { return std::optional<QPointF>{}; },
          [](const LinearElement& e) {
            const QVector2D centre = (e.start + e.end) * 0.5F;
            return std::optional<QPointF>(QPointF(static_cast<double>(centre.x()),
                                                  static_cast<double>(centre.y())));
          },
          [](const auto& e) {
            return std::optional<QPointF>(
                QPointF(static_cast<double>(e.x), static_cast<double>(e.z)));
          },
      },
      snap);
}

auto translated(const ElementSnapshot& snap, const QPointF& delta) -> ElementSnapshot {
  const auto dx = static_cast<float>(delta.x());
  const auto dz = static_cast<float>(delta.y());
  return std::visit(Overloaded{
                        [](std::monostate) { return ElementSnapshot{}; },
                        [&](LinearElement e) {
                          const QVector2D shift(dx, dz);
                          e.start += shift;
                          e.end += shift;
                          for (QPointF& waypoint : e.waypoints) {
                            waypoint += QPointF(static_cast<double>(dx),
                                                static_cast<double>(dz));
                          }
                          return ElementSnapshot{e};
                        },
                        [&](TerrainElement e) {
                          e.x += dx;
                          e.z += dz;
                          e.entrances = translated_entrances(e.entrances, dx, dz);
                          return ElementSnapshot{e};
                        },
                        [&](auto e) {
                          e.x += dx;
                          e.z += dz;
                          return ElementSnapshot{e};
                        },
                    },
                    snap);
}

auto moved_to(const ElementSnapshot& snap, const QPointF& pos) -> ElementSnapshot {
  const std::optional<QPointF> current = position(snap);
  if (!current.has_value()) {
    return snap;
  }
  return translated(snap, pos - *current);
}

auto snapped_to_grid(const ElementSnapshot& snap) -> ElementSnapshot {
  return std::visit(
      Overloaded{
          [](std::monostate) { return ElementSnapshot{}; },
          [](LinearElement e) {
            const QVector2D shift(round_cell(e.start.x()) - e.start.x(),
                                  round_cell(e.start.y()) - e.start.y());
            e.start = QVector2D(round_cell(e.start.x()), round_cell(e.start.y()));
            e.end = QVector2D(round_cell(e.end.x()), round_cell(e.end.y()));
            for (QPointF& waypoint : e.waypoints) {
              waypoint += QPointF(static_cast<double>(shift.x()),
                                  static_cast<double>(shift.y()));
            }
            return ElementSnapshot{e};
          },
          [](TerrainElement e) {
            const float dx = round_cell(e.x) - e.x;
            const float dz = round_cell(e.z) - e.z;
            e.x += dx;
            e.z += dz;
            e.entrances = translated_entrances(e.entrances, dx, dz);
            return ElementSnapshot{e};
          },
          [](auto e) {
            e.x = round_cell(e.x);
            e.z = round_cell(e.z);
            return ElementSnapshot{e};
          },
      },
      snap);
}

auto has_moved(const ElementSnapshot& before, const ElementSnapshot& after) -> bool {
  if (before.index() != after.index()) {
    return true;
  }
  if (const auto* a = std::get_if<LinearElement>(&before)) {
    const auto* b = std::get_if<LinearElement>(&after);
    return a->start != b->start || a->end != b->end;
  }
  const std::optional<QPointF> a = position(before);
  const std::optional<QPointF> b = position(after);
  return a.has_value() && b.has_value() && *a != *b;
}

auto player_id(const ElementSnapshot& snap) -> int {
  return std::visit(Overloaded{
                        [](const LinearElement& e) { return e.player_id; },
                        [](const StructureElement& e) { return e.player_id; },
                        [](const TroopSpawnElement& e) { return e.player_id; },
                        [](const UndeadZoneElement& e) { return e.owner_id; },
                        [](const auto&) { return -1; },
                    },
                    snap);
}

auto supports_player_id(const ElementSnapshot& snap) -> bool {
  if (const auto* linear = std::get_if<LinearElement>(&snap)) {
    return linear->type == QStringLiteral("wall");
  }
  return std::holds_alternative<StructureElement>(snap) ||
         std::holds_alternative<TroopSpawnElement>(snap);
}

auto with_player_id(const ElementSnapshot& snap, int id) -> ElementSnapshot {
  return std::visit(Overloaded{
                        [&](LinearElement e) {
                          e.player_id = id;
                          return ElementSnapshot{e};
                        },
                        [&](StructureElement e) {
                          e.player_id = id;
                          return ElementSnapshot{e};
                        },
                        [&](TroopSpawnElement e) {
                          e.player_id = id;
                          return ElementSnapshot{e};
                        },
                        [](const auto& e) { return ElementSnapshot{e}; },
                    },
                    snap);
}

auto summary(const ElementSnapshot& snap) -> QString {
  const int kind = kind_of(snap);
  if (!is_valid_kind(kind)) {
    return {};
  }

  QString text = QStringLiteral("%1: %2").arg(category_label(kind), display_name(snap));

  if (const auto* linear = std::get_if<LinearElement>(&snap)) {
    text += QStringLiteral("\n(%1, %2) → (%3, %4)")
                .arg(static_cast<double>(linear->start.x()), 0, 'f', 1)
                .arg(static_cast<double>(linear->start.y()), 0, 'f', 1)
                .arg(static_cast<double>(linear->end.x()), 0, 'f', 1)
                .arg(static_cast<double>(linear->end.y()), 0, 'f', 1);
    text +=
        QStringLiteral("\nwidth %1").arg(static_cast<double>(linear->width), 0, 'f', 1);
  } else if (const std::optional<QPointF> pos = position(snap); pos.has_value()) {
    text +=
        QStringLiteral("\n(%1, %2)").arg(pos->x(), 0, 'f', 1).arg(pos->y(), 0, 'f', 1);
  }

  if (supports_player_id(snap)) {
    const int id = player_id(snap);
    text += QStringLiteral("\nplayer %1")
                .arg(id < 0 ? QStringLiteral("—") : QString::number(id));
  }

  if (const auto* terrain = std::get_if<TerrainElement>(&snap)) {
    text += QStringLiteral("\nheight %1, radius %2")
                .arg(static_cast<double>(terrain->height), 0, 'f', 1)
                .arg(static_cast<double>(terrain->radius), 0, 'f', 1);
  } else if (const auto* zone = std::get_if<UndeadZoneElement>(&snap)) {
    text += QStringLiteral("\nradius %1, leash %2")
                .arg(static_cast<double>(zone->radius), 0, 'f', 1)
                .arg(static_cast<double>(zone->leash_radius), 0, 'f', 1);
  }

  return text;
}

void apply(MapData& data, int index, const ElementSnapshot& snap) {
  std::visit(
      Overloaded{
          [](std::monostate) {},
          [&](const TerrainElement& e) { data.update_terrain_element(index, e); },
          [&](const WorldPropElement& e) { data.update_world_prop(index, e); },
          [&](const LinearElement& e) { data.update_linear_element(index, e); },
          [&](const StructureElement& e) { data.update_structure(index, e); },
          [&](const TroopSpawnElement& e) { data.update_troop_spawn(index, e); },
          [&](const UndeadZoneElement& e) { data.update_undead_zone(index, e); },
      },
      snap);
}

auto make_add(MapData& data, const ElementSnapshot& snap) -> std::unique_ptr<Command> {
  return std::visit(Overloaded{
                        [](std::monostate) { return std::unique_ptr<Command>{}; },
                        [&](const TerrainElement& e) -> std::unique_ptr<Command> {
                          return std::make_unique<AddTerrainCmd>(&data, e);
                        },
                        [&](const WorldPropElement& e) -> std::unique_ptr<Command> {
                          return std::make_unique<AddWorldPropCmd>(&data, e);
                        },
                        [&](const LinearElement& e) -> std::unique_ptr<Command> {
                          return std::make_unique<AddLinearCmd>(&data, e);
                        },
                        [&](const StructureElement& e) -> std::unique_ptr<Command> {
                          return std::make_unique<AddStructureCmd>(&data, e);
                        },
                        [&](const TroopSpawnElement& e) -> std::unique_ptr<Command> {
                          return std::make_unique<AddTroopSpawnCmd>(&data, e);
                        },
                        [&](const UndeadZoneElement& e) -> std::unique_ptr<Command> {
                          return std::make_unique<AddUndeadZoneCmd>(&data, e);
                        },
                    },
                    snap);
}

auto make_remove(MapData& data, int kind, int index) -> std::unique_ptr<Command> {
  if (!index_valid(data, kind, index)) {
    return {};
  }
  switch (static_cast<ElementKind>(kind)) {
  case ElementKind::Terrain:
    return std::make_unique<RemoveTerrainCmd>(
        &data, index, data.terrain_elements()[index]);
  case ElementKind::WorldProp:
    return std::make_unique<RemoveWorldPropCmd>(
        &data, index, data.world_props()[index]);
  case ElementKind::Linear:
    return std::make_unique<RemoveLinearCmd>(
        &data, index, data.linear_elements()[index]);
  case ElementKind::Structure:
    return std::make_unique<RemoveStructureCmd>(&data, index, data.structures()[index]);
  case ElementKind::TroopSpawn:
    return std::make_unique<RemoveTroopSpawnCmd>(
        &data, index, data.troop_spawns()[index]);
  case ElementKind::UndeadZone:
    return std::make_unique<RemoveUndeadZoneCmd>(
        &data, index, data.undead_zones()[index]);
  }
  return {};
}

auto make_update(MapData& data,
                 int index,
                 const ElementSnapshot& before,
                 const ElementSnapshot& after,
                 const QString& description) -> std::unique_ptr<Command> {
  if (before.index() != after.index()) {
    return {};
  }
  return std::visit(
      Overloaded{
          [](std::monostate) { return std::unique_ptr<Command>{}; },
          [&](const TerrainElement& e) -> std::unique_ptr<Command> {
            return std::make_unique<UpdateTerrainCmd>(
                &data, index, std::get<TerrainElement>(before), e, description);
          },
          [&](const WorldPropElement& e) -> std::unique_ptr<Command> {
            return std::make_unique<UpdateWorldPropCmd>(
                &data, index, std::get<WorldPropElement>(before), e, description);
          },
          [&](const LinearElement& e) -> std::unique_ptr<Command> {
            return std::make_unique<UpdateLinearCmd>(
                &data, index, std::get<LinearElement>(before), e, description);
          },
          [&](const StructureElement& e) -> std::unique_ptr<Command> {
            return std::make_unique<UpdateStructureCmd>(
                &data, index, std::get<StructureElement>(before), e, description);
          },
          [&](const TroopSpawnElement& e) -> std::unique_ptr<Command> {
            return std::make_unique<UpdateTroopSpawnCmd>(
                &data, index, std::get<TroopSpawnElement>(before), e, description);
          },
          [&](const UndeadZoneElement& e) -> std::unique_ptr<Command> {
            return std::make_unique<UpdateUndeadZoneCmd>(
                &data, index, std::get<UndeadZoneElement>(before), e, description);
          },
      },
      after);
}

auto snapshot(const MapData& data, const ElementRef& ref) -> ElementSnapshot {
  return snapshot(data, ref.kind, ref.index);
}

auto snapshots(const MapData& data,
               const QVector<ElementRef>& refs) -> QVector<ElementSnapshot> {
  QVector<ElementSnapshot> result;
  result.reserve(refs.size());
  for (const ElementRef& ref : refs) {
    result.append(snapshot(data, ref));
  }
  return result;
}

auto make_remove_many(MapData& data,
                      QVector<ElementRef> refs) -> std::unique_ptr<Command> {

  std::sort(refs.begin(), refs.end(), [](const ElementRef& lhs, const ElementRef& rhs) {
    return lhs.kind != rhs.kind ? lhs.kind < rhs.kind : lhs.index > rhs.index;
  });

  std::vector<std::unique_ptr<Command>> commands;
  for (const ElementRef& ref : refs) {
    if (std::unique_ptr<Command> cmd = make_remove(data, ref.kind, ref.index)) {
      commands.push_back(std::move(cmd));
    }
  }

  if (commands.empty()) {
    return {};
  }
  if (commands.size() == 1) {
    return std::move(commands.front());
  }
  return std::make_unique<CompositeCmd>(
      std::move(commands), QStringLiteral("Erase %1 elements").arg(commands.size()));
}

auto make_add_many(MapData& data,
                   const QVector<ElementSnapshot>& snaps) -> std::unique_ptr<Command> {
  std::vector<std::unique_ptr<Command>> commands;
  for (const ElementSnapshot& snap : snaps) {
    if (std::unique_ptr<Command> cmd = make_add(data, snap)) {
      commands.push_back(std::move(cmd));
    }
  }

  if (commands.empty()) {
    return {};
  }
  if (commands.size() == 1) {
    return std::move(commands.front());
  }
  return std::make_unique<CompositeCmd>(
      std::move(commands), QStringLiteral("Place %1 elements").arg(commands.size()));
}

auto make_update_many(MapData& data,
                      const QVector<ElementRef>& refs,
                      const QVector<ElementSnapshot>& before,
                      const QVector<ElementSnapshot>& after,
                      const QString& description) -> std::unique_ptr<Command> {
  if (refs.size() != before.size() || refs.size() != after.size()) {
    return {};
  }

  std::vector<std::unique_ptr<Command>> commands;
  for (int i = 0; i < refs.size(); ++i) {
    if (std::unique_ptr<Command> cmd =
            make_update(data, refs[i].index, before[i], after[i], description)) {
      commands.push_back(std::move(cmd));
    }
  }

  if (commands.empty()) {
    return {};
  }
  if (commands.size() == 1) {
    return std::move(commands.front());
  }
  return std::make_unique<CompositeCmd>(std::move(commands), description);
}

auto group_anchor(const QVector<ElementSnapshot>& snaps) -> std::optional<QPointF> {
  for (const ElementSnapshot& snap : snaps) {
    if (const std::optional<QPointF> pos = position(snap); pos.has_value()) {
      return pos;
    }
  }
  return std::nullopt;
}

} // namespace MapEditor::ElementOps
