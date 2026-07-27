#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include <memory>
#include <optional>
#include <variant>

#include "map_data.h"

namespace MapEditor {

// Canvas element categories. The integer values are the element_type ids used by
// MapCanvas signals and by EditorWindow, so they must stay stable.
enum class ElementKind {
  Terrain = 0,
  WorldProp = 1,
  Linear = 2,
  Structure = 3,
  TroopSpawn = 4,
  UndeadZone = 5,
};

inline constexpr int k_element_kind_count = 6;

// Identifies one element by category and position in that category's vector.
// Indices shift when elements are added or removed, so refs are only valid until
// the next structural edit.
struct ElementRef {
  int kind = -1;
  int index = -1;

  [[nodiscard]] auto is_valid() const -> bool { return kind >= 0 && index >= 0; }

  friend auto operator==(const ElementRef& lhs, const ElementRef& rhs) -> bool {
    return lhs.kind == rhs.kind && lhs.index == rhs.index;
  }
  friend auto operator!=(const ElementRef& lhs, const ElementRef& rhs) -> bool {
    return !(lhs == rhs);
  }
};

// A copy of one element of any category. std::monostate means "nothing selected",
// which keeps the variant index equal to the ElementKind value plus one.
using ElementSnapshot = std::variant<std::monostate,
                                     TerrainElement,
                                     WorldPropElement,
                                     LinearElement,
                                     StructureElement,
                                     TroopSpawnElement,
                                     UndeadZoneElement>;

// Per-category behaviour used by the canvas, the context menu and the editor
// window. Everything here dispatches on the element kind exactly once so adding a
// category means touching one switch per operation instead of six ladders.
namespace ElementOps {

[[nodiscard]] auto is_valid_kind(int kind) -> bool;
[[nodiscard]] auto kind_of(const ElementSnapshot& snap) -> int;
[[nodiscard]] auto category_label(int kind) -> QString;

[[nodiscard]] auto count(const MapData& data, int kind) -> int;
[[nodiscard]] auto index_valid(const MapData& data, int kind, int index) -> bool;
[[nodiscard]] auto
snapshot(const MapData& data, int kind, int index) -> ElementSnapshot;

[[nodiscard]] auto type_name(const ElementSnapshot& snap) -> QString;
[[nodiscard]] auto display_name(const ElementSnapshot& snap) -> QString;
[[nodiscard]] auto summary(const ElementSnapshot& snap) -> QString;

// Anchor position in grid cells; for linear elements this is the segment centre.
[[nodiscard]] auto position(const ElementSnapshot& snap) -> std::optional<QPointF>;
[[nodiscard]] auto translated(const ElementSnapshot& snap,
                              const QPointF& delta) -> ElementSnapshot;
[[nodiscard]] auto moved_to(const ElementSnapshot& snap,
                            const QPointF& pos) -> ElementSnapshot;
[[nodiscard]] auto snapped_to_grid(const ElementSnapshot& snap) -> ElementSnapshot;
[[nodiscard]] auto has_moved(const ElementSnapshot& before,
                             const ElementSnapshot& after) -> bool;

[[nodiscard]] auto player_id(const ElementSnapshot& snap) -> int;
[[nodiscard]] auto supports_player_id(const ElementSnapshot& snap) -> bool;
[[nodiscard]] auto with_player_id(const ElementSnapshot& snap,
                                  int player_id) -> ElementSnapshot;

// Writes the element back without pushing an undo entry; used during live drags.
void apply(MapData& data, int index, const ElementSnapshot& snap);

[[nodiscard]] auto make_add(MapData& data,
                            const ElementSnapshot& snap) -> std::unique_ptr<Command>;
[[nodiscard]] auto
make_remove(MapData& data, int kind, int index) -> std::unique_ptr<Command>;
[[nodiscard]] auto make_update(MapData& data,
                               int index,
                               const ElementSnapshot& before,
                               const ElementSnapshot& after,
                               const QString& description) -> std::unique_ptr<Command>;

// Bulk variants used by the multi-selection. Each returns a single command so a
// group edit is one undo step; a null return means there was nothing to do.
[[nodiscard]] auto snapshot(const MapData& data,
                            const ElementRef& ref) -> ElementSnapshot;
[[nodiscard]] auto snapshots(const MapData& data, const QVector<ElementRef>& refs)
    -> QVector<ElementSnapshot>;
[[nodiscard]] auto
make_remove_many(MapData& data, QVector<ElementRef> refs) -> std::unique_ptr<Command>;
[[nodiscard]] auto make_add_many(MapData& data, const QVector<ElementSnapshot>& snaps)
    -> std::unique_ptr<Command>;
[[nodiscard]] auto
make_update_many(MapData& data,
                 const QVector<ElementRef>& refs,
                 const QVector<ElementSnapshot>& before,
                 const QVector<ElementSnapshot>& after,
                 const QString& description) -> std::unique_ptr<Command>;

// Group anchor: the position of the first element that has one.
[[nodiscard]] auto
group_anchor(const QVector<ElementSnapshot>& snaps) -> std::optional<QPointF>;

} // namespace ElementOps

} // namespace MapEditor
