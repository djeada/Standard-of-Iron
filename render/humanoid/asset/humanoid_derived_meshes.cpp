#include "render/humanoid/asset/humanoid_derived_meshes.h"

#include <QVector3D>

#include <algorithm>
#include <memory>
#include <vector>

#include "render/gl/primitives.h"
#include "render/gl/shared_geometry_cache.h"

namespace Render::Humanoid {

namespace {

constexpr auto k_torso_no_bottom_cap_key =
    Render::GL::geometry_key("humanoid/torso_without_bottom_cap");

constexpr float k_band_height = 0.02F;
constexpr float k_bottom_threshold = 0.45F;
constexpr float k_facing_down_normal_y = 0.35F;

auto build_torso_without_bottom_cap(Render::GL::Mesh& base)
    -> std::unique_ptr<Render::GL::Mesh> {
  return base.clone_with_filtered_indices(
      [](unsigned int a,
         unsigned int b,
         unsigned int c,
         const std::vector<Render::GL::Vertex>& verts) -> bool {
        auto sample = [&](unsigned int idx) -> QVector3D {
          return {
              verts[idx].position[0], verts[idx].position[1], verts[idx].position[2]};
        };
        QVector3D const pa = sample(a);
        QVector3D const pb = sample(b);
        QVector3D const pc = sample(c);
        float const min_y = std::min({pa.y(), pb.y(), pc.y()});
        float const max_y = std::max({pa.y(), pb.y(), pc.y()});

        QVector3D n(verts[a].normal[0] + verts[b].normal[0] + verts[c].normal[0],
                    verts[a].normal[1] + verts[b].normal[1] + verts[c].normal[1],
                    verts[a].normal[2] + verts[b].normal[2] + verts[c].normal[2]);
        if (n.lengthSquared() > 0.0F) {
          n.normalize();
        }

        bool const is_flat = (max_y - min_y) < k_band_height;
        bool const is_at_bottom = min_y > k_bottom_threshold;
        bool const facing_down = (n.y() > k_facing_down_normal_y);
        return is_flat && is_at_bottom && facing_down;
      });
}

auto torso_without_bottom_cap() -> Render::GL::Mesh* {
  Render::GL::Mesh* base = Render::GL::get_unit_torso();
  if (base == nullptr) {
    return nullptr;
  }

  Render::GL::Mesh* filtered = Render::GL::SharedGeometryCache::instance().get_or_build(
      k_torso_no_bottom_cap_key, [base]() -> std::unique_ptr<Render::GL::Mesh> {
        return build_torso_without_bottom_cap(*base);
      });

  return filtered != nullptr ? filtered : base;
}

} // namespace

void build_humanoid_derived_meshes() {
  (void)torso_without_bottom_cap();
}

auto humanoid_mesh_part(HumanoidMeshPart part) -> Render::GL::Mesh* {
  switch (part) {
  case HumanoidMeshPart::TorsoNoBottomCap:
    return torso_without_bottom_cap();
  }
  return nullptr;
}

} // namespace Render::Humanoid
