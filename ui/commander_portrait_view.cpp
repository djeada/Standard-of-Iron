#include "commander_portrait_view.h"

#include <QDir>
#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>

#include "animation/rig/humanoid_proportions.h"
#include "animation/showcase_pose_manifest.h"
#include "commander_portrait_scenes.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/systems/nation_id.h"
#include "render/creature/pipeline/creature_bone_probe.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/gl/primitives.h"
#include "render/graphics_settings.h"
#include "render/humanoid/schema/skeleton_schema.h"
#include "render/palette.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "scene/environment_lighting.h"

namespace {

constexpr float k_field_of_view = 32.0F;

constexpr float k_bust_distance = 1.16F;
constexpr QVector3D k_bust_direction = QVector3D(0.30F, 0.09F, 0.95F);

constexpr float k_bust_drop = 0.075F;

constexpr float k_default_focus_height = 1.31F;

constexpr float k_focus_tau = 0.30F;

constexpr float k_max_frame_seconds = 0.05F;

constexpr float k_portrait_seed_tick_seconds = 1.0F / 60.0F;

constexpr int k_portrait_supersample = 2;
constexpr int k_portrait_max_target_edge = 1024;

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;

constexpr float k_head_radius = Render::GL::HumanProportions::HEAD_RADIUS;
constexpr float k_cranium_rise = k_head_radius * 0.06F;

constexpr float k_face_surface = k_head_radius * 2.02F;
constexpr float k_mouth_surface = k_head_radius * 2.20F;
constexpr float k_face_drop = k_head_radius * 0.80F;

constexpr float k_eye_shell_bias = k_head_radius * 0.035F;

constexpr float k_blink_seconds = 0.22F;
constexpr float k_blink_gap_min = 2.4F;
constexpr float k_blink_gap_span = 3.4F;
constexpr float k_double_blink_chance = 0.22F;
constexpr float k_gaze_hold_min = 0.9F;
constexpr float k_gaze_hold_span = 2.4F;
constexpr float k_gaze_centre_chance = 0.55F;
constexpr float k_gaze_tau = 0.07F;
constexpr float k_mouth_attack_tau = 0.030F;
constexpr float k_mouth_release_tau = 0.060F;
constexpr float k_brow_tau = 0.24F;

constexpr QVector3D k_eye_white{0.92F, 0.89F, 0.82F};
constexpr QVector3D k_roman_iris{0.36F, 0.21F, 0.085F};
constexpr QVector3D k_carthage_iris{0.24F, 0.30F, 0.21F};
constexpr QVector3D k_pupil{0.025F, 0.018F, 0.012F};
constexpr QVector3D k_catchlight{1.0F, 0.97F, 0.90F};
constexpr QVector3D k_brow{0.13F, 0.075F, 0.035F};
constexpr QVector3D k_mouth{0.16F, 0.055F, 0.035F};
constexpr QVector3D k_mouth_interior{0.085F, 0.030F, 0.024F};
constexpr QVector3D k_lip{0.38F, 0.16F, 0.11F};
constexpr QVector3D k_default_skin{0.78F, 0.62F, 0.49F};
constexpr float k_lid_shade = 0.90F;
constexpr float k_lid_visible_close = 0.02F;

struct PortraitDebug {
  bool trace = false;
  QString dump_dir;
  int dump_frames = 0;
  bool camera_override = false;
  float yaw_degrees = 0.0F;
  float distance = k_bust_distance;
  float focus_y = k_default_focus_height;
};

auto portrait_debug() -> const PortraitDebug& {
  static const PortraitDebug debug = [] {
    PortraitDebug out;
    out.trace = std::getenv("SOI_PORTRAIT_TRACE") != nullptr;
    if (const char* dump = std::getenv("SOI_PORTRAIT_DUMP")) {
      out.dump_dir = QString::fromUtf8(dump);
      out.dump_frames = 240;
      if (const char* frames = std::getenv("SOI_PORTRAIT_DUMP_FRAMES")) {
        out.dump_frames = std::max(1, std::atoi(frames));
      }
    }
    if (const char* camera = std::getenv("SOI_PORTRAIT_CAMERA")) {
      float yaw = 0.0F;
      float distance = k_bust_distance;
      float focus_y = k_default_focus_height;
      if (std::sscanf(camera, "%f,%f,%f", &yaw, &distance, &focus_y) >= 1) {
        out.camera_override = true;
        out.yaw_degrees = yaw;
        out.distance = distance;
        out.focus_y = focus_y;
      }
    }
    return out;
  }();
  return debug;
}

auto hash01(std::uint32_t n) -> float {
  n ^= n >> 16U;
  n *= 0x7feb352dU;
  n ^= n >> 15U;
  n *= 0x846ca68bU;
  n ^= n >> 16U;
  return static_cast<float>(n & 0xffffffU) / 16777216.0F;
}

auto ease_toward(float current, float target, float delta, float tau) -> float {
  if (delta <= 0.0F) {
    return current;
  }
  const float alpha = 1.0F - std::exp(-delta / std::max(tau, 1.0e-4F));
  return current + (target - current) * alpha;
}

auto raised_cosine(float u) -> float {
  if (u <= 0.0F || u >= 1.0F) {
    return 0.0F;
  }
  return 0.5F - 0.5F * std::cos(k_two_pi * u);
}

struct ExpressionState {
  float time = 0.0F;
  float mouth = 0.0F;
  float lid = 1.0F;
  float brow_lift = 0.0F;
  QVector2D gaze{};
  QVector2D gaze_target{};
  float next_blink_at = 1.6F;
  float blink_started_at = -1.0F;
  bool double_blink = false;
  std::uint32_t blink_index = 0;
  float next_gaze_at = 1.1F;
  std::uint32_t gaze_index = 0;

