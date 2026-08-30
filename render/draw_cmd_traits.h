#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

#include "render/draw_commands.h"

namespace Render::GL {

namespace detail {

template <typename Cmd, typename Variant>
struct DrawCmdIndexOf;

template <typename Cmd, typename... Alternatives>
struct DrawCmdIndexOf<Cmd, std::variant<Alternatives...>> {
  static constexpr std::size_t value = [] {
    constexpr bool matches[] = {std::is_same_v<Cmd, Alternatives>...};
    std::size_t found = sizeof...(Alternatives);
    for (std::size_t i = 0; i < sizeof...(Alternatives); ++i) {
      if (matches[i]) {
        found = i;
        break;
      }
    }
    return found;
  }();
};

} // namespace detail

template <typename Cmd>
inline constexpr std::size_t cmd_index_of = detail::DrawCmdIndexOf<Cmd, DrawCmd>::value;

inline constexpr std::size_t k_draw_cmd_type_count = std::variant_size_v<DrawCmd>;

enum class DrawCmdType : std::uint8_t {
  Grid = static_cast<std::uint8_t>(cmd_index_of<GridCmd>),
  GroundMarker = static_cast<std::uint8_t>(cmd_index_of<GroundMarkerCmd>),
  SelectionSmoke = static_cast<std::uint8_t>(cmd_index_of<SelectionSmokeCmd>),
  Cylinder = static_cast<std::uint8_t>(cmd_index_of<CylinderCmd>),
  Mesh = static_cast<std::uint8_t>(cmd_index_of<MeshCmd>),
  FogBatch = static_cast<std::uint8_t>(cmd_index_of<FogBatchCmd>),
  TerrainScatter = static_cast<std::uint8_t>(cmd_index_of<TerrainScatterCmd>),
  RainBatch = static_cast<std::uint8_t>(cmd_index_of<RainBatchCmd>),
  TerrainSurface = static_cast<std::uint8_t>(cmd_index_of<TerrainSurfaceCmd>),
  TerrainFeature = static_cast<std::uint8_t>(cmd_index_of<TerrainFeatureCmd>),
  PrimitiveBatch = static_cast<std::uint8_t>(cmd_index_of<PrimitiveBatchCmd>),
  EffectBatch = static_cast<std::uint8_t>(cmd_index_of<EffectBatchCmd>),
  ModeIndicator = static_cast<std::uint8_t>(cmd_index_of<ModeIndicatorCmd>),
  DrawPart = static_cast<std::uint8_t>(cmd_index_of<DrawPartCmd>),
  RiggedCreature = static_cast<std::uint8_t>(cmd_index_of<RiggedCreatureCmd>)
};

constexpr std::size_t MeshCmdIndex = cmd_index_of<MeshCmd>;
constexpr std::size_t GridCmdIndex = cmd_index_of<GridCmd>;
constexpr std::size_t GroundMarkerCmdIndex = cmd_index_of<GroundMarkerCmd>;
constexpr std::size_t SelectionSmokeCmdIndex = cmd_index_of<SelectionSmokeCmd>;
constexpr std::size_t CylinderCmdIndex = cmd_index_of<CylinderCmd>;
constexpr std::size_t FogBatchCmdIndex = cmd_index_of<FogBatchCmd>;
constexpr std::size_t TerrainScatterCmdIndex = cmd_index_of<TerrainScatterCmd>;
constexpr std::size_t RainBatchCmdIndex = cmd_index_of<RainBatchCmd>;
constexpr std::size_t TerrainSurfaceCmdIndex = cmd_index_of<TerrainSurfaceCmd>;
constexpr std::size_t TerrainFeatureCmdIndex = cmd_index_of<TerrainFeatureCmd>;
constexpr std::size_t PrimitiveBatchCmdIndex = cmd_index_of<PrimitiveBatchCmd>;
constexpr std::size_t EffectBatchCmdIndex = cmd_index_of<EffectBatchCmd>;
constexpr std::size_t ModeIndicatorCmdIndex = cmd_index_of<ModeIndicatorCmd>;
constexpr std::size_t DrawPartCmdIndex = cmd_index_of<DrawPartCmd>;
constexpr std::size_t RiggedCreatureCmdIndex = cmd_index_of<RiggedCreatureCmd>;

[[nodiscard]] inline constexpr auto
draw_cmd_type_name(std::size_t index) noexcept -> const char* {
  switch (index) {
  case GridCmdIndex:
    return "grid";
  case GroundMarkerCmdIndex:
    return "ground_marker";
  case SelectionSmokeCmdIndex:
    return "selection_smoke";
  case CylinderCmdIndex:
    return "cylinder";
  case MeshCmdIndex:
    return "mesh";
  case FogBatchCmdIndex:
    return "fog_batch";
  case TerrainScatterCmdIndex:
    return "terrain_scatter";
  case RainBatchCmdIndex:
    return "rain_batch";
  case TerrainSurfaceCmdIndex:
    return "terrain_surface";
  case TerrainFeatureCmdIndex:
    return "terrain_feature";
  case PrimitiveBatchCmdIndex:
    return "primitive_batch";
  case EffectBatchCmdIndex:
    return "effect_batch";
  case ModeIndicatorCmdIndex:
    return "mode_indicator";
  case DrawPartCmdIndex:
    return "draw_part";
  case RiggedCreatureCmdIndex:
    return "rigged_creature";
  default:
    return "unknown";
  }
}

enum class RenderPassOrder : std::uint8_t {
  TerrainSurface = 0,
  TerrainFeature = 1,
  TerrainScatter = 2,
  RainBatch = 3,
  PrimitiveBatch = 4,
  Mesh = 5,
  Cylinder = 6,
  FogBatch = 7,
  SelectionSmoke = 8,
  Grid = 9,
  EffectBatch = 10,
  GroundMarker = 16,
  ModeIndicator = 17
};

template <typename Cmd>
struct DrawCmdTraits;

template <>
struct DrawCmdTraits<GridCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::Grid;
};
template <>
struct DrawCmdTraits<GroundMarkerCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::GroundMarker;
};
template <>
struct DrawCmdTraits<SelectionSmokeCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::SelectionSmoke;
};
template <>
struct DrawCmdTraits<CylinderCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::Cylinder;
};
template <>
struct DrawCmdTraits<MeshCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::Mesh;
};
template <>
struct DrawCmdTraits<FogBatchCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::FogBatch;
};
template <>
struct DrawCmdTraits<TerrainScatterCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::TerrainScatter;
};
template <>
struct DrawCmdTraits<RainBatchCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::RainBatch;
};
template <>
struct DrawCmdTraits<TerrainSurfaceCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::TerrainSurface;
};
template <>
struct DrawCmdTraits<TerrainFeatureCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::TerrainFeature;
};
template <>
struct DrawCmdTraits<PrimitiveBatchCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::PrimitiveBatch;
};
template <>
struct DrawCmdTraits<EffectBatchCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::EffectBatch;
};
template <>
struct DrawCmdTraits<ModeIndicatorCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::ModeIndicator;
};
template <>
struct DrawCmdTraits<DrawPartCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::Mesh;
};
template <>
struct DrawCmdTraits<RiggedCreatureCmd> {
  static constexpr RenderPassOrder pass = RenderPassOrder::Mesh;
};

namespace detail {

template <std::size_t... Index>
constexpr auto build_render_pass_table(std::index_sequence<Index...>)
    -> std::array<std::uint8_t, sizeof...(Index)> {
  return {static_cast<std::uint8_t>(
      DrawCmdTraits<std::variant_alternative_t<Index, DrawCmd>>::pass)...};
}

} // namespace detail

inline constexpr auto k_render_pass_by_cmd_type =
    detail::build_render_pass_table(std::make_index_sequence<k_draw_cmd_type_count>{});

inline auto draw_cmd_type(const DrawCmd& cmd) -> DrawCmdType {
  return static_cast<DrawCmdType>(cmd.index());
}

inline auto extract_cmd_priority(const DrawCmd& cmd) -> CommandPriority {
  return std::visit([](const auto& c) -> CommandPriority { return c.priority; }, cmd);
}
} // namespace Render::GL
