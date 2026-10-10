#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "tools/campaign_film/campaign_film_overlay.h"
#include "tools/campaign_film/campaign_film_spec.h"
#include "ui/campaign_route_path.h"

namespace {

using CampaignFilm::Ease;
using CampaignFilm::Ends;
using CampaignMapFilm::RoutePath;

auto json(const char* text) -> QJsonObject {
  return QJsonDocument::fromJson(QByteArray(text)).object();
}

auto straight_march() -> CampaignFilm::March {
  const QJsonObject object = json(R"({
    "march": {
      "points": [[0.1, 0.5], [0.3, 0.5], [0.5, 0.5], [0.7, 0.5], [0.9, 0.5]],
      "stops": [
        {"id": "a", "name": "Alpha", "date": "218 BC", "kind": "city", "uv": [0.1, 0.5], "index": 0},
        {"id": "b", "name": "Beta", "date": "", "kind": "battle", "uv": [0.5, 0.5], "index": 2},
        {"id": "c", "name": "Gamma", "date": "216 BC", "kind": "battle", "uv": [0.9, 0.5], "index": 4}
      ]
    }
  })");
  QString error;
  auto march = CampaignFilm::parse_march(object, &error);
  EXPECT_TRUE(march.has_value()) << error.toStdString();
  return *march;
}

auto parse(const CampaignFilm::March& march,
           const char* text,
           QString* error) -> std::optional<CampaignFilm::Spec> {
  CampaignFilm::LoadContext context;
  context.march = &march;
  return CampaignFilm::parse_spec(json(text), context, error);
}

constexpr const char* k_minimal_spec = R"({
  "id": "test",
  "width": 640, "height": 360, "fps": 10, "duration": 4,
  "camera": [
    {"time": 0, "look": {"stop": "a"}, "distance": 0.8, "yaw": 180, "pitch": 60},
    {"time": 4, "look": {"stop": "c"}, "distance": 0.4}
  ],
  "route": {
    "from": "a", "to": "c",
    "keys": [{"time": 1, "progress": 0}, {"time": 2, "at": "b"}, {"time": 3, "at": "c"}]
  },
  "markers": [{"site": "b", "appear": "arrival", "fade": 0.5, "label_hold": 1.0}],
  "armies": [{"id": "h", "anchor": "head", "keys": [
    {"time": 0, "foot": 100}, {"at": "b", "foot": 60}, {"at": "c", "foot": 40}
  ]}]
})";

} // namespace

TEST(CampaignRoutePath, StraightLineArcLengthAndPoints) {
  const RoutePath path(
      {QVector2D(0.0F, 0.0F), QVector2D(1.0F, 0.0F), QVector2D(2.0F, 0.0F)});
  ASSERT_FALSE(path.empty());
  EXPECT_NEAR(path.length(), 2.0F, 1e-4F);
  EXPECT_NEAR(path.progress_at_raw_index(0), 0.0F, 1e-6F);
  EXPECT_NEAR(path.progress_at_raw_index(1), 0.5F, 1e-4F);
  EXPECT_NEAR(path.progress_at_raw_index(2), 1.0F, 1e-6F);
  EXPECT_NEAR(path.point_at(0.25F).x(), 0.5F, 1e-4F);
  EXPECT_NEAR(path.point_at(-1.0F).x(), 0.0F, 1e-6F);
  EXPECT_NEAR(path.point_at(2.0F).x(), 2.0F, 1e-6F);
  EXPECT_NEAR(path.direction_at(0.5F).x(), 1.0F, 1e-4F);
}

TEST(CampaignRoutePath, RawPointsSurviveSmoothing) {
  const std::vector<QVector2D> raw{QVector2D(0.0F, 0.0F),
                                   QVector2D(0.2F, 0.3F),
                                   QVector2D(0.5F, 0.1F),
                                   QVector2D(0.7F, 0.6F)};
  const RoutePath path(raw);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const QVector2D at = path.point_at(path.progress_at_raw_index(i));
    EXPECT_NEAR(at.x(), raw[i].x(), 1e-4F) << i;
    EXPECT_NEAR(at.y(), raw[i].y(), 1e-4F) << i;
  }
  float previous = -1.0F;
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const float progress = path.progress_at_raw_index(i);
    EXPECT_GT(progress, previous);
    previous = progress;
  }
}

