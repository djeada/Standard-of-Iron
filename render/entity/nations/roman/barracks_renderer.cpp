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

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinderBetween;

// Roman fortress: thick stone walls, multiple levels, defensive architecture
struct RomanPalette {
  QVector3D stone_light{0.62F, 0.60F, 0.58F};
  QVector3D stone_dark{0.50F, 0.48F, 0.46F};
  QVector3D stone_base{0.55F, 0.53F, 0.51F};
  QVector3D brick{0.75F, 0.52F, 0.42F};
  QVector3D brick_dark{0.62F, 0.42F, 0.32F};
  QVector3D tile_red{0.72F, 0.40F, 0.30F};
  QVector3D tile_dark{0.58F, 0.30F, 0.22F};
  QVector3D wood{0.42F, 0.28F, 0.16F};
  QVector3D wood_dark{0.32F, 0.20F, 0.10F};
  QVector3D iron{0.35F, 0.35F, 0.38F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto makePalette(const QVector3D &team) -> RomanPalette {
  RomanPalette p;
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

// Roman fortress base - thick stone foundation
void drawFortressBase(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const RomanPalette &c) {
  // Massive stone foundation
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.15F, 0.0F),
          QVector3D(1.8F, 0.15F, 1.5F), c.stone_base);
  
  // Stone blocks around perimeter
  for (float x = -1.6F; x <= 1.6F; x += 0.4F) {
    drawBox(out, unit, white, p.model, QVector3D(x, 0.35F, -1.4F),
            QVector3D(0.18F, 0.08F, 0.08F), c.stone_dark);
    drawBox(out, unit, white, p.model, QVector3D(x, 0.35F, 1.4F),
            QVector3D(0.18F, 0.08F, 0.08F), c.stone_dark);
  }
  for (float z = -1.3F; z <= 1.3F; z += 0.4F) {
    drawBox(out, unit, white, p.model, QVector3D(-1.7F, 0.35F, z),
            QVector3D(0.08F, 0.08F, 0.18F), c.stone_dark);
    drawBox(out, unit, white, p.model, QVector3D(1.7F, 0.35F, z),
            QVector3D(0.08F, 0.08F, 0.18F), c.stone_dark);
  }
}

// Main fortress walls - thick stone construction
void drawFortressWalls(const DrawContext &p, ISubmitter &out, Mesh *unit,
                       Texture *white, const RomanPalette &c) {
  float const wall_height = 1.2F;
  
  // Four main walls
  drawBox(out, unit, white, p.model, QVector3D(0.0F, wall_height * 0.5F + 0.3F, -1.3F),
          QVector3D(1.5F, wall_height * 0.5F, 0.12F), c.stone_light);
  drawBox(out, unit, white, p.model, QVector3D(0.0F, wall_height * 0.5F + 0.3F, 1.3F),
          QVector3D(1.5F, wall_height * 0.5F, 0.12F), c.stone_light);
  drawBox(out, unit, white, p.model, QVector3D(-1.6F, wall_height * 0.5F + 0.3F, 0.0F),
          QVector3D(0.12F, wall_height * 0.5F, 1.2F), c.stone_light);
  drawBox(out, unit, white, p.model, QVector3D(1.6F, wall_height * 0.5F + 0.3F, 0.0F),
          QVector3D(0.12F, wall_height * 0.5F, 1.2F), c.stone_light);
  
  // Brick accents
  for (int i = 0; i < 6; ++i) {
    float const x = -1.2F + float(i) * 0.5F;
    drawBox(out, unit, white, p.model, QVector3D(x, wall_height + 0.35F, -1.25F),
            QVector3D(0.2F, 0.05F, 0.05F), c.brick);
  }
}

// Corner towers - distinctive Roman feature
void drawCornerTowers(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const RomanPalette &c) {
  QVector3D corners[4] = {
    QVector3D(-1.5F, 0.0F, -1.2F),
    QVector3D(1.5F, 0.0F, -1.2F),
    QVector3D(-1.5F, 0.0F, 1.2F),
    QVector3D(1.5F, 0.0F, 1.2F)
  };
  
  for (int i = 0; i < 4; ++i) {
    // Tower base
    drawBox(out, unit, white, p.model, 
            QVector3D(corners[i].x(), 0.65F, corners[i].z()),
            QVector3D(0.25F, 0.65F, 0.25F), c.stone_dark);
    
    // Tower top
    drawBox(out, unit, white, p.model,
            QVector3D(corners[i].x(), 1.45F, corners[i].z()),
            QVector3D(0.28F, 0.15F, 0.28F), c.brick_dark);
    
    // Crenellations (battlements)
    for (int j = 0; j < 4; ++j) {
      float angle = float(j) * 1.57F;
      float ox = sinf(angle) * 0.18F;
      float oz = cosf(angle) * 0.18F;
      drawBox(out, unit, white, p.model,
              QVector3D(corners[i].x() + ox, 1.68F, corners[i].z() + oz),
              QVector3D(0.06F, 0.08F, 0.06F), c.stone_light);
    }
  }
}

// Interior courtyard structure
void drawCourtyard(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const RomanPalette &c) {
  // Courtyard floor
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.32F, 0.0F),
          QVector3D(1.2F, 0.02F, 0.9F), c.stone_base);
  
  // Central pillar
  drawCyl(out, p.model, QVector3D(0.0F, 0.3F, 0.0F),
          QVector3D(0.0F, 0.95F, 0.0F), 0.08F, c.stone_light, white);
  
  // Archways
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.65F, -0.85F),
          QVector3D(0.35F, 0.35F, 0.08F), c.brick);
}

