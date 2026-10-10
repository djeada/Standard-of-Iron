#include <QDebug>
#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QPainter>
#include <QVector2D>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "arena_casting.h"
#include "arena_viewport.h"
#include "game/map/terrain_noise.h"
#include "game/map/terrain_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/player_resource_registry.h"
#include "game/visuals/team_colors.h"
#include "render/profiling/frame_continuity_analyzer.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

auto ArenaViewport::casting_snapshot() const -> Arena::ArenaCastingSnapshot {
  Arena::ArenaCastingSnapshot snapshot;
  if (m_scenario_runner == nullptr) {
    return snapshot;
  }
  snapshot.valid = true;
  snapshot.elapsed_seconds = m_scenario_runner->elapsed_seconds();
  snapshot.decided = m_scenario_runner->battle_decided();
  const auto& economy = m_session.economy();
  for (const auto& side : m_scenario_runner->live_battle_sides()) {
    Arena::ArenaCastingSide cast;
    cast.census = side;
    cast.color = Game::Visuals::team_colorForOwner(side.owner_id);
    cast.gold = economy.get(side.owner_id, Game::Systems::ResourceType::Gold);
    cast.food = economy.get(side.owner_id, Game::Systems::ResourceType::Food);
    cast.wood = economy.get(side.owner_id, Game::Systems::ResourceType::Wood);
    cast.stone = economy.get(side.owner_id, Game::Systems::ResourceType::Stone);
    cast.iron = economy.get(side.owner_id, Game::Systems::ResourceType::Iron);
    snapshot.sides.push_back(std::move(cast));
  }
  return snapshot;
}

void ArenaViewport::set_capture_gameplay_ui(bool enabled, bool all_owners) {
  m_capture_gameplay_ui = enabled;
  m_capture_gameplay_ui_all_owners = enabled && all_owners;
}

void ArenaViewport::paint_capture_gameplay_ui(QImage& frame) const {
  if (!m_capture_gameplay_ui || m_camera == nullptr || frame.isNull() ||
      frame.width() <= 0 || frame.height() <= 0) {
    return;
  }
  const qreal frame_width = frame.width();
  const qreal frame_height = frame.height();
  const float ui_scale =
      static_cast<float>(std::clamp(frame_height / 1080.0, 0.6, 2.4));

  QPainter painter(&frame);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  m_feedback.draw(
      painter,
      [this, frame_width, frame_height](float x, float y, float z, QPointF& out) {
        return m_camera->world_to_screen(
            QVector3D(x, y, z), frame_width, frame_height, out);
      },
      ui_scale);
}

void ArenaViewport::set_promo_mode(bool enabled) {
  m_promo_mode = enabled;
  if (m_renderer != nullptr) {
    m_renderer->set_cinematic_mode(enabled);
  }
  if (enabled) {
    m_clean_capture = true;
    m_controls_overlay_visible = false;
    if (auto* selection = selection_system()) {
      selection->clear_selection();
    }

    if (m_camera != nullptr) {
      m_camera->clear_map_bounds();
    }
  }
  set_batch_fixed_step(m_batch_fixed_step);
}

void ArenaViewport::set_capture_resolution(int width, int height) {
  if (width == m_capture_width && height == m_capture_height) {
    return;
  }
  m_capture_width = std::max(0, width);
  m_capture_height = std::max(0, height);
  if (m_capture_target != nullptr && context() != nullptr && context()->isValid()) {
    makeCurrent();
    m_capture_target.reset();
    m_capture_preview_resolve.reset();
    doneCurrent();
  } else {
    m_capture_target.reset();
    m_capture_preview_resolve.reset();
  }
}

void ArenaViewport::set_capture_sink(std::function<void(const QImage&)> sink) {
  m_capture_sink = std::move(sink);
}

void ArenaViewport::set_capture_active(bool active) {
  m_capture_active = active;
}

void ArenaViewport::set_frame_hook(std::function<void(float)> hook) {
  m_frame_hook = std::move(hook);
}