TEST(CampaignRoutePath, SliceRevealsExactlyTheWindow) {
  const RoutePath path(
      {QVector2D(0.0F, 0.0F), QVector2D(1.0F, 0.0F), QVector2D(1.0F, 1.0F)});
  const auto slice = path.slice(0.2F, 0.7F);
  ASSERT_GE(slice.size(), 2U);
  EXPECT_NEAR((slice.front() - path.point_at(0.2F)).length(), 0.0F, 1e-5F);
  EXPECT_NEAR((slice.back() - path.point_at(0.7F)).length(), 0.0F, 1e-5F);
  float length = 0.0F;
  for (std::size_t i = 1; i < slice.size(); ++i) {
    length += (slice[i] - slice[i - 1]).length();
  }
  EXPECT_NEAR(length, path.length() * 0.5F, path.length() * 0.01F);
  EXPECT_TRUE(path.slice(0.6F, 0.6F).empty());
  EXPECT_TRUE(path.slice(0.8F, 0.1F).empty());
}

TEST(CampaignRoutePath, ProjectionPutsTheTargetAtScreenCentre) {
  CampaignMapFilm::CameraPose pose;
  pose.target = QVector2D(0.3F, 0.7F);
  pose.distance = 0.5F;
  pose.yaw = 180.0F;
  pose.pitch = 60.0F;
  const QMatrix4x4 vp = CampaignMapFilm::view_projection(1920.0F, 1080.0F, pose);
  const auto centre = CampaignMapFilm::project(
      vp, CampaignMapFilm::world_point(pose.target, 0.0F), 1920.0F, 1080.0F);
  ASSERT_TRUE(centre.in_front);
  EXPECT_NEAR(centre.pixel.x(), 960.0, 0.5);
  EXPECT_NEAR(centre.pixel.y(), 540.0, 0.5);
  const auto north = CampaignMapFilm::project(
      vp,
      CampaignMapFilm::world_point(pose.target + QVector2D(0.0F, 0.05F), 0.0F),
      1920.0F,
      1080.0F);
  const auto east = CampaignMapFilm::project(
      vp,
      CampaignMapFilm::world_point(pose.target + QVector2D(0.05F, 0.0F), 0.0F),
      1920.0F,
      1080.0F);
  EXPECT_LT(north.pixel.y(), centre.pixel.y());
  EXPECT_GT(east.pixel.x(), centre.pixel.x());
}

TEST(CampaignFilmCurves, EasingShapes) {
  EXPECT_FLOAT_EQ(CampaignFilm::ease_value(Ease::Linear, 0.25F), 0.25F);
  EXPECT_FLOAT_EQ(CampaignFilm::ease_value(Ease::Smooth, 0.5F), 0.5F);
  EXPECT_FLOAT_EQ(CampaignFilm::ease_value(Ease::EaseIn, 0.5F), 0.125F);
  EXPECT_FLOAT_EQ(CampaignFilm::ease_value(Ease::EaseOut, 0.5F), 0.875F);
  EXPECT_FLOAT_EQ(CampaignFilm::ease_value(Ease::Smooth, 2.0F), 1.0F);
  EXPECT_EQ(CampaignFilm::parse_ease(QStringLiteral("in"), Ease::Linear), Ease::EaseIn);
  EXPECT_EQ(CampaignFilm::parse_ease(QString(), Ease::Linear), Ease::Linear);
}

