#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>

#include <array>
#include <mutex>

namespace Render::GL {

namespace CameraDefaults {
inline constexpr float k_default_rts_distance = 10.0F;
inline constexpr float k_default_rts_angle = 45.0F;
inline constexpr float k_default_rts_yaw = 45.0F;
inline constexpr float k_default_fov = 45.0F;
inline constexpr float k_default_aspect_ratio = 16.0F / 9.0F;
inline constexpr float k_default_far_plane = 200.0F;
inline constexpr float k_default_ortho_size = 10.0F;
inline constexpr float k_default_pitch_min = -85.0F;
} // namespace CameraDefaults

class Camera {
  friend void solve_constraints(Render::GL::Camera* self, bool allow_target_shift);

public:
  Camera();

  void set_position(const QVector3D& position);
  void set_target(const QVector3D& target);
  void set_up(const QVector3D& up);
  void look_at(const QVector3D& position, const QVector3D& target, const QVector3D& up);

  void set_perspective(float fov, float aspect, float near_plane, float far_plane);
  void set_orthographic(float left,
                        float right,
                        float bottom,
                        float top,
                        float near_plane,
                        float far_plane);

  void move_forward(float distance);
  void move_right(float distance);
  void move_up(float distance);
  void zoom(float delta);

  void zoom_distance(float delta);
  void zoom_distance(float delta, float min_distance, float max_distance);
  void rotate(float yaw, float pitch);

  void pan(float right_dist, float forward_dist);

  void elevate(float dy);
  void yaw(float degrees);
  void orbit(float yaw_deg, float pitch_deg);

  void update(float dt);
  auto screen_to_ground(qreal sx,
                        qreal sy,
                        qreal screen_w,
                        qreal screen_h,
                        QVector3D& out_world) const -> bool;
  auto screen_to_world_ray(qreal sx,
                           qreal sy,
                           qreal screen_w,
                           qreal screen_h,
                           QVector3D& out_origin,
                           QVector3D& out_direction) const -> bool;
  auto world_to_screen(const QVector3D& world,
                       qreal screen_w,
                       qreal screen_h,
                       QPointF& out_screen) const -> bool;

  void set_follow_enabled(bool enable) { m_follow_enabled = enable; }
  [[nodiscard]] auto is_follow_enabled() const -> bool { return m_follow_enabled; }
  void set_follow_lerp(float alpha) { m_follow_lerp = alpha; }
  void set_follow_offset(const QVector3D& off) { m_follow_offset = off; }
  void capture_follow_offset() { m_follow_offset = m_position - m_target; }
  void update_follow(const QVector3D& target_center);

  void set_rts_view(const QVector3D& center,
                    float distance = CameraDefaults::k_default_rts_distance,
                    float angle = CameraDefaults::k_default_rts_angle,
                    float yaw_deg = CameraDefaults::k_default_rts_yaw);
  void set_top_down_view(const QVector3D& center,
                         float distance = CameraDefaults::k_default_rts_distance);
  void apply_soft_boundaries(bool is_panning = false);

  [[nodiscard]] auto get_view_matrix() const -> QMatrix4x4;
  [[nodiscard]] auto get_projection_matrix() const -> QMatrix4x4;
  [[nodiscard]] auto get_view_projection_matrix() const -> QMatrix4x4;

  [[nodiscard]] auto get_target() const -> const QVector3D& { return m_target; }
  [[nodiscard]] auto get_up_vector() const -> const QVector3D& { return m_up; }
  [[nodiscard]] auto get_right_vector() const -> const QVector3D& { return m_right; }
  [[nodiscard]] auto get_forward_vector() const -> const QVector3D& { return m_front; }
  [[nodiscard]] auto get_position() const -> const QVector3D& { return m_position; }
  [[nodiscard]] auto get_distance() const -> float;
  [[nodiscard]] auto get_pitch_deg() const -> float;
  [[nodiscard]] auto get_fov() const -> float { return m_fov; }
  [[nodiscard]] auto get_aspect() const -> float { return m_aspect; }
  [[nodiscard]] auto get_near() const -> float { return m_near_plane; }
  [[nodiscard]] auto get_far() const -> float { return m_far_plane; }

