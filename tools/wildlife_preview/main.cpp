#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QPainter>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "game/session/session_context.h"
#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/schema/creature_runtime_manifest.h"
#include "render/rigged_mesh_bake.h"
#include "render/snapshot_mesh_bake.h"
#include "render/software/software_rasterizer.h"
#include "render/wildlife/sheep_manifest.h"
#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wildlife_rig.h"
#include "render/wildlife/wolf_manifest.h"
#include "render/wildlife/wolf_spec.h"

namespace fs = std::filesystem;

namespace {

using Render::Creature::BakedRiggedMeshCpu;
using Render::Creature::BakeInput;
using Render::Software::ColoredTriangle;
using Render::Software::RasterSettings;
using Render::Software::SoftwareRasterizer;

struct Bounds {
  QVector3D min{1.0e9F, 1.0e9F, 1.0e9F};
  QVector3D max{-1.0e9F, -1.0e9F, -1.0e9F};

  void include(const QVector3D& point) {
    min.setX(std::min(min.x(), point.x()));
    min.setY(std::min(min.y(), point.y()));
    min.setZ(std::min(min.z(), point.z()));
    max.setX(std::max(max.x(), point.x()));
    max.setY(std::max(max.y(), point.y()));
    max.setZ(std::max(max.z(), point.z()));
  }
  [[nodiscard]] auto center() const -> QVector3D { return (min + max) * 0.5F; }
  [[nodiscard]] auto span() const -> QVector3D { return max - min; }
};

auto wolf_role_colors() -> std::vector<QVector3D> {
  QVector3D const base(0.42F, 0.37F, 0.32F);
  return {base,
          QVector3D(0.25F, 0.23F, 0.22F),
          QVector3D(0.63F, 0.58F, 0.51F),
          QVector3D(0.70F, 0.66F, 0.58F),
          QVector3D(0.33F, 0.29F, 0.26F),
          QVector3D(0.29F, 0.26F, 0.24F),
          QVector3D(0.07F, 0.065F, 0.07F),
          QVector3D(0.09F, 0.07F, 0.05F),
          QVector3D(0.86F, 0.64F, 0.16F)};
}

auto sheep_role_colors() -> std::vector<QVector3D> {
  QVector3D const wool(0.82F, 0.79F, 0.72F);
  return {wool,
          QVector3D(0.93F, 0.91F, 0.86F),
          QVector3D(0.56F, 0.52F, 0.46F),
          QVector3D(0.50F, 0.44F, 0.35F),
          QVector3D(0.24F, 0.20F, 0.18F),
          QVector3D(0.16F, 0.14F, 0.13F),
          QVector3D(0.10F, 0.09F, 0.09F),
          QVector3D(0.07F, 0.06F, 0.05F)};
}

struct ViewSpec {
  std::string name;
  QVector3D eye_dir;
};

auto make_view_projection(const Bounds& bounds,
                          const QVector3D& eye_dir,
                          int image_width,
                          int image_height) -> QMatrix4x4 {
  QVector3D const center = bounds.center();
  QVector3D const span = bounds.span();
  float const radius = std::max({span.x(), span.y(), span.z(), 1.0F});

  QVector3D dir = eye_dir.normalized();
  QMatrix4x4 view_mat;
  view_mat.lookAt(center + (dir * radius * 6.0F), center, QVector3D(0.0F, 1.0F, 0.0F));

  float const aspect =
      static_cast<float>(image_width) / static_cast<float>(std::max(image_height, 1));
  float ortho_h = radius * 0.78F;
  float ortho_w = ortho_h * aspect;
  float const needed_w = span.x() * 1.15F;
  if (ortho_w < needed_w) {
    ortho_w = needed_w;
    ortho_h = ortho_w / std::max(aspect, 1.0e-6F);
  }

  QMatrix4x4 proj;
  proj.ortho(-ortho_w * 0.5F,
             ortho_w * 0.5F,
             -ortho_h * 0.5F,
             ortho_h * 0.5F,
             0.1F,
             radius * 20.0F);
  return proj * view_mat;
}

auto skin(const BakedRiggedMeshCpu& baked,
          std::span<const QMatrix4x4> bind,
          std::span<const QMatrix4x4> frame) -> std::vector<QVector3D> {
  std::vector<QMatrix4x4> delta(bind.size());
  for (std::size_t i = 0; i < bind.size(); ++i) {
    delta[i] = frame[i] * bind[i].inverted();
  }
  auto const skinned = Render::GL::bake_snapshot_vertices(
      baked.vertices, std::span<const QMatrix4x4>(delta.data(), delta.size()));
  std::vector<QVector3D> positions;
  positions.reserve(skinned.size());
  for (auto const& vertex : skinned) {
    positions.emplace_back(vertex.position_bone_local[0],
                           vertex.position_bone_local[1],
                           vertex.position_bone_local[2]);
  }
  return positions;
}

void submit(const BakedRiggedMeshCpu& baked,
            const std::vector<QVector3D>& positions,
            std::span<const QVector3D> role_colors,
            SoftwareRasterizer& rasterizer) {
  for (std::size_t i = 0; i + 2 < baked.indices.size(); i += 3) {
    std::uint32_t const ia = baked.indices[i];
    std::uint32_t const ib = baked.indices[i + 1];
    std::uint32_t const ic = baked.indices[i + 2];
    if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
      continue;
    }
    std::uint8_t const role = baked.vertices[ia].color_role;
    QVector3D const color = (role > 0U && role <= role_colors.size())
                                ? role_colors[role - 1U]
                                : QVector3D(0.8F, 0.8F, 0.8F);
    rasterizer.submit(
        ColoredTriangle{positions[ia], positions[ib], positions[ic], color, 1.0F});
  }
}