void ArenaViewport::set_cinematic_view(const QVector3D& target,
                                       float distance,
                                       float pitch_degrees,
                                       float yaw_degrees,
                                       float fov_degrees,
                                       float roll_degrees) {
  m_cinematic_target = target;
  m_cinematic_distance = std::max(0.2F, distance);
  m_cinematic_pitch = std::clamp(pitch_degrees, -89.0F, 89.0F);
  m_cinematic_yaw = yaw_degrees;
  m_cinematic_fov = std::clamp(fov_degrees, 5.0F, 120.0F);
  m_cinematic_roll = roll_degrees;
  m_cinematic_eye_valid = false;
  m_cinematic_view_valid = true;
}

void ArenaViewport::clear_cinematic_view() {
  m_cinematic_view_valid = false;
  m_cinematic_eye_valid = false;
}

void ArenaViewport::set_cinematic_eye(const QVector3D& eye,
                                      const QVector3D& target,
                                      float fov_degrees,
                                      float roll_degrees) {
  m_cinematic_eye = eye;
  m_cinematic_target = target;
  m_cinematic_fov = std::clamp(fov_degrees, 5.0F, 120.0F);
  m_cinematic_roll = roll_degrees;
  m_cinematic_eye_valid = true;
  m_cinematic_view_valid = true;
}

void ArenaViewport::set_cinematic_lens(float near_plane, float ground_clearance) {
  if (m_camera != nullptr) {
    if (m_cinematic_near <= 0.0F && near_plane > 0.0F) {
      m_saved_near = m_camera->get_near();
    } else if (m_cinematic_near > 0.0F && near_plane <= 0.0F && m_saved_near > 0.0F) {
      m_camera->set_perspective(m_camera->get_fov(),
                                m_camera->get_aspect(),
                                m_saved_near,
                                m_camera->get_far());
    }
  }
  m_cinematic_near = near_plane;
  m_cinematic_ground_clearance = ground_clearance;
}

void ArenaViewport::set_promo_lighting(const Arena::Promo::LightingOverride& lighting) {
  m_promo_lighting = lighting;
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
}

void ArenaViewport::set_capture_stabilization(float seconds) {
  m_capture_stabilize_seconds = std::max(0.0F, seconds);
  m_stabilized_lens_valid = false;
}

void ArenaViewport::apply_capture_stabilization(float dt) {
  if (m_capture_stabilize_seconds <= 0.0F || m_cinematic_view_valid ||
      m_camera == nullptr || dt <= 0.0F) {
    m_stabilized_lens_valid = false;
    return;
  }
  const QVector3D eye = m_camera->get_position();
  const QVector3D target = m_camera->get_target();
  const float fov = m_camera->get_fov();
  if (!m_stabilized_lens_valid) {
    m_stabilized_eye = eye;
    m_stabilized_target = target;
    m_stabilized_fov = fov;
    m_stabilized_lens_valid = true;
    return;
  }

  constexpr float k_target_share = 0.6F;
  const float eye_alpha = 1.0F - std::exp(-dt / m_capture_stabilize_seconds);
  const float target_alpha =
      1.0F - std::exp(-dt / (m_capture_stabilize_seconds * k_target_share));
  m_stabilized_eye += (eye - m_stabilized_eye) * eye_alpha;
  m_stabilized_target += (target - m_stabilized_target) * target_alpha;
  m_stabilized_fov += (fov - m_stabilized_fov) * eye_alpha;
  m_camera->look_at(m_stabilized_eye, m_stabilized_target, QVector3D(0.0F, 1.0F, 0.0F));
  m_camera->set_perspective(m_stabilized_fov,
                            m_camera->get_aspect(),
                            m_camera->get_near(),
                            m_camera->get_far());
}

namespace {

constexpr float k_cinematic_ground_clearance = 2.2F;
constexpr float k_cinematic_max_lift = 9.0F;
constexpr float k_cinematic_clear_fraction = 0.18F;
constexpr int k_cinematic_ray_samples = 10;

auto lift_camera_over_terrain(QVector3D position,
                              QVector3D const& target,
                              const Game::Map::TerrainService& terrain,
                              float ground_clearance = k_cinematic_ground_clearance)
    -> QVector3D {
  if (terrain.terrain_field().empty()) {
    return position;
  }
  float required = position.y();
  for (int sample = 0; sample <= k_cinematic_ray_samples; ++sample) {
    float const t = k_cinematic_clear_fraction * static_cast<float>(sample) /
                    static_cast<float>(k_cinematic_ray_samples);
    QVector3D const at = position * (1.0F - t) + target * t;
    float const clearance =
        terrain.get_terrain_height(at.x(), at.z()) + ground_clearance;
    required = std::max(required, (clearance - t * target.y()) / (1.0F - t));
  }
  position.setY(std::min(required, position.y() + k_cinematic_max_lift));
  return position;
}

} // namespace

