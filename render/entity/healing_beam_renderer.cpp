#include "healing_beam_renderer.h"
#include "../../game/systems/healing_beam.h"
#include "../../game/systems/healing_beam_system.h"
#include "../gl/backend.h"
#include "../gl/camera.h"
#include "../gl/mesh.h"
#include "../gl/resource_manager.h"
#include "../gl/shader.h"
#include "../scene_renderer.h"
#include <QMatrix4x4>
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

  initializeOpenGLFunctions();

  m_backend = backend;
  if (m_backend == nullptr) {
    return false;
  }

  // Get healing beam shader
  m_beam_shader = m_backend->shader(QStringLiteral("healing_beam"));
  if (m_beam_shader == nullptr) {
    return false;
  }

  // Cache uniform handles
  m_u_mvp = m_beam_shader->uniformHandle("u_mvp");
  m_u_model = m_beam_shader->uniformHandle("u_model");
  m_u_time = m_beam_shader->uniformHandle("u_time");
  m_u_progress = m_beam_shader->uniformHandle("u_progress");
  m_u_start_pos = m_beam_shader->uniformHandle("u_startPos");
  m_u_end_pos = m_beam_shader->uniformHandle("u_endPos");
  m_u_beam_width = m_beam_shader->uniformHandle("u_beamWidth");
  m_u_heal_color = m_beam_shader->uniformHandle("u_healColor");
  m_u_alpha = m_beam_shader->uniformHandle("u_alpha");

  // Create beam mesh
  m_beam_mesh = create_beam_mesh();
  if (!m_beam_mesh) {
    return false;
  }

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

  // Create a tube mesh along Z axis (0 to 1)
  // The vertex shader will deform this along the beam path
  constexpr int segments_along = 32;   // Along beam length
  constexpr int segments_around = 12;  // Around beam circumference
  constexpr float pi = std::numbers::pi_v<float>;

  for (int i = 0; i <= segments_along; ++i) {
    float t = static_cast<float>(i) / segments_along;

    for (int j = 0; j <= segments_around; ++j) {
      float angle = static_cast<float>(j) / segments_around * 2.0F * pi;

      float x = std::cos(angle);
      float y = std::sin(angle);

      // Position: x,y define cross-section, z is position along beam
      Vertex v;
      v.position = {x, y, t};
      v.normal = {x, y, 0.0F};
      v.tex_coord = {static_cast<float>(j) / segments_around, t};
      vertices.push_back(v);
    }
  }

  // Generate indices for triangle strip
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
  if (!m_initialized || beam_system == nullptr ||
      beam_system->get_beam_count() == 0) {
    return;
  }

  // Enable blending for glow effect
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for glow
  glDepthMask(GL_FALSE);             // Don't write to depth buffer

  m_beam_shader->use();

  for (const auto &beam : beam_system->get_beams()) {
    if (beam && beam->is_active()) {
      render_beam(*beam, cam, animation_time);
    }
  }

  // Restore state
  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void HealingBeamRenderer::render_beam(const Game::Systems::HealingBeam &beam,
                                      const Camera &cam,
                                      float animation_time) {
  QMatrix4x4 model;
  model.setToIdentity();

  QMatrix4x4 mvp = cam.getProjectionMatrix() * cam.getViewMatrix() * model;

  m_beam_shader->setUniform(m_u_mvp, mvp);
  m_beam_shader->setUniform(m_u_model, model);
  m_beam_shader->setUniform(m_u_time, animation_time);
  m_beam_shader->setUniform(m_u_progress,
                            std::min(beam.get_progress(), 1.0F));
  m_beam_shader->setUniform(m_u_start_pos, beam.get_start());
  m_beam_shader->setUniform(m_u_end_pos, beam.get_end());
  m_beam_shader->setUniform(m_u_beam_width, beam.get_beam_width());
  m_beam_shader->setUniform(m_u_heal_color, beam.get_color());
  m_beam_shader->setUniform(m_u_alpha, beam.get_intensity());

  m_beam_mesh->draw();
}

// Static renderer instance for the free function
namespace {
HealingBeamRenderer &get_healing_beam_renderer() {
  static HealingBeamRenderer renderer;
  return renderer;
}
} // namespace

void render_healing_beams(
    Renderer *renderer, ResourceManager * /*resources*/,
    const Game::Systems::HealingBeamSystem &beam_system) {
  if (renderer == nullptr || beam_system.get_beam_count() == 0) {
    return;
  }

  auto *backend = renderer->backend();
  auto *camera = renderer->camera();
  if (backend == nullptr || camera == nullptr) {
    return;
  }

  auto &beam_renderer = get_healing_beam_renderer();
  if (!beam_renderer.initialize(backend)) {
    return;
  }

  beam_renderer.render(&beam_system, *camera, renderer->get_animation_time());
}

} // namespace Render::GL