  [[nodiscard]] auto is_in_frustum(const QVector3D& center, float radius) const -> bool;

private:
  struct FrustumPlane {
    QVector3D normal;
    float distance{0.0F};
  };

  // A lock that is reset (not copied) on Camera copy/move so Camera stays
  // copyable — copies are made single-threaded during setup (e.g. deriving the
  // commander camera from the RTS camera), and each Camera owns its own lock.
  // Satisfies BasicLockable so it works directly with std::lock_guard.
  class CacheLock {
  public:
    CacheLock() = default;
    CacheLock(const CacheLock&) noexcept {}
    CacheLock(CacheLock&&) noexcept {}
    auto operator=(const CacheLock&) noexcept -> CacheLock& { return *this; }
    auto operator=(CacheLock&&) noexcept -> CacheLock& { return *this; }
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    auto try_lock() -> bool { return m_mutex.try_lock(); }

  private:
    std::mutex m_mutex;
  };

  QVector3D m_position{0.0F, 0.0F, 0.0F};
  QVector3D m_target{0.0F, 0.0F, -1.0F};
  QVector3D m_up{0.0F, 1.0F, 0.0F};
  QVector3D m_front{0.0F, 0.0F, -1.0F};
  QVector3D m_right{1.0F, 0.0F, 0.0F};
  QVector3D m_last_position;

  bool m_is_perspective = true;
  float m_fov = CameraDefaults::k_default_fov;
  float m_aspect = CameraDefaults::k_default_aspect_ratio;

  float m_near_plane = 1.0F;
  float m_far_plane = CameraDefaults::k_default_far_plane;

  float m_ortho_left = -CameraDefaults::k_default_ortho_size;
  float m_ortho_right = CameraDefaults::k_default_ortho_size;
  float m_ortho_bottom = -CameraDefaults::k_default_ortho_size;
  float m_ortho_top = CameraDefaults::k_default_ortho_size;

  bool m_follow_enabled = false;
  QVector3D m_follow_offset{0, 0, 0};
  float m_follow_lerp = 0.15F;

  float m_ground_y = 0.0F;
  float m_min_height = 0.5F;

  float m_pitch_min_deg = CameraDefaults::k_default_pitch_min;
  float m_pitch_max_deg = -5.0F;

  bool m_orbit_pending = false;
  float m_orbit_start_yaw = 0.0F;
  float m_orbit_start_pitch = 0.0F;
  float m_orbit_target_yaw = 0.0F;
  float m_orbit_target_pitch = 0.0F;
  float m_orbit_time = 0.0F;
  float m_orbit_duration = 0.12F;

  // The cache is rebuilt lazily on the render thread (submission frustum tests,
  // matrix accessors) while camera mutators run on the GUI thread in the
  // threaded Qt Quick render loop. Without serialization a reader can observe a
  // half-written view-projection matrix and derive a scrambled frustum that
  // rejects the whole scene for one frame (a full-screen flash). The mutex
  // guards every read/rebuild/invalidate of the cached geometry below.
  mutable CacheLock m_cache_mutex;
  mutable QMatrix4x4 m_cached_view;
  mutable QMatrix4x4 m_cached_projection;
  mutable QMatrix4x4 m_cached_view_projection;
  mutable std::array<FrustumPlane, 6> m_cached_frustum{};
  mutable bool m_cached_geometry_dirty{true};

  void update_vectors();
  void invalidate_cached_geometry() {
    const std::lock_guard<CacheLock> guard(m_cache_mutex);
    m_cached_geometry_dirty = true;
  }
  // Assumes m_cache_mutex is already held by the caller.
  void rebuild_cached_geometry() const;

  void clamp_above_ground();
  static void
  compute_yaw_pitch_from_offset(const QVector3D& off, float& yaw_deg, float& pitch_deg);
};

} // namespace Render::GL
