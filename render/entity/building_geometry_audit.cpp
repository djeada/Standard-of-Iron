#include "building_geometry_audit.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <sstream>

#include "building_decay.h"

namespace Render::GL {
namespace {

constexpr float k_axis_aligned_tolerance = 1.0e-4F;
constexpr int k_up_axis = 1;

struct AuditBox {
  std::size_t index{0};
  std::string name;
  QVector3D min;
  QVector3D max;
  QVector3D color;
  std::uint8_t palette_slot{k_render_archetype_fixed_color_slot};
  int material_id{0};
  float alpha{1.0F};
};

auto is_box_kind(BuildingPartKind kind) -> bool {
  return kind == BuildingPartKind::Box || kind == BuildingPartKind::PaletteBox;
}

auto is_rotated_box_kind(BuildingPartKind kind) -> bool {
  return kind == BuildingPartKind::RotatedBox ||
         kind == BuildingPartKind::PaletteRotatedBox;
}

auto is_palette_kind(BuildingPartKind kind) -> bool {
  return kind == BuildingPartKind::PaletteBox ||
         kind == BuildingPartKind::PaletteRotatedBox ||
         kind == BuildingPartKind::PaletteCylinder;
}

auto quarter_turn_extent(const BuildingPartDesc& part, QVector3D& half_out) -> bool {
  QMatrix4x4 rotation;
  rotation.rotate(part.euler_deg.z(), 0.0F, 0.0F, 1.0F);
  rotation.rotate(part.euler_deg.y(), 0.0F, 1.0F, 0.0F);
  rotation.rotate(part.euler_deg.x(), 1.0F, 0.0F, 0.0F);

  QVector3D half(0.0F, 0.0F, 0.0F);
  for (int column = 0; column < 3; ++column) {
    const QVector3D axis = rotation.column(column).toVector3D();
    int nonzero = 0;
    int mapped = -1;
    for (int row = 0; row < 3; ++row) {
      const float value = std::fabs(axis[row]);
      if (value > k_axis_aligned_tolerance) {
        ++nonzero;
        mapped = row;
      }
    }
    if (nonzero != 1) {
      return false;
    }
    half[mapped] = std::fabs(part.point_b[column]);
  }

  half_out = half;
  return true;
}

auto short_file(const char* path) -> std::string {
  const std::string full = path != nullptr ? path : "";
  for (const char* root : {"/render/", "/game/", "/tools/", "/tests/"}) {
    const std::size_t cut = full.rfind(root);
    if (cut != std::string::npos) {
      return full.substr(cut + 1);
    }
  }
  return full;
}

auto part_label(const BuildingPartDesc& part, std::size_t index) -> std::string {
  std::ostringstream out;
  if (part.name.empty()) {
    out << "part";
  } else {
    out << part.name;
  }
  out << '[' << index << "] " << short_file(part.origin.file_name()) << ':'
      << part.origin.line();
  return out.str();
}

auto colors_match(const AuditBox& a, const AuditBox& b) -> bool {
  if (a.material_id != b.material_id || std::fabs(a.alpha - b.alpha) > 1.0e-4F) {
    return false;
  }
  const bool a_palette = a.palette_slot != k_render_archetype_fixed_color_slot;
  const bool b_palette = b.palette_slot != k_render_archetype_fixed_color_slot;
  if (a_palette != b_palette) {
    return false;
  }
  if (a_palette) {
    return a.palette_slot == b.palette_slot;
  }
  return (a.color - b.color).lengthSquared() < 1.0e-6F;
}

auto overlap_span(float a_min,
                  float a_max,
                  float b_min,
                  float b_max) -> std::array<float, 2> {
  return {std::max(a_min, b_min), std::min(a_max, b_max)};
}

auto exposed_fraction(const std::vector<AuditBox>& boxes,
                      std::size_t skip_a,
                      std::size_t skip_b,
                      int axis,
                      int outward,
                      float plane,
                      const std::array<float, 2>& span_u,
                      const std::array<float, 2>& span_v) -> float {
  constexpr int k_samples = 9;
  constexpr float k_plane_tolerance = 1.0e-4F;

  const int u = (axis + 1) % 3;
  const int v = (axis + 2) % 3;
  const float step_u = (span_u[1] - span_u[0]) / static_cast<float>(k_samples);
  const float step_v = (span_v[1] - span_v[0]) / static_cast<float>(k_samples);

  int exposed = 0;
  for (int iu = 0; iu < k_samples; ++iu) {
    const float su = span_u[0] + (step_u * (static_cast<float>(iu) + 0.5F));
    for (int iv = 0; iv < k_samples; ++iv) {
      const float sv = span_v[0] + (step_v * (static_cast<float>(iv) + 0.5F));

      bool covered = false;
      for (std::size_t k = 0; k < boxes.size() && !covered; ++k) {
        if (k == skip_a || k == skip_b) {
          continue;
        }
        const AuditBox& other = boxes[k];
        const bool beyond = outward > 0 ? other.max[axis] > plane + k_plane_tolerance
                                        : other.min[axis] < plane - k_plane_tolerance;
        if (!beyond) {
          continue;
        }
        covered = su >= other.min[u] && su <= other.max[u] && sv >= other.min[v] &&
                  sv <= other.max[v];
      }
      exposed += covered ? 0 : 1;
    }
  }

  return static_cast<float>(exposed) / static_cast<float>(k_samples * k_samples);
}

} // namespace

auto audit_building_desc(const BuildingArchetypeDesc& desc,
                         BuildingState state,
                         const BuildingGeometryAuditConfig& config)
    -> BuildingGeometryAuditResult {
  BuildingGeometryAuditResult result;

  std::vector<AuditBox> boxes;
  boxes.reserve(desc.parts().size());

  int seed = 0;
  for (std::size_t index = 0; index < desc.parts().size(); ++index) {
    const auto& part = desc.parts()[index];
    ++seed;
    if (!part_supports_state(part.states, state)) {
      continue;
    }

    QVector3D half;
    if (is_box_kind(part.kind)) {
      half = QVector3D(std::fabs(part.point_b.x()),
                       std::fabs(part.point_b.y()),
                       std::fabs(part.point_b.z()));
    } else if (is_rotated_box_kind(part.kind) && quarter_turn_extent(part, half)) {

    } else {
      ++result.skipped_parts;
      continue;
    }

    AuditBox box;
    box.index = index;
    box.name = part_label(part, index);
    box.min = part.point_a - half;
    box.max = part.point_a + half;
    box.color = decayed_color(part.color, state, seed);
    box.palette_slot =
        is_palette_kind(part.kind)
            ? part.palette_slot
            : static_cast<std::uint8_t>(k_render_archetype_fixed_color_slot);
    box.material_id = part.material_id;
    box.alpha = part.alpha;
    boxes.push_back(std::move(box));
  }

  result.analyzed_parts = boxes.size();

  for (std::size_t i = 0; i < boxes.size(); ++i) {
    float visible = 0.0F;
    for (int axis = 0; axis < 3 && visible <= config.buried_exposure; ++axis) {
      const int u = (axis + 1) % 3;
      const int v = (axis + 2) % 3;
      std::array<float, 2> span_u{boxes[i].min[u], boxes[i].max[u]};
      std::array<float, 2> span_v{boxes[i].min[v], boxes[i].max[v]};
      if (u == k_up_axis) {
        span_u[0] = std::max(span_u[0], config.ground_y);
      }
      if (v == k_up_axis) {
        span_v[0] = std::max(span_v[0], config.ground_y);
      }
      if (span_u[1] <= span_u[0] || span_v[1] <= span_v[0]) {
        continue;
      }
      for (const int outward : {1, -1}) {
        if (axis == k_up_axis && outward < 0) {
          continue;
        }
        const float plane = outward > 0 ? boxes[i].max[axis] : boxes[i].min[axis];
        if (axis == k_up_axis && plane < config.ground_y) {
          continue;
        }
        visible =
            std::max(visible,
                     exposed_fraction(
                         boxes, i, boxes.size(), axis, outward, plane, span_u, span_v));
      }
    }
    if (visible <= config.buried_exposure) {
      result.buried.push_back(
          BuriedPart{desc.name(), state, boxes[i].index, boxes[i].name});
    }
  }

  for (std::size_t i = 0; i + 1 < boxes.size(); ++i) {
    for (std::size_t j = i + 1; j < boxes.size(); ++j) {
      const AuditBox& a = boxes[i];
      const AuditBox& b = boxes[j];

      for (int axis = 0; axis < 3; ++axis) {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;

        auto span_u = overlap_span(a.min[u], a.max[u], b.min[u], b.max[u]);
        auto span_v = overlap_span(a.min[v], a.max[v], b.min[v], b.max[v]);

        if (u == k_up_axis) {
          span_u[0] = std::max(span_u[0], config.ground_y);
        }
        if (v == k_up_axis) {
          span_v[0] = std::max(span_v[0], config.ground_y);
        }
        const float extent_u = span_u[1] - span_u[0];
        const float extent_v = span_v[1] - span_v[0];
        if (extent_u < config.min_overlap || extent_v < config.min_overlap ||
            (extent_u * extent_v) < config.min_overlap_area) {
          continue;
        }

        for (const int outward : {1, -1}) {

          if (axis == k_up_axis && outward < 0) {
            continue;
          }
          const float plane_a = outward > 0 ? a.max[axis] : a.min[axis];
          const float plane_b = outward > 0 ? b.max[axis] : b.min[axis];
          if (axis == k_up_axis && plane_a < config.ground_y) {
            continue;
          }
          const float separation = std::fabs(plane_a - plane_b);
          if (separation >= config.min_separation) {
            continue;
          }

          const float exposure =
              exposed_fraction(boxes, i, j, axis, outward, plane_a, span_u, span_v);
          const float exposed_area = exposure * extent_u * extent_v;
          if (exposure < config.min_exposure ||
              exposed_area < config.min_overlap_area) {
            continue;
          }

          FacadeConflict conflict;
          conflict.archetype = desc.name();
          conflict.state = state;
          conflict.part_a = a.index;
          conflict.part_b = b.index;
          conflict.name_a = a.name;
          conflict.name_b = b.name;
          conflict.min_a = a.min;
          conflict.max_a = a.max;
          conflict.min_b = b.min;
          conflict.max_b = b.max;
          conflict.axis = axis;
          conflict.outward = outward;
          conflict.plane = plane_a;
          conflict.separation = separation;
          conflict.overlap_u = span_u;
          conflict.overlap_v = span_v;
          conflict.overlap_area = extent_u * extent_v;
          conflict.exposure = exposure;
          conflict.same_color = colors_match(a, b);

          if (conflict.same_color) {
            result.benign.push_back(std::move(conflict));
          } else {
            result.conflicts.push_back(std::move(conflict));
          }
        }
      }
    }
  }

  return result;
}

auto describe_facade_conflict(const FacadeConflict& conflict) -> std::string {
  static constexpr std::array<const char*, 3> k_axis_name{{"x", "y", "z"}};
  static constexpr std::array<const char*, 3> k_state_name{
      {"normal", "damaged", "destroyed"}};

  const int axis = std::clamp(conflict.axis, 0, 2);
  const int u = (axis + 1) % 3;
  const int v = (axis + 2) % 3;

  std::ostringstream out;
  out << "archetype=" << conflict.archetype << " state="
      << k_state_name[static_cast<std::size_t>(
             std::clamp(static_cast<int>(conflict.state), 0, 2))]
      << "\n  part_a=" << conflict.name_a << " part_b=" << conflict.name_b
      << "\n  plane=" << (conflict.outward > 0 ? "max_" : "min_") << k_axis_name[axis]
      << " depth=" << conflict.plane << " separation=" << conflict.separation
      << "\n  overlap_" << k_axis_name[u] << "=[" << conflict.overlap_u[0] << ", "
      << conflict.overlap_u[1] << "]" << " overlap_" << k_axis_name[v] << "=["
      << conflict.overlap_v[0] << ", " << conflict.overlap_v[1] << "]"
      << " area=" << conflict.overlap_area << " exposed=" << conflict.exposure
      << "\n  box_a=[" << conflict.min_a.x() << ',' << conflict.min_a.y() << ','
      << conflict.min_a.z() << "]..[" << conflict.max_a.x() << ',' << conflict.max_a.y()
      << ',' << conflict.max_a.z() << ']' << " box_b=[" << conflict.min_b.x() << ','
      << conflict.min_b.y() << ',' << conflict.min_b.z() << "]..[" << conflict.max_b.x()
      << ',' << conflict.max_b.y() << ',' << conflict.max_b.z() << ']';
  return out.str();
}

auto describe_buried_part(const BuriedPart& part) -> std::string {
  static constexpr std::array<const char*, 3> k_state_name{
      {"normal", "damaged", "destroyed"}};

  std::ostringstream out;
  out << "archetype=" << part.archetype << " state="
      << k_state_name[static_cast<std::size_t>(
             std::clamp(static_cast<int>(part.state), 0, 2))]
      << " part=" << part.name;
  return out.str();
}

} // namespace Render::GL
