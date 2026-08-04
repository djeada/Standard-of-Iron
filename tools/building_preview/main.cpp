

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPainter>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/nation_id.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"
#include "render/gl/mesh.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/render_archetype.h"
#include "render/software/software_rasterizer.h"
#include "render/submitter.h"

namespace {

using Render::GL::DrawContext;
using Render::GL::EntityRendererRegistry;
using Render::GL::ISubmitter;
using Render::GL::Mesh;
using Render::Software::ColoredTriangle;
using Render::Software::RasterSettings;
using Render::Software::SoftwareRasterizer;

struct Capture {
  Mesh* mesh{nullptr};
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

class CapturingSubmitter : public ISubmitter {
public:
  std::vector<Capture> captures;

  void mesh(Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Render::GL::Texture* = nullptr,
            float alpha = 1.0F,
            int = 0) override {
    if (mesh != nullptr) {
      captures.push_back(Capture{mesh, model, color, alpha});
    }
  }

  void cylinder(const QVector3D& start,
                const QVector3D& end,
                float radius,
                const QVector3D& color,
                float alpha = 1.0F) override {
    Mesh* cyl = Render::GL::get_unit_cylinder();
    if (cyl != nullptr) {
      captures.push_back(Capture{
          cyl, Render::GL::cylinder_local_model(start, end, radius), color, alpha});
    }
  }

  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

struct Bounds {
  QVector3D min{1e9F, 1e9F, 1e9F};
  QVector3D max{-1e9F, -1e9F, -1e9F};
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
  [[nodiscard]] auto valid() const -> bool { return max.x() >= min.x(); }
};

auto replay_to_triangles(const std::vector<Capture>& captures,
                         Bounds& bounds) -> std::vector<ColoredTriangle> {
  std::vector<ColoredTriangle> tris;
  for (const auto& cap : captures) {
    const auto& verts = cap.mesh->get_vertices();
    const auto& idx = cap.mesh->get_indices();
    for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
      const auto& va = verts[idx[i]].position;
      const auto& vb = verts[idx[i + 1]].position;
      const auto& vc = verts[idx[i + 2]].position;
      QVector3D const p0 = cap.model.map(QVector3D(va[0], va[1], va[2]));
      QVector3D const p1 = cap.model.map(QVector3D(vb[0], vb[1], vb[2]));
      QVector3D const p2 = cap.model.map(QVector3D(vc[0], vc[1], vc[2]));
      bounds.include(p0);
      bounds.include(p1);
      bounds.include(p2);
      tris.push_back(ColoredTriangle{p0, p1, p2, cap.color, cap.alpha});
    }
  }
  return tris;
}

auto make_view_projection(const Bounds& bounds,
                          const std::vector<ColoredTriangle>& tris,
                          const QVector3D& eye_dir,
                          int width,
                          int height) -> QMatrix4x4 {
  QVector3D const center = bounds.center();
  QVector3D const span = bounds.span();
  float const radius = std::max({span.x(), span.y(), span.z(), 1.0F});
  QVector3D const dir = eye_dir.normalized();

  QMatrix4x4 view_mat;
  view_mat.lookAt(center + dir * (radius * 5.0F), center, QVector3D(0, 1, 0));

  Bounds cam;
  for (const auto& t : tris) {
    cam.include((view_mat * QVector4D(t.v0, 1.0F)).toVector3D());
    cam.include((view_mat * QVector4D(t.v1, 1.0F)).toVector3D());
    cam.include((view_mat * QVector4D(t.v2, 1.0F)).toVector3D());
  }
  float const x_pad = std::max(cam.span().x() * 0.10F, 0.1F);
  float const y_pad = std::max(cam.span().y() * 0.10F, 0.1F);
  float const depth_pad = std::max(cam.span().z() * 0.10F, 1.0F);
  float const view_w = cam.span().x() + x_pad * 2.0F;
  float const view_h = cam.span().y() + y_pad * 2.0F;
  float const aspect =
      height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0F;
  float ortho_h = std::max(view_h, view_w / std::max(aspect, 1e-6F));
  float ortho_w = ortho_h * aspect;
  if (ortho_w < view_w) {
    ortho_w = view_w;
    ortho_h = ortho_w / std::max(aspect, 1e-6F);
  }
  QVector3D const cc = cam.center();
  QMatrix4x4 proj;
  float const near_plane = std::max(0.1F, -cam.max.z() - depth_pad);
  float const far_plane = std::max(near_plane + 1.0F, -cam.min.z() + depth_pad);
  proj.ortho(cc.x() - ortho_w * 0.5F,
             cc.x() + ortho_w * 0.5F,
             cc.y() - ortho_h * 0.5F,
             cc.y() + ortho_h * 0.5F,
             near_plane,
             far_plane);
  return proj * view_mat;
}

auto render_building(EntityRendererRegistry& registry,
                     Render::GL::ResourceManager* resources,
                     const std::string& nation,
                     const std::string& type,
                     const std::string& key_suffix,
                     const QVector3D& team_color,
                     const QVector3D& eye_dir,
                     const QVector3D& light_dir,
                     float health_ratio,
                     int width,
                     int height) -> QImage {
  Render::GL::RenderFunc const func =
      registry.get("troops/" + nation + "/" + key_suffix);
  QImage img(width, height, QImage::Format_ARGB32);
  img.fill(QColor(60, 70, 50));
  if (!func) {
    return img;
  }

  Engine::Core::Entity entity(1);
  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>(
      std::string{}, std::string{});
  renderable->color = {team_color.x(), team_color.y(), team_color.z()};
  entity.add_component<Engine::Core::TransformComponent>();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->health = static_cast<int>(static_cast<float>(unit->max_health) * health_ratio);
  unit->nation_id = (nation == "carthage") ? Game::Systems::NationID::Carthage
                                           : Game::Systems::NationID::RomanRepublic;

  DrawContext ctx;
  ctx.entity = &entity;
  ctx.resources = resources;
  ctx.model = QMatrix4x4{};
  ctx.distance_sq = 0.0F;
  (void)type;

  CapturingSubmitter sub;
  func(ctx, sub);

  Bounds bounds;
  std::vector<ColoredTriangle> const tris = replay_to_triangles(sub.captures, bounds);
  if (!bounds.valid() || tris.empty()) {
    return img;
  }

  RasterSettings settings;
  settings.width = width;
  settings.height = height;
  settings.clear_color = QColor(60, 70, 50);
  settings.light_dir = light_dir;
  SoftwareRasterizer raster(settings);
  raster.set_view_projection(
      make_view_projection(bounds, tris, eye_dir, width, height));
  for (const auto& t : tris) {
    raster.submit(t);
  }
  return raster.render();
}

} // namespace

auto main(int argc, char** argv) -> int {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication const app(argc, argv);

  QOpenGLContext gl;
  if (!gl.create()) {
    std::cerr << "Failed to create GL context\n";
    return 2;
  }
  QOffscreenSurface surface;
  surface.create();
  if (!gl.makeCurrent(&surface)) {
    std::cerr << "Failed to make GL context current\n";
    return 3;
  }

  std::string out_dir = "build/building_preview";
  std::string filter;
  bool show_states = false;
  for (int i = 1; i < argc; ++i) {
    std::string const arg = argv[i];
    if (arg == "--only" && i + 1 < argc) {
      filter = argv[++i];
    } else if (arg == "--states") {
      show_states = true;
    } else {
      out_dir = arg;
    }
  }
  std::filesystem::create_directories(out_dir);

  EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);

