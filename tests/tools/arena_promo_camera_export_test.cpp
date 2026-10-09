#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QTemporaryDir>

#include <cmath>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

#include "scene/camera.h"
#include "tools/arena/promo_camera_export.h"
#include "tools/arena/promo_spec.h"

namespace {

using Arena::Promo::CameraSample;
using Arena::Promo::GroupSample;
using Arena::Promo::UnitSample;

auto write_spec(QTemporaryDir& dir, const char* json) -> QString {
  const QString path = dir.filePath(QStringLiteral("spec.json"));
  QFile file(path);
  file.open(QIODevice::WriteOnly | QIODevice::Truncate);
  file.write(json);
  file.close();
  return path;
}

auto element(const QJsonArray& matrix, int row, int column) -> double {
  return matrix.at((column * 4) + row).toDouble();
}

auto project(const QJsonObject& frame, double x, double y, double z, QPointF& out)
    -> bool {
  const QJsonArray view = frame.value(QStringLiteral("view")).toArray();
  const QJsonArray projection = frame.value(QStringLiteral("projection")).toArray();
  const QJsonArray viewport = frame.value(QStringLiteral("viewport")).toArray();
  const double world[4] = {x, y, z, 1.0};
  double eye[4] = {0, 0, 0, 0};
  double clip[4] = {0, 0, 0, 0};
  for (int row = 0; row < 4; ++row) {
    for (int k = 0; k < 4; ++k) {
      eye[row] += element(view, row, k) * world[k];
    }
  }
  for (int row = 0; row < 4; ++row) {
    for (int k = 0; k < 4; ++k) {
      clip[row] += element(projection, row, k) * eye[k];
    }
  }
  if (clip[3] <= 1e-6) {
    return false;
  }
  const double width = viewport.at(2).toDouble();
  const double height = viewport.at(3).toDouble();
  out = QPointF(((clip[0] / clip[3]) * 0.5 + 0.5) * width,
                (1.0 - ((clip[1] / clip[3]) * 0.5 + 0.5)) * height);
  return true;
}

TEST(ArenaPromoCameraExportTest, ExportedMatricesProjectLikeTheGameCamera) {
  Render::GL::Camera camera;
  camera.look_at(QVector3D(-30.0F, 22.0F, 41.0F),
                 QVector3D(4.0F, 1.5F, -2.0F),
                 QVector3D(0.0F, 1.0F, 0.0F));
  camera.set_perspective(28.0F, 1920.0F / 1080.0F, 0.5F, 900.0F);

  CameraSample sample;
  sample.valid = true;
  sample.view = camera.get_view_matrix();
  sample.projection = camera.get_projection_matrix();
  sample.render_width = 3840;
  sample.render_height = 2160;

  Arena::Promo::FrameStamp stamp;
  stamp.frame = 7;
  stamp.fps = 30;
  stamp.output_width = 1920;
  stamp.output_height = 1080;
  const QJsonObject frame = Arena::Promo::camera_frame_json(stamp, sample, {});

  EXPECT_NEAR(frame.value(QStringLiteral("t")).toDouble(), 7.0 / 30.0, 1e-6);
  const std::vector<QVector3D> probes = {
      {4.0F, 1.5F, -2.0F}, {12.0F, 0.0F, 6.0F}, {-8.0F, 3.0F, -14.0F}};
  for (const QVector3D& probe : probes) {
    QPointF expected;
    ASSERT_TRUE(camera.world_to_screen(probe, 1920.0, 1080.0, expected));
    QPointF exported;
    ASSERT_TRUE(project(frame, probe.x(), probe.y(), probe.z(), exported));
    EXPECT_NEAR(exported.x(), expected.x(), 0.01) << "column-major layout";
    EXPECT_NEAR(exported.y(), expected.y(), 0.01);
  }
  QPointF centre;
  ASSERT_TRUE(project(frame, 4.0, 1.5, -2.0, centre));
  EXPECT_NEAR(centre.x(), 960.0, 0.01) << "the look-at target is the frame centre";
  EXPECT_NEAR(centre.y(), 540.0, 0.01);
}

TEST(ArenaPromoCameraExportTest, FrontageFollowsTheGroupsFacing) {
  std::vector<UnitSample> units;
  for (int index = 0; index < 5; ++index) {
    units.push_back(UnitSample{static_cast<std::uint64_t>(index + 1),
                               QVector3D(-46.0F, 0.0F, -8.0F + (4.0F * index)),
                               90.0F});
  }
  units.push_back(UnitSample{9, QVector3D(-50.0F, 0.0F, 0.0F), 90.0F});

  const auto frontage = Arena::Promo::group_frontage(units);
  ASSERT_TRUE(frontage.valid);
  EXPECT_EQ(frontage.alive, 6);
  EXPECT_NEAR(frontage.forward.x(), 1.0F, 1e-4F) << "yaw 90 faces +x";
  EXPECT_NEAR(frontage.width, 16.0F, 1e-3F);
  EXPECT_NEAR(frontage.front_center.x(), -46.0F, 1e-3F)
      << "the front sits on the leading rank, not the centroid";
  EXPECT_NEAR(frontage.front_left.z(), -8.0F, 1e-3F);
  EXPECT_NEAR(frontage.front_right.z(), 8.0F, 1e-3F);

  GroupSample group;
  group.name = QStringLiteral("rome_swords");
  group.owner = 1;
  group.units = units;
  const QJsonObject json = Arena::Promo::group_json(group);
  EXPECT_EQ(json.value(QStringLiteral("alive")).toInt(), 6);
  EXPECT_EQ(json.value(QStringLiteral("units")).toArray().size(), 6);
  EXPECT_EQ(json.value(QStringLiteral("front")).toArray().size(), 2);
}

TEST(ArenaPromoCameraExportTest, TerrainGridRoundTripsHeights) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto grid = Arena::Promo::terrain_grid_for(3.0F, 1.0F);
  EXPECT_EQ(grid.columns, 7);
  EXPECT_FLOAT_EQ(grid.origin_x, -3.0F);
  QString error;
  ASSERT_TRUE(Arena::Promo::write_terrain_grid(
      dir.filePath(QStringLiteral("terrain_probe_1.json")),
      grid,
      [](float x, float z) { return (x * 0.5F) + (z * 2.0F); },
      &error))
      << error.toStdString();
  QFile data(dir.filePath(QStringLiteral("terrain_probe_1.f32")));
  ASSERT_TRUE(data.open(QIODevice::ReadOnly));
  const QByteArray bytes = data.readAll();
  ASSERT_EQ(bytes.size(), 7 * 7 * 4);
  float value = 0.0F;
  const int row = 5;
  const int column = 1;
  std::memcpy(&value, bytes.constData() + (((row * 7) + column) * 4), sizeof(float));
  EXPECT_FLOAT_EQ(value, (-2.0F * 0.5F) + (2.0F * 2.0F));
}