void ArenaViewport::apply_cinematic_view() {
  if (!m_cinematic_view_valid || m_camera == nullptr) {
    return;
  }
  float const clearance = m_cinematic_ground_clearance >= 0.0F
                              ? m_cinematic_ground_clearance
                              : k_cinematic_ground_clearance;
  QVector3D position;
  if (m_cinematic_eye_valid) {
    position = lift_camera_over_terrain(
        m_cinematic_eye, m_cinematic_target, m_session.terrain(), clearance);
  } else {
    float const pitch = qDegreesToRadians(m_cinematic_pitch);
    float const yaw = qDegreesToRadians(m_cinematic_yaw);
    float const horizontal = m_cinematic_distance * std::cos(pitch);
    QVector3D const offset(std::sin(yaw) * horizontal,
                           m_cinematic_distance * std::sin(pitch),
                           std::cos(yaw) * horizontal);
    position = lift_camera_over_terrain(m_cinematic_target + offset,
                                        m_cinematic_target,
                                        m_session.terrain(),
                                        clearance);
  }

  QVector3D up(0.0F, 1.0F, 0.0F);
  if (std::abs(m_cinematic_roll) > 0.01F) {
    QVector3D const forward = (m_cinematic_target - position).normalized();
    QMatrix4x4 tilt;
    tilt.rotate(m_cinematic_roll, forward);
    up = tilt.map(up);
  }

  m_camera->look_at(position, m_cinematic_target, up);
  m_camera->set_perspective(m_cinematic_fov,
                            m_camera->get_aspect(),
                            m_cinematic_near > 0.0F ? m_cinematic_near
                                                    : m_camera->get_near(),
                            m_camera->get_far());
}

auto ArenaViewport::ensure_capture_target() -> bool {
  if (m_capture_width <= 0 || m_capture_height <= 0) {
    return false;
  }
  if (m_capture_target != nullptr && m_capture_target->width() == m_capture_width &&
      m_capture_target->height() == m_capture_height) {
    return true;
  }

  QOpenGLFramebufferObjectFormat format;
  format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
  format.setInternalTextureFormat(GL_RGBA8);
  format.setSamples(4);
  m_capture_target = std::make_unique<QOpenGLFramebufferObject>(
      m_capture_width, m_capture_height, format);
  if (!m_capture_target->isValid()) {
    format.setSamples(0);
    m_capture_target = std::make_unique<QOpenGLFramebufferObject>(
        m_capture_width, m_capture_height, format);
  }
  if (!m_capture_target->isValid()) {
    qWarning() << "ArenaViewport: could not create a" << m_capture_width << "x"
               << m_capture_height << "capture target";
    m_capture_target.reset();
    return false;
  }
  return true;
}

