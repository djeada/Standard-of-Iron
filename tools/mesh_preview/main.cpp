#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QPainter>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/spec.h"
#include "render/elephant/dimensions.h"
#include "render/elephant/elephant_source_asset.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_anatomy.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_profile_data.h"
#include "render/horse/horse_source_asset.h"
#include "render/horse/horse_spec.h"
#include "render/rigged_mesh_bake.h"
#include "render/snapshot_mesh_bake.h"
#include "render/software/software_rasterizer.h"

namespace fs = std::filesystem;

namespace {

using Render::Creature::BakedRiggedMeshCpu;
using Render::Creature::BakeInput;
using Render::Creature::BoneWorldMatrix;
using Render::Creature::CreatureSpec;
using Render::Creature::PartGraph;
using Render::Creature::PrimitiveInstance;
using Render::GL::RiggedVertex;
using Render::Software::ColoredTriangle;
using Render::Software::RasterSettings;
using Render::Software::SoftwareRasterizer;

enum class Species {
  Horse,
  Elephant,
};

enum class SourceKind {
  Species,
  Stl,
  Obj,
  ShapeVerification,
};

enum class PartFocus {
  Full,
  Torso,
  Legs,
  NeckHead,
};

enum class PoseMode {
  Rest,
  Walk,
  Trot,
  Run,
  Gallop,
};

struct CliOptions {
  SourceKind source_kind{SourceKind::Species};
  Species species{Species::Horse};
  fs::path input_path;
  fs::path out_dir{"dist/mesh"};
  PartFocus focus{PartFocus::Full};
  PoseMode pose{PoseMode::Rest};
  std::string output_prefix;
};

struct TriangleMesh {
  std::vector<QVector3D> positions;
  std::vector<std::uint32_t> indices;
};

struct Bounds {
  QVector3D min{std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
  QVector3D max{std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()};

  void include(const QVector3D& p) {
    min.setX(std::min(min.x(), p.x()));
    min.setY(std::min(min.y(), p.y()));
    min.setZ(std::min(min.z(), p.z()));
    max.setX(std::max(max.x(), p.x()));
    max.setY(std::max(max.y(), p.y()));
    max.setZ(std::max(max.z(), p.z()));
  }

  [[nodiscard]] auto center() const -> QVector3D { return (min + max) * 0.5F; }
  [[nodiscard]] auto span() const -> QVector3D { return max - min; }
};

struct ViewSpec {
  std::string name;
  QVector3D eye_dir;
  QVector3D up;
};

void print_usage() {
  std::cerr << "usage:\n"
            << "  mesh_preview <horse|elephant> [output_dir] "
               "[full|torso|legs|neck_head] [rest|walk|trot|run|gallop]\n"
            << "  mesh_preview stl <path> [output_dir] [full|torso|legs|neck_head]\n"
            << "  mesh_preview obj <path> [output_dir] [full|torso|legs|neck_head]\n"
            << "  mesh_preview verify-<horse|elephant>-shape [output_dir]\n";
}

auto parse_pose_mode(std::string_view value) -> std::optional<PoseMode> {
  if (value == "rest") {
    return PoseMode::Rest;
  }
  if (value == "walk") {
    return PoseMode::Walk;
  }
  if (value == "trot") {
    return PoseMode::Trot;
  }
  if (value == "run") {
    return PoseMode::Run;
  }
  if (value == "gallop") {
    return PoseMode::Gallop;
  }
  return std::nullopt;
}

[[nodiscard]] auto pose_mode_name(PoseMode pose) -> std::string_view {
  switch (pose) {
  case PoseMode::Rest:
    return "rest";
  case PoseMode::Walk:
    return "walk";
  case PoseMode::Trot:
    return "trot";
  case PoseMode::Run:
    return "run";
  case PoseMode::Gallop:
    return "gallop";
  }
  return "rest";
}

[[nodiscard]] auto species_name(Species species) -> std::string_view;

auto parse_species(std::string_view value) -> Species {
  if (value == "horse") {
    return Species::Horse;
  }
  if (value == "elephant") {
    return Species::Elephant;
  }
  throw std::runtime_error("expected species to be 'horse' or 'elephant'");
}

auto parse_part_focus(std::string_view value) -> std::optional<PartFocus> {
  if (value == "full") {
    return PartFocus::Full;
  }
  if (value == "torso") {
    return PartFocus::Torso;
  }
  if (value == "legs") {
    return PartFocus::Legs;
  }
  if (value == "neck_head") {
    return PartFocus::NeckHead;
  }
  return std::nullopt;
}

[[nodiscard]] auto part_focus_name(PartFocus focus) -> std::string_view {
  switch (focus) {
  case PartFocus::Full:
    return "full";
  case PartFocus::Torso:
    return "torso";
  case PartFocus::Legs:
    return "legs";
  case PartFocus::NeckHead:
    return "neck_head";
  }
  return "full";
}

[[nodiscard]] auto make_output_prefix(std::string_view base,
                                      PartFocus focus) -> std::string {
  std::string prefix(base);
  if (focus == PartFocus::Full) {
    prefix += "_mesh";
  } else {
    prefix += "_";
    prefix += part_focus_name(focus);
    prefix += "_mesh";
  }
  return prefix;
}

auto parse_cli(int argc, char** argv) -> CliOptions {
  if (argc < 2 || argc > 5) {
    throw std::runtime_error("invalid argument count");
  }

  CliOptions options;
  std::string_view const first = argv[1];
  if (first == "verify-horse-shape" || first == "verify-elephant-shape") {
    if (argc > 3) {
      throw std::runtime_error("shape verification expects an optional output_dir");
    }
    options.source_kind = SourceKind::ShapeVerification;
    options.species =
        first == "verify-horse-shape" ? Species::Horse : Species::Elephant;
    if (argc == 3) {
      options.out_dir = fs::path(argv[2]);
    }
    options.output_prefix =
        std::string(species_name(options.species)) + "_shape_verification";
    return options;
  }
  if (first == "stl" || first == "obj") {
    if (argc < 3) {
      throw std::runtime_error("missing mesh path");
    }
    options.source_kind = first == "stl" ? SourceKind::Stl : SourceKind::Obj;
    options.input_path = fs::path(argv[2]);
    int index = 3;
    if (index < argc) {
      if (index + 1 == argc) {
        if (auto const focus = parse_part_focus(argv[index]); focus.has_value()) {
          options.focus = *focus;
        } else {
          options.out_dir = fs::path(argv[index]);
        }
      } else {
        options.out_dir = fs::path(argv[index++]);
        auto const focus = parse_part_focus(argv[index]);
        if (!focus.has_value()) {
          throw std::runtime_error("expected part focus after output_dir");
        }
        options.focus = *focus;
      }
    }
    options.output_prefix =
        make_output_prefix(options.input_path.stem().string(), options.focus);
    return options;
  }

  options.source_kind = SourceKind::Species;
  options.species = parse_species(first);
  int index = 2;
  if (index < argc) {
    if (index + 1 == argc) {
      if (auto const focus = parse_part_focus(argv[index]); focus.has_value()) {
        options.focus = *focus;
      } else {
        options.out_dir = fs::path(argv[index]);
      }
    } else {
      options.out_dir = fs::path(argv[index++]);
      auto const focus = parse_part_focus(argv[index++]);
      if (!focus.has_value()) {
        throw std::runtime_error("expected part focus after output_dir");
      }
      options.focus = *focus;
      if (index < argc) {
        auto const pose = parse_pose_mode(argv[index]);
        if (!pose.has_value()) {
          throw std::runtime_error("expected rest, walk, trot, or gallop after focus");
        }
        options.pose = *pose;
      }
    }
  }
  options.output_prefix =
      make_output_prefix(species_name(options.species), options.focus);
  if (options.pose != PoseMode::Rest) {
    options.output_prefix += "_";
    options.output_prefix += pose_mode_name(options.pose);
  }
  return options;
}

[[nodiscard]] auto species_name(Species species) -> std::string_view {
  switch (species) {
  case Species::Horse:
    return "horse";
  case Species::Elephant:
    return "elephant";
  }
  return "unknown";
}

[[nodiscard]] auto species_spec(Species species) -> const CreatureSpec& {
  switch (species) {
  case Species::Horse:
    return Render::Horse::horse_creature_spec();
  case Species::Elephant:
    return Render::Elephant::elephant_creature_spec();
  }
  return Render::Horse::horse_creature_spec();
}

[[nodiscard]] auto
species_bind_palette(Species species) -> std::span<const QMatrix4x4> {
  switch (species) {
  case Species::Horse:
    return Render::Horse::horse_bind_palette();
  case Species::Elephant:
    return Render::Elephant::elephant_bind_palette();
  }
  return {};
}

[[nodiscard]] auto species_role_colors(Species species) -> std::vector<QVector3D> {
  switch (species) {
  case Species::Horse: {
    Render::GL::HorseVariant const variant = Render::GL::make_horse_variant(
        0U, QVector3D(0.62F, 0.46F, 0.26F), QVector3D(0.82F, 0.82F, 0.80F));
    std::array<QVector3D, 8> roles{};
    Render::Horse::fill_horse_role_colors(variant, roles);
    roles[0] = QVector3D(0.96F, 0.78F, 0.54F);
    roles[1] = QVector3D(0.90F, 0.62F, 0.36F);
    roles[2] = QVector3D(0.92F, 0.66F, 0.40F);
    roles[4] = QVector3D(0.34F, 0.21F, 0.12F);
    roles[5] = QVector3D(0.34F, 0.21F, 0.12F);
    roles[6] = QVector3D(0.76F, 0.56F, 0.38F);
    return {roles.begin(), roles.end()};
  }
  case Species::Elephant: {
    Render::GL::ElephantVariant const variant = Render::GL::make_elephant_variant(
        0U, QVector3D(0.68F, 0.28F, 0.20F), QVector3D(0.78F, 0.70F, 0.42F));
    std::array<QVector3D, Render::Elephant::k_elephant_role_count> roles{};
    Render::Elephant::fill_elephant_role_colors(variant, roles);
    roles[0] = QVector3D(0.74F, 0.74F, 0.70F);
    roles[1] = QVector3D(0.62F, 0.62F, 0.58F);
    roles[2] = QVector3D(0.54F, 0.54F, 0.50F);
    return {roles.begin(), roles.end()};
  }
  }
  return {};
}

auto rest_position(const RiggedVertex& vertex) -> QVector3D {
  return {vertex.position_bone_local[0],
          vertex.position_bone_local[1],
          vertex.position_bone_local[2]};
}

auto collect_world_positions(const BakedRiggedMeshCpu& baked)
    -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.reserve(baked.vertices.size());
  for (auto const& vertex : baked.vertices) {
    positions.push_back(rest_position(vertex));
  }
  return positions;
}

auto collect_pose_positions(Species species,
                            const BakedRiggedMeshCpu& baked,
                            std::span<const QMatrix4x4> bind,
                            PoseMode mode,
                            float phase) -> std::vector<QVector3D> {
  std::array<QMatrix4x4, Render::Creature::Pipeline::k_max_creature_bones> world{};
  bool sampled = false;
  if (species == Species::Horse) {
    std::string_view const clip = mode == PoseMode::Gallop ? "Gallop" : "Walk";
    sampled = Render::Horse::horse_source_sample_clip(clip, phase, world);
  } else {
    std::string_view const clip =
        (mode == PoseMode::Run || mode == PoseMode::Gallop) ? "Run" : "Walk";
    sampled = Render::Elephant::elephant_source_sample_clip(clip, phase, world);
  }
  if (!sampled) {
    throw std::runtime_error("failed to sample source-authored creature clip");
  }
  std::array<QMatrix4x4, Render::Creature::Pipeline::k_max_creature_bones> delta{};
  for (std::size_t i = 0; i < bind.size(); ++i) {
    delta[i] = world[i] * bind[i].inverted();
  }
  auto const skinned = Render::GL::bake_snapshot_vertices(
      baked.vertices, std::span<const QMatrix4x4>(delta.data(), bind.size()));
  std::vector<QVector3D> positions;
  positions.reserve(skinned.size());
  for (auto const& vertex : skinned) {
    positions.emplace_back(vertex.position_bone_local[0],
                           vertex.position_bone_local[1],
                           vertex.position_bone_local[2]);
  }
  return positions;
}

auto normalized_component(float value, float min_value, float max_value) -> float {
  float const span = max_value - min_value;
  if (span <= 1.0e-6F) {
    return 0.5F;
  }
  return (value - min_value) / span;
}

auto triangle_matches_focus(const QVector3D& a,
                            const QVector3D& b,
                            const QVector3D& c,
                            const Bounds& bounds,
                            PartFocus focus) -> bool {
  if (focus == PartFocus::Full) {
    return true;
  }
  QVector3D const centroid = (a + b + c) / 3.0F;
  float const ny = normalized_component(centroid.y(), bounds.min.y(), bounds.max.y());
  float const nz = normalized_component(centroid.z(), bounds.min.z(), bounds.max.z());
  switch (focus) {
  case PartFocus::Legs:
    return ny <= 0.42F;
  case PartFocus::Torso:
    return ny >= 0.18F && ny <= 0.78F && nz >= 0.10F && nz <= 0.72F;
  case PartFocus::NeckHead:
    return ny >= 0.40F && nz >= 0.58F;
  case PartFocus::Full:
  default:
    return true;
  }
}

auto collect_bounds(const std::vector<QVector3D>& positions) -> Bounds {
  Bounds bounds;
  for (auto const& position : positions) {
    bounds.include(position);
  }
  return bounds;
}

auto filter_indices_for_focus(std::span<const QVector3D> positions,
                              std::span<const std::uint32_t> indices,
                              PartFocus focus) -> std::vector<std::uint32_t> {
  if (focus == PartFocus::Full) {
    return {indices.begin(), indices.end()};
  }
  Bounds const full_bounds =
      collect_bounds(std::vector<QVector3D>(positions.begin(), positions.end()));
  std::vector<std::uint32_t> filtered;
  filtered.reserve(indices.size());
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    std::uint32_t const ia = indices[i];
    std::uint32_t const ib = indices[i + 1];
    std::uint32_t const ic = indices[i + 2];
    if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
      continue;
    }
    if (!triangle_matches_focus(
            positions[ia], positions[ib], positions[ic], full_bounds, focus)) {
      continue;
    }
    filtered.insert(filtered.end(), {ia, ib, ic});
  }
  return filtered;
}

