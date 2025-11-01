#include "barracks_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../barracks_flag_renderer.h"
#include "../../registry.h"
#include "../../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinderBetween;

// Carthaginian: white limestone, open colonnade, Mediterranean villa style
struct CarthagePalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.38F, 0.26F, 0.16F};
  QVector3D terracotta{0.82F, 0.62F, 0.45F};
  QVector3D terracotta_dark{0.68F, 0.48F, 0.32F};
  QVector3D blue_accent{0.28F, 0.48F, 0.68F};
  QVector3D blue_light{0.40F, 0.60F, 0.80F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto makePalette(const QVector3D &team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clampVec01(team);
  p.team_trim = clampVec01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

inline void drawBox(ISubmitter &out, Mesh *unit, Texture *white,
                   const QMatrix4x4 &model, const QVector3D &pos,
                   const QVector3D &size, const QVector3D &color) {
  QMatrix4x4 m = model;
  m.translate(pos);
  m.scale(size);
  out.mesh(unit, m, color, white, 1.0F);
}

inline void drawCyl(ISubmitter &out, const QMatrix4x4 &model,
                   const QVector3D &a, const QVector3D &b, float r,
                   const QVector3D &color, Texture *white) {
  out.mesh(getUnitCylinder(), model * cylinderBetween(a, b, r), color, white, 1.0F);
}

// Raised limestone platform
void drawPlatform(const DrawContext &p, ISubmitter &out, Mesh *unit,
                  Texture *white, const CarthagePalette &c) {
  // Wide stepped platform
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.08F, 0.0F),
          QVector3D(2.0F, 0.08F, 1.8F), c.limestone_dark);
  
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.18F, 0.0F),
          QVector3D(1.8F, 0.02F, 1.6F), c.limestone);
  
  // Decorative tiles
  for (float x = -1.5F; x <= 1.5F; x += 0.35F) {
    for (float z = -1.3F; z <= 1.3F; z += 0.35F) {
      if (fabsf(x) > 0.6F || fabsf(z) > 0.5F) {
        drawBox(out, unit, white, p.model, QVector3D(x, 0.21F, z),
                QVector3D(0.15F, 0.01F, 0.15F), c.terracotta);
      }
    }
  }
}

// Magnificent colonnade - 12 columns in peristyle arrangement
void drawColonnade(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const CarthagePalette &c) {
  float const col_height = 1.6F;
  float const col_radius = 0.10F;
  
  // Front colonnade (6 columns)
  for (int i = 0; i < 6; ++i) {
    float const x = -1.25F + float(i) * 0.5F;
    float const z = 1.4F;
    
    // Base
    drawBox(out, unit, white, p.model, QVector3D(x, 0.25F, z),
            QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    
    // Fluted column shaft
    drawCyl(out, p.model, QVector3D(x, 0.2F, z),
            QVector3D(x, 0.2F + col_height, z), col_radius, c.limestone, white);
    
    // Capital with volutes
    drawBox(out, unit, white, p.model,
            QVector3D(x, 0.2F + col_height + 0.05F, z),
            QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);
    
    drawBox(out, unit, white, p.model,
            QVector3D(x, 0.2F + col_height + 0.12F, z),
            QVector3D(col_radius * 1.3F, 0.04F, col_radius * 1.3F), c.gold);
  }
  
  // Side columns (3 per side)
  for (int i = 0; i < 3; ++i) {
    float const z = -1.0F + float(i) * 1.0F;
    
    // Left side
    float const x_left = -1.6F;
    drawBox(out, unit, white, p.model, QVector3D(x_left, 0.25F, z),
            QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    drawCyl(out, p.model, QVector3D(x_left, 0.2F, z),
            QVector3D(x_left, 0.2F + col_height, z), col_radius, c.limestone, white);
    drawBox(out, unit, white, p.model,
            QVector3D(x_left, 0.2F + col_height + 0.05F, z),
            QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);
    
    // Right side
    float const x_right = 1.6F;
    drawBox(out, unit, white, p.model, QVector3D(x_right, 0.25F, z),
            QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    drawCyl(out, p.model, QVector3D(x_right, 0.2F, z),
            QVector3D(x_right, 0.2F + col_height, z), col_radius, c.limestone, white);
    drawBox(out, unit, white, p.model,
            QVector3D(x_right, 0.2F + col_height + 0.05F, z),
            QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);
  }
}

// Open central courtyard with pool
void drawCentralCourtyard(const DrawContext &p, ISubmitter &out, Mesh *unit,
                          Texture *white, const CarthagePalette &c) {
  // Courtyard floor
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.22F, 0.0F),
          QVector3D(1.3F, 0.01F, 1.1F), c.limestone_shade);
  
  // Central reflecting pool
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.24F, 0.0F),
          QVector3D(0.7F, 0.02F, 0.5F), c.blue_light);
  
  // Pool rim
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.25F, -0.52F),
          QVector3D(0.72F, 0.02F, 0.02F), c.blue_accent);
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.25F, 0.52F),
          QVector3D(0.72F, 0.02F, 0.02F), c.blue_accent);
  
  // Decorative fountain pillar
  drawCyl(out, p.model, QVector3D(0.0F, 0.25F, 0.0F),
          QVector3D(0.0F, 0.55F, 0.0F), 0.06F, c.marble, white);
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.58F, 0.0F),
          QVector3D(0.08F, 0.03F, 0.08F), c.blue_accent);
}