TEST(CampaignFilmCurves, HermiteSplinePassesThroughKeysAndIsSmooth) {
  const std::vector<float> times{0.0F, 1.0F, 3.0F, 4.0F};
  const std::vector<float> values{0.0F, 2.0F, 1.0F, 5.0F};
  for (std::size_t i = 0; i < times.size(); ++i) {
    EXPECT_NEAR(CampaignFilm::hermite_channel(times, values, times[i], Ends::Ease),
                values[i],
                1e-5F);
  }
  const float h = 1e-3F;
  for (const float key_time : {1.0F, 3.0F}) {
    const float left =
        (CampaignFilm::hermite_channel(times, values, key_time, Ends::Ease) -
         CampaignFilm::hermite_channel(times, values, key_time - h, Ends::Ease)) /
        h;
    const float right =
        (CampaignFilm::hermite_channel(times, values, key_time + h, Ends::Ease) -
         CampaignFilm::hermite_channel(times, values, key_time, Ends::Ease)) /
        h;
    EXPECT_NEAR(left, right, 0.05F) << key_time;
  }
  const float start_slope =
      (CampaignFilm::hermite_channel(times, values, h, Ends::Ease) - values[0]) / h;
  EXPECT_NEAR(start_slope, 0.0F, 0.05F);
  const float moving_slope =
      (CampaignFilm::hermite_channel(times, values, h, Ends::Moving) - values[0]) / h;
  EXPECT_NEAR(moving_slope, 2.0F, 0.05F);
}

TEST(CampaignFilmCurves, KeyedChannelUsesTheIncomingKeysEase) {
  const std::vector<float> times{0.0F, 2.0F};
  const std::vector<float> values{10.0F, 20.0F};
  EXPECT_NEAR(
      CampaignFilm::keyed_channel(times, values, {Ease::Linear, Ease::Linear}, 1.0F),
      15.0F,
      1e-5F);
  EXPECT_NEAR(
      CampaignFilm::keyed_channel(times, values, {Ease::Linear, Ease::EaseIn}, 1.0F),
      11.25F,
      1e-5F);
  EXPECT_NEAR(CampaignFilm::keyed_channel(times, values, {}, -1.0F), 10.0F, 1e-6F);
  EXPECT_NEAR(CampaignFilm::keyed_channel(times, values, {}, 9.0F), 20.0F, 1e-6F);
}

TEST(CampaignFilmCurves, WindowFadesInAndOut) {
  CampaignFilm::Window window;
  window.in = 1.0F;
  window.out = 3.0F;
  window.fade = 0.5F;
  EXPECT_FLOAT_EQ(window.alpha(0.9F), 0.0F);
  EXPECT_FLOAT_EQ(window.alpha(1.25F), 0.5F);
  EXPECT_FLOAT_EQ(window.alpha(2.0F), 1.0F);
  EXPECT_FLOAT_EQ(window.alpha(2.75F), 0.5F);
  EXPECT_FLOAT_EQ(window.alpha(3.1F), 0.0F);
}