namespace {

constexpr char k_flame_card_vertex[] = R"(#version 330 core
out vec2 v_uv;
void main() {
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    v_uv = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr char k_flame_card_fragment[] = R"(#version 330 core
in vec2 v_uv;
out vec4 frag_colour;

uniform float u_time;
uniform float u_intensity;
uniform vec2 u_resolution;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float value_noise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(cell);
    float b = hash(cell + vec2(1.0, 0.0));
    float c = hash(cell + vec2(0.0, 1.0));
    float d = hash(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float sum = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave) {
        sum += amplitude * value_noise(p);
        p *= 2.02;
        amplitude *= 0.5;
    }
    return sum;
}

// Separate sheets give the fire depth: broad rolling flames behind sharper,
// faster tongues. Positive screen Y is up, so noise travels toward negative Y.
float flame_sheet(vec2 p, float time, float seed) {
    vec2 flow = p * vec2(5.2, 2.4) + vec2(seed, -time);
    vec2 curl = vec2(fbm(flow * 0.65 + 13.7),
                     fbm(flow * 0.65 + 39.1)) - 0.47;
    flow += curl * vec2(2.3, 1.1);
    float billow = fbm(flow);
    float lace = value_noise(flow * vec2(2.8, 1.7) + curl * 2.0);
    float fuel = value_noise(vec2(p.x * 3.6 + seed, time * 0.23));
    float envelope = 0.76 - p.y + (fuel - 0.5) * 0.24;
    return max(envelope + (billow - 0.48) * 1.2
               + (lace - 0.5) * 0.12, 0.0);
}

vec3 fire_colour(float heat) {
    vec3 colour = mix(vec3(0.65, 0.018, 0.001), vec3(2.8, 0.38, 0.015),
                      smoothstep(0.04, 0.28, heat));
    colour = mix(colour, vec3(4.5, 1.8, 0.28), smoothstep(0.24, 0.55, heat));
    colour = mix(colour, vec3(5.5, 4.2, 2.1), smoothstep(0.52, 0.85, heat));
    return colour * smoothstep(0.0, 0.12, heat);
}

void main() {
    vec2 uv = v_uv;
    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 p = vec2((uv.x - 0.5) * aspect, uv.y);
    float intensity = max(u_intensity, 0.0);
    float sides = smoothstep(0.0, 0.22, uv.x)
                * (1.0 - smoothstep(0.78, 1.0, uv.x));
    float edge_fuel = mix(0.55, 1.0, sides);
    float rear = flame_sheet(p, u_time * 0.72, 7.3) * edge_fuel;
    float front = flame_sheet(p * vec2(1.35, 1.15), u_time * 1.08, 31.8)
                * edge_fuel;

    // Warm light scatters into rolling smoke, with dark soot between tongues.
    vec2 smoke_flow = p * vec2(3.0, 2.2) + vec2(0.0, -u_time * 0.24);
    float smoke = fbm(smoke_flow + vec2(fbm(smoke_flow + 8.1), 0.0));
    float soot = smoothstep(0.38, 0.75, smoke) * smoothstep(0.15, 0.85, uv.y);
    vec3 colour = vec3(0.13, 0.029, 0.008) * exp(-uv.y * 3.4)
                * (0.6 + smoke) * edge_fuel;
    colour += fire_colour(rear) * 0.65 * exp(-soot * 2.5);
    float front_alpha = smoothstep(0.0, 0.22, front);
    colour = colour * (1.0 - front_alpha * 0.72) + fire_colour(front);
    colour += vec3(0.11, 0.065, 0.038) * soot * exp(-uv.y * 1.8);

    // Individually seeded embers rise, drift and cool. Two depth layers keep
    // them sparse; pixel-aware cores remain visible in supersampled captures.
    vec3 ember_light = vec3(0.0);
    float pixel = 1.0 / max(u_resolution.y, 1.0);
    for (int layer = 0; layer < 2; ++layer) {
        float depth = float(layer);
        float spacing = 0.12 + depth * 0.07;
        float column = floor(p.x / spacing);
        for (int neighbour = -1; neighbour <= 1; ++neighbour) {
            float id = column + float(neighbour);
            float seed = hash(vec2(id, depth + 19.0));
            float age = u_time * (0.18 + seed * 0.17) + seed * 13.0;
            float cycle = floor(age);
            float life = fract(age);
            float jitter = hash(vec2(id + cycle * 17.0, depth + 41.0));
            float x = (id + 0.5) * spacing
                    + sin(life * 7.0 + seed * 31.0) * spacing * 0.32;
            float y = -0.04 + life * (1.15 + jitter * 0.25);
            vec2 delta = p - vec2(x, y);
            float radius = max(pixel * 0.8, 0.0015 + jitter * 0.0018)
                         / (1.0 + depth * 0.5);
            vec2 core = delta / vec2(radius, radius * (2.0 + seed * 3.0));
            float spark = exp(-dot(core, core));
            float halo = exp(-dot(delta, delta) / (radius * radius * 22.0));
            float fade = smoothstep(0.0, 0.08, life)
                       * (1.0 - smoothstep(0.45, 1.0, life));
            ember_light += mix(vec3(3.8, 1.6, 0.35), vec3(0.7, 0.06, 0.003), life)
                         * (spark + halo * 0.12) * fade / (1.0 + depth);
        }
    }

    colour += ember_light;
    // Soft exposure retains orange detail in hot cores. Keep the upper field
    // dark enough for act titles, and avoid undefined reversed smoothstep edges.
    float vignette = 1.0 - smoothstep(0.35, 1.1, length((uv - 0.5) * vec2(1.1, 0.8)));
    colour = vec3(1.0) - exp(-colour * intensity * 0.85);
    frag_colour = vec4(colour * mix(0.65, 1.0, vignette), 1.0);
}
)";

} // namespace

