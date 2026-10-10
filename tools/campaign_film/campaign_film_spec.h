#pragma once

#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "ui/campaign_route_path.h"

namespace CampaignFilm {

enum class Ease : std::uint8_t {
  Linear,
  Smooth,
  EaseIn,
  EaseOut
};
enum class Interp : std::uint8_t {
  Keys,
  Spline
};
enum class Ends : std::uint8_t {
  Moving,
  Ease
};

[[nodiscard]] auto parse_ease(const QString& name, Ease fallback) -> Ease;
[[nodiscard]] auto ease_value(Ease ease, float t) -> float;

struct MapBounds {
  double lon_min = -10.0;
  double lon_max = 18.0;
  double lat_min = 30.0;
  double lat_max = 47.5;

  [[nodiscard]] auto to_uv(double lon, double lat) const -> QVector2D {
    return {static_cast<float>((lon - lon_min) / (lon_max - lon_min)),
            static_cast<float>((lat - lat_min) / (lat_max - lat_min))};
  }
};

struct MarchStop {
  QString id;
  QString name;
  QString date;
  QString kind;
  QVector2D uv;
  std::size_t index = 0;
};

struct March {
  std::vector<QVector2D> points;
  std::vector<MarchStop> stops;
  CampaignMapFilm::RoutePath path;

  [[nodiscard]] auto find(const QString& id) const -> const MarchStop*;
  [[nodiscard]] auto progress_of(const QString& id) const -> std::optional<float>;
};

[[nodiscard]] auto parse_march(const QJsonObject& hannibal_path,
                               QString* error) -> std::optional<March>;

struct CatalogRegion {
  QString id;
  QString name;
  QStringList provinces;
  std::vector<QVector2D> polygon;
};

struct Catalog {
  MapBounds bounds;
  std::vector<CatalogRegion> regions;

  [[nodiscard]] auto find(const QString& id) const -> const CatalogRegion*;
};

[[nodiscard]] auto parse_catalog(const QJsonObject& object,
                                 const MapBounds& bounds,
                                 QString* error) -> std::optional<Catalog>;

[[nodiscard]] auto
triangulate(const std::vector<QVector2D>& polygon) -> std::vector<QVector2D>;

struct Target {
  enum class Kind : std::uint8_t {
    Uv,
    Stop,
    Head
  };
  Kind kind = Kind::Uv;
  QVector2D uv{0.5F, 0.5F};
  QString stop;
  QVector2D offset;
  float smooth_seconds = 0.0F;
};

struct CameraKey {
  float time = 0.0F;
  Target look;
  float distance = 1.0F;
  float yaw = 180.0F;
  float pitch = 60.0F;
  float fov = 45.0F;
  Ease ease = Ease::Smooth;
};

struct RouteKey {
  float time = 0.0F;
  float progress = 0.0F;
  QString at;
  Ease ease = Ease::Linear;
};

struct RouteTrack {
  bool enabled = false;
  QString from;
  QString to;
  std::vector<RouteKey> keys;
  CampaignMapFilm::RouteStyle style;
};

struct Window {
  float in = 0.0F;
  float out = std::numeric_limits<float>::infinity();
  float fade = 0.6F;

  [[nodiscard]] auto alpha(float time) const -> float;
};

struct ScalarKey {
  float time = 0.0F;
  float value = 0.0F;
  Ease ease = Ease::Smooth;
};

struct RegionTrack {
  QString id;
  QString name;
  QStringList provinces;
  std::vector<QVector2D> triangles;
  QVector4D fill{0.93F, 0.74F, 0.38F, 0.30F};
  QVector4D rim{0.98F, 0.83F, 0.48F, 0.95F};
  float rim_px = 2.0F;
  float dim_outside = 0.0F;
  Window window;
  std::vector<ScalarKey> keys;
};

struct MarkerTrack {
  QString site;
  QString name;
  QString date;
  QString kind = QStringLiteral("battle");
  QVector2D uv;
  bool show_name = true;
  bool show_date = true;
  bool on_arrival = false;
  float hold = std::numeric_limits<float>::infinity();
  float label_hold = std::numeric_limits<float>::infinity();
  QString label_side;
  Window window;
};

struct LabelTrack {
  QString text;
  Target at;
  QString style = QStringLiteral("place");
  float size_px = 0.0F;
  QPointF offset_px;
  Window window;
};

struct StampTrack {
  QString title;
  QString subtitle;
  Window window;
};

struct ArmyKey {
  float time = 0.0F;
  QString at;
  QMap<QString, double> values;
};

struct ArmyTrack {
  QString id;
  QString label;
  QString side;
  QString source;
  Target anchor;
  bool linear = false;
  Window window;
  std::vector<ArmyKey> keys;
};

struct Spec {
  QString id;
  QString title;
  int width = 1920;
  int height = 1080;
  int fps = 30;
  int supersample = 1;
  float duration = 10.0F;
  float reference_height = 1080.0F;
  float terrain_height_scale = 0.10F;
  bool province_fills = false;
  float province_fill_alpha = 0.6F;
  bool show_symbols = true;
  bool show_borders = false;
  bool show_game_route = false;
  bool burn_text = true;
  bool forbid_world_edge = false;
  float drape_radius = 0.008F;
  Interp interp = Interp::Spline;
  Ends ends = Ends::Ease;
  std::vector<CameraKey> camera;
  RouteTrack route;
  std::vector<RegionTrack> regions;
  std::vector<MarkerTrack> markers;
  std::vector<LabelTrack> labels;
  std::vector<StampTrack> stamps;
  std::vector<ArmyTrack> armies;

