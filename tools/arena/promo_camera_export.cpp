#include "promo_camera_export.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Arena::Promo {
namespace {

auto rounded(double value, double scale) -> double {
  return std::round(value * scale) / scale;
}

} // namespace

auto unit_forward(float yaw_degrees) -> QVector3D {
  const float yaw = qDegreesToRadians(yaw_degrees);
  return {std::sin(yaw), 0.0F, std::cos(yaw)};
}

auto group_frontage(const std::vector<UnitSample>& units) -> GroupFrontage {
  GroupFrontage result;
  if (units.empty()) {
    return result;
  }
  QVector3D centroid;
  QVector3D heading;
  for (const UnitSample& unit : units) {
    centroid += unit.position;
    heading += unit_forward(unit.yaw_degrees);
  }
  centroid /= static_cast<float>(units.size());
  heading.setY(0.0F);
  if (heading.lengthSquared() < 1e-6F) {
    heading = QVector3D(0.0F, 0.0F, 1.0F);
  }
  heading.normalize();
  const QVector3D right(-heading.z(), 0.0F, heading.x());

  float lateral_min = std::numeric_limits<float>::max();
  float lateral_max = std::numeric_limits<float>::lowest();
  float depth_min = std::numeric_limits<float>::max();
  float depth_max = std::numeric_limits<float>::lowest();
  for (const UnitSample& unit : units) {
    QVector3D offset = unit.position - centroid;
    offset.setY(0.0F);
    const float lateral = QVector3D::dotProduct(offset, right);
    const float depth = QVector3D::dotProduct(offset, heading);
    lateral_min = std::min(lateral_min, lateral);
    lateral_max = std::max(lateral_max, lateral);
    depth_min = std::min(depth_min, depth);
    depth_max = std::max(depth_max, depth);
  }

  result.valid = true;
  result.alive = static_cast<int>(units.size());
  result.centroid = centroid;
  result.forward = heading;
  result.right = right;
  result.front_left = centroid + (right * lateral_min) + (heading * depth_max);
  result.front_right = centroid + (right * lateral_max) + (heading * depth_max);
  result.front_center = (result.front_left + result.front_right) * 0.5F;
  result.width = lateral_max - lateral_min;
  result.depth = depth_max - depth_min;
  return result;
}

auto matrix_json(const QMatrix4x4& matrix) -> QJsonArray {
  QJsonArray values;
  const float* data = matrix.constData();
  for (int index = 0; index < 16; ++index) {
    values.append(static_cast<double>(data[index]));
  }
  return values;
}

auto vector_json(const QVector3D& vector) -> QJsonArray {
  return QJsonArray{rounded(vector.x(), 1e4),
                    rounded(vector.y(), 1e4),
                    rounded(vector.z(), 1e4)};
}

auto group_json(const GroupSample& group) -> QJsonObject {
  const GroupFrontage frontage = group_frontage(group.units);
  QJsonObject result{{QStringLiteral("owner"), group.owner},
                     {QStringLiteral("alive"), frontage.alive}};
  if (!frontage.valid) {
    return result;
  }
  result.insert(QStringLiteral("centroid"), vector_json(frontage.centroid));
  result.insert(QStringLiteral("forward"), vector_json(frontage.forward));
  result.insert(QStringLiteral("front"),
                QJsonArray{vector_json(frontage.front_left),
                           vector_json(frontage.front_right)});
  result.insert(QStringLiteral("width"), rounded(frontage.width, 1e3));
  result.insert(QStringLiteral("depth"), rounded(frontage.depth, 1e3));
  QJsonArray units;
  for (const UnitSample& unit : group.units) {
    units.append(QJsonArray{static_cast<double>(unit.id),
                            rounded(unit.position.x(), 1e3),
                            rounded(unit.position.y(), 1e3),
                            rounded(unit.position.z(), 1e3),
                            rounded(unit.yaw_degrees, 1e2)});
  }
  result.insert(QStringLiteral("units"), units);
  return result;
}

auto camera_frame_json(const FrameStamp& stamp,
                       const CameraSample& camera,
                       const std::vector<GroupSample>& groups) -> QJsonObject {
  QJsonObject frame{
      {QStringLiteral("frame"), stamp.frame},
      {QStringLiteral("t"),
       rounded(static_cast<double>(stamp.frame) / std::max(1, stamp.fps), 1e6)},
      {QStringLiteral("scene_t"), rounded(stamp.scene_seconds, 1e6)},
      {QStringLiteral("shot_t"), rounded(stamp.shot_seconds, 1e6)},
  };
  if (!camera.valid) {
    frame.insert(QStringLiteral("camera"), false);
    return frame;
  }
  frame.insert(QStringLiteral("view"), matrix_json(camera.view));
  frame.insert(QStringLiteral("projection"), matrix_json(camera.projection));
  frame.insert(QStringLiteral("eye"), vector_json(camera.eye));
  frame.insert(QStringLiteral("target"), vector_json(camera.target));
  frame.insert(QStringLiteral("up"), vector_json(camera.up));
  frame.insert(QStringLiteral("fov_y"), rounded(camera.fov_y, 1e5));
  frame.insert(QStringLiteral("aspect"), rounded(camera.aspect, 1e6));
  frame.insert(QStringLiteral("near"), static_cast<double>(camera.near_plane));
  frame.insert(QStringLiteral("far"), static_cast<double>(camera.far_plane));
  frame.insert(QStringLiteral("render_size"),
               QJsonArray{camera.render_width, camera.render_height});
  frame.insert(QStringLiteral("viewport"),
               QJsonArray{0, 0, stamp.output_width, stamp.output_height});
  if (!groups.empty()) {
    QJsonObject by_name;
    for (const GroupSample& group : groups) {
      by_name.insert(group.name, group_json(group));
    }
    frame.insert(QStringLiteral("groups"), by_name);
  }
  return frame;
}