auto collect_bounds_from_indices(std::span<const QVector3D> positions,
                                 std::span<const std::uint32_t> indices) -> Bounds {
  Bounds bounds;
  for (std::uint32_t const index : indices) {
    if (index >= positions.size()) {
      continue;
    }
    bounds.include(positions[index]);
  }
  return bounds;
}

auto collect_positions_from_indices(std::span<const QVector3D> positions,
                                    std::span<const std::uint32_t> indices)
    -> std::vector<QVector3D> {
  std::vector<QVector3D> selected;
  selected.reserve(indices.size());
  for (std::uint32_t const index : indices) {
    if (index >= positions.size()) {
      continue;
    }
    selected.push_back(positions[index]);
  }
  return selected;
}

void submit_baked_mesh(const BakedRiggedMeshCpu& baked,
                       const std::vector<QVector3D>& positions,
                       std::span<const QVector3D> role_colors,
                       std::span<const std::uint32_t> indices,
                       SoftwareRasterizer& rasterizer) {
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    std::uint32_t const ia = indices[i];
    std::uint32_t const ib = indices[i + 1];
    std::uint32_t const ic = indices[i + 2];
    if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
      continue;
    }
    std::uint8_t const role = baked.vertices[ia].color_role;
    QVector3D const color =
        role < role_colors.size() ? role_colors[role] : QVector3D(0.8F, 0.8F, 0.8F);
    rasterizer.submit(
        ColoredTriangle{positions[ia], positions[ib], positions[ic], color, 1.0F});
  }
}

