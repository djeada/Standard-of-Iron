

#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "render/cache/static_attachment_spec.h"
#include "render/creature/gpu/rigged_mesh.h"
#include "render/creature/part_graph.h"

namespace Render::Creature {

using BoneWorldMatrix = QMatrix4x4;

struct BakeInput {
  const PartGraph* graph{nullptr};
  std::span<const BoneWorldMatrix> bind_pose{};

  std::span<const StaticAttachmentSpec> attachments{};

  CreatureLOD lod{CreatureLOD::Full};
};

struct BakedRiggedMeshCpu {
  std::vector<Render::GL::RiggedVertex> vertices;
  std::vector<std::uint32_t> indices;
};

[[nodiscard]] auto bake_rigged_mesh_cpu(const BakeInput& in) -> BakedRiggedMeshCpu;

[[nodiscard]] auto
bake_rigged_mesh(const BakeInput& in) -> std::unique_ptr<Render::GL::RiggedMesh>;

} // namespace Render::Creature
