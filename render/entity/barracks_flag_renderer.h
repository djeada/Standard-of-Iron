#pragma once

#include "../../game/core/component.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/banner_cloth.h"
#include "../geom/flag.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../gl/shader.h"
#include "../scene_renderer.h"
#include "renderer_constants.h"
#include "submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render::GL {

namespace BarracksFlagRenderer {

struct FlagColors {
  QVector3D team;
  QVector3D team_trim;
  QVector3D timber;
  QVector3D timber_light;
  QVector3D wood_dark;
};

struct ClothBannerResources {
  Mesh *cloth_mesh = nullptr;
  Shader *banner_shader = nullptr;
};

struct BannerShaderScope {
  explicit BannerShaderScope(ISubmitter &submitter, Shader *shader) {
    if (shader == nullptr) {
      return;
    }

    if (auto *queue = dynamic_cast<QueueSubmitter *>(&submitter)) {
      m_queue = queue;
      m_previousQueueShader = queue->shader();
      queue->set_shader(shader);
      return;
    }

    ISubmitter *fallback = &submitter;
    if (auto *batch = dynamic_cast<BatchingSubmitter *>(&submitter)) {
      if (batch->fallback_submitter() != nullptr) {
        fallback = batch->fallback_submitter();
      }
    }

    m_renderer = dynamic_cast<Renderer *>(fallback);
    if (m_renderer != nullptr) {
      m_previousRendererShader = m_renderer->get_current_shader();
      m_renderer->set_current_shader(shader);
    }
  }

  ~BannerShaderScope() {
    if (m_queue != nullptr) {
      m_queue->set_shader(m_previousQueueShader);
    }
    if (m_renderer != nullptr) {
      m_renderer->set_current_shader(m_previousRendererShader);
    }
  }

private:
  QueueSubmitter *m_queue = nullptr;
  Shader *m_previousQueueShader = nullptr;
  Renderer *m_renderer = nullptr;
  Shader *m_previousRendererShader = nullptr;
};

inline void draw_rally_flag_if_any(const DrawContext &p, ISubmitter &out,
                                   Texture *white, const FlagColors &colors) {
  if (auto *prod =
          p.entity->get_component<Engine::Core::ProductionComponent>()) {
    if (prod->rally_set && (p.resources != nullptr)) {

      auto flag = Render::Geom::Flag::create(prod->rally_x, prod->rally_z,
                                             QVector3D(1.0F, 0.95F, 0.3F),
                                             colors.wood_dark, 1.6F);

      Mesh *unit = p.resources->unit();
      out.mesh(unit, flag.pole, flag.pole_color, white, 1.0F);
      out.mesh(unit, flag.pennant, flag.pennant_color, white, 1.0F);
      out.mesh(unit, flag.finial, flag.pennant_color, white, 1.0F);
    }
  }
}

inline void draw_banner_with_tassels(
    const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white,
    const QVector3D &banner_center, float half_width, float half_height,
    float depth, const QVector3D &banner_color, const QVector3D &trim_color,
    const ClothBannerResources *cloth = nullptr, int material_id = 0) {
  (void)trim_color;
  (void)depth;

  QMatrix4x4 banner_transform;
  banner_transform.translate(banner_center);

  banner_transform.rotate(90.0F, 1.0F, 0.0F, 0.0F);
  banner_transform.scale(half_width * 2.0F, half_height * 2.0F, 1.0F);

  if (cloth != nullptr && cloth->cloth_mesh != nullptr &&
      cloth->banner_shader != nullptr) {
    BannerShaderScope shader_scope(out, cloth->banner_shader);
    out.mesh(cloth->cloth_mesh, p.model * banner_transform, banner_color, white,
             1.0F, material_id);
  } else {

    QMatrix4x4 box_transform =
        Render::Geom::BannerCloth::generate_banner_transform(
            banner_center, half_width, half_height, 0.02F);
    out.mesh(unit, p.model * box_transform, banner_color, white, 1.0F,
             material_id);
  }
}

inline void draw_pole_with_banner(
    const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white,
    const QVector3D &pole_start, const QVector3D &pole_end, float pole_radius,
    const QVector3D &pole_color, const QVector3D &banner_center,
    const QVector3D &banner_half_size, const QVector3D &banner_color,
    bool enableCapture = false) {
  QVector3D actual_banner_color = banner_color;

  if (enableCapture && p.entity != nullptr) {
    auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
    if ((capture != nullptr) && capture->is_being_captured) {
      float const progress = std::clamp(
          capture->capture_progress / capture->required_time, 0.0F, 1.0F);
      QVector3D const new_team_color =
          Game::Visuals::team_colorForOwner(capture->capturing_player_id);
      actual_banner_color = QVector3D(
          banner_color.x() * (1.0F - progress) + new_team_color.x() * progress,
          banner_color.y() * (1.0F - progress) + new_team_color.y() * progress,
          banner_color.z() * (1.0F - progress) + new_team_color.z() * progress);
    }
  }

  out.mesh(get_unit_cylinder(),
           p.model * Render::Geom::cylinder_between(pole_start, pole_end,
                                                    pole_radius),
           pole_color, white, 1.0F);

  QMatrix4x4 banner_transform = p.model;
  banner_transform.translate(banner_center);
  banner_transform.scale(banner_half_size);
  out.mesh(unit, banner_transform, actual_banner_color, white, 1.0F);
}

struct CaptureColors {
  QVector3D teamColor;
  QVector3D team_trim_color;
  float lowering_offset;
};

inline CaptureColors get_capture_colors(const DrawContext &p,
                                        const QVector3D &base_team_color,
                                        const QVector3D &base_team_trim,
                                        float max_lowering = 0.0F) {
  CaptureColors result{base_team_color, base_team_trim, 0.0F};

  if (p.entity != nullptr) {
    auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
    if ((capture != nullptr) && capture->is_being_captured) {
      float const progress = std::clamp(
          capture->capture_progress / capture->required_time, 0.0F, 1.0F);

      QVector3D const new_team_color =
          Game::Visuals::team_colorForOwner(capture->capturing_player_id);
      result.teamColor = QVector3D(base_team_color.x() * (1.0F - progress) +
                                       new_team_color.x() * progress,
                                   base_team_color.y() * (1.0F - progress) +
                                       new_team_color.y() * progress,
                                   base_team_color.z() * (1.0F - progress) +
                                       new_team_color.z() * progress);
      result.team_trim_color =
          QVector3D(base_team_trim.x() * (1.0F - progress) +
                        new_team_color.x() * 0.6F * progress,
                    base_team_trim.y() * (1.0F - progress) +
                        new_team_color.y() * 0.6F * progress,
                    base_team_trim.z() * (1.0F - progress) +
                        new_team_color.z() * 0.6F * progress);
      result.lowering_offset = progress * max_lowering;
    }
  }

  return result;
}

} // namespace BarracksFlagRenderer

} // namespace Render::GL