  void reset() { *this = ExpressionState{}; }

  void advance(float delta, bool talking) {
    time += std::max(0.0F, delta);
    advance_blink();
    advance_gaze(delta);
    advance_mouth(delta, talking);
  }

private:
  void advance_blink() {
    if (blink_started_at < 0.0F && time >= next_blink_at) {
      blink_started_at = time;
      ++blink_index;
      double_blink = hash01(blink_index * 7U + 3U) < k_double_blink_chance;
    }
    float close = 0.0F;
    if (blink_started_at >= 0.0F) {
      const float u = (time - blink_started_at) / k_blink_seconds;
      close = raised_cosine(u);
      const float second_start = 1.35F;
      if (double_blink) {
        close = std::max(close, raised_cosine(u - second_start) * 0.85F);
      }
      const float end = double_blink ? second_start + 1.0F : 1.0F;
      if (u >= end) {
        blink_started_at = -1.0F;
        next_blink_at =
            time + k_blink_gap_min + k_blink_gap_span * hash01(blink_index * 13U + 1U);
      }
    }
    lid = 1.0F - 0.88F * close;
  }

  void advance_gaze(float delta) {
    if (time >= next_gaze_at) {
      ++gaze_index;
      if (hash01(gaze_index * 5U + 2U) < k_gaze_centre_chance) {
        gaze_target = QVector2D(0.0F, 0.0F);
      } else {
        gaze_target = QVector2D((hash01(gaze_index * 3U) * 2.0F - 1.0F),
                                (hash01(gaze_index * 3U + 1U) * 2.0F - 1.0F) * 0.6F);
      }
      next_gaze_at =
          time + k_gaze_hold_min + k_gaze_hold_span * hash01(gaze_index * 11U + 4U);
    }
    gaze.setX(ease_toward(gaze.x(), gaze_target.x(), delta, k_gaze_tau));
    gaze.setY(ease_toward(gaze.y(), gaze_target.y(), delta, k_gaze_tau));
  }