void submit_triangle_mesh(std::span<const QVector3D> positions,
                          std::span<const std::uint32_t> indices,
                          const QVector3D& color,
                          SoftwareRasterizer& rasterizer) {
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    std::uint32_t const ia = indices[i];
    std::uint32_t const ib = indices[i + 1];
    std::uint32_t const ic = indices[i + 2];
    if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
      continue;
    }
    rasterizer.submit(
        ColoredTriangle{positions[ia], positions[ib], positions[ic], color, 1.0F});
  }
}

auto make_view_projection(const Bounds& bounds,
                          std::span<const QVector3D> positions,
                          const ViewSpec& view,
                          int image_width,
                          int image_height) -> QMatrix4x4 {
  QVector3D const center = bounds.center();
  QVector3D const span = bounds.span();
  float const radius = std::max({span.x(), span.y(), span.z(), 1.0F}) * 0.70F;
  QVector3D dir = view.eye_dir;
  if (dir.lengthSquared() <= 1.0e-6F) {
    dir = QVector3D(0.0F, 0.0F, 1.0F);
  }
  dir.normalize();

  QMatrix4x4 view_mat;
  view_mat.lookAt(center + dir * (radius * 5.0F), center, view.up);

  Bounds camera_bounds;
  for (QVector3D const& position : positions) {
    camera_bounds.include((view_mat * QVector4D(position, 1.0F)).toVector3D());
  }

  float const x_pad = std::max(camera_bounds.span().x() * 0.18F, 0.12F);
  float const y_pad = std::max(camera_bounds.span().y() * 0.18F, 0.12F);
  float const depth_pad = std::max(camera_bounds.span().z() * 0.10F, 1.0F);

  float const view_w = camera_bounds.span().x() + x_pad * 2.0F;
  float const view_h = camera_bounds.span().y() + y_pad * 2.0F;
  float const aspect = image_height > 0 ? static_cast<float>(image_width) /
                                              static_cast<float>(image_height)
                                        : 1.0F;
  float ortho_h = std::max(view_h, view_w / std::max(aspect, 1.0e-6F));
  float ortho_w = ortho_h * aspect;
  if (ortho_w < view_w) {
    ortho_w = view_w;
    ortho_h = ortho_w / std::max(aspect, 1.0e-6F);
  }
  QVector3D const camera_center = camera_bounds.center();

  QMatrix4x4 proj;
  float const near_plane = std::max(0.1F, -camera_bounds.max.z() - depth_pad);
  float const far_plane =
      std::max(near_plane + 1.0F, -camera_bounds.min.z() + depth_pad);
  proj.ortho(camera_center.x() - ortho_w * 0.5F,
             camera_center.x() + ortho_w * 0.5F,
             camera_center.y() - ortho_h * 0.5F,
             camera_center.y() + ortho_h * 0.5F,
             near_plane,
             far_plane);
  return proj * view_mat;
}