TEST(ArenaPromoCameraExportTest, VerticalVariantSharesThePassAndOverridesTheLens) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = write_spec(dir, R"({
    "id": "probe",
    "width": 1920, "height": 1080,
    "overlay_groups": ["rome_swords"],
    "shots": [
      { "name": "line", "scenario": "arena", "start": 2.0, "duration": 4.0,
        "focus": { "mode": "point", "point": [0, 0, 0] },
        "camera": [ { "time": 0.0, "fov": 30, "distance": 40, "yaw": 90 },
                    { "time": 4.0, "fov": 30, "distance": 40, "yaw": 100 } ],
        "vertical": { "fov_scale": 1.5, "yaw_offset": 10,
                      "focus": { "offset": [0, 0, 4] } },
        "variants": [ { "name": "square", "width": 1080, "height": 1080,
                        "camera": [ { "time": 0.0, "fov": 20, "distance": 30 } ] } ] }
    ]
  })");
  QString error;
  const auto spec = Arena::Promo::load(path, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  ASSERT_EQ(spec->shots.size(), 1U);
  const auto& shot = spec->shots[0];
  EXPECT_FALSE(shot.overlay_groups.all);
  EXPECT_EQ(shot.overlay_groups.names, QStringList{QStringLiteral("rome_swords")});
  ASSERT_EQ(shot.variants.size(), 2U);

  const auto& vertical = shot.variants[0];
  EXPECT_EQ(vertical.name, QStringLiteral("vertical"));
  EXPECT_EQ(vertical.width, 1080);
  EXPECT_EQ(vertical.height, 1920);
  EXPECT_FLOAT_EQ(vertical.camera.start_seconds, 2.0F);
  EXPECT_FLOAT_EQ(vertical.camera.duration_seconds, 4.0F);
  ASSERT_EQ(vertical.camera.keys.size(), 2U);
  EXPECT_FLOAT_EQ(vertical.camera.keys[0].fov, 45.0F);
  EXPECT_FLOAT_EQ(vertical.camera.keys[1].yaw, 110.0F);
  EXPECT_FLOAT_EQ(vertical.camera.focus.offset.z(), 4.0F);
  EXPECT_FLOAT_EQ(shot.keys[0].fov, 30.0F) << "the 16:9 take keeps its own lens";

  const auto& square = shot.variants[1];
  EXPECT_EQ(square.name, QStringLiteral("square"));
  ASSERT_EQ(square.camera.keys.size(), 1U);
  EXPECT_FLOAT_EQ(square.camera.keys[0].fov, 20.0F);
}

TEST(ArenaPromoCameraExportTest, VariantsCannotLeaveTheScenarioPass) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = write_spec(dir, R"({
    "id": "probe",
    "shots": [
      { "name": "line", "scenario": "arena", "duration": 4.0,
        "focus": { "mode": "all" },
        "camera": [ { "time": 0.0 }, { "time": 4.0 } ],
        "vertical": { "start": 3.0 } }
    ]
  })");
  QString error;
  EXPECT_FALSE(Arena::Promo::load(path, &error).has_value());
  EXPECT_TRUE(error.contains(QStringLiteral("start"))) << error.toStdString();
}

TEST(ArenaPromoCameraExportTest, SpecLevelVerticalAppliesToEveryShotUnlessDeclined) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = write_spec(dir, R"({
    "id": "probe",
    "vertical": { "width": 720, "height": 1280 },
    "shots": [
      { "name": "a", "scenario": "arena", "duration": 2.0,
        "focus": { "mode": "all" }, "camera": [ { "time": 0.0 } ] },
      { "name": "b", "scenario": "arena", "start": 3.0, "duration": 2.0,
        "vertical": false,
        "focus": { "mode": "all" }, "camera": [ { "time": 0.0 } ] }
    ]
  })");
  QString error;
  const auto spec = Arena::Promo::load(path, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  ASSERT_EQ(spec->shots[0].variants.size(), 1U);
  EXPECT_EQ(spec->shots[0].variants[0].width, 720);
  EXPECT_TRUE(spec->shots[1].variants.empty());
  EXPECT_TRUE(spec->overlay_groups.all) << "every group is exported by default";
}

} // namespace