auto compose(const std::vector<QImage>& tiles,
             const std::vector<std::string>& labels,
             int columns) -> QImage {
  if (tiles.empty()) {
    return {};
  }
  int const tile_w = tiles.front().width();
  int const tile_h = tiles.front().height();
  constexpr int k_label = 18;
  int const rows = (static_cast<int>(tiles.size()) + columns - 1) / columns;
  QImage sheet(tile_w * columns, (tile_h + k_label) * rows, QImage::Format_ARGB32);
  sheet.fill(QColor(18, 20, 18));
  QPainter painter(&sheet);
  painter.setPen(QColor(220, 220, 210));
  for (std::size_t i = 0; i < tiles.size(); ++i) {
    int const col = static_cast<int>(i) % columns;
    int const row = static_cast<int>(i) / columns;
    int const x = col * tile_w;
    int const y = row * (tile_h + k_label);
    painter.drawImage(x, y, tiles[i]);
    painter.drawText(
        x + 4, y + tile_h + k_label - 5, QString::fromStdString(labels[i]));
  }
  painter.end();
  return sheet;
}

auto render_clip(const Render::Creature::CreatureBakeRecipe& recipe,
                 std::span<const QVector3D> role_colors,
                 std::size_t clip_index,
                 int samples,
                 const ViewSpec& view,
                 const RasterSettings& settings,
                 std::vector<QImage>& out_tiles,
                 std::vector<std::string>& out_labels) -> bool {
  auto const& spec = recipe.runtime->creature_spec();
  auto const bind = recipe.runtime->bind_palette();
  BakedRiggedMeshCpu const baked =
      Render::Creature::bake_rigged_mesh_cpu({&spec.lod_full, bind});
  if (baked.vertices.empty()) {
    std::cerr << "bake produced no geometry\n";
    return false;
  }

  auto const& desc = recipe.clips[clip_index];
  std::uint32_t const frame_count = std::max<std::uint32_t>(desc.frame_count, 1U);

  Bounds clip_bounds;
  std::vector<std::vector<QVector3D>> frames;
  frames.reserve(static_cast<std::size_t>(samples));
  for (int sample = 0; sample < samples; ++sample) {
    auto const frame_index = static_cast<std::uint32_t>(
        (static_cast<float>(sample) / static_cast<float>(samples)) *
        static_cast<float>(frame_count));
    std::vector<QMatrix4x4> palette;
    recipe.bake_clip_frame(
        clip_index, std::min(frame_index, frame_count - 1U), palette, nullptr);
    if (palette.size() < bind.size()) {
      std::cerr << "clip frame produced a short palette\n";
      return false;
    }
    auto positions = skin(baked, bind, palette);
    for (auto const& position : positions) {
      clip_bounds.include(position);
    }
    frames.push_back(std::move(positions));
  }

  QMatrix4x4 const view_proj =
      make_view_projection(clip_bounds, view.eye_dir, settings.width, settings.height);
  for (int sample = 0; sample < samples; ++sample) {
    SoftwareRasterizer rasterizer(settings);
    rasterizer.set_view_projection(view_proj);
    submit(baked, frames[static_cast<std::size_t>(sample)], role_colors, rasterizer);
    out_tiles.push_back(rasterizer.render());
    out_labels.push_back(std::string(desc.name) + " " +
                         std::to_string(sample * 100 / samples) + "%");
  }
  return true;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QGuiApplication app(argc, argv);

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);

  std::string species = argc > 1 ? argv[1] : "wolf";
  std::string out_dir = argc > 2 ? argv[2] : "wildlife_preview";
  std::string wanted_clip = argc > 3 ? argv[3] : "";
  int samples = argc > 4 ? std::atoi(argv[4]) : 8;
  std::string view_name = argc > 5 ? argv[5] : "quarter";

  if (species != "wolf" && species != "sheep") {
    std::cerr << "usage: wildlife_preview <wolf|sheep> [out_dir] [clip] [samples] "
                 "[side|quarter|front]\n";
    return 1;
  }

  auto const& recipe = species == "wolf" ? Render::Wildlife::wolf_bake_recipe()
                                         : Render::Wildlife::sheep_bake_recipe();
  auto const colors = species == "wolf" ? wolf_role_colors() : sheep_role_colors();

  ViewSpec view{"quarter", QVector3D(0.85F, 0.42F, 0.75F)};
  if (view_name == "side") {
    view = {"side", QVector3D(1.0F, 0.10F, 0.0F)};
  } else if (view_name == "front") {
    view = {"front", QVector3D(0.15F, 0.30F, 1.0F)};
  }

  RasterSettings settings;
  settings.width = 300;
  settings.height = 240;
  settings.clear_color = QColor(46, 58, 40);

  fs::create_directories(out_dir);

  std::vector<QImage> tiles;
  std::vector<std::string> labels;
  for (std::size_t index = 0; index < recipe.clips.size(); ++index) {
    std::string const name(recipe.clips[index].name);
    if (!wanted_clip.empty() && name != wanted_clip) {
      continue;
    }
    if (!render_clip(recipe, colors, index, samples, view, settings, tiles, labels)) {
      return 1;
    }
  }
  if (tiles.empty()) {
    std::cerr << "no clip matched '" << wanted_clip << "'\n";
    return 1;
  }

  QImage const sheet = compose(tiles, labels, samples);
  fs::path const path =
      fs::path(out_dir) /
      (species + "_" + (wanted_clip.empty() ? std::string("all") : wanted_clip) + "_" +
       view.name + ".png");
  sheet.save(QString::fromStdString(path.string()));
  std::cout << "wrote " << path << " (" << tiles.size() << " frames)\n";
  return 0;
}
