#pragma once

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QString>
#include <QStringList>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Arena::Promo {

inline constexpr int k_camera_track_version = 1;

struct CameraSample {
  bool valid{false};
  QMatrix4x4 view;
  QMatrix4x4 projection;
  QVector3D eye;
  QVector3D target;
  QVector3D up{0.0F, 1.0F, 0.0F};
  float fov_y{40.0F};
  float aspect{1.0F};
  float near_plane{0.1F};
  float far_plane{200.0F};
  int render_width{0};
  int render_height{0};
};

struct UnitSample {
  std::uint64_t id{0};
  QVector3D position;
  float yaw_degrees{0.0F};
};

struct GroupSample {
  QString name;
  int owner{0};
  std::vector<UnitSample> units;
};

struct GroupFrontage {
  bool valid{false};
  int alive{0};
  QVector3D centroid;
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D front_left;
  QVector3D front_right;
  QVector3D front_center;
  float width{0.0F};
  float depth{0.0F};
};

[[nodiscard]] auto unit_forward(float yaw_degrees) -> QVector3D;

[[nodiscard]] auto group_frontage(const std::vector<UnitSample>& units) -> GroupFrontage;

[[nodiscard]] auto matrix_json(const QMatrix4x4& matrix) -> QJsonArray;

[[nodiscard]] auto vector_json(const QVector3D& vector) -> QJsonArray;

[[nodiscard]] auto group_json(const GroupSample& group) -> QJsonObject;

struct FrameStamp {
  int frame{0};
  int fps{30};
  double scene_seconds{0.0};
  double shot_seconds{0.0};
  int output_width{0};
  int output_height{0};
};

[[nodiscard]] auto camera_frame_json(const FrameStamp& stamp,
                                     const CameraSample& camera,
                                     const std::vector<GroupSample>& groups)
    -> QJsonObject;

struct TrackHeader {
  QString spec_id;
  QString shot;
  QString variant;
  QString scenario;
  int seed{0};
  QString clip;
  int fps{30};
  int width{0};
  int height{0};
  int supersample{1};
  double slow_motion{1.0};
  double start_seconds{0.0};
  QString terrain;
  QStringList groups;
};

[[nodiscard]] auto track_header_json(const TrackHeader& header) -> QJsonObject;

class CameraTrackWriter {
public:
  auto open(const QString& path, const TrackHeader& header, QString* error) -> bool;
  auto write(const QJsonObject& frame) -> bool;
  void close();
  [[nodiscard]] auto is_open() const -> bool { return m_file != nullptr; }
  [[nodiscard]] auto frames() const -> int { return m_frames; }

private:
  std::unique_ptr<QFile> m_file;
  int m_frames{0};
};

struct TerrainGrid {
  float origin_x{0.0F};
  float origin_z{0.0F};
  float spacing{1.0F};
  int columns{0};
  int rows{0};
};

[[nodiscard]] auto terrain_grid_for(float half_extent, float spacing) -> TerrainGrid;

auto write_terrain_grid(const QString& json_path,
                        const TerrainGrid& grid,
                        const std::function<float(float, float)>& height_at,
                        QString* error) -> bool;

} // namespace Arena::Promo
