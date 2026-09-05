#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

namespace Arena::Promo {

enum class FocusMode : std::uint8_t {
  Point,
  Group,
  GroupPair,
  AllUnits,
  Battle,
  Army,
};

enum class ReportCardStyle : std::uint8_t {
  Reel,

  Matchup,
};

enum class Ease : std::uint8_t {
  Linear,
  Smooth,
  EaseIn,
  EaseOut,
};

struct Focus {
  FocusMode mode{FocusMode::AllUnits};
  QVector3D point;
  QString group;
  QString second_group;
  QVector3D offset;

  int owner{0};
  float engagement_radius{14.0F};
  float home_radius{26.0F};

  float smoothing{0.25F};
};

struct CameraKey {
  float time{0.0F};
  float distance{16.0F};
  float pitch{18.0F};
  float yaw{40.0F};
  float fov{40.0F};
  float roll{0.0F};
  float height{0.0F};
  Ease ease{Ease::Smooth};
};

struct StartOn {
  QString event;
  QString side;
  float offset_seconds{0.0F};
};

inline constexpr const char* k_event_first_wave = "first_wave";
inline constexpr const char* k_event_first_contact = "first_contact";
inline constexpr const char* k_event_first_building_lost = "first_building_lost";
inline constexpr const char* k_event_decision = "decision";

[[nodiscard]] auto known_start_event(const QString& event) -> bool;

struct Shot {
  QString name;
  QString scenario;
  int seed{1337};

  float start_seconds{0.0F};
  std::optional<StartOn> start_on;
  float duration_seconds{3.0F};

  float slow_motion{1.0F};
  float shake{0.0F};

  bool casting_overlay{false};

  bool gameplay_camera{false};

  bool flame_card{false};
  float flame_speed{1.0F};
  float flame_intensity{1.0F};

  bool rpg_hud{false};

  bool gameplay_ui{false};

  bool gameplay_ui_all_owners{false};

  float report_card_seconds{0.0F};
  Focus focus;
  std::vector<CameraKey> keys;
};

struct MotionLimits {
  float yaw_degrees_per_second{12.0F};
  float pitch_degrees_per_second{6.0F};
  float fov_degrees_per_second{6.0F};
  float roll_degrees_per_second{4.0F};
  float roll_magnitude_degrees{5.0F};
  float shake{0.03F};
  float minimum_clip_seconds{1.5F};
  float mean_clip_seconds{2.0F};
};

struct Spec {
  QString id;
  QString title;
  int width{1080};
  int height{1920};
  int fps{60};
  int supersample{1};
  bool audio{false};

  QString music_track;
  float music_volume{0.18F};

  QString report_sound_decided;
  QString report_sound_undecided;
  float report_sound_volume{0.45F};
  bool gameplay_ui{false};
  bool gameplay_ui_all_owners{false};
  bool casting_overlay{false};

  bool require_decision{false};

  bool forbid_world_edge{false};

  ReportCardStyle report_card_style{ReportCardStyle::Reel};
  std::vector<Shot> shots;
};

struct GroundFootprint {
  bool horizon_visible{false};
  float max_abs_extent{0.0F};
};

struct Pose;

[[nodiscard]] auto view_ground_footprint(const Pose& pose,
                                         const QVector3D& focus,
                                         float aspect,
                                         float ground_y = 0.0F) -> GroundFootprint;

[[nodiscard]] auto frames_world_edge(const GroundFootprint& footprint,
                                     float floor_half_extent) -> bool;

[[nodiscard]] auto world_edge_violations(const Spec& spec, float floor_half_extent)
    -> std::vector<QString>;

[[nodiscard]] auto uses_start_events(const Spec& spec) -> bool;

struct Pose {
  QVector3D target;
  float distance{16.0F};
  float pitch{18.0F};
  float yaw{40.0F};
  float fov{40.0F};
  float roll{0.0F};
  float height{0.0F};
};

struct CapturePass {
  QString scenario;
  int seed{0};
  std::vector<std::size_t> shots;
};

[[nodiscard]] auto plan_passes(const Spec& spec) -> std::vector<CapturePass>;

[[nodiscard]] auto load(const QString& path, QString* error) -> std::optional<Spec>;

[[nodiscard]] auto motion_violations(const Spec& spec, const MotionLimits& limits = {})
    -> std::vector<QString>;

[[nodiscard]] auto evaluate(const std::vector<CameraKey>& keys,
                            float shot_time) -> Pose;

[[nodiscard]] auto shake_offset(int frame_index, float amount) -> QVector3D;

} // namespace Arena::Promo