TEST(CampaignFilmSpec, ParsesAMinimalSpec) {
  const auto march = straight_march();
  QString error;
  const auto spec = parse(march, k_minimal_spec, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  EXPECT_EQ(spec->frame_count(), 40);
  ASSERT_EQ(spec->camera.size(), 2U);
  EXPECT_FLOAT_EQ(spec->camera[1].yaw, 180.0F);
  EXPECT_FLOAT_EQ(spec->camera[1].pitch, 60.0F);
  EXPECT_EQ(spec->camera[1].look.kind, CampaignFilm::Target::Kind::Stop);
  EXPECT_TRUE(spec->route.enabled);
  ASSERT_EQ(spec->route.keys.size(), 3U);
  EXPECT_EQ(spec->route.keys[1].at, QStringLiteral("b"));
  ASSERT_EQ(spec->markers.size(), 1U);
  EXPECT_EQ(spec->markers[0].name, QStringLiteral("Beta"));
  EXPECT_EQ(spec->markers[0].kind, QStringLiteral("battle"));
  EXPECT_TRUE(spec->markers[0].on_arrival);
}

TEST(CampaignFilmSpec, RejectsBrokenSpecs) {
  const auto march = straight_march();
  QString error;
  EXPECT_FALSE(parse(march, R"({"camera": [{"time": 0, "look": "a"}]})", &error));
  EXPECT_TRUE(error.contains(QStringLiteral("id")));
  EXPECT_FALSE(parse(
      march, R"({"id": "x", "camera": [{"time": 0, "look": "nowhere"}]})", &error));
  EXPECT_TRUE(error.contains(QStringLiteral("nowhere")));
  EXPECT_FALSE(parse(march,
                     R"({"id": "x", "camera": [
      {"time": 2, "look": "a"}, {"time": 1, "look": "b"}]})",
                     &error));
  EXPECT_FALSE(
      parse(march,
            R"({"id": "x", "camera": [{"time": 0, "look": "a", "pitch": 95}]})",
            &error));
  EXPECT_FALSE(parse(march,
                     R"({"id": "x", "camera": [{"time": 0, "look": "a"}],
      "route": {"from": "b", "to": "c", "keys": [{"time": 0, "at": "a"}]}})",
                     &error));
  EXPECT_TRUE(error.contains(QStringLiteral("outside")));
  EXPECT_FALSE(parse(march,
                     R"({"id": "x", "camera": [{"time": 0, "look": "a"}],
      "route": {"from": "c", "to": "a", "keys": [{"time": 0, "progress": 0}]}})",
                     &error));
  EXPECT_FALSE(parse(march,
                     R"({"id": "x", "camera": [{"time": 0, "look": "a"}],
      "markers": [{"site": "b", "appear": "arrival"}]})",
                     &error));
}