  void advance_mouth(float delta, bool talking) {
    float target = 0.0F;
    float brow_target = 0.0F;
    if (talking) {
      const float t = time;
      const float wobble = 1.1F * std::sin(k_two_pi * 0.43F * t + 0.7F);
      const float syllable = 0.5F + 0.5F * std::sin(k_two_pi * 3.2F * t + wobble);
      const float word =
          0.35F + 0.65F * (0.5F + 0.5F * std::sin(k_two_pi * 0.85F * t + 2.1F));
      target = 0.06F + 0.80F * std::pow(syllable, 1.6F) * word;
      brow_target = 0.25F + 0.45F * word;
    }
    const float tau = target > mouth ? k_mouth_attack_tau : k_mouth_release_tau;
    mouth = ease_toward(mouth, target, delta, tau);
    brow_lift = ease_toward(brow_lift, brow_target, delta, k_brow_tau);
  }
};

auto face_shell_z(float shell_radius, float feature_y) -> float {
  const float inner = (shell_radius * shell_radius) - (feature_y * feature_y);
  return inner > 0.0F ? std::sqrt(inner) : 0.0F;
}

auto face_feature_model(const QMatrix4x4& head_world,
                        const QVector3D& centre,
                        const QVector3D& radii,
                        float roll_degrees = 0.0F) -> QMatrix4x4 {
  QMatrix4x4 model = head_world;
  model.translate(centre);
  if (std::abs(roll_degrees) > 1.0e-4F) {
    model.rotate(roll_degrees, 0.0F, 0.0F, 1.0F);
  }
  model.scale(radii.x(), radii.y(), radii.z());
  return model;
}

auto face_surface_frame(const QMatrix4x4& head_world,
                        const QVector3D& anchor) -> QMatrix4x4 {
  QVector3D forward = anchor;
  if (forward.lengthSquared() < 1.0e-8F) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  forward.normalize();
  QVector3D right = QVector3D::crossProduct(QVector3D(0.0F, 1.0F, 0.0F), forward);
  if (right.lengthSquared() < 1.0e-6F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right.normalize();
  const QVector3D up = QVector3D::crossProduct(forward, right);

  QMatrix4x4 frame;
  frame.setColumn(0, QVector4D(right, 0.0F));
  frame.setColumn(1, QVector4D(up, 0.0F));
  frame.setColumn(2, QVector4D(forward, 0.0F));
  QMatrix4x4 model = head_world;
  model.translate(anchor);
  return model * frame;
}

auto face_surface_model(const QMatrix4x4& surface_frame,
                        const QVector3D& offset,
                        const QVector3D& radii,
                        float roll_degrees = 0.0F) -> QMatrix4x4 {
  QMatrix4x4 model = surface_frame;
  model.translate(offset);
  if (std::abs(roll_degrees) > 1.0e-4F) {
    model.rotate(roll_degrees, 0.0F, 0.0F, 1.0F);
  }
  model.scale(radii.x(), radii.y(), radii.z());
  return model;
}

auto face_disc_model(const QMatrix4x4& surface_frame,
                     const QVector3D& offset,
                     float radius_x,
                     float radius_y,
                     float thickness) -> QMatrix4x4 {
  QMatrix4x4 model = surface_frame;
  model.translate(offset);
  model.rotate(90.0F, 1.0F, 0.0F, 0.0F);
  model.scale(radius_x, thickness, radius_y);
  return model;
}

auto portrait_lighting() -> Render::EnvironmentLightingState {
  Render::EnvironmentLightingState lighting;
  lighting.primary_direction = QVector3D(0.45F, 0.80F, 0.55F);
  lighting.primary_color = QVector3D(1.0F, 0.82F, 0.58F);
  lighting.primary_intensity = 1.15F;
  lighting.sky_color = QVector3D(0.055F, 0.048F, 0.042F);
  lighting.ground_bounce_color = QVector3D(0.12F, 0.08F, 0.05F);
  lighting.ambient_intensity = 0.44F;
  lighting.fog_density = 0.0F;
  lighting.shadow_tint = QVector3D(0.10F, 0.09F, 0.10F);
  lighting.shadow_strength = 0.5F;
  return lighting;
}

auto move_for_pose(const QString& pose) -> Animation::HumanoidShowcaseMove {
  if (pose == QStringLiteral("dismissive")) {
    return Animation::HumanoidShowcaseMove::TauntDismissive;
  }
  return Animation::HumanoidShowcaseMove::TauntCynical;
}

auto camera_direction() -> QVector3D {
  const auto& debug = portrait_debug();
  if (!debug.camera_override) {
    return k_bust_direction.normalized();
  }
  QMatrix4x4 yaw;
  yaw.rotate(debug.yaw_degrees, 0.0F, 1.0F, 0.0F);
  return yaw.map(k_bust_direction).normalized();
}

auto camera_distance() -> float {
  const auto& debug = portrait_debug();
  return debug.camera_override ? debug.distance : k_bust_distance;
}

auto review_lighting() -> Render::EnvironmentLightingState {
  Render::EnvironmentLightingState lighting = portrait_lighting();
  const auto& debug = portrait_debug();
  if (debug.camera_override) {
    QMatrix4x4 yaw;
    yaw.rotate(debug.yaw_degrees, 0.0F, 1.0F, 0.0F);
    lighting.primary_direction = yaw.map(lighting.primary_direction);
  }
  return lighting;
}

} // namespace

class CommanderPortraitView::PortraitRenderer
    : public QQuickFramebufferObject::Renderer {
public:
  PortraitRenderer() { UI::CommanderPortraitScenes::instance().add_reference(); }
  ~PortraitRenderer() override {
    release_scene();
    UI::CommanderPortraitScenes::instance().release_reference();
  }

  PortraitRenderer(const PortraitRenderer&) = delete;
  auto operator=(const PortraitRenderer&) -> PortraitRenderer& = delete;
  PortraitRenderer(PortraitRenderer&&) = delete;
  auto operator=(PortraitRenderer&&) -> PortraitRenderer& = delete;

  auto
  createFramebufferObject(const QSize& size) -> QOpenGLFramebufferObject* override {
    QSize target = size * k_portrait_supersample;
    target.setWidth(std::clamp(target.width(), 1, k_portrait_max_target_edge));
    target.setHeight(std::clamp(target.height(), 1, k_portrait_max_target_edge));
    m_size = target;

    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    const int samples =
        Render::GraphicsSettings::instance().presentation().msaa_samples;
    format.setSamples(samples);
    auto* fbo = new QOpenGLFramebufferObject(target, format);
    if (samples > 0 && !fbo->isValid()) {
      delete fbo;
      format.setSamples(0);
      fbo = new QOpenGLFramebufferObject(target, format);
    }
    return fbo;
  }

  void synchronize(QQuickFramebufferObject* item) override {
    auto* view = dynamic_cast<CommanderPortraitView*>(item);
    if (view == nullptr) {
      return;
    }
    const QString troop_type = view->troop_type();
    const QString pose = view->pose();
    m_nation = view->nation();
    m_speaking = view->speaking();
    m_talking = view->talking();

    qreal ratio = 1.0;
    if (auto* window = view->window()) {
      ratio = window->effectiveDevicePixelRatio();
    }
    QSize item_size(std::max<int>(1, static_cast<int>(view->width())),
                    std::max<int>(1, static_cast<int>(view->height())));
    item_size *= ratio;
    if (item_size != m_item_size) {
      const bool had_size = m_item_size.isValid();
      m_item_size = item_size;
      if (had_size) {
        invalidateFramebufferObject();
      }
    }

    if (troop_type != m_troop_type) {
      m_troop_type = troop_type;
      m_scene_dirty = true;
    }
    if (pose != m_pose) {
      m_pose = pose;
      m_pose_dirty = true;
    }
  }

  void render() override;

private:
  void release_scene();
  auto ensure_scene() -> bool;
  void apply_pose();
  [[nodiscard]] auto resolve_skin() const -> QVector3D;

  void submit_face(const QMatrix4x4& head_world);
  [[nodiscard]] auto advance_focus(float delta) -> QVector3D;
  void debug_after_frame(const Render::Creature::Pipeline::BoneProbe& probe);

  QSize m_size;
  QSize m_item_size;
  QVector3D m_focus{0.0F, k_default_focus_height, 0.0F};
  QVector3D m_focus_target{0.0F, k_default_focus_height, 0.0F};
  bool m_focus_settled = false;
  bool m_focus_locked = false;
  QString m_troop_type;
  QString m_nation;
  QString m_pose;
  QVector3D m_skin = k_default_skin;
  bool m_speaking = false;
  bool m_talking = false;
  bool m_scene_dirty = true;
  bool m_pose_dirty = true;

  ExpressionState m_expression;

  Render::GL::Renderer* m_renderer = nullptr;
  Render::GL::Camera* m_camera = nullptr;
  Engine::Core::World* m_world = nullptr;
  Engine::Core::EntityID m_entity = 0;
  bool m_renderer_failed = false;

  std::chrono::steady_clock::time_point m_last_frame{};
  int m_dumped_frames = 0;
  float m_wall_ms = 0.0F;
  float m_render_ms = 0.0F;
  QVector3D m_last_head{};
  bool m_has_last_head = false;
};

void CommanderPortraitView::PortraitRenderer::release_scene() {
  m_focus = QVector3D(0.0F, k_default_focus_height, 0.0F);
  m_focus_target = m_focus;
  m_focus_settled = false;
  m_focus_locked = false;
  m_world = nullptr;
  m_entity = 0;
  m_renderer = nullptr;
  m_camera = nullptr;
}

auto CommanderPortraitView::PortraitRenderer::ensure_scene() -> bool {
  if (m_renderer_failed) {
    return false;
  }
  if (m_scene_dirty) {
    m_entity = 0;
    m_world = nullptr;
    m_focus_settled = false;
    m_focus_locked = false;
  }

  auto& scenes = UI::CommanderPortraitScenes::instance();
  m_renderer = scenes.renderer();
  m_camera = scenes.camera();
  if (m_renderer == nullptr || m_camera == nullptr) {
    m_renderer_failed = true;
    return false;
  }

  if (m_world == nullptr) {
    const auto scene = scenes.acquire(m_troop_type);
    if (scene.world == nullptr) {
      return false;
    }
    m_world = scene.world;
    m_entity = scene.entity;
    m_pose_dirty = true;
    m_skin = resolve_skin();
  }

  m_scene_dirty = false;
  return true;
}

auto CommanderPortraitView::PortraitRenderer::resolve_skin() const -> QVector3D {
  if (m_world == nullptr) {
    return k_default_skin;
  }
  auto* entity = m_world->get_entity(m_entity);
  if (entity == nullptr) {
    return k_default_skin;
  }
  std::uint32_t seed = static_cast<std::uint32_t>(entity->get_id()) * 0x9E3779B9U;
  if (auto* unit = entity->get_component<Engine::Core::UnitComponent>()) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id) * 0x85EBCA6BU;
  }
  return Render::GL::make_humanoid_palette(QVector3D(0.5F, 0.5F, 0.5F), seed).skin;
}