// Rear chamber - sleeping quarters
void drawChamber(const DrawContext &p, ISubmitter &out, Mesh *unit,
                 Texture *white, const CarthagePalette &c) {
  float const wall_h = 1.4F;
  
  // Back wall
  drawBox(out, unit, white, p.model, QVector3D(0.0F, wall_h * 0.5F + 0.2F, -1.2F),
          QVector3D(1.4F, wall_h * 0.5F, 0.1F), c.limestone);
  
  // Side walls (partial)
  drawBox(out, unit, white, p.model, QVector3D(-1.5F, wall_h * 0.5F + 0.2F, -0.5F),
          QVector3D(0.1F, wall_h * 0.5F, 0.6F), c.limestone);
  drawBox(out, unit, white, p.model, QVector3D(1.5F, wall_h * 0.5F + 0.2F, -0.5F),
          QVector3D(0.1F, wall_h * 0.5F, 0.6F), c.limestone);
  
  // Arched doorways with blue accents
  drawBox(out, unit, white, p.model, QVector3D(-0.6F, 0.65F, -1.15F),
          QVector3D(0.25F, 0.35F, 0.03F), c.cedar_dark);
  drawBox(out, unit, white, p.model, QVector3D(-0.6F, 0.98F, -1.15F),
          QVector3D(0.25F, 0.05F, 0.03F), c.blue_accent);
  
  drawBox(out, unit, white, p.model, QVector3D(0.6F, 0.65F, -1.15F),
          QVector3D(0.25F, 0.35F, 0.03F), c.cedar_dark);
  drawBox(out, unit, white, p.model, QVector3D(0.6F, 0.98F, -1.15F),
          QVector3D(0.25F, 0.05F, 0.03F), c.blue_accent);
}

// Flat terrace roof with parapet
void drawTerrace(const DrawContext &p, ISubmitter &out, Mesh *unit,
                 Texture *white, const CarthagePalette &c) {
  // Entablature (architrave above columns)
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 2.05F, 0.0F),
          QVector3D(1.7F, 0.08F, 1.5F), c.marble);
  
  // Frieze with decorative band
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 2.12F, 1.45F),
          QVector3D(1.65F, 0.05F, 0.05F), c.gold);
  
  // Flat roof terrace
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 2.18F, -0.2F),
          QVector3D(1.5F, 0.04F, 1.0F), c.terracotta);
  
  // Low parapet walls
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 2.28F, -0.65F),
          QVector3D(1.45F, 0.06F, 0.05F), c.limestone);
  
  // Corner acroteria (decorative roof ornaments)
  for (float x : {-1.4F, 1.4F}) {
    drawBox(out, unit, white, p.model, QVector3D(x, 2.35F, -0.65F),
            QVector3D(0.08F, 0.08F, 0.08F), c.gold);
  }
}

