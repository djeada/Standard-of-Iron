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

struct Shot {
  QString name;
  QString scenario;
  int seed{1337};

  float start_seconds{0.0F};
  float duration_seconds{3.0F};
  float slow_motion{1.0F};
  float shake{0.0F};

  bool gameplay_camera{false};

  bool rpg_hud{false};
  Focus focus;
  std::vector<CameraKey> keys;
};

struct Spec {
  QString id;
  QString title;
  int width{1080};
  int height{1920};
  int fps{60};
  int supersample{1};
  std::vector<Shot> shots;
};

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

[[nodiscard]] auto evaluate(const std::vector<CameraKey>& keys,
                            float shot_time) -> Pose;

[[nodiscard]] auto shake_offset(int frame_index, float amount) -> QVector3D;

} // namespace Arena::Promo