auto compose_contact_sheet(const std::vector<std::pair<std::string, QImage>>& images)
    -> QImage {
  if (images.empty()) {
    return {};
  }

  int const tile_w = images.front().second.width();
  int const tile_h = images.front().second.height();
  int const cols = 2;
  int const rows = 2;
  int const label_h = 28;

  QImage sheet(tile_w * cols, (tile_h + label_h) * rows, QImage::Format_ARGB32);
  sheet.fill(QColor(238, 232, 220));

  QPainter painter(&sheet);
  painter.setPen(QColor(32, 24, 16));
  painter.setFont(QFont("Sans", 12));
  for (std::size_t i = 0; i < images.size(); ++i) {
    int const col = static_cast<int>(i % cols);
    int const row = static_cast<int>(i / cols);
    int const x = col * tile_w;
    int const y = row * (tile_h + label_h);
    painter.drawText(x + 8, y + 20, QString::fromStdString(images[i].first));
    painter.drawImage(x, y + label_h, images[i].second);
  }
  painter.end();
  return sheet;
}

auto compose_comparison_sheet(const std::vector<std::pair<std::string, QImage>>& images)
    -> QImage {
  if (images.empty()) {
    return {};
  }
  int const tile_w = images.front().second.width();
  int const tile_h = images.front().second.height();
  int const cols = 4;
  int const rows = static_cast<int>((images.size() + cols - 1U) / cols);
  int const label_h = 30;
  QImage sheet(tile_w * cols, (tile_h + label_h) * rows, QImage::Format_ARGB32);
  sheet.fill(QColor(238, 232, 220));
  QPainter painter(&sheet);
  painter.setPen(QColor(32, 24, 16));
  painter.setFont(QFont("Sans", 11));
  for (std::size_t i = 0U; i < images.size(); ++i) {
    int const col = static_cast<int>(i % cols);
    int const row = static_cast<int>(i / cols);
    int const x = col * tile_w;
    int const y = row * (tile_h + label_h);
    painter.drawText(x + 7, y + 20, QString::fromStdString(images[i].first));
    painter.drawImage(x, y + label_h, images[i].second);
  }
  painter.end();
  return sheet;
}