auto ArenaViewport::ensure_flame_card_program() -> bool {
  if (m_flame_card_program != nullptr) {
    return m_flame_card_program->isLinked();
  }
  m_flame_card_program = std::make_unique<QOpenGLShaderProgram>();
  if (!m_flame_card_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                     k_flame_card_vertex) ||
      !m_flame_card_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                     k_flame_card_fragment) ||
      !m_flame_card_program->link()) {
    qWarning() << "ArenaViewport: flame card shader failed:"
               << m_flame_card_program->log();
    return false;
  }
  m_flame_card_vao = std::make_unique<QOpenGLVertexArrayObject>();
  if (!m_flame_card_vao->create()) {
    qWarning() << "ArenaViewport: could not create the flame card vertex array";
    return false;
  }
  return true;
}

void ArenaViewport::render_flame_card(int width, int height) {
  if (width <= 0 || height <= 0 || !ensure_flame_card_program()) {
    return;
  }
  auto* gl = context() != nullptr ? context()->extraFunctions() : nullptr;
  if (gl == nullptr) {
    return;
  }

  const float seconds =
      static_cast<float>(m_flame_card_frame) / 60.0F * m_flame_card_speed;

  gl->glViewport(0, 0, width, height);
  gl->glDisable(GL_DEPTH_TEST);
  gl->glDisable(GL_BLEND);
  gl->glDisable(GL_CULL_FACE);
  gl->glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_flame_card_program->bind();
  m_flame_card_program->setUniformValue("u_time", seconds);
  m_flame_card_program->setUniformValue("u_intensity", m_flame_card_intensity);
  m_flame_card_program->setUniformValue(
      "u_resolution", QVector2D(static_cast<float>(width), static_cast<float>(height)));
  m_flame_card_vao->bind();
  gl->glDrawArrays(GL_TRIANGLES, 0, 3);
  m_flame_card_vao->release();
  m_flame_card_program->release();
  gl->glEnable(GL_DEPTH_TEST);
}

void ArenaViewport::set_flame_card(bool enabled, float speed, float intensity) {
  if (enabled && !m_flame_card_active) {
    m_flame_card_frame = 0;
  }
  m_flame_card_active = enabled;
  m_flame_card_speed = speed;
  m_flame_card_intensity = intensity;
}

