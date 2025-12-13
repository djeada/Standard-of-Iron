#include "healing_beam_renderer.h"
#include "../../game/systems/healing_beam.h"
#include "../../game/systems/healing_beam_system.h"
#include "../gl/backend.h"
#include "../gl/backend/healing_beam_pipeline.h"
#include "../gl/camera.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../gl/shader.h"
#include "../scene_renderer.h"
#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <cmath>
#include <numbers>
#include <vector>

namespace Render::GL {

HealingBeamRenderer::HealingBeamRenderer() = default;
HealingBeamRenderer::~HealingBeamRenderer() { shutdown(); }

auto HealingBeamRenderer::initialize(Backend *backend) -> bool {
  if (m_initialized) {
    return true;
  }

  if (QOpenGLContext::currentContext() == nullptr) {
    qWarning() << "HealingBeamRenderer initialize: no current GL context";
    return false;
  }

  initializeOpenGLFunctions();

  m_backend = backend;
  if (m_backend == nullptr) {
    return false;
  }

  m_beam_shader = m_backend->shader(QStringLiteral("healing_beam"));
  if (m_beam_shader == nullptr) {
    qWarning() << "HealingBeamRenderer failed: shader 'healing_beam' missing";
    return false;
  }

  m_u_mvp = m_beam_shader->uniform_handle("u_mvp");

  m_u_model = m_beam_shader->optional_uniform_handle("u_model");
  m_u_time = m_beam_shader->uniform_handle("u_time");
  m_u_progress = m_beam_shader->uniform_handle("u_progress");
  m_u_start_pos = m_beam_shader->uniform_handle("u_startPos");
  m_u_end_pos = m_beam_shader->uniform_handle("u_endPos");
  m_u_beam_width = m_beam_shader->uniform_handle("u_beamWidth");
  m_u_heal_color = m_beam_shader->uniform_handle("u_healColor");
  m_u_alpha = m_beam_shader->uniform_handle("u_alpha");

  m_beam_mesh = create_beam_mesh();
  if (!m_beam_mesh) {
    return false;
  }

  auto required_valid = [this]() {
    return m_u_mvp != Shader::InvalidUniform &&
           m_u_time != Shader::InvalidUniform &&
           m_u_progress != Shader::InvalidUniform &&
           m_u_start_pos != Shader::InvalidUniform &&
           m_u_end_pos != Shader::InvalidUniform &&
           m_u_beam_width != Shader::InvalidUniform &&
           m_u_heal_color != Shader::InvalidUniform &&
           m_u_alpha != Shader::InvalidUniform;
  };
  if (!required_valid()) {
    qWarning() << "HealingBeamRenderer failed: missing required uniform handles"
               << "mvp" << m_u_mvp << "time" << m_u_time << "progress"
               << m_u_progress << "start" << m_u_start_pos << "end"
               << m_u_end_pos << "width" << m_u_beam_width << "color"
               << m_u_heal_color << "alpha" << m_u_alpha;
    return false;
  }

  qInfo() << "HealingBeamRenderer initialized. Vertices:"
          << m_beam_mesh->getVertices().size()
          << "Indices:" << m_beam_mesh->getIndices().size();
  qInfo() << "HealingBeamRenderer initialized";
  m_initialized = true;
  return true;
}

void HealingBeamRenderer::shutdown() {
  m_beam_mesh.reset();
  m_beam_shader = nullptr;
  m_backend = nullptr;
  m_initialized = false;
}

auto HealingBeamRenderer::create_beam_mesh() -> std::unique_ptr<Mesh> {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int segments_along = 32;
  constexpr int segments_around = 12;
  constexpr float pi = std::numbers::pi_v<float>;

  for (int i = 0; i <= segments_along; ++i) {
    float t = static_cast<float>(i) / segments_along;

    for (int j = 0; j <= segments_around; ++j) {
      float angle = static_cast<float>(j) / segments_around * 2.0F * pi;

      float x = std::cos(angle);
      float y = std::sin(angle);

      Vertex v;
      v.position = {x, y, t};
      v.normal = {x, y, 0.0F};
      v.tex_coord = {static_cast<float>(j) / segments_around, t};
      vertices.push_back(v);
    }
  }

  for (int i = 0; i < segments_along; ++i) {
    for (int j = 0; j < segments_around; ++j) {
      int curr = i * (segments_around + 1) + j;
      int next = curr + segments_around + 1;

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  return std::make_unique<Mesh>(vertices, indices);
}

void HealingBeamRenderer::render(
    const Game::Systems::HealingBeamSystem *beam_system, const Camera &cam,
    float animation_time) {
  if (QOpenGLContext::currentContext() == nullptr) {
    qWarning() << "HealingBeamRenderer render: no current GL context, skip";
    return;
  }
  if (!m_initialized || beam_system == nullptr ||
      beam_system->get_beam_count() == 0) {
    return;
  }

  GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);

  m_beam_shader->use();
  qDebug() << "Rendering healing beams count:" << beam_system->get_beam_count();

  for (const auto &beam : beam_system->get_beams()) {
    if (beam && beam->is_active()) {
      render_beam(*beam, cam, animation_time);
    }
  }

  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (depthEnabled == GL_TRUE) {
    glEnable(GL_DEPTH_TEST);
  }
  if (cullEnabled == GL_TRUE) {
    glEnable(GL_CULL_FACE);
  }
}

void HealingBeamRenderer::render_beam(const Game::Systems::HealingBeam &beam,
                                      const Camera &cam, float animation_time) {
  QMatrix4x4 model;
  model.setToIdentity();

  QMatrix4x4 mvp = cam.get_projection_matrix() * cam.get_view_matrix() * model;

  m_beam_shader->set_uniform(m_u_mvp, mvp);
  m_beam_shader->set_uniform(m_u_model, model);
  m_beam_shader->set_uniform(m_u_time, animation_time);
  m_beam_shader->set_uniform(m_u_progress, std::min(beam.get_progress(), 1.0F));
  m_beam_shader->set_uniform(m_u_start_pos, beam.get_start());
  m_beam_shader->set_uniform(m_u_end_pos, beam.get_end());
  m_beam_shader->set_uniform(m_u_beam_width, beam.get_beam_width());
  m_beam_shader->set_uniform(m_u_heal_color, beam.get_color());
  m_beam_shader->set_uniform(m_u_alpha, beam.get_intensity());

  m_beam_mesh->draw();

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "HealingBeamRenderer GL error" << err;
  }
}

void render_healing_beams(Renderer *renderer, ResourceManager *,
                          const Game::Systems::HealingBeamSystem &beam_system) {
  if (renderer == nullptr || beam_system.get_beam_count() == 0) {
    return;
  }

  auto *backend = renderer->backend();
  auto *camera = renderer->camera();
  if (backend == nullptr || camera == nullptr) {
    return;
  }

  auto *pipeline = backend->healing_beam_pipeline();
  if (pipeline == nullptr) {
    qWarning() << "HealingBeamPipeline not available";
    return;
  }

  pipeline->render(&beam_system, *camera, renderer->get_animation_time());
}

} // namespace Render::GL