// Phoenician trading goods
void drawTradingGoods(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const CarthagePalette &c) {
  // Amphoras near entrance
  drawCyl(out, p.model, QVector3D(-1.2F, 0.2F, 1.1F),
          QVector3D(-1.2F, 0.5F, 1.1F), 0.08F, c.terracotta_dark, white);
  drawCyl(out, p.model, QVector3D(-0.9F, 0.2F, 1.15F),
          QVector3D(-0.9F, 0.45F, 1.15F), 0.07F, c.terracotta, white);
  
  // Decorative vases
  drawCyl(out, p.model, QVector3D(1.1F, 0.2F, -0.9F),
          QVector3D(1.1F, 0.42F, -0.9F), 0.06F, c.blue_accent, white);
}

// Phoenician banner
void drawPhoenicianBanner(const DrawContext &p, ISubmitter &out, Mesh *unit,
                          Texture *white, const CarthagePalette &c) {
  // Pole on rear wall
  drawCyl(out, p.model, QVector3D(0.0F, 1.6F, -1.25F),
          QVector3D(0.0F, 2.4F, -1.25F), 0.03F, c.cedar, white);
  
  // Banner with team colors
  drawBox(out, unit, white, p.model, QVector3D(0.2F, 2.2F, -1.23F),
          QVector3D(0.25F, 0.2F, 0.02F), c.team);
  
  // Decorative trim
  drawBox(out, unit, white, p.model, QVector3D(0.2F, 2.32F, -1.22F),
          QVector3D(0.26F, 0.02F, 0.01F), c.gold);
}

void drawRallyFlag(const DrawContext &p, ISubmitter &out,
                   Texture *white, const CarthagePalette &c) {
  BarracksFlagRenderer::FlagColors colors{
      .team = c.team,
      .teamTrim = c.team_trim,
      .timber = c.cedar,
      .timberLight = c.limestone,
      .woodDark = c.cedar_dark};
  BarracksFlagRenderer::drawRallyFlagIfAny(p, out, white, colors);
}

void drawHealthBar(const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white) {
  if (p.entity == nullptr) return;
  auto *u = p.entity->getComponent<Engine::Core::UnitComponent>();
  if (u == nullptr) return;

  float const ratio = std::clamp(u->health / float(std::max(1, u->max_health)), 0.0F, 1.0F);
  if (ratio <= 0.0F) return;

  QVector3D const bg(0.06F, 0.06F, 0.06F);
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 2.65F, 0.0F),
          QVector3D(0.9F, 0.04F, 0.06F), bg);

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  drawBox(out, unit, white, p.model,
          QVector3D(-(0.9F * (1.0F - ratio)) * 0.5F, 2.66F, 0.0F),
          QVector3D(0.9F * ratio * 0.5F, 0.035F, 0.055F), fg);
}

void drawSelection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(2.6F, 1.0F, 2.2F);
  if (p.selected) {
    out.selectionSmoke(m, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (p.hovered) {
    out.selectionSmoke(m, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

void drawBarracks(const DrawContext &p, ISubmitter &out) {
  if (!p.resources || !p.entity) return;

  auto *t = p.entity->getComponent<Engine::Core::TransformComponent>();
  auto *r = p.entity->getComponent<Engine::Core::RenderableComponent>();
  if (!t || !r) return;

  Mesh *unit = p.resources->unit();
  Texture *white = p.resources->white();
  QVector3D const team(r->color[0], r->color[1], r->color[2]);
  CarthagePalette const c = makePalette(team);

  drawPlatform(p, out, unit, white, c);
  drawColonnade(p, out, unit, white, c);
  drawCentralCourtyard(p, out, unit, white, c);
  drawChamber(p, out, unit, white, c);
  drawTerrace(p, out, unit, white, c);
  drawTradingGoods(p, out, unit, white, c);
  drawPhoenicianBanner(p, out, unit, white, c);
  drawRallyFlag(p, out, white, c);
  drawHealthBar(p, out, unit, white);
  drawSelection(p, out);
}

} // namespace

void registerBarracksRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer("barracks_carthage", drawBarracks);
}

} // namespace Render::GL::Carthage