void CommanderPortraitView::PortraitRenderer::apply_pose() {
  if (!m_pose_dirty || m_world == nullptr) {
    return;
  }
  auto* entity = m_world->get_entity(m_entity);
  if (entity == nullptr) {
    return;
  }
  auto* routine = entity->get_component<Engine::Core::ShowcaseRoutineComponent>();
  if (routine == nullptr) {
    return;
  }

  const auto move = move_for_pose(m_pose);
  routine->steps.clear();

  routine->steps.push_back(
      {.move = static_cast<std::uint8_t>(move), .duration = 0.0F, .hold_after = 0.0F});

  routine->index = 0;
  routine->elapsed = 0.0F;
  routine->phase = 0.0F;
  routine->finished = false;
  routine->active = true;
  routine->loop = true;
  routine->loop_from = 0U;
  m_pose_dirty = false;
  m_focus_locked = false;

  m_world->update(k_portrait_seed_tick_seconds);
}

void CommanderPortraitView::PortraitRenderer::submit_face(
    const QMatrix4x4& head_world) {
  if (m_renderer == nullptr) {
    return;
  }

  auto* sphere = Render::GL::get_unit_sphere();
  if (sphere == nullptr) {
    return;
  }
  auto* disc = Render::GL::get_unit_cylinder();
  if (disc == nullptr) {
    disc = sphere;
  }

  const float lid_close = std::clamp((1.0F - m_expression.lid) / 0.88F, 0.0F, 1.0F);
  const float mouth_open = std::clamp(m_expression.mouth, 0.0F, 1.0F);
  const QVector2D gaze(m_expression.gaze.x() * k_head_radius * 0.075F,
                       m_expression.gaze.y() * k_head_radius * 0.050F);
  const float brow_rise = m_expression.brow_lift * k_head_radius * 0.030F;

  const QVector3D iris =
      m_nation == QStringLiteral("carthage") ? k_carthage_iris : k_roman_iris;
  const float eye_y = (k_head_radius * 0.12F) - k_face_drop;
  const float brow_y = (k_head_radius * 0.47F) - k_face_drop + brow_rise;
  const float eye_z = face_shell_z(k_face_surface, eye_y);
  const float brow_z = face_shell_z(k_face_surface, brow_y);
  for (float side : {-1.0F, 1.0F}) {
    const float eye_x = side * k_head_radius * 0.48F;

    const QMatrix4x4 eye_frame =
        face_surface_frame(head_world, QVector3D(eye_x, eye_y, eye_z));
    m_renderer->mesh(disc,
                     face_disc_model(eye_frame,
                                     QVector3D(0.0F, 0.0F, k_eye_shell_bias),
                                     k_head_radius * 0.33F,
                                     k_head_radius * 0.27F,
                                     k_head_radius * 0.012F),
                     k_eye_white);
    const QVector3D gaze_offset(gaze.x(), gaze.y(), 0.0F);
    m_renderer->mesh(
        disc,
        face_disc_model(
            eye_frame,
            gaze_offset +
                QVector3D(0.0F, 0.0F, k_eye_shell_bias + (k_head_radius * 0.010F)),
            k_head_radius * 0.155F,
            k_head_radius * 0.165F,
            k_head_radius * 0.010F),
        iris);
    m_renderer->mesh(
        disc,
        face_disc_model(
            eye_frame,
            gaze_offset +
                QVector3D(0.0F, 0.0F, k_eye_shell_bias + (k_head_radius * 0.018F)),
            k_head_radius * 0.075F,
            k_head_radius * 0.090F,
            k_head_radius * 0.008F),
        k_pupil);
    m_renderer->mesh(
        disc,
        face_disc_model(eye_frame,
                        gaze_offset +
                            QVector3D(k_head_radius * 0.080F,
                                      k_head_radius * 0.082F,
                                      k_eye_shell_bias + (k_head_radius * 0.026F)),
                        k_head_radius * 0.038F,
                        k_head_radius * 0.038F,
                        k_head_radius * 0.008F),
        k_catchlight);

    if (lid_close > k_lid_visible_close) {
      const float lid_height = k_head_radius * 0.31F;
      const float lid_rest = lid_height * 2.0F;
      m_renderer->mesh(
          disc,
          face_disc_model(eye_frame,
                          QVector3D(0.0F,
                                    lid_rest * (1.0F - lid_close),
                                    k_eye_shell_bias + (k_head_radius * 0.040F)),
                          k_head_radius * 0.37F,
                          lid_height,
                          k_head_radius * 0.008F),
          m_skin * k_lid_shade);
    }

    const float brow_roll =
        side * (m_pose == QStringLiteral("dismissive") ? 5.0F : -3.0F);
    const QMatrix4x4 brow_frame =
        face_surface_frame(head_world, QVector3D(eye_x, brow_y, brow_z));
    m_renderer->mesh(sphere,
                     face_surface_model(brow_frame,
                                        QVector3D(0.0F, 0.0F, k_head_radius * 0.015F),
                                        QVector3D(k_head_radius * 0.28F,
                                                  k_head_radius * 0.045F,
                                                  k_head_radius * 0.032F),
                                        brow_roll),
                     k_brow);
  }

  const float mouth_height = k_head_radius * (0.025F + 0.14F * mouth_open);
  const float mouth_y = (-k_head_radius * 0.43F) - k_face_drop;
  const float lip_y = (-k_head_radius * (0.46F + 0.10F * mouth_open)) - k_face_drop;
  const QVector3D mouth_colour =
      k_mouth * (1.0F - mouth_open) + k_mouth_interior * mouth_open;
  m_renderer->mesh(
      sphere,
      face_feature_model(
          head_world,
          QVector3D(0.0F, mouth_y, face_shell_z(k_mouth_surface, mouth_y)),
          QVector3D(k_head_radius * 0.39F, mouth_height, k_head_radius * 0.035F)),
      mouth_colour);
  m_renderer->mesh(
      sphere,
      face_feature_model(
          head_world,
          QVector3D(0.0F,
                    lip_y,
                    face_shell_z(k_mouth_surface, lip_y) + (k_head_radius * 0.040F)),
          QVector3D(
              k_head_radius * 0.34F, k_head_radius * 0.035F, k_head_radius * 0.020F)),
      k_lip);
}

