#pragma once

#include "../../gl/mesh.h"
#include "../part_graph.h"

#include <QVector3D>

#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Render::Creature::Quadruped {

struct BarrelRing {
  float z{0.0F};
  float y{0.0F};
  float half_width{0.0F};
  float top{0.0F};
  float bottom{0.0F};
};

struct BarrelNode {
  std::vector<BarrelRing> rings{};
  QVector3D scale{1.0F, 1.0F, 1.0F};
  bool horse_rump_profile{false};
  bool horse_head_profile{false};
  bool horse_muzzle_profile{false};
};

struct EllipsoidNode {
  QVector3D center{};
  QVector3D radii{1.0F, 1.0F, 1.0F};
  std::uint8_t ring_count{5U};
  std::uint8_t ring_vertices{8U};
};

struct ColumnLegNode {
  QVector3D top_center{};
  float bottom_y{0.0F};
  float top_radius_x{0.1F};
  float top_radius_z{0.1F};
  float shaft_taper{0.75F};
  float foot_radius_scale{1.5F};
  std::uint8_t ring_count{6U};
  std::uint8_t ring_vertices{6U};
};

struct SnoutNode {
  QVector3D start{};
  QVector3D end{};
  float base_radius{0.12F};
  float tip_radius{0.06F};
  float sag{0.0F};
  std::uint8_t segment_count{8U};
  std::uint8_t ring_vertices{6U};
};

struct FlatFanNode {
  std::vector<QVector3D> outline{};
  QVector3D thickness_axis{0.0F, 0.0F, 1.0F};
  float thickness{0.01F};
};

struct ConeNode {
  QVector3D base_center{};
  QVector3D tip{};
  float base_radius{0.1F};
  std::uint8_t ring_vertices{6U};
};

struct TubeNode {
  QVector3D start{};
  QVector3D end{};
  float start_radius{0.08F};
  float end_radius{0.04F};
  float sag{0.0F};
  std::uint8_t segment_count{5U};
  std::uint8_t ring_vertices{6U};
};

using MeshNodeData = std::variant<BarrelNode, EllipsoidNode, ColumnLegNode,
                                  SnoutNode, FlatFanNode, ConeNode, TubeNode>;

struct MeshNode {
  std::string_view debug_name{};
  BoneIndex anchor_bone{kInvalidBone};
  std::uint8_t color_role{0U};
  std::uint8_t lod_mask{kLodAll};
  int material_id{0};
  MeshNodeData data{};
};

struct CompiledMeshGraph {
  std::deque<std::string> debug_names{};
  std::vector<std::unique_ptr<Render::GL::Mesh>> meshes{};
  std::vector<PrimitiveInstance> primitives{};

  [[nodiscard]] auto part_graph() const noexcept -> PartGraph {
    return {std::span<const PrimitiveInstance>(primitives)};
  }
};

[[nodiscard]] auto
compile_mesh_graph(std::span<const MeshNode> nodes) -> CompiledMeshGraph;

[[nodiscard]] auto compile_combined_mesh_graph(std::span<const MeshNode> nodes)
    -> std::unique_ptr<Render::GL::Mesh>;

} // namespace Render::Creature::Quadruped
