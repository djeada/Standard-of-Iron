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

struct HangingBannerStyle {
  QVector3D pole_base;
  float pole_height{3.0F};
  float pole_radius{0.045F};
  float banner_width{0.9F};
  float banner_height{0.6F};
  float beam_inset{0.02F};
  float banner_depth{0.02F};
  float banner_z_offset{0.02F};
  float connector_drop_ratio{0.35F};
  float capture_lowering_ratio{0.85F};
  QVector3D pole_color;
  QVector3D beam_color;
  QVector3D connector_color;
  QVector3D ornament_offset;
  QVector3D ornament_size;
  QVector3D ornament_color;
  int ring_count{0};
  float ring_y_start{0.4F};
  float ring_spacing{0.5F};
  float ring_height{0.025F};
  float ring_radius_scale{2.0F};
  QVector3D ring_color;
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

inline void draw_hanging_banner(const DrawContext &p, ISubmitter &out,
                                Mesh *unit, Texture *white,
                                const QVector3D &base_team_color,
                                const QVector3D &base_team_trim,
                                const HangingBannerStyle &style,
                                const ClothBannerResources *cloth = nullptr) {
  QVector3D const pole_center(style.pole_base.x(), style.pole_height / 2.0F,
                              style.pole_base.z());
  QVector3D const pole_size(style.pole_radius * 1.8F, style.pole_height / 2.0F,
                            style.pole_radius * 1.8F);

  QMatrix4x4 pole_transform = p.model;
  pole_transform.translate(pole_center);
  pole_transform.scale(pole_size);
  out.mesh(unit, pole_transform, style.pole_color, white, 1.0F);

  auto capture_colors =
      get_capture_colors(p, base_team_color, base_team_trim,
                         style.pole_height * style.capture_lowering_ratio);

  float const beam_length = style.banner_width * 0.5F;
  float const beam_y = style.pole_height - style.banner_height * 0.2F -
                       capture_colors.lowering_offset;
  float const banner_y = style.pole_height - style.banner_height / 2.0F -
                         capture_colors.lowering_offset;

  QVector3D const beam_start(style.pole_base.x() + style.beam_inset, beam_y,
                             style.pole_base.z());
  QVector3D const beam_end(style.pole_base.x() + beam_length + style.beam_inset,
                           beam_y, style.pole_base.z());
  out.mesh(get_unit_cylinder(),
           p.model * Render::Geom::cylinder_between(beam_start, beam_end,
                                                    style.pole_radius * 0.35F),
           style.beam_color, white, 1.0F);

  QVector3D const connector_end(beam_end.x(),
                                beam_end.y() - style.banner_height *
                                                   style.connector_drop_ratio,
                                beam_end.z());
  out.mesh(get_unit_cylinder(),
           p.model * Render::Geom::cylinder_between(beam_end, connector_end,
                                                    style.pole_radius * 0.18F),
           style.connector_color, white, 1.0F);

  QVector3D const banner_center(beam_end.x(), banner_y,
                                style.pole_base.z() + style.banner_z_offset);
  draw_banner_with_tassels(
      p, out, unit, white, banner_center, style.banner_width * 0.5F,
      style.banner_height * 0.5F, style.banner_depth, capture_colors.teamColor,
      capture_colors.team_trim_color, cloth);

  QMatrix4x4 ornament_transform = p.model;
  ornament_transform.translate(style.pole_base + style.ornament_offset);
  ornament_transform.scale(style.ornament_size);
  out.mesh(unit, ornament_transform, style.ornament_color, white, 1.0F);

  for (int i = 0; i < style.ring_count; ++i) {
    float const ring_y =
        style.ring_y_start + static_cast<float>(i) * style.ring_spacing;
    QVector3D const ring_start(style.pole_base.x(), ring_y,
                               style.pole_base.z());
    QVector3D const ring_end(style.pole_base.x(), ring_y + style.ring_height,
                             style.pole_base.z());
    out.mesh(get_unit_cylinder(),
             p.model * Render::Geom::cylinder_between(
                           ring_start, ring_end,
                           style.pole_radius * style.ring_radius_scale),
             style.ring_color, white, 1.0F);
  }
}

} // namespace BarracksFlagRenderer

} // namespace Render::GL
