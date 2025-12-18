#include "camera.h"
#include "../../game/map/visibility_service.h"
#include "render_constants.h"
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <qglobal.h>
#include <qmath.h>
#include <qmatrix4x4.h>
#include <qnumeric.h>
#include <qpoint.h>
#include <qvectornd.h>

namespace Render::GL {

using namespace Render::GL::FrustumPlane;

namespace {
constexpr float k_eps = 1e-6F;
constexpr float k_tiny = 1e-4F;
constexpr float k_min_dist = 1.0F;
constexpr float k_max_dist = 200.0F;
constexpr float k_min_fov = 1.0F;
constexpr float k_max_fov = 89.0F;

constexpr float k_min_margin_percent = 0.03F;
constexpr float k_max_margin_percent = 0.10F;
constexpr float k_boundary_smoothness = 0.3F;

constexpr float k_reference_height = 50.0F;
constexpr float k_height_factor_min = 0.5F;
constexpr float k_height_factor_max = 2.0F;

constexpr float k_max_pitch_angle = 90.0F;
constexpr float k_pitch_factor_min = 0.5F;
constexpr float k_pitch_factor_max = 1.5F;

constexpr float k_max_ortho_scale = 20.0F;
constexpr float k_min_ortho_scale = 0.05F;
constexpr float k_zoom_delta_multiplier = 0.1F;
constexpr float k_zoom_distance_delta = 0.15F;
constexpr float k_zoom_factor_min = 0.1F;
constexpr float k_zoom_factor_max = 10.0F;

constexpr double k_ndc_scale = 2.0;
constexpr double k_ndc_offset = 1.0;
constexpr double k_ndc_half = 0.5;

constexpr float k_boundary_panning_smoothness = 0.7F;

inline auto finite(const QVector3D &v) -> bool {
  return qIsFinite(v.x()) && qIsFinite(v.y()) && qIsFinite(v.z());
}
inline auto finite(float v) -> bool { return qIsFinite(v); }

inline auto safeNormalize(const QVector3D &v, const QVector3D &fallback,
                          float eps = k_eps) -> QVector3D {
  if (!finite(v)) {
    return fallback;
  }
  float const len2 = v.lengthSquared();
  if (len2 < eps) {
    return fallback;
  }
  return v / std::sqrt(len2);
}

inline void orthonormalize(const QVector3D &frontIn, QVector3D &frontOut,
                           QVector3D &rightOut, QVector3D &upOut) {
  QVector3D const world_up(0.F, 1.F, 0.F);
  QVector3D const f = safeNormalize(frontIn, QVector3D(0, 0, -1));

  QVector3D u = (std::abs(QVector3D::dotProduct(f, world_up)) > 1.F - 1e-3F)
                    ? QVector3D(0, 0, 1)
                    : world_up;
  QVector3D r = QVector3D::crossProduct(f, u);
  if (r.lengthSquared() < k_eps) {
    r = QVector3D(1, 0, 0);
  }
  r = r.normalized();
  u = QVector3D::crossProduct(r, f).normalized();

  frontOut = f;
  rightOut = r;
  upOut = u;
}

inline void clampOrthoBox(float &left, float &right, float &bottom,
                          float &top) {
  if (left == right) {
    left -= 0.5F;
    right += 0.5F;
  } else if (left > right) {
    std::swap(left, right);
  }
  if (bottom == top) {
    bottom -= 0.5F;
    top += 0.5F;
  } else if (bottom > top) {
    std::swap(bottom, top);
  }
}

inline auto calculateDynamicMargin(float baseMargin, float camera_height,
                                   float pitch_deg) -> float {

  float const height_factor =
      std::clamp(camera_height / k_reference_height, k_height_factor_min,
                 k_height_factor_max);

  float const pitch_factor =
      std::clamp(1.0F - std::abs(pitch_deg) / k_max_pitch_angle,
                 k_pitch_factor_min, k_pitch_factor_max);

  return baseMargin * height_factor * pitch_factor;
}

inline auto smoothApproach(float current, float target,
                           float smoothness) -> float {
  if (std::abs(current - target) < k_tiny) {
    return target;
  }

  return current +
         (target - current) * std::clamp(1.0F - smoothness, 0.01F, 0.99F);
}

} 

Camera::Camera() { update_vectors(); }

void Camera::setPosition(const QVector3D &position) {
  if (!finite(position)) {
    return;
  }
  m_position = position;
  apply_soft_boundaries();

  QVector3D const new_front = (m_target - m_position);
  orthonormalize(new_front, m_front, m_right, m_up);
}

void Camera::setTarget(const QVector3D &target) {
  if (!finite(target)) {
    return;
  }
  m_target = target;
  apply_soft_boundaries();

  QVector3D dir = (m_target - m_position);
  if (dir.lengthSquared() < k_eps) {
    m_target =
        m_position +
        (m_front.lengthSquared() < k_eps ? QVector3D(0, 0, -1) : m_front);
    dir = (m_target - m_position);
  }
  orthonormalize(dir, m_front, m_right, m_up);
}

void Camera::setUp(const QVector3D &up) {
  if (!finite(up)) {
    return;
  }
  QVector3D up_n = up;
  if (up_n.lengthSquared() < k_eps) {
    up_n = QVector3D(0, 1, 0);
  }

  orthonormalize(m_target - m_position, m_front, m_right, m_up);
}

void Camera::lookAt(const QVector3D &position, const QVector3D &target,
                    const QVector3D &up) {
  if (!finite(position) || !finite(target) || !finite(up)) {
    return;
  }
  m_position = position;
  m_target = (position == target) ? position + QVector3D(0, 0, -1) : target;

  apply_soft_boundaries();

  QVector3D const f = (m_target - m_position);
  m_up = up.lengthSquared() < k_eps ? QVector3D(0, 1, 0) : up.normalized();
  orthonormalize(f, m_front, m_right, m_up);
}

void Camera::set_perspective(float fov, float aspect, float near_plane,
                             float far_plane) {
  if (!finite(fov) || !finite(aspect) || !finite(near_plane) ||
      !finite(far_plane)) {
    return;
  }

  m_isPerspective = true;

  m_fov = std::clamp(fov, k_min_fov, k_max_fov);
  m_aspect = std::max(aspect, 1e-6F);
  m_near_plane = std::max(near_plane, 1e-4F);
  m_far_plane = std::max(far_plane, m_near_plane + 1e-3F);
}

void Camera::set_orthographic(float left, float right, float bottom, float top,
                              float near_plane, float far_plane) {
  if (!finite(left) || !finite(right) || !finite(bottom) || !finite(top) ||
      !finite(near_plane) || !finite(far_plane)) {
    return;
  }

  m_isPerspective = false;
  clampOrthoBox(left, right, bottom, top);
  m_orthoLeft = left;
  m_orthoRight = right;
  m_orthoBottom = bottom;
  m_orthoTop = top;
  m_near_plane = std::min(near_plane, far_plane - 1e-3F);
  m_far_plane = std::max(far_plane, m_near_plane + 1e-3F);
}

void Camera::move_forward(float distance) {
  if (!finite(distance)) {
    return;
  }
  m_position += m_front * distance;
  m_target = m_position + m_front;
  apply_soft_boundaries();
}

void Camera::move_right(float distance) {
  if (!finite(distance)) {
    return;
  }
  m_position += m_right * distance;
  m_target = m_position + m_front;
  apply_soft_boundaries();
}

void Camera::move_up(float distance) {
  if (!finite(distance)) {
    return;
  }
  m_position += QVector3D(0, 1, 0) * distance;
  m_target = m_position + m_front;
  apply_soft_boundaries();
}

void Camera::zoom(float delta) {
  if (!finite(delta)) {
    return;
  }
  if (m_isPerspective) {
    m_fov = qBound(k_min_fov, m_fov - delta, k_max_fov);
  } else {
    float scale = 1.0F + delta * k_zoom_delta_multiplier;
    if (!finite(scale) || scale <= k_min_ortho_scale) {
      scale = k_min_ortho_scale;
    }
    if (scale > k_max_ortho_scale) {
      scale = k_max_ortho_scale;
    }
    m_orthoLeft *= scale;
    m_orthoRight *= scale;
    m_orthoBottom *= scale;
    m_orthoTop *= scale;
    clampOrthoBox(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop);
  }
}

void Camera::zoom_distance(float delta) {
  if (!finite(delta)) {
    return;
  }

  QVector3D const offset = m_position - m_target;
  float r = offset.length();
  if (r < k_tiny) {
    r = k_tiny;
  }

  float factor = 1.0F - delta * k_zoom_distance_delta;
  if (!finite(factor)) {
    factor = 1.0F;
  }
  factor = std::clamp(factor, k_zoom_factor_min, k_zoom_factor_max);

  float const new_r = std::clamp(r * factor, k_min_dist, k_max_dist);
  QVector3D const dir = safeNormalize(offset, QVector3D(0, 0, 1));
  QVector3D const new_pos = m_target + dir * new_r;

  m_position = new_pos;

  apply_soft_boundaries();

  QVector3D const f = (m_target - m_position);
  orthonormalize(f, m_front, m_right, m_up);
}

void Camera::rotate(float yaw, float pitch) { orbit(yaw, pitch); }

void Camera::pan(float right_dist, float forwardDist) {
  if (!finite(right_dist) || !finite(forwardDist)) {
    return;
  }

  QVector3D const right = m_right;
  QVector3D front = m_front;
  front.setY(0.0F);
  if (front.lengthSquared() > 0) {
    front.normalize();
  }

  QVector3D const delta = right * right_dist + front * forwardDist;
  if (!finite(delta)) {
    return;
  }

  m_position += delta;
  m_target += delta;

  apply_soft_boundaries(true);
}

void Camera::elevate(float dy) {
  if (!finite(dy)) {
    return;
  }
  m_position.setY(m_position.y() + dy);
  apply_soft_boundaries();
}

void Camera::yaw(float degrees) {
  if (!finite(degrees)) {
    return;
  }

  orbit(degrees, 0.0F);
}

void Camera::orbit(float yaw_deg, float pitch_deg) {
  if (!finite(yaw_deg) || !finite(pitch_deg)) {
    return;
  }

  QVector3D const offset = m_position - m_target;
  float cur_yaw = 0.F;
  float cur_pitch = 0.F;
  computeYawPitchFromOffset(offset, cur_yaw, cur_pitch);

  m_orbitStartYaw = cur_yaw;
  m_orbitStartPitch = cur_pitch;
  m_orbitTargetYaw = cur_yaw + yaw_deg;
  m_orbitTargetPitch =
      qBound(m_pitchMinDeg, cur_pitch + pitch_deg, m_pitchMaxDeg);
  m_orbitTime = 0.0F;
  m_orbitPending = true;
}

void Camera::update(float dt) {
  if (!m_orbitPending) {
    return;
  }
  if (!finite(dt)) {
    return;
  }

  m_orbitTime += std::max(0.0F, dt);
  float const t = (m_orbitDuration <= 0.0F)
                      ? 1.0F
                      : std::clamp(m_orbitTime / m_orbitDuration, 0.0F, 1.0F);

  float const s = t * t * (3.0F - 2.0F * t);

  float const new_yaw =
      m_orbitStartYaw + (m_orbitTargetYaw - m_orbitStartYaw) * s;
  float const new_pitch =
      m_orbitStartPitch + (m_orbitTargetPitch - m_orbitStartPitch) * s;

  QVector3D const offset = m_position - m_target;
  float r = offset.length();
  if (r < k_tiny) {
    r = k_tiny;
  }

  float const yaw_rad = qDegreesToRadians(new_yaw);
  float const pitch_rad = qDegreesToRadians(new_pitch);
  QVector3D const new_dir(std::sin(yaw_rad) * std::cos(pitch_rad),
                          std::sin(pitch_rad),
                          std::cos(yaw_rad) * std::cos(pitch_rad));

  QVector3D const fwd = safeNormalize(new_dir, m_front);
  m_position = m_target - fwd * r;

  apply_soft_boundaries();

  orthonormalize((m_target - m_position), m_front, m_right, m_up);

  if (t >= 1.0F) {
    m_orbitPending = false;
  }
}

auto Camera::screen_to_ground(qreal sx, qreal sy, qreal screenW, qreal screenH,
                              QVector3D &outWorld) const -> bool {
  if (screenW <= 0 || screenH <= 0) {
    return false;
  }
  if (!qIsFinite(sx) || !qIsFinite(sy)) {
    return false;
  }

  double const x = (k_ndc_scale * sx / screenW) - k_ndc_offset;
  double const y = k_ndc_offset - (k_ndc_scale * sy / screenH);

  bool ok = false;
  QMatrix4x4 const inv_vp =
      (get_projection_matrix() * get_view_matrix()).inverted(&ok);
  if (!ok) {
    return false;
  }

  QVector4D const near_clip(float(x), float(y), 0.0F, 1.0F);
  QVector4D const far_clip(float(x), float(y), 1.0F, 1.0F);
  QVector4D const near_world4 = inv_vp * near_clip;
  QVector4D const far_world4 = inv_vp * far_clip;

  if (std::abs(near_world4.w()) < k_eps || std::abs(far_world4.w()) < k_eps) {
    return false;
  }

  QVector3D const ray_origin = (near_world4 / near_world4.w()).toVector3D();
  QVector3D const ray_end = (far_world4 / far_world4.w()).toVector3D();
  if (!finite(ray_origin) || !finite(ray_end)) {
    return false;
  }

  QVector3D const ray_dir =
      safeNormalize(ray_end - ray_origin, QVector3D(0, -1, 0));
  if (std::abs(ray_dir.y()) < k_eps) {
    return false;
  }

  float const t = (m_ground_y - ray_origin.y()) / ray_dir.y();
  if (!finite(t) || t < 0.0F) {
    return false;
  }

  outWorld = ray_origin + ray_dir * t;
  return finite(outWorld);
}

auto Camera::world_to_screen(const QVector3D &world, qreal screenW,
                             qreal screenH, QPointF &outScreen) const -> bool {
  if (screenW <= 0 || screenH <= 0) {
    return false;
  }
  if (!finite(world)) {
    return false;
  }

  QVector4D const clip =
      get_projection_matrix() * get_view_matrix() * QVector4D(world, 1.0F);
  if (std::abs(clip.w()) < k_eps) {
    return false;
  }

  QVector3D const ndc = (clip / clip.w()).toVector3D();
  if (!qIsFinite(ndc.x()) || !qIsFinite(ndc.y()) || !qIsFinite(ndc.z())) {
    return false;
  }
  if (ndc.z() < -1.0F || ndc.z() > 1.0F) {
    return false;
  }

  qreal const sx = (ndc.x() * k_ndc_half + k_ndc_half) * screenW;
  qreal const sy =
      (k_ndc_offset - (ndc.y() * k_ndc_half + k_ndc_half)) * screenH;
  outScreen = QPointF(sx, sy);
  return qIsFinite(sx) && qIsFinite(sy);
}

void Camera::update_follow(const QVector3D &targetCenter) {
  if (!m_followEnabled) {
    return;
  }
  if (!finite(targetCenter)) {
    return;
  }

  if (m_followOffset.lengthSquared() < 1e-5F) {
    m_followOffset = m_position - m_target;
  }
  QVector3D const desired_pos = targetCenter + m_followOffset;
  QVector3D const new_pos =
      (m_followLerp >= 0.999F)
          ? desired_pos
          : (m_position +
             (desired_pos - m_position) * std::clamp(m_followLerp, 0.0F, 1.0F));

  if (!finite(new_pos)) {
    return;
  }

  m_target = targetCenter;
  m_position = new_pos;

  apply_soft_boundaries();

  orthonormalize((m_target - m_position), m_front, m_right, m_up);
}

void Camera::set_rts_view(const QVector3D &center, float distance, float angle,
                          float yaw_deg) {
  if (!finite(center) || !finite(distance) || !finite(angle) ||
      !finite(yaw_deg)) {
    return;
  }

  m_target = center;

  distance = std::max(distance, 0.01F);
  float const pitch_rad = qDegreesToRadians(angle);
  float const yaw_rad = qDegreesToRadians(yaw_deg);

  float const y = distance * qSin(pitch_rad);
  float const horiz = distance * qCos(pitch_rad);

  float const x = std::sin(yaw_rad) * horiz;
  float const z = std::cos(yaw_rad) * horiz;

  m_position = center + QVector3D(x, y, z);

  QVector3D const f = (m_target - m_position);
  orthonormalize(f, m_front, m_right, m_up);

  apply_soft_boundaries();
}

void Camera::set_top_down_view(const QVector3D &center, float distance) {
  if (!finite(center) || !finite(distance)) {
    return;
  }

  m_target = center;
  m_position = center + QVector3D(0, std::max(distance, 0.01F), 0);
  m_up = QVector3D(0, 0, -1);
  m_front = safeNormalize((m_target - m_position), QVector3D(0, 0, 1));
  update_vectors();

  apply_soft_boundaries();
}

auto Camera::get_view_matrix() const -> QMatrix4x4 {
  QMatrix4x4 view;
  view.lookAt(m_position, m_target, m_up);
  return view;
}

auto Camera::get_projection_matrix() const -> QMatrix4x4 {
  QMatrix4x4 projection;
  if (m_isPerspective) {
    projection.perspective(m_fov, m_aspect, m_near_plane, m_far_plane);
  } else {
    float left = m_orthoLeft;
    float right = m_orthoRight;
    float bottom = m_orthoBottom;
    float top = m_orthoTop;
    clampOrthoBox(left, right, bottom, top);
    projection.ortho(left, right, bottom, top, m_near_plane, m_far_plane);
  }
  return projection;
}

auto Camera::get_view_projection_matrix() const -> QMatrix4x4 {
  return get_projection_matrix() * get_view_matrix();
}

auto Camera::get_distance() const -> float {
  return (m_position - m_target).length();
}

auto Camera::get_pitch_deg() const -> float {
  QVector3D const off = m_position - m_target;
  QVector3D const dir = -off;
  if (dir.lengthSquared() < 1e-6F) {
    return 0.0F;
  }
  float const len_xz = std::sqrt(dir.x() * dir.x() + dir.z() * dir.z());
  float const pitch_rad = std::atan2(dir.y(), len_xz);
  return qRadiansToDegrees(pitch_rad);
}

void Camera::update_vectors() {
  QVector3D const f = (m_target - m_position);
  orthonormalize(f, m_front, m_right, m_up);
}

void Camera::apply_soft_boundaries(bool isPanning) {
  if (!qIsFinite(m_position.y())) {
    return;
  }

  if (m_position.y() < m_ground_y + m_min_height) {
    m_position.setY(m_ground_y + m_min_height);
  }

  auto &vis = Game::Map::VisibilityService::instance();
  if (!vis.is_initialized()) {
    return;
  }

  const float tile = vis.getTileSize();
  const float half_w = vis.getWidth() * 0.5F - 0.5F;
  const float half_h = vis.getHeight() * 0.5F - 0.5F;

  if (tile <= 0.0F || half_w < 0.0F || half_h < 0.0F) {
    return;
  }

  const float map_min_x = -half_w * tile;
  const float map_max_x = half_w * tile;
  const float map_min_z = -half_h * tile;
  const float map_max_z = half_h * tile;

  float const camera_height = m_position.y() - m_ground_y;
  float const pitch_deg = get_pitch_deg();

  float const map_width = map_max_x - map_min_x;
  float const map_depth = map_max_z - map_min_z;
  float const base_margin_x =
      map_width * std::lerp(k_min_margin_percent, k_max_margin_percent,
                            std::min(camera_height / k_reference_height, 1.0F));
  float const base_margin_z =
      map_depth * std::lerp(k_min_margin_percent, k_max_margin_percent,
                            std::min(camera_height / k_reference_height, 1.0F));

  float const margin_x =
      calculateDynamicMargin(base_margin_x, camera_height, pitch_deg);
  float const margin_z =
      calculateDynamicMargin(base_margin_z, camera_height, pitch_deg);

  float const ext_min_x = map_min_x - margin_x;
  float const ext_max_x = map_max_x + margin_x;
  float const ext_min_z = map_min_z - margin_z;
  float const ext_max_z = map_max_z + margin_z;

  QVector3D const target_to_pos = m_position - m_target;
  float const target_to_pos_dist = target_to_pos.length();

  QVector3D position_adjustment(0, 0, 0);
  QVector3D target_adjustment(0, 0, 0);

  if (m_position.x() < ext_min_x) {
    position_adjustment.setX(ext_min_x - m_position.x());
  } else if (m_position.x() > ext_max_x) {
    position_adjustment.setX(ext_max_x - m_position.x());
  }

  if (m_position.z() < ext_min_z) {
    position_adjustment.setZ(ext_min_z - m_position.z());
  } else if (m_position.z() > ext_max_z) {
    position_adjustment.setZ(ext_max_z - m_position.z());
  }

  if (m_target.x() < map_min_x) {
    target_adjustment.setX(map_min_x - m_target.x());
  } else if (m_target.x() > map_max_x) {
    target_adjustment.setX(map_max_x - m_target.x());
  }

  if (m_target.z() < map_min_z) {
    target_adjustment.setZ(map_min_z - m_target.z());
  } else if (m_target.z() > map_max_z) {
    target_adjustment.setZ(map_max_z - m_target.z());
  }

  if (isPanning) {

    if ((position_adjustment.x() > 0 && m_lastPosition.x() < m_position.x()) ||
        (position_adjustment.x() < 0 && m_lastPosition.x() > m_position.x())) {
      position_adjustment.setX(0);
    }

    if ((position_adjustment.z() > 0 && m_lastPosition.z() < m_position.z()) ||
        (position_adjustment.z() < 0 && m_lastPosition.z() > m_position.z())) {
      position_adjustment.setZ(0);
    }
  }

  if (!position_adjustment.isNull()) {
    m_position +=
        position_adjustment *
        (isPanning ? k_boundary_panning_smoothness : k_boundary_smoothness);
  }

  if (!target_adjustment.isNull()) {
    m_target += target_adjustment * (isPanning ? k_boundary_panning_smoothness
                                               : k_boundary_smoothness);

    if (target_to_pos_dist > k_tiny) {
      QVector3D const dir = target_to_pos.normalized();
      m_position = m_target + dir * target_to_pos_dist;
    }
  }

  m_lastPosition = m_position;
}

void Camera::clamp_above_ground() {
  if (!qIsFinite(m_position.y())) {
    return;
  }

  if (m_position.y() < m_ground_y + m_min_height) {
    m_position.setY(m_ground_y + m_min_height);
  }
}

void Camera::computeYawPitchFromOffset(const QVector3D &off, float &yaw_deg,
                                       float &pitch_deg) {
  QVector3D const dir = -off;
  if (dir.lengthSquared() < 1e-6F) {
    yaw_deg = 0.F;
    pitch_deg = 0.F;
    return;
  }
  float const yaw = qRadiansToDegrees(std::atan2(dir.x(), dir.z()));
  float const len_xz = std::sqrt(dir.x() * dir.x() + dir.z() * dir.z());
  float const pitch = qRadiansToDegrees(std::atan2(dir.y(), len_xz));
  yaw_deg = yaw;
  pitch_deg = pitch;
}

auto Camera::is_in_frustum(const QVector3D &center,
                           float radius) const -> bool {

  QMatrix4x4 const vp = get_view_projection_matrix();

  float m[MatrixSize];
  const float *data = vp.constData();
  for (int i = 0; i < MatrixSize; ++i) {
    m[i] = data[i];
  }

  QVector3D const left_n(m[Index3] + m[Index0], m[Index7] + m[Index4],
                         m[Index11] + m[Index8]);
  float const left_d = m[15] + m[12];

  QVector3D const right_n(m[Index3] - m[Index0], m[Index7] - m[Index4],
                          m[Index11] - m[Index8]);
  float const right_d = m[15] - m[12];

  QVector3D const bottom_n(m[Index3] + m[Index1], m[Index7] + m[Index5],
                           m[Index11] + m[Index9]);
  float const bottom_d = m[15] + m[13];

  QVector3D const top_n(m[Index3] - m[Index1], m[Index7] - m[Index5],
                        m[Index11] - m[Index9]);
  float const top_d = m[15] - m[13];

  QVector3D const near_n(m[Index3] + m[Index2], m[Index7] + m[Index6],
                         m[Index11] + m[Index10]);
  float const near_d = m[15] + m[14];

  QVector3D const far_n(m[Index3] - m[Index2], m[Index7] - m[Index6],
                        m[Index11] - m[Index10]);
  float const far_d = m[15] - m[14];

  auto test_plane = [&center, radius](const QVector3D &n, float d) -> bool {
    float const len = n.length();
    if (len < 1e-6F) {
      return true;
    }
    float const dist = QVector3D::dotProduct(center, n) + d;
    return dist >= -radius * len;
  };

  return test_plane(left_n, left_d) && test_plane(right_n, right_d) &&
         test_plane(bottom_n, bottom_d) && test_plane(top_n, top_d) &&
         test_plane(near_n, near_d) && test_plane(far_n, far_d);
}

} 