  Render::GL::ResourceManager resources;

  std::vector<std::string> types = {
      "home", "barracks", "defense_tower", "marketplace", "temple", "wall"};
  std::vector<std::string> keys = {"home",
                                   "barracks",
                                   "defense_tower",
                                   "marketplace",
                                   "temple",
                                   "wall_segment_straight"};
  if (!filter.empty()) {
    std::vector<std::string> f_types;
    std::vector<std::string> f_keys;
    for (std::size_t i = 0; i < types.size(); ++i) {
      if (types[i].find(filter) != std::string::npos) {
        f_types.push_back(types[i]);
        f_keys.push_back(keys[i]);
      }
    }
    types = f_types;
    keys = f_keys;
  }
  const std::vector<std::string> nations = {"roman", "carthage"};
  const QVector3D team_color(0.30F, 0.55F, 0.95F);

  struct View {
    const char* name;
    QVector3D dir;
    QVector3D light;
    float health{1.0F};
  };

  const QVector3D game_sun(-0.35F, -0.85F, -0.42F);
  const QVector3D facade_sun(0.35F, -0.85F, -0.42F);

  constexpr float k_cam_elev = 0.7071F;
  constexpr float k_cam_horiz = 0.7071F;
  std::vector<View> views = {{"iso", QVector3D(0.82F, 0.62F, 0.78F), game_sun}};
  if (!filter.empty()) {
    views = {{"front", QVector3D(-k_cam_horiz, k_cam_elev, 0.0F), facade_sun},
             {"front3q",
              QVector3D(-k_cam_horiz * 0.707F, k_cam_elev, k_cam_horiz * 0.707F),
              facade_sun},
             {"side", QVector3D(0.0F, k_cam_elev, k_cam_horiz), game_sun},
             {"rear3q",
              QVector3D(k_cam_horiz * 0.707F, k_cam_elev, k_cam_horiz * 0.707F),
              game_sun}};
  }
  if (show_states) {
    const QVector3D three_quarter(
        -k_cam_horiz * 0.707F, k_cam_elev, k_cam_horiz * 0.707F);
    views = {{"intact", three_quarter, facade_sun, 1.0F},
             {"damaged", three_quarter, facade_sun, 0.45F},
             {"destroyed", three_quarter, facade_sun, 0.10F}};
  }

  const int tile_w = 460;
  const int tile_h = 460;
  const int label_h = 26;
  const int cols = static_cast<int>(nations.size() * views.size());
  const int rows = static_cast<int>(types.size());

  QImage sheet(tile_w * cols, (tile_h + label_h) * rows, QImage::Format_ARGB32);
  sheet.fill(QColor(232, 228, 218));
  QPainter painter(&sheet);
  painter.setPen(QColor(20, 16, 12));
  painter.setFont(QFont("Sans", 12));

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const std::string& nation = nations[c / static_cast<int>(views.size())];
      const View& view = views[c % static_cast<int>(views.size())];
      QImage const tile = render_building(registry,
                                          &resources,
                                          nation,
                                          types[r],
                                          keys[r],
                                          team_color,
                                          view.dir,
                                          view.light,
                                          view.health,
                                          tile_w,
                                          tile_h);
      int const x = c * tile_w;
      int const y = r * (tile_h + label_h);
      painter.drawText(
          x + 8,
          y + 18,
          QString::fromStdString(nation + " / " + types[r] + " / " + view.name));
      painter.drawImage(x, y + label_h, tile);

      std::string const stem = nation + "_" + types[r] +
                               (views.size() > 1 ? std::string("_") + view.name : "");
      tile.save(QString::fromStdString(out_dir + "/" + stem + ".png"));
    }
  }
  painter.end();

  std::string const sheet_path = out_dir + "/contact_sheet.png";
  if (!sheet.save(QString::fromStdString(sheet_path))) {
    std::cerr << "Failed to save " << sheet_path << "\n";
    return 4;
  }
  std::cout << "Wrote " << sheet_path << "\n";
  gl.doneCurrent();
  return 0;
}