  [[nodiscard]] auto frame_count() const -> int;
};

struct LoadContext {
  const March* march = nullptr;
  const Catalog* catalog = nullptr;
  MapBounds bounds;
};

[[nodiscard]] auto parse_spec(const QJsonObject& object,
                              const LoadContext& context,
                              QString* error) -> std::optional<Spec>;

[[nodiscard]] auto load_json_object(const QString& path,
                                    QString* error) -> std::optional<QJsonObject>;

struct ArmyValue {
  QString id;
  QVector2D uv;
  float alpha = 0.0F;
  QMap<QString, double> values;
};

struct LabelValue {
  std::size_t index = 0;
  QVector2D uv;
  float alpha = 0.0F;
};

struct MarkerValue {
  std::size_t index = 0;
  float alpha = 0.0F;
  float text_alpha = 0.0F;
  float pop = 1.0F;
};

struct StampValue {
  std::size_t index = 0;
  float alpha = 0.0F;
};

struct FrameEval {
  float time = 0.0F;
  CampaignMapFilm::CameraPose camera;
  float route_from = 0.0F;
  float route_head = 0.0F;
  float route_end = 0.0F;
  QVector2D head_uv;
  std::vector<float> region_amounts;
  std::vector<MarkerValue> markers;
  std::vector<LabelValue> labels;
  std::vector<StampValue> stamps;
  std::vector<ArmyValue> armies;
};

class Timeline {
public:
  Timeline(const Spec& spec, const March& march);

  [[nodiscard]] auto route_window() const -> std::pair<float, float> {
    return {m_window_from, m_window_to};
  }
  [[nodiscard]] auto route_progress(float time) const -> float;
  [[nodiscard]] auto arrival_time(float progress) const -> std::optional<float>;
  [[nodiscard]] auto arrival_time(const QString& stop) const -> std::optional<float>;
  [[nodiscard]] auto resolve(const Target& target, float time) const -> QVector2D;
  [[nodiscard]] auto camera(float time) const -> CampaignMapFilm::CameraPose;
  [[nodiscard]] auto evaluate(float time) const -> FrameEval;
  [[nodiscard]] auto
  frame_state(const FrameEval& eval) const -> CampaignMapFilm::FrameState;
  [[nodiscard]] auto marker_appear_time(const MarkerTrack& marker) const -> float;
  [[nodiscard]] auto army_key_time(const ArmyKey& key) const -> std::optional<float>;
  [[nodiscard]] auto warnings() const -> const QStringList& { return m_warnings; }

private:
  [[nodiscard]] auto key_progress(const RouteKey& key) const -> float;

  const Spec& m_spec;
  const March& m_march;
  float m_window_from = 0.0F;
  float m_window_to = 1.0F;
  std::vector<float> m_key_progress;
  QStringList m_warnings;
};

[[nodiscard]] auto hermite_channel(const std::vector<float>& times,
                                   const std::vector<float>& values,
                                   float time,
                                   Ends ends) -> float;

[[nodiscard]] auto keyed_channel(const std::vector<float>& times,
                                 const std::vector<float>& values,
                                 const std::vector<Ease>& eases,
                                 float time) -> float;

} // namespace CampaignFilm