auto CommanderPortraitView::PortraitRenderer::advance_focus(float delta) -> QVector3D {
  if (!m_focus_settled) {
    m_focus = m_focus_target;
    m_focus_settled = true;
    return m_focus;
  }
  float const alpha = delta > 0.0F ? 1.0F - std::exp(-delta / k_focus_tau) : 0.0F;
  m_focus += (m_focus_target - m_focus) * alpha;
  return m_focus;
}

void CommanderPortraitView::PortraitRenderer::debug_after_frame(
    const Render::Creature::Pipeline::BoneProbe& probe) {
  const auto& debug = portrait_debug();
  if (debug.trace && probe.resolved) {
    const QVector3D head = probe.world.column(3).toVector3D();
    float step_mm = 0.0F;
    if (m_has_last_head) {
      step_mm = (head - m_last_head).length() * 1000.0F;
    }
    m_last_head = head;
    m_has_last_head = true;
    std::fprintf(stderr,
                 "SOI_PORTRAIT_TRACE t=%.3f wall_ms=%.1f render_ms=%.1f "
                 "head=(%.4f,%.4f,%.4f) step_mm=%.2f mouth=%.2f lid=%.2f\n",
                 static_cast<double>(m_expression.time),
                 static_cast<double>(m_wall_ms),
                 static_cast<double>(m_render_ms),
                 static_cast<double>(head.x()),
                 static_cast<double>(head.y()),
                 static_cast<double>(head.z()),
                 static_cast<double>(step_mm),
                 static_cast<double>(m_expression.mouth),
                 static_cast<double>(m_expression.lid));
  }
  if (!debug.dump_dir.isEmpty() && m_dumped_frames < debug.dump_frames) {
    if (auto* fbo = framebufferObject()) {
      QDir().mkpath(debug.dump_dir);
      const QString path = QStringLiteral("%1/portrait_%2.png")
                               .arg(debug.dump_dir)
                               .arg(m_dumped_frames, 4, 10, QLatin1Char('0'));
      fbo->toImage().save(path);
      ++m_dumped_frames;
    }
  }
}