template <typename T>
auto read_little(const char* data) -> T {
  T value{};
  std::memcpy(&value, data, sizeof(T));
  return value;
}

auto load_binary_stl(const fs::path& path) -> TriangleMesh {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open STL file");
  }
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
  if (bytes.size() < 84U) {
    throw std::runtime_error("binary STL too small");
  }
  auto const triangle_count = read_little<std::uint32_t>(bytes.data() + 80);
  std::size_t const expected_size =
      84U + static_cast<std::size_t>(triangle_count) * 50U;
  if (expected_size != bytes.size()) {
    throw std::runtime_error("file is not a binary STL");
  }

  TriangleMesh mesh;
  mesh.positions.reserve(static_cast<std::size_t>(triangle_count) * 3U);
  mesh.indices.reserve(static_cast<std::size_t>(triangle_count) * 3U);
  std::size_t offset = 84U;
  for (std::uint32_t i = 0; i < triangle_count; ++i) {
    offset += 12U;
    for (int vertex = 0; vertex < 3; ++vertex) {
      auto const x = read_little<float>(bytes.data() + offset);
      auto const y = read_little<float>(bytes.data() + offset + 4U);
      auto const z = read_little<float>(bytes.data() + offset + 8U);
      mesh.positions.emplace_back(x, y, z);
      mesh.indices.push_back(static_cast<std::uint32_t>(mesh.positions.size() - 1U));
      offset += 12U;
    }
    offset += 2U;
  }
  return mesh;
}

auto load_ascii_stl(const fs::path& path) -> TriangleMesh {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open STL file");
  }

  TriangleMesh mesh;
  std::vector<QVector3D> facet_vertices;
  facet_vertices.reserve(3U);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string token;
    if (!(iss >> token) || token != "vertex") {
      continue;
    }
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    if (!(iss >> x >> y >> z)) {
      throw std::runtime_error("failed to parse ASCII STL vertex");
    }
    facet_vertices.emplace_back(x, y, z);
    if (facet_vertices.size() == 3U) {
      for (QVector3D const& vertex : facet_vertices) {
        mesh.positions.push_back(vertex);
        mesh.indices.push_back(static_cast<std::uint32_t>(mesh.positions.size() - 1U));
      }
      facet_vertices.clear();
    }
  }
  if (mesh.indices.empty()) {
    throw std::runtime_error("ASCII STL contained no triangles");
  }
  return mesh;
}