auto track_header_json(const TrackHeader& header) -> QJsonObject {
  return QJsonObject{
      {QStringLiteral("type"), QStringLiteral("soi_camera_track")},
      {QStringLiteral("version"), k_camera_track_version},
      {QStringLiteral("spec"), header.spec_id},
      {QStringLiteral("shot"), header.shot},
      {QStringLiteral("variant"), header.variant},
      {QStringLiteral("scenario"), header.scenario},
      {QStringLiteral("seed"), header.seed},
      {QStringLiteral("clip"), header.clip},
      {QStringLiteral("fps"), header.fps},
      {QStringLiteral("width"), header.width},
      {QStringLiteral("height"), header.height},
      {QStringLiteral("supersample"), header.supersample},
      {QStringLiteral("slow_motion"), header.slow_motion},
      {QStringLiteral("start_seconds"), header.start_seconds},
      {QStringLiteral("terrain"), header.terrain},
      {QStringLiteral("groups"), QJsonArray::fromStringList(header.groups)},
      {QStringLiteral("matrix_layout"), QStringLiteral("column_major")},
      {QStringLiteral("clip_space"), QStringLiteral("opengl")},
      {QStringLiteral("pixel_origin"), QStringLiteral("top_left")},
      {QStringLiteral("world_up"), QStringLiteral("+y")},
  };
}

auto CameraTrackWriter::open(const QString& path,
                             const TrackHeader& header,
                             QString* error) -> bool {
  close();
  m_file = std::make_unique<QFile>(path);
  if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not write camera track %1").arg(path);
    }
    m_file.reset();
    return false;
  }
  m_frames = 0;
  m_file->write(QJsonDocument(track_header_json(header)).toJson(QJsonDocument::Compact));
  m_file->write("\n");
  return true;
}

auto CameraTrackWriter::write(const QJsonObject& frame) -> bool {
  if (m_file == nullptr) {
    return false;
  }
  m_file->write(QJsonDocument(frame).toJson(QJsonDocument::Compact));
  m_file->write("\n");
  ++m_frames;
  return true;
}

void CameraTrackWriter::close() {
  if (m_file != nullptr) {
    m_file->close();
    m_file.reset();
  }
}

auto terrain_grid_for(float half_extent, float spacing) -> TerrainGrid {
  TerrainGrid grid;
  grid.spacing = std::max(0.05F, spacing);
  const int half_cells =
      std::max(1, static_cast<int>(std::ceil(std::max(0.0F, half_extent) / grid.spacing)));
  grid.columns = (half_cells * 2) + 1;
  grid.rows = grid.columns;
  grid.origin_x = -static_cast<float>(half_cells) * grid.spacing;
  grid.origin_z = grid.origin_x;
  return grid;
}

auto write_terrain_grid(const QString& json_path,
                        const TerrainGrid& grid,
                        const std::function<float(float, float)>& height_at,
                        QString* error) -> bool {
  const QFileInfo info(json_path);
  const QString data_name = info.completeBaseName() + QStringLiteral(".f32");
  QFile data(info.dir().filePath(data_name));
  if (!data.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not write terrain heights %1").arg(data.fileName());
    }
    return false;
  }
  QDataStream stream(&data);
  stream.setByteOrder(QDataStream::LittleEndian);
  stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
  float lowest = std::numeric_limits<float>::max();
  float highest = std::numeric_limits<float>::lowest();
  for (int row = 0; row < grid.rows; ++row) {
    const float z = grid.origin_z + (static_cast<float>(row) * grid.spacing);
    for (int column = 0; column < grid.columns; ++column) {
      const float x = grid.origin_x + (static_cast<float>(column) * grid.spacing);
      const float height = height_at(x, z);
      lowest = std::min(lowest, height);
      highest = std::max(highest, height);
      stream << height;
    }
  }
  data.close();

  QFile header(json_path);
  if (!header.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not write terrain header %1").arg(json_path);
    }
    return false;
  }
  header.write(
      QJsonDocument(
          QJsonObject{
              {QStringLiteral("type"), QStringLiteral("soi_terrain_heights")},
              {QStringLiteral("version"), 1},
              {QStringLiteral("data"), data_name},
              {QStringLiteral("encoding"), QStringLiteral("float32le")},
              {QStringLiteral("layout"), QStringLiteral("row_major_z_then_x")},
              {QStringLiteral("origin"),
               QJsonArray{static_cast<double>(grid.origin_x),
                          static_cast<double>(grid.origin_z)}},
              {QStringLiteral("spacing"), static_cast<double>(grid.spacing)},
              {QStringLiteral("columns"), grid.columns},
              {QStringLiteral("rows"), grid.rows},
              {QStringLiteral("min"), static_cast<double>(lowest)},
              {QStringLiteral("max"), static_cast<double>(highest)},
              {QStringLiteral("sampler"),
               QStringLiteral("TerrainService::get_terrain_height")}})
          .toJson(QJsonDocument::Indented));
  return true;
}

} // namespace Arena::Promo