void CommanderPortraitView::PortraitRenderer::render() {
  const auto now = std::chrono::steady_clock::now();
  float delta = 0.0F;
  if (m_last_frame.time_since_epoch().count() != 0) {
    delta = std::clamp(std::chrono::duration<float>(now - m_last_frame).count(),
                       0.0F,
                       k_max_frame_seconds);
    m_wall_ms = std::chrono::duration<float, std::milli>(now - m_last_frame).count();
  }
  m_last_frame = now;

  if (!m_speaking) {
    m_expression.reset();
    m_has_last_head = false;
    return;
  }

  Render::Creature::RuntimeBakeAllowScope const allow_bakes;

  if (!ensure_scene()) {
    return;
  }
  apply_pose();

  if (delta > 0.0F) {
    m_world->update(delta);
  }
  m_expression.advance(delta, m_talking);

  const float aspect = m_size.height() > 0 ? static_cast<float>(m_size.width()) /
                                                 static_cast<float>(m_size.height())
                                           : 1.0F;
  const QVector3D focus = advance_focus(delta);
  m_camera->look_at(focus + (camera_direction() * camera_distance()),
                    focus,
                    QVector3D(0.0F, 1.0F, 0.0F));
  m_camera->set_perspective(k_field_of_view, aspect, 0.05F, 40.0F);

  m_renderer->set_camera(m_camera);
  m_renderer->set_viewport(m_size.width(), m_size.height());
  m_renderer->set_environment_lighting(review_lighting());

  m_renderer->set_clear_color(0.055F, 0.048F, 0.042F, 1.0F);
  m_renderer->set_local_owner_id(1);
  m_renderer->set_force_full_creature_lod(true);
  m_renderer->update_animation_time(delta);
  Render::Creature::Pipeline::BoneProbe head_probe{};
  head_probe.entity_id = static_cast<std::uint32_t>(m_entity);
  head_probe.instance_index = 0U;
  head_probe.bone_index =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);

  m_renderer->begin_frame();
  {
    Render::Creature::Pipeline::ScopedBoneProbe const probe_scope(&head_probe);
    m_renderer->render_world(m_world);
  }

  if (head_probe.resolved) {
    submit_face(head_probe.world);
    if (!m_focus_locked) {
      QVector3D const head =
          head_probe.world.map(QVector3D(0.0F, k_cranium_rise, 0.0F));
      m_focus_target = QVector3D(head.x(), head.y() - k_bust_drop, head.z());
      if (portrait_debug().camera_override) {
        m_focus_target.setY(portrait_debug().focus_y);
      }
      m_focus_locked = true;
    }
  }
  m_renderer->end_frame();

  m_render_ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - now)
          .count();
  debug_after_frame(head_probe);

  update();
}

CommanderPortraitView::CommanderPortraitView() {
  setMirrorVertically(true);
  setTextureFollowsItemSize(false);
}

CommanderPortraitView::~CommanderPortraitView() = default;

auto CommanderPortraitView::createRenderer() const
    -> QQuickFramebufferObject::Renderer* {
  return new PortraitRenderer();
}

void CommanderPortraitView::set_troop_type(const QString& value) {
  if (m_troop_type == value) {
    return;
  }
  m_troop_type = value;
  emit troop_type_changed();
  update();
}

void CommanderPortraitView::set_nation(const QString& value) {
  if (m_nation == value) {
    return;
  }
  m_nation = value;
  emit nation_changed();
  update();
}

void CommanderPortraitView::set_pose(const QString& value) {
  if (m_pose == value) {
    return;
  }
  m_pose = value;
  emit pose_changed();
  update();
}

void CommanderPortraitView::set_speaking(bool value) {
  if (m_speaking == value) {
    return;
  }
  m_speaking = value;
  emit speaking_changed();
  update();
}

void CommanderPortraitView::set_talking(bool value) {
  if (m_talking == value) {
    return;
  }
  m_talking = value;
  emit talking_changed();
  update();
}