// Flat Roman roof with tiles
void drawRomanRoof(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const RomanPalette &c) {
  // Main roof
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 1.58F, 0.0F),
          QVector3D(1.55F, 0.05F, 1.25F), c.tile_red);
  
  // Roof details - tile rows
  for (float z = -1.0F; z <= 1.0F; z += 0.3F) {
    drawBox(out, unit, white, p.model, QVector3D(0.0F, 1.62F, z),
            QVector3D(1.5F, 0.02F, 0.08F), c.tile_dark);
  }
}

// Gate entrance
void drawGate(const DrawContext &p, ISubmitter &out, Mesh *unit,
              Texture *white, const RomanPalette &c) {
  // Gate frame
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 0.6F, 1.35F),
          QVector3D(0.5F, 0.6F, 0.08F), c.wood_dark);
  
  // Iron reinforcements
  for (int i = 0; i < 3; ++i) {
    float y = 0.3F + float(i) * 0.3F;
    drawBox(out, unit, white, p.model, QVector3D(0.0F, y, 1.37F),
            QVector3D(0.45F, 0.03F, 0.02F), c.iron);
  }
}

// Roman military standards/flags
void drawStandards(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const RomanPalette &c) {
  // Eagle standard on tower
  drawCyl(out, p.model, QVector3D(1.5F, 1.6F, -1.2F),
          QVector3D(1.5F, 2.1F, -1.2F), 0.03F, c.wood, white);
  
  // Eagle top
  drawBox(out, unit, white, p.model, QVector3D(1.5F, 2.15F, -1.2F),
          QVector3D(0.08F, 0.06F, 0.08F), c.iron);
  
  // Banner
  drawBox(out, unit, white, p.model, QVector3D(1.5F, 1.95F, -1.15F),
          QVector3D(0.25F, 0.15F, 0.02F), c.team);
}

void drawRallyFlag(const DrawContext &p, ISubmitter &out,
                   Texture *white, const RomanPalette &c) {
  BarracksFlagRenderer::FlagColors colors{
      .team = c.team,
      .teamTrim = c.team_trim,
      .timber = c.wood,
      .timberLight = c.stone_light,
      .woodDark = c.wood_dark};
  BarracksFlagRenderer::drawRallyFlagIfAny(p, out, white, colors);
}

void drawHealthBar(const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white) {
  if (p.entity == nullptr) return;
  auto *u = p.entity->getComponent<Engine::Core::UnitComponent>();
  if (u == nullptr) return;

  float const ratio = std::clamp(u->health / float(std::max(1, u->max_health)), 0.0F, 1.0F);
  if (ratio <= 0.0F) return;

  QVector3D const bg(0.06F, 0.06F, 0.06F);
  drawBox(out, unit, white, p.model, QVector3D(0.0F, 2.35F, 0.0F),
          QVector3D(0.9F, 0.04F, 0.06F), bg);

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  drawBox(out, unit, white, p.model,
          QVector3D(-(0.9F * (1.0F - ratio)) * 0.5F, 2.36F, 0.0F),
          QVector3D(0.9F * ratio * 0.5F, 0.035F, 0.055F), fg);
}

void drawSelection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(2.4F, 1.0F, 2.0F);
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
  RomanPalette const c = makePalette(team);

  drawFortressBase(p, out, unit, white, c);
  drawFortressWalls(p, out, unit, white, c);
  drawCornerTowers(p, out, unit, white, c);
  drawCourtyard(p, out, unit, white, c);
  drawRomanRoof(p, out, unit, white, c);
  drawGate(p, out, unit, white, c);
  drawStandards(p, out, unit, white, c);
  drawRallyFlag(p, out, white, c);
  drawHealthBar(p, out, unit, white);
  drawSelection(p, out);
}

} // namespace

void registerBarracksRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer("barracks_roman", drawBarracks);
}

} // namespace Render::GL::Roman