void ArenaViewport::present_capture_preview() {
  if (m_capture_target == nullptr || context() == nullptr) {
    return;
  }
  auto* gl = context()->extraFunctions();
  if (gl == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  float const capture_aspect =
      static_cast<float>(m_capture_width) / static_cast<float>(m_capture_height);
  float const widget_aspect =
      static_cast<float>(width()) / static_cast<float>(height());
  int fit_width = width();
  int fit_height = height();
  if (capture_aspect < widget_aspect) {
    fit_width =
        static_cast<int>(std::lround(static_cast<float>(height()) * capture_aspect));
  } else {
    fit_height =
        static_cast<int>(std::lround(static_cast<float>(width()) / capture_aspect));
  }
  int const offset_x = (width() - fit_width) / 2;
  int const offset_y = (height() - fit_height) / 2;

  gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
  gl->glViewport(0, 0, width(), height());
  gl->glDisable(GL_SCISSOR_TEST);
  gl->glClearColor(0.02F, 0.02F, 0.03F, 1.0F);
  gl->glClear(GL_COLOR_BUFFER_BIT);

  GLuint source = m_capture_target->handle();
  if (m_capture_target->format().samples() > 0) {
    if (m_capture_preview_resolve == nullptr ||
        m_capture_preview_resolve->size() != m_capture_target->size()) {
      m_capture_preview_resolve =
          std::make_unique<QOpenGLFramebufferObject>(m_capture_target->size());
    }
    QOpenGLFramebufferObject::blitFramebuffer(m_capture_preview_resolve.get(),
                                              m_capture_target.get());
    source = m_capture_preview_resolve->handle();
    gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
  }
  gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
  gl->glBlitFramebuffer(0,
                        0,
                        m_capture_width,
                        m_capture_height,
                        offset_x,
                        offset_y,
                        offset_x + fit_width,
                        offset_y + fit_height,
                        GL_COLOR_BUFFER_BIT,
                        GL_LINEAR);
  gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
}

void ArenaViewport::set_force_full_creature_lod(bool enabled) {
  m_force_full_creature_lod = enabled;
  if (m_renderer != nullptr) {
    m_renderer->set_force_full_creature_lod(enabled);
  }
}

void ArenaViewport::stamp_capture_alpha_opaque() {
  auto* functions = context() != nullptr ? context()->functions() : nullptr;
  if (functions == nullptr) {
    return;
  }

  const bool scissor_was_enabled = functions->glIsEnabled(GL_SCISSOR_TEST);
  if (scissor_was_enabled) {
    functions->glDisable(GL_SCISSOR_TEST);
  }
  functions->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
  functions->glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  functions->glClear(GL_COLOR_BUFFER_BIT);
  functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (scissor_was_enabled) {
    functions->glEnable(GL_SCISSOR_TEST);
  }
}

void ArenaViewport::sample_frame_continuity() {
  if (m_frame_continuity_analyzer == nullptr || m_scenario_runner == nullptr ||
      context() == nullptr || !context()->isValid()) {
    return;
  }
  auto* functions = context()->functions();
  if (functions == nullptr) {
    return;
  }

  GLint viewport[4] = {0, 0, 0, 0};
  functions->glGetIntegerv(GL_VIEWPORT, viewport);
  const int pixel_width = std::max(1, viewport[2]);
  const int pixel_height = std::max(1, viewport[3]);
  QImage frame(pixel_width, pixel_height, QImage::Format_RGBA8888);

  GLint previous_pack_alignment = 4;
  functions->glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
  functions->glPixelStorei(GL_PACK_ALIGNMENT, 1);
  functions->glReadPixels(
      0, 0, pixel_width, pixel_height, GL_RGBA, GL_UNSIGNED_BYTE, frame.bits());
  functions->glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);

  const auto issue = m_frame_continuity_analyzer->observe(frame);
  if (issue.has_value()) {
    m_scenario_runner->report_external_issue(QStringLiteral("fullscreen_flash"),
                                             issue->message());
  }
}

auto ArenaViewport::project_to_capture(const QVector3D& world,
                                       const QSize& frame_size,
                                       QPointF& out) const -> bool {
  return m_camera != nullptr && frame_size.width() > 0 && frame_size.height() > 0 &&
         m_camera->world_to_screen(world, frame_size.width(), frame_size.height(), out);
}

auto ArenaViewport::cinematic_state() const -> CinematicState {
  CinematicState state;
  state.view_valid = m_cinematic_view_valid;
  state.eye_valid = m_cinematic_eye_valid;
  state.target = m_cinematic_target;
  state.eye = m_cinematic_eye;
  state.distance = m_cinematic_distance;
  state.pitch = m_cinematic_pitch;
  state.yaw = m_cinematic_yaw;
  state.fov = m_cinematic_fov;
  state.roll = m_cinematic_roll;
  state.near_plane = m_cinematic_near;
  state.ground_clearance = m_cinematic_ground_clearance;
  return state;
}

void ArenaViewport::set_cinematic_state(const CinematicState& state) {
  m_cinematic_view_valid = state.view_valid;
  m_cinematic_eye_valid = state.eye_valid;
  m_cinematic_target = state.target;
  m_cinematic_eye = state.eye;
  m_cinematic_distance = state.distance;
  m_cinematic_pitch = state.pitch;
  m_cinematic_yaw = state.yaw;
  m_cinematic_fov = state.fov;
  m_cinematic_roll = state.roll;
  m_cinematic_near = state.near_plane;
  m_cinematic_ground_clearance = state.ground_clearance;
}

void ArenaViewport::set_capture_variants(std::vector<CaptureVariant> variants) {
  m_capture_variants = std::move(variants);
}

