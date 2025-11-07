#pragma once

#include "../../game/core/component.h"
#include "../../game/visuals/team_colors.h"
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
          p.entity->getComponent<Engine::Core::ProductionComponent>()) {
    if (prod->rallySet && (p.resources != nullptr)) {
      auto flag = Render::Geom::Flag::create(prod->rallyX, prod->rallyZ,
                                             QVector3D(1.0F, 0.9F, 0.2F),
                                             colors.woodDark, 1.0F);
      Mesh *unit = p.resources->unit();
      out.mesh(unit, flag.pole, flag.poleColor, white, 1.0F);
      out.mesh(unit, flag.pennant, flag.pennantColor, white, 1.0F);
      out.mesh(unit, flag.finial, flag.pennantColor, white, 1.0F);
    }
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
    auto *capture = p.entity->getComponent<Engine::Core::CaptureComponent>();
    if ((capture != nullptr) && capture->isBeingCaptured) {
      float const progress = std::clamp(
          capture->captureProgress / capture->requiredTime, 0.0F, 1.0F);
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
    auto *capture = p.entity->getComponent<Engine::Core::CaptureComponent>();
    if ((capture != nullptr) && capture->isBeingCaptured) {
      float const progress = std::clamp(
          capture->captureProgress / capture->requiredTime, 0.0F, 1.0F);

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