TEST(CampaignFilmTimeline, RouteProgressHitsStopsOnTheirKeys) {
  const auto march = straight_march();
  QString error;
  const auto spec = parse(march, k_minimal_spec, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  const CampaignFilm::Timeline timeline(*spec, march);
  const float b = *march.progress_of(QStringLiteral("b"));
  const float c = *march.progress_of(QStringLiteral("c"));
  EXPECT_NEAR(timeline.route_progress(0.0F), 0.0F, 1e-6F);
  EXPECT_NEAR(timeline.route_progress(1.0F), 0.0F, 1e-6F);
  EXPECT_NEAR(timeline.route_progress(2.0F), b, 1e-6F);
  EXPECT_NEAR(timeline.route_progress(3.0F), c, 1e-6F);
  EXPECT_NEAR(timeline.route_progress(1.5F), b * 0.5F, 1e-4F);
  EXPECT_NEAR(timeline.route_progress(10.0F), c, 1e-6F);
  const auto arrival = timeline.arrival_time(QStringLiteral("b"));
  ASSERT_TRUE(arrival.has_value());
  EXPECT_NEAR(*arrival, 2.0F, 0.01F);
  EXPECT_NEAR(
      (timeline.evaluate(2.0F).head_uv - QVector2D(0.5F, 0.5F)).length(), 0.0F, 1e-3F);
}

TEST(CampaignFilmTimeline, MarkersAndArmiesFollowArrivals) {
  const auto march = straight_march();
  QString error;
  const auto spec = parse(march, k_minimal_spec, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  const CampaignFilm::Timeline timeline(*spec, march);
  EXPECT_TRUE(timeline.warnings().isEmpty());
  EXPECT_FLOAT_EQ(timeline.evaluate(1.9F).markers[0].alpha, 0.0F);
  EXPECT_GT(timeline.evaluate(2.3F).markers[0].alpha, 0.5F);
  EXPECT_GT(timeline.evaluate(3.3F).markers[0].alpha, 0.99F);
  EXPECT_LT(timeline.evaluate(3.3F).markers[0].text_alpha, 0.01F);
  EXPECT_DOUBLE_EQ(
      timeline.evaluate(1.5F).armies[0].values.value(QStringLiteral("foot")), 100.0);
  EXPECT_DOUBLE_EQ(
      timeline.evaluate(2.5F).armies[0].values.value(QStringLiteral("foot")), 60.0);
  EXPECT_DOUBLE_EQ(
      timeline.evaluate(3.5F).armies[0].values.value(QStringLiteral("foot")), 40.0);
  EXPECT_NEAR((timeline.evaluate(2.0F).armies[0].uv - QVector2D(0.5F, 0.5F)).length(),
              0.0F,
              1e-3F);
}

TEST(CampaignFilmTimeline, CameraMovesBetweenStopsAndZoomsGeometrically) {
  const auto march = straight_march();
  QString error;
  const auto spec = parse(march, k_minimal_spec, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  const CampaignFilm::Timeline timeline(*spec, march);
  const auto start = timeline.camera(0.0F);
  const auto end = timeline.camera(4.0F);
  const auto middle = timeline.camera(2.0F);
  EXPECT_NEAR(start.target.x(), 0.1F, 1e-5F);
  EXPECT_NEAR(end.target.x(), 0.9F, 1e-5F);
  EXPECT_NEAR(middle.target.x(), 0.5F, 1e-4F);
  EXPECT_NEAR(middle.distance, std::sqrt(0.8F * 0.4F), 1e-4F);
  const auto frame = timeline.frame_state(timeline.evaluate(2.5F));
  EXPECT_TRUE(frame.active);
  EXPECT_TRUE(frame.route_visible);
  EXPECT_GT(frame.route_to, frame.route_from);
  EXPECT_NEAR(frame.route_window_end, *march.progress_of(QStringLiteral("c")), 1e-6F);
}

TEST(CampaignFilmRegions, TriangulationPreservesArea) {
  auto area = [](const std::vector<QVector2D>& tris) {
    double total = 0.0;
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
      const QVector2D a = tris[i];
      const QVector2D b = tris[i + 1];
      const QVector2D c = tris[i + 2];
      total += std::abs((b.x() - a.x()) * (c.y() - a.y()) -
                        (c.x() - a.x()) * (b.y() - a.y())) *
               0.5;
    }
    return total;
  };
  const std::vector<QVector2D> square{
      QVector2D(0, 0), QVector2D(1, 0), QVector2D(1, 1), QVector2D(0, 1)};
  EXPECT_EQ(CampaignFilm::triangulate(square).size(), 6U);
  EXPECT_NEAR(area(CampaignFilm::triangulate(square)), 1.0, 1e-6);
  const std::vector<QVector2D> ell{QVector2D(0, 0),
                                   QVector2D(2, 0),
                                   QVector2D(2, 1),
                                   QVector2D(1, 1),
                                   QVector2D(1, 2),
                                   QVector2D(0, 2)};
  EXPECT_NEAR(area(CampaignFilm::triangulate(ell)), 3.0, 1e-6);
  std::vector<QVector2D> clockwise(ell.rbegin(), ell.rend());
  EXPECT_NEAR(area(CampaignFilm::triangulate(clockwise)), 3.0, 1e-6);
}

TEST(CampaignFilmShipped, CarthagoNovaToCannaeSpecIsValid) {
  QString error;
  const auto bounds_object = CampaignFilm::load_json_object(
      QStringLiteral("tools/map_pipeline/map_bounds.json"), &error);
  ASSERT_TRUE(bounds_object.has_value()) << error.toStdString();
  CampaignFilm::MapBounds bounds;
  bounds.lon_min = bounds_object->value(QStringLiteral("lon_min")).toDouble();
  bounds.lon_max = bounds_object->value(QStringLiteral("lon_max")).toDouble();
  bounds.lat_min = bounds_object->value(QStringLiteral("lat_min")).toDouble();
  bounds.lat_max = bounds_object->value(QStringLiteral("lat_max")).toDouble();

  const auto path_object = CampaignFilm::load_json_object(
      QStringLiteral("assets/campaign_map/hannibal_path.json"), &error);
  ASSERT_TRUE(path_object.has_value()) << error.toStdString();
  EXPECT_EQ(path_object->value(QStringLiteral("lines")).toArray().size(), 8);
  const auto march = CampaignFilm::parse_march(*path_object, &error);
  ASSERT_TRUE(march.has_value()) << error.toStdString();
  for (const char* id : {"carthago_nova",
                         "saguntum",
                         "rhone",
                         "alps",
                         "ticinus",
                         "trebia",
                         "trasimene",
                         "cannae",
                         "zama"}) {
    EXPECT_NE(march->find(QString::fromLatin1(id)), nullptr) << id;
  }
  float previous = -1.0F;
  for (const auto& stop : march->stops) {
    const float progress = march->path.progress_at_raw_index(stop.index);
    EXPECT_GT(progress, previous) << stop.id.toStdString();
    previous = progress;
  }

  const auto catalog_object = CampaignFilm::load_json_object(
      QStringLiteral("tools/campaign_film/regions.json"), &error);
  ASSERT_TRUE(catalog_object.has_value()) << error.toStdString();
  const auto catalog = CampaignFilm::parse_catalog(*catalog_object, bounds, &error);
  ASSERT_TRUE(catalog.has_value()) << error.toStdString();
  for (const auto& region : catalog->regions) {
    if (region.polygon.size() >= 3) {
      EXPECT_EQ(CampaignFilm::triangulate(region.polygon).size(),
                (region.polygon.size() - 2) * 3)
          << region.id.toStdString();
    }
  }

  const auto spec_object = CampaignFilm::load_json_object(
      QStringLiteral("tools/campaign_film/specs/carthago_nova_to_cannae.json"), &error);
  ASSERT_TRUE(spec_object.has_value()) << error.toStdString();
  CampaignFilm::LoadContext context;
  context.march = &*march;
  context.catalog = &*catalog;
  context.bounds = bounds;
  const auto spec = CampaignFilm::parse_spec(*spec_object, context, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  EXPECT_FLOAT_EQ(spec->duration, 20.0F);
  EXPECT_EQ(spec->width, 3840);
  EXPECT_EQ(spec->height, 2160);
  EXPECT_EQ(spec->supersample, 2);

  const CampaignFilm::Timeline timeline(*spec, *march);
  EXPECT_TRUE(timeline.warnings().isEmpty())
      << timeline.warnings().join(';').toStdString();
  const auto [from, to] = timeline.route_window();
  EXPECT_NEAR(from, *march->progress_of(QStringLiteral("carthago_nova")), 1e-6F);
  EXPECT_NEAR(to, *march->progress_of(QStringLiteral("cannae")), 1e-6F);
  EXPECT_NEAR(timeline.route_progress(0.0F), from, 1e-6F);
  EXPECT_NEAR(timeline.route_progress(20.0F), to, 1e-6F);
  float last = from;
  for (float t = 0.0F; t <= 20.0F; t += 0.05F) {
    const float progress = timeline.route_progress(t);
    EXPECT_GE(progress, last - 1e-6F) << t;
    last = progress;
  }
  for (const auto& marker : spec->markers) {
    EXPECT_LT(timeline.marker_appear_time(marker), 20.0F) << marker.site.toStdString();
  }
}

TEST(CampaignFilmOverlay, PaintsMarkersOnlyWhereProjected) {
  const auto march = straight_march();
  QString error;
  const auto spec = parse(march, k_minimal_spec, &error);
  ASSERT_TRUE(spec.has_value()) << error.toStdString();
  const CampaignFilm::Timeline timeline(*spec, march);
  QImage image(200, 100, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);
  const CampaignFilm::Projector project =
      [](const QVector2D&) -> std::optional<QPointF> {
    return QPointF(100.0, 50.0);
  };
  CampaignFilm::OverlayOptions options;
  options.draw_text = false;
  CampaignFilm::paint_overlay(image,
                              *spec,
                              timeline.evaluate(3.5F),
                              project,
                              {QStringLiteral("serif"), QStringLiteral("serif")},
                              options);
  EXPECT_NE(image.pixelColor(100, 50), QColor(Qt::white));
  EXPECT_EQ(image.pixelColor(5, 5), QColor(Qt::white));
  EXPECT_EQ(CampaignFilm::format_army_line(
                {{QStringLiteral("foot"), 38000.0}, {QStringLiteral("horse"), 8000.0}}),
            QStringLiteral("38,000 foot, 8,000 horse"));
}