void ArenaViewport::set_capture_variant_lens(std::size_t index,
                                             const CinematicState& lens) {
  if (index < m_capture_variants.size()) {
    m_capture_variants[index].lens = lens;
  }
}

auto ArenaViewport::sample_capture_camera(int width, int height) const
    -> Arena::Promo::CameraSample {
  Arena::Promo::CameraSample sample;
  if (m_camera == nullptr) {
    return sample;
  }
  sample.valid = true;
  sample.view = m_camera->get_view_matrix();
  sample.projection = m_camera->get_projection_matrix();
  sample.eye = m_camera->get_position();
  sample.target = m_camera->get_target();
  sample.up = m_camera->get_up_vector();
  sample.fov_y = m_camera->get_fov();
  sample.aspect = m_camera->get_aspect();
  sample.near_plane = m_camera->get_near();
  sample.far_plane = m_camera->get_far();
  sample.render_width = width;
  sample.render_height = height;
  return sample;
}

void ArenaViewport::render_capture_variants(bool flame_card) {
  if (m_capture_variants.empty() || m_camera == nullptr || context() == nullptr) {
    return;
  }
  auto* gl = context()->functions();
  const CinematicState primary = cinematic_state();
  const QVector3D eye = m_camera->get_position();
  const QVector3D target = m_camera->get_target();
  const QVector3D up = m_camera->get_up_vector();
  const float fov = m_camera->get_fov();
  const float aspect = m_camera->get_aspect();
  const float near_plane = m_camera->get_near();
  const float far_plane = m_camera->get_far();
  const Arena::Promo::CameraSample primary_camera = m_capture_camera;

  if (m_variant_targets.size() != m_capture_variants.size()) {
    m_variant_targets.resize(m_capture_variants.size());
  }
  for (std::size_t index = 0; index < m_capture_variants.size(); ++index) {
    CaptureVariant& variant = m_capture_variants[index];
    if (variant.width <= 0 || variant.height <= 0 || !variant.sink) {
      continue;
    }
    auto& fbo = m_variant_targets[index];
    if (fbo == nullptr || fbo->width() != variant.width ||
        fbo->height() != variant.height) {
      QOpenGLFramebufferObjectFormat format;
      format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
      format.setInternalTextureFormat(GL_RGBA8);
      format.setSamples(4);
      fbo = std::make_unique<QOpenGLFramebufferObject>(
          variant.width, variant.height, format);
      if (!fbo->isValid()) {
        format.setSamples(0);
        fbo = std::make_unique<QOpenGLFramebufferObject>(
            variant.width, variant.height, format);
      }
      if (!fbo->isValid()) {
        qWarning() << "ArenaViewport: could not create a" << variant.width << "x"
                   << variant.height << "variant capture target";
        fbo.reset();
        continue;
      }
    }

    if (variant.lens.view_valid) {
      set_cinematic_state(variant.lens);
      apply_cinematic_view();
    } else {
      m_camera->look_at(eye, target, up);
      m_camera->set_perspective(fov, aspect, near_plane, far_plane);
    }
    fbo->bind();
    m_renderer->set_viewport(variant.width, variant.height);
    if (flame_card) {
      render_flame_card(variant.width, variant.height);
      m_capture_camera = {};
    } else {
      m_renderer->set_world_view(Render::WorldView::of(m_session));
      m_renderer->begin_frame();
      submit_terrain_layers();
      m_renderer->render_world(m_world.get());
      if (auto* res = m_renderer->resources(); res != nullptr) {
        submit_world_effects(res);
      }
      m_renderer->end_frame();
      m_capture_camera = sample_capture_camera(variant.width, variant.height);
    }
    stamp_capture_alpha_opaque();
    QImage const captured = fbo->toImage();
    fbo->release();
    gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    if (!captured.isNull()) {
      variant.sink(captured);
    }
  }

  set_cinematic_state(primary);
  m_camera->look_at(eye, target, up);
  m_camera->set_perspective(fov, aspect, near_plane, far_plane);
  m_renderer->set_viewport(m_capture_width, m_capture_height);
  m_camera->set_perspective(fov, aspect, near_plane, far_plane);
  m_capture_camera = primary_camera;
}
