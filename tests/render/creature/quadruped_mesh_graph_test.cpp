#include "render/creature/quadruped/mesh_graph.h"
#include "render/creature/quadruped/skeleton_factory.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace {

using namespace Render::Creature;
using namespace Render::Creature::Quadruped;

struct Bounds {
  QVector3D min{std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
  QVector3D max{std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()};
};

auto mesh_bounds(const Render::GL::Mesh &mesh) -> Bounds {
  Bounds b;
  for (const auto &v : mesh.get_vertices()) {
    QVector3D const p(v.position[0], v.position[1], v.position[2]);
    b.min.setX(std::min(b.min.x(), p.x()));
    b.min.setY(std::min(b.min.y(), p.y()));
    b.min.setZ(std::min(b.min.z(), p.z()));
    b.max.setX(std::max(b.max.x(), p.x()));
    b.max.setY(std::max(b.max.y(), p.y()));
    b.max.setZ(std::max(b.max.z(), p.z()));
  }
  return b;
}

TEST(QuadrupedMeshGraphTest, CompilesAllSupportedNodeKindsToValidPartGraph) {
  TopologyOptions topo_opts;
  topo_opts.include_appendage_tip = true;
  topo_opts.appendage_tip_name = "TrunkTip";
  auto topology_storage = make_topology(topo_opts);

  BarrelNode barrel;
  barrel.scale = QVector3D(1.2F, 1.0F, 1.6F);
  barrel.rings = {{-0.6F, 0.0F, 0.3F, 0.2F, 0.2F},
                  {-0.2F, 0.1F, 0.5F, 0.3F, 0.3F},
                  {0.2F, 0.1F, 0.5F, 0.3F, 0.3F},
                  {0.6F, 0.0F, 0.3F, 0.2F, 0.2F}};

  EllipsoidNode head;
  head.center = QVector3D(0.0F, 0.2F, 1.2F);
  head.radii = QVector3D(0.35F, 0.28F, 0.42F);

  ColumnLegNode leg;
  leg.top_center = QVector3D(0.45F, -0.1F, 0.4F);
  leg.bottom_y = -1.0F;
  leg.top_radius_x = 0.12F;
  leg.top_radius_z = 0.11F;

  SnoutNode snout;
  snout.start = QVector3D(0.0F, 0.1F, 1.45F);
  snout.end = QVector3D(0.0F, -0.8F, 1.75F);
  snout.base_radius = 0.10F;
  snout.tip_radius = 0.05F;
  snout.sag = -0.08F;

  FlatFanNode ear;
  ear.outline = {{0.0F, 0.35F, 0.95F},
                 {0.38F, 0.25F, 0.90F},
                 {0.30F, -0.10F, 0.88F},
                 {0.02F, -0.18F, 0.92F}};
  ear.thickness_axis = QVector3D(0.0F, 0.0F, 1.0F);
  ear.thickness = 0.03F;

  ConeNode tusk;
  tusk.base_center = QVector3D(0.14F, -0.05F, 1.45F);
  tusk.tip = QVector3D(0.28F, -0.42F, 1.85F);
  tusk.base_radius = 0.03F;

  TubeNode tail;
  tail.start = QVector3D(0.0F, 0.05F, -1.0F);
  tail.end = QVector3D(0.0F, -0.55F, -1.22F);
  tail.start_radius = 0.03F;
  tail.end_radius = 0.015F;
  tail.sag = -0.04F;

  std::vector<MeshNode> nodes;
  nodes.push_back({"body", topology_storage.body, 1U, kLodAll, 0, barrel});
  nodes.push_back({"head", topology_storage.head, 1U, kLodAll, 0, head});
  nodes.push_back({"leg", topology_storage.foot_fl, 2U, kLodAll, 0, leg});
  nodes.push_back(
      {"snout", topology_storage.appendage_tip, 3U, kLodAll, 0, snout});
  nodes.push_back({"ear", topology_storage.head, 4U, kLodFull, 0, ear});
  nodes.push_back({"tusk", topology_storage.head, 5U, kLodFull, 0, tusk});
  nodes.push_back({"tail", topology_storage.body, 6U, kLodAll, 0, tail});

  auto compiled = compile_mesh_graph(nodes);

  ASSERT_EQ(compiled.meshes.size(), nodes.size());
  ASSERT_EQ(compiled.primitives.size(), nodes.size());
  EXPECT_TRUE(
      validate_part_graph(topology_storage.topology(), compiled.part_graph()));
  for (const auto &mesh : compiled.meshes) {
    ASSERT_NE(mesh, nullptr);
    EXPECT_FALSE(mesh->get_vertices().empty());
    EXPECT_FALSE(mesh->get_indices().empty());
  }
}

TEST(QuadrupedMeshGraphTest, BarrelAndLegBoundsFollowAuthoringDimensions) {
  BarrelNode barrel;
  barrel.scale = QVector3D(2.0F, 1.2F, 3.0F);
  barrel.rings = {{-0.5F, 0.0F, 0.4F, 0.2F, 0.2F},
                  {0.0F, 0.1F, 0.6F, 0.3F, 0.3F},
                  {0.5F, 0.0F, 0.4F, 0.2F, 0.2F}};

  ColumnLegNode leg;
  leg.top_center = QVector3D(0.0F, 0.0F, 0.0F);
  leg.bottom_y = -1.2F;
  leg.top_radius_x = 0.15F;
  leg.top_radius_z = 0.12F;

  std::vector<MeshNode> nodes;
  nodes.push_back({"body", 0, 1U, kLodAll, 0, barrel});
  nodes.push_back({"leg", 0, 2U, kLodAll, 0, leg});

  auto compiled = compile_mesh_graph(nodes);
  ASSERT_EQ(compiled.meshes.size(), 2U);

  Bounds const body_bounds = mesh_bounds(*compiled.meshes[0]);
  Bounds const leg_bounds = mesh_bounds(*compiled.meshes[1]);

  EXPECT_GT(body_bounds.max.x() - body_bounds.min.x(), 2.0F);
  EXPECT_GT(body_bounds.max.z() - body_bounds.min.z(), 2.5F);
  EXPECT_LT(leg_bounds.min.y(), -1.0F);
  EXPECT_GT(leg_bounds.max.y(), -0.05F);
}

TEST(QuadrupedMeshGraphTest, CombinedMeshGraphMergesNodesIntoSingleMesh) {
  BarrelNode body;
  body.rings = {{-0.5F, 0.0F, 0.4F, 0.2F, 0.2F},
                {0.0F, 0.0F, 0.6F, 0.3F, 0.3F},
                {0.5F, 0.0F, 0.4F, 0.2F, 0.2F}};

  ConeNode horn;
  horn.base_center = QVector3D(0.0F, 0.2F, 0.8F);
  horn.tip = QVector3D(0.0F, 0.5F, 1.2F);
  horn.base_radius = 0.08F;

  std::vector<MeshNode> nodes;
  nodes.push_back({"body", 0, 1U, kLodAll, 0, body});
  nodes.push_back({"horn", 0, 2U, kLodAll, 0, horn});

  auto combined = compile_combined_mesh_graph(nodes);
  ASSERT_NE(combined, nullptr);
  EXPECT_FALSE(combined->get_vertices().empty());
  EXPECT_FALSE(combined->get_indices().empty());

  Bounds const bounds = mesh_bounds(*combined);
  EXPECT_GT(bounds.max.z(), 1.0F);
  EXPECT_LT(bounds.min.z(), -0.4F);
}

} // namespace
