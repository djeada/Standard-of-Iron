#pragma once

#include "../../game/core/component.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/banner_cloth.h"
#include "../geom/flag.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "renderer_constants.h"
#include "submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render::GL {

namespace BarracksFlagRenderer {

struct FlagColors {
  QVector3D team;
  QVector3D teamTrim;
  QVector3D timber;
  QVector3D timberLight;
  QVector3D woodDark;
};

inline void draw_rally_flag_if_any(const DrawContext &p, ISubmitter &out,
                                   Texture *white, const FlagColors &colors) {
  if (auto *prod =
          p.entity->get_component<Engine::Core::ProductionComponent>()) {
    if (prod->rally_set && (p.resources != nullptr)) {
      // Simple static flag - animation would require shader integration
      auto flag = Render::Geom::Flag::create(prod->rally_x, prod->rally_z,
                                             QVector3D(1.0F, 0.95F, 0.3F),
                                             colors.woodDark, 1.6F);

      Mesh *unit = p.resources->unit();
      out.mesh(unit, flag.pole, flag.poleColor, white, 1.0F);
      out.mesh(unit, flag.pennant, flag.pennantColor, white, 1.0F);
      out.mesh(unit, flag.finial, flag.pennantColor, white, 1.0F);
    }
  }
}

/**
 * @brief Draw a banner with decorative tassels.
 *
 * Main cloth animation is done via GPU shader (banner.vert/frag).
 * This function adds CPU-side decorative elements like tassels.
 */
inline void drawBannerWithTassels(const DrawContext &p, ISubmitter &out,
                                  Mesh *unit, Texture *white,
                                  const QVector3D &banner_center,
                                  float half_width, float half_height,
                                  float depth, const QVector3D &banner_color,
                                  const QVector3D &trim_color,
                                  int material_id = 0) {
  // Main banner - uses shader for cloth animation
  QMatrix4x4 banner_transform = Render::Geom::BannerCloth::generate_banner_transform(
      banner_center, half_width, half_height, depth);
  out.mesh(unit, p.model * banner_transform, banner_color, white, 1.0F,
           material_id);

  // Add decorative tassels (simple CPU animation)
  auto tassels = Render::Geom::BannerTassels::generate_bottom_tassels(
      banner_center, half_width * 2.0F, half_height * 2.0F, 0.06F, 5,
      p.animation_time, trim_color,
      QVector3D(0.85F, 0.75F, 0.45F)); // Gold tips

  for (const auto &thread : tassels.thread_transforms) {
    out.mesh(getUnitCylinder(), p.model * thread, tassels.thread_color, white,
             1.0F);
  }

  for (const auto &tip : tassels.tip_transforms) {
    out.mesh(unit, p.model * tip, tassels.tip_color, white, 1.0F);
  }

  // Decorative border trim overlay
  auto trims = Render::Geom::BannerEmbroidery::generate_border_trim(
      banner_center, half_width, half_height, 0.035F, trim_color);

  for (const auto &trim : trims) {
    out.mesh(unit, p.model * trim.transform, trim.color, white, trim.alpha);
  }
}

inline void
drawPoleWithBanner(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const QVector3D &poleStart,
                   const QVector3D &poleEnd, float poleRadius,
                   const QVector3D &poleColor, const QVector3D &bannerCenter,
                   const QVector3D &bannerHalfSize,
                   const QVector3D &bannerColor, bool enableCapture = false) {
  QVector3D actualBannerColor = bannerColor;

  if (enableCapture && p.entity != nullptr) {
    auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
    if ((capture != nullptr) && capture->is_being_captured) {
      float const progress = std::clamp(
          capture->capture_progress / capture->required_time, 0.0F, 1.0F);
      QVector3D const new_team_color =
          Game::Visuals::team_colorForOwner(capture->capturing_player_id);
      actualBannerColor = QVector3D(
          bannerColor.x() * (1.0F - progress) + new_team_color.x() * progress,
          bannerColor.y() * (1.0F - progress) + new_team_color.y() * progress,
          bannerColor.z() * (1.0F - progress) + new_team_color.z() * progress);
    }
  }

  out.mesh(getUnitCylinder(),
           p.model *
               Render::Geom::cylinderBetween(poleStart, poleEnd, poleRadius),
           poleColor, white, 1.0F);

  QMatrix4x4 bannerTransform = p.model;
  bannerTransform.translate(bannerCenter);
  bannerTransform.scale(bannerHalfSize);
  out.mesh(unit, bannerTransform, actualBannerColor, white, 1.0F);
}

struct CaptureColors {
  QVector3D teamColor;
  QVector3D teamTrimColor;
  float loweringOffset;
};

inline CaptureColors get_capture_colors(const DrawContext &p,
                                        const QVector3D &baseTeamColor,
                                        const QVector3D &baseTeamTrim,
                                        float maxLowering = 0.0F) {
  CaptureColors result{baseTeamColor, baseTeamTrim, 0.0F};

  if (p.entity != nullptr) {
    auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
    if ((capture != nullptr) && capture->is_being_captured) {
      float const progress = std::clamp(
          capture->capture_progress / capture->required_time, 0.0F, 1.0F);

      QVector3D const new_team_color =
          Game::Visuals::team_colorForOwner(capture->capturing_player_id);
      result.teamColor = QVector3D(
          baseTeamColor.x() * (1.0F - progress) + new_team_color.x() * progress,
          baseTeamColor.y() * (1.0F - progress) + new_team_color.y() * progress,
          baseTeamColor.z() * (1.0F - progress) +
              new_team_color.z() * progress);
      result.teamTrimColor =
          QVector3D(baseTeamTrim.x() * (1.0F - progress) +
                        new_team_color.x() * 0.6F * progress,
                    baseTeamTrim.y() * (1.0F - progress) +
                        new_team_color.y() * 0.6F * progress,
                    baseTeamTrim.z() * (1.0F - progress) +
                        new_team_color.z() * 0.6F * progress);
      result.loweringOffset = progress * maxLowering;
    }
  }

  return result;
}

} // namespace BarracksFlagRenderer

} // namespace Render::GL