auto load_stl_mesh(const fs::path& path) -> TriangleMesh {
  try {
    return load_binary_stl(path);
  } catch (std::runtime_error const&) {
    return load_ascii_stl(path);
  }
}

auto parse_obj_index(std::string_view token,
                     std::size_t vertex_count) -> std::uint32_t {
  std::size_t const slash = token.find('/');
  std::string_view const vertex_token = token.substr(0, slash);
  if (vertex_token.empty()) {
    throw std::runtime_error("OBJ face index is missing a vertex index");
  }

  int const index = std::stoi(std::string(vertex_token));
  if (index == 0) {
    throw std::runtime_error("OBJ indices are 1-based and cannot be zero");
  }

  if (index > 0) {
    auto const obj_index = static_cast<std::size_t>(index - 1);
    if (obj_index >= vertex_count) {
      throw std::runtime_error("OBJ face index exceeds vertex count");
    }
    return static_cast<std::uint32_t>(obj_index);
  }

  int const resolved = static_cast<int>(vertex_count) + index;
  if (resolved < 0 || static_cast<std::size_t>(resolved) >= vertex_count) {
    throw std::runtime_error("OBJ negative face index exceeds vertex count");
  }
  return static_cast<std::uint32_t>(resolved);
}

auto load_obj_mesh(const fs::path& path) -> TriangleMesh {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open OBJ file");
  }

  TriangleMesh mesh;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string token;
    if (!(iss >> token)) {
      continue;
    }

    if (token == "v") {
      float x = 0.0F;
      float y = 0.0F;
      float z = 0.0F;
      if (!(iss >> x >> y >> z)) {
        throw std::runtime_error("failed to parse OBJ vertex");
      }
      mesh.positions.emplace_back(x, y, z);
      continue;
    }

    if (token != "f") {
      continue;
    }

    std::vector<std::uint32_t> face_indices;
    std::string index_token;
    while (iss >> index_token) {
      face_indices.push_back(parse_obj_index(index_token, mesh.positions.size()));
    }
    if (face_indices.size() < 3U) {
      throw std::runtime_error("OBJ face had fewer than three vertices");
    }
    for (std::size_t i = 1; i + 1 < face_indices.size(); ++i) {
      mesh.indices.push_back(face_indices[0]);
      mesh.indices.push_back(face_indices[i]);
      mesh.indices.push_back(face_indices[i + 1]);
    }
  }

  if (mesh.positions.empty() || mesh.indices.empty()) {
    throw std::runtime_error("OBJ input produced no triangles");
  }
  return mesh;
}

auto run_shape_verification(const CliOptions& options,
                            const std::vector<ViewSpec>& views,
                            RasterSettings settings) -> int {
  auto const& spec = species_spec(options.species);
  auto const bind = species_bind_palette(options.species);
  BakedRiggedMeshCpu const mesh =
      Render::Creature::bake_rigged_mesh_cpu({&spec.lod_full, bind});
  std::vector<QVector3D> const positions = collect_world_positions(mesh);
  if (positions.empty() || mesh.indices.empty()) {
    std::cerr << "production " << species_name(options.species)
              << " bake produced no geometry\n";
    return 1;
  }

  std::size_t const expected_vertices =
      options.species == Species::Horse ? 4400U : 1464U;
  std::size_t const expected_triangles =
      options.species == Species::Horse ? 2182U : 760U;
  QVector3D const expected_span = options.species == Species::Horse
                                      ? QVector3D(0.830092F, 2.84613F, 2.846574F)
                                      : QVector3D(1.342232F, 1.73836F, 2.19962F);
  constexpr float k_extent_tolerance = 5.0e-5F;
  Bounds const bounds = collect_bounds(positions);
  QVector3D const span = bounds.span();
  bool passed = mesh.vertices.size() == expected_vertices &&
                mesh.indices.size() / 3U == expected_triangles &&
                std::abs(bounds.min.y()) <= 2.0e-6F;
  for (int axis = 0; axis < 3; ++axis) {
    passed = passed && std::isfinite(bounds.min[axis]) &&
             std::isfinite(bounds.max[axis]) &&
             std::abs(span[axis] - expected_span[axis]) <= k_extent_tolerance;
  }

  std::array<PartFocus, 4> const focuses{
      PartFocus::Full, PartFocus::Torso, PartFocus::Legs, PartFocus::NeckHead};
  std::vector<std::pair<std::string, QImage>> renders;
  auto const role_colors = species_role_colors(options.species);
  for (PartFocus const focus : focuses) {
    auto const indices = filter_indices_for_focus(positions, mesh.indices, focus);
    passed = passed && !indices.empty();
    for (ViewSpec const& view : views) {
      QMatrix4x4 const view_projection = make_view_projection(
          bounds, positions, view, settings.width, settings.height);
      SoftwareRasterizer rasterizer(settings);
      rasterizer.set_view_projection(view_projection);
      submit_baked_mesh(mesh, positions, role_colors, indices, rasterizer);
      renders.emplace_back(std::string(part_focus_name(focus)) + '/' + view.name,
                           rasterizer.render());
    }
  }

  QImage const sheet = compose_comparison_sheet(renders);
  fs::path const sheet_path =
      options.out_dir /
      (std::string(species_name(options.species)) + "_shape_verification.png");
  sheet.save(QString::fromStdString(sheet_path.string()));

  fs::path const report_path =
      options.out_dir /
      (std::string(species_name(options.species)) + "_shape_verification.json");
  std::ofstream report(report_path);
  report << std::fixed << std::setprecision(8) << "{\n"
         << R"(  "species": ")" << species_name(options.species) << "\",\n"
         << "  \"passed\": " << (passed ? "true" : "false") << ",\n"
         << "  \"vertices\": " << mesh.vertices.size() << ",\n"
         << "  \"triangles\": " << mesh.indices.size() / 3U << ",\n"
         << "  \"bounds_min\": [" << bounds.min.x() << ", " << bounds.min.y() << ", "
         << bounds.min.z() << "],\n"
         << "  \"bounds_max\": [" << bounds.max.x() << ", " << bounds.max.y() << ", "
         << bounds.max.z() << "],\n"
         << "  \"span\": [" << span.x() << ", " << span.y() << ", " << span.z()
         << "],\n"
         << "  \"extent_tolerance\": " << k_extent_tolerance << "\n}\n";
  report.close();

  std::cout << (passed ? "PASS" : "FAIL") << ' ' << species_name(options.species)
            << " shape verification: vertices=" << mesh.vertices.size()
            << " triangles=" << mesh.indices.size() / 3U << " span=" << span.x() << ','
            << span.y() << ',' << span.z() << '\n'
            << "wrote " << sheet_path << '\n'
            << "wrote " << report_path << '\n';
  return passed ? 0 : 2;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QGuiApplication const app(argc, argv);

  CliOptions options;
  try {
    options = parse_cli(argc, argv);
  } catch (std::runtime_error const& error) {
    std::cerr << error.what() << '\n';
    print_usage();
    return 1;
  }

  fs::create_directories(options.out_dir);

  RasterSettings settings;
  settings.width = 720;
  settings.height = 720;
  settings.clear_color = QColor(238, 232, 220);
  settings.light_dir = QVector3D(-0.4F, -0.9F, -0.3F);
  settings.depth_sort = true;

  std::vector<ViewSpec> const views{
      {"left", QVector3D(1.0F, 0.08F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F)},
      {"rear", QVector3D(0.0F, 0.05F, -1.0F), QVector3D(0.0F, 1.0F, 0.0F)},
      {"front_quarter", QVector3D(0.85F, 0.15F, 0.75F), QVector3D(0.0F, 1.0F, 0.0F)},
      {"top", QVector3D(0.05F, 1.0F, -0.05F), QVector3D(0.0F, 0.0F, -1.0F)},
  };

  if (options.source_kind == SourceKind::ShapeVerification) {
    try {
      return run_shape_verification(options, views, settings);
    } catch (std::runtime_error const& error) {
      std::cerr << "shape verification failed: " << error.what() << '\n';
      return 1;
    }
  }

  std::vector<std::pair<std::string, QImage>> rendered;
  rendered.reserve(views.size());
  if (options.source_kind == SourceKind::Species) {
    if (options.species == Species::Horse) {
      auto const& asset_status = Render::Horse::horse_source_asset_status();
      if (!asset_status.loaded) {
        std::cerr << "horse production asset failed: " << asset_status.error << '\n';
        return 1;
      }
      auto const anatomy =
          Render::Horse::make_horse_anatomy(Render::GL::make_horse_dimensions(0U));
      auto const validation = Render::Horse::validate_horse_anatomy(anatomy);
      if (!validation.valid()) {
        std::cerr << "horse anatomy contract failed at fault indices:";
        for (std::size_t index = 0; index < validation.faults.size(); ++index) {
          if (validation.faults[index]) {
            std::cerr << ' ' << index;
          }
        }
        std::cerr << '\n';
        return 1;
      }
    }
    auto const& spec = species_spec(options.species);
    auto const bind = species_bind_palette(options.species);
    BakeInput const input{&spec.lod_full, bind};
    BakedRiggedMeshCpu const baked = Render::Creature::bake_rigged_mesh_cpu(input);
    if (baked.vertices.empty() || baked.indices.empty()) {
      std::cerr << species_name(options.species) << " bake produced no geometry\n";
      return 1;
    }

    auto const positions = collect_world_positions(baked);
    Bounds const production_bounds = collect_bounds(positions);
    std::cout << "bounds min " << production_bounds.min.x() << ' '
              << production_bounds.min.y() << ' ' << production_bounds.min.z()
              << " max " << production_bounds.max.x() << ' '
              << production_bounds.max.y() << ' ' << production_bounds.max.z() << '\n';
    auto const filtered_indices =
        filter_indices_for_focus(positions, baked.indices, options.focus);
    if (filtered_indices.empty()) {
      std::cerr << "part focus '" << part_focus_name(options.focus)
                << "' produced no geometry\n";
      return 1;
    }
    auto const role_colors = species_role_colors(options.species);
    std::vector<ViewSpec> render_views = views;
    if (options.pose != PoseMode::Rest) {
      render_views = {
          {"phase_0", QVector3D(1.0F, 0.08F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F)},
          {"phase_25", QVector3D(1.0F, 0.08F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F)},
          {"phase_50", QVector3D(1.0F, 0.08F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F)},
          {"phase_75", QVector3D(1.0F, 0.08F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F)}};
    }
    for (std::size_t view_index = 0; view_index < render_views.size(); ++view_index) {
      auto const& view = render_views[view_index];
      auto frame_positions = positions;
      if (options.pose != PoseMode::Rest) {
        frame_positions =
            collect_pose_positions(options.species,
                                   baked,
                                   bind,
                                   options.pose,
                                   static_cast<float>(view_index) * 0.25F);
      }
      Bounds const frame_bounds =
          collect_bounds_from_indices(frame_positions, filtered_indices);
      auto const frame_framed_positions =
          collect_positions_from_indices(frame_positions, filtered_indices);
      SoftwareRasterizer rasterizer(settings);
      rasterizer.set_view_projection(make_view_projection(
          frame_bounds, frame_framed_positions, view, settings.width, settings.height));
      submit_baked_mesh(
          baked, frame_positions, role_colors, filtered_indices, rasterizer);
      QImage const image = rasterizer.render();
      fs::path const out_file =
          options.out_dir / (options.output_prefix + "_" + view.name + ".png");
      image.save(QString::fromStdString(out_file.string()));
      rendered.emplace_back(view.name, image);
    }
  } else {
    TriangleMesh const mesh = options.source_kind == SourceKind::Stl
                                  ? load_stl_mesh(options.input_path)
                                  : load_obj_mesh(options.input_path);
    if (mesh.positions.empty() || mesh.indices.empty()) {
      std::cerr << "mesh input produced no geometry\n";
      return 1;
    }
    auto const filtered_indices =
        filter_indices_for_focus(mesh.positions, mesh.indices, options.focus);
    if (filtered_indices.empty()) {
      std::cerr << "part focus '" << part_focus_name(options.focus)
                << "' produced no geometry\n";
      return 1;
    }
    Bounds const bounds = collect_bounds_from_indices(mesh.positions, filtered_indices);
    auto const framed_positions =
        collect_positions_from_indices(mesh.positions, filtered_indices);
    QVector3D const mesh_color(0.76F, 0.78F, 0.82F);
    for (auto const& view : views) {
      SoftwareRasterizer rasterizer(settings);
      rasterizer.set_view_projection(make_view_projection(
          bounds, framed_positions, view, settings.width, settings.height));
      submit_triangle_mesh(mesh.positions, filtered_indices, mesh_color, rasterizer);
      QImage const image = rasterizer.render();
      fs::path const out_file =
          options.out_dir / (options.output_prefix + "_" + view.name + ".png");
      image.save(QString::fromStdString(out_file.string()));
      rendered.emplace_back(view.name, image);
    }
  }

  QImage const sheet = compose_contact_sheet(rendered);
  fs::path const sheet_path = options.out_dir / (options.output_prefix + "_views.png");
  sheet.save(QString::fromStdString(sheet_path.string()));

  std::cout << "wrote " << sheet_path << "\n";
  for (auto const& [name, image] : rendered) {
    (void)image;
    std::cout << "wrote "
              << (options.out_dir / (options.output_prefix + "_" + name + ".png"))
              << "\n";
  }
  return 0;
}
