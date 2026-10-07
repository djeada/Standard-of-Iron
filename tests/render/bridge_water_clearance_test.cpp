#include <QDir>
#include <QString>
#include <QStringList>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "game/map/bridge_geometry.h"
#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "render/gl/mesh.h"
#include "render/ground/linear_feature_geometry.h"

namespace {

constexpr float k_required_clearance = 0.05F;
constexpr float k_sample_spacing = 0.25F;
constexpr float k_bucket_size = 1.0F;

using Triangle = std::array<QVector3D, 3>;

auto triangle_height_at(const Triangle& tri, float x, float z) -> std::optional<float> {
  const QVector3D& a = tri[0];
  const QVector3D& b = tri[1];
  const QVector3D& c = tri[2];
  float const denom =
      (b.z() - c.z()) * (a.x() - c.x()) + (c.x() - b.x()) * (a.z() - c.z());
  if (std::abs(denom) < 1.0e-7F) {
    return std::nullopt;
  }
  float const w0 =
      ((b.z() - c.z()) * (x - c.x()) + (c.x() - b.x()) * (z - c.z())) / denom;
  float const w1 =
      ((c.z() - a.z()) * (x - c.x()) + (a.x() - c.x()) * (z - c.z())) / denom;
  float const w2 = 1.0F - w0 - w1;
  constexpr float k_epsilon = 1.0e-4F;
  if (w0 < -k_epsilon || w1 < -k_epsilon || w2 < -k_epsilon) {
    return std::nullopt;
  }
  return w0 * a.y() + w1 * b.y() + w2 * c.y();
}

class SurfaceField {
public:
  SurfaceField(float min_x, float max_x, float min_z, float max_z)
      : m_min_x(min_x)
      , m_max_x(max_x)
      , m_min_z(min_z)
      , m_max_z(max_z) {}

  void add(const Render::GL::Mesh* mesh) {
    if (mesh == nullptr) {
      return;
    }
    const auto& vertices = mesh->get_vertices();
    const auto& indices = mesh->get_indices();
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
      Triangle tri;
      for (std::size_t corner = 0; corner < 3; ++corner) {
        const auto& p = vertices[indices[i + corner]].position;
        tri[corner] = QVector3D(p[0], p[1], p[2]);
      }
      float const lo_x = std::min({tri[0].x(), tri[1].x(), tri[2].x()});
      float const hi_x = std::max({tri[0].x(), tri[1].x(), tri[2].x()});
      float const lo_z = std::min({tri[0].z(), tri[1].z(), tri[2].z()});
      float const hi_z = std::max({tri[0].z(), tri[1].z(), tri[2].z()});
      if (hi_x < m_min_x || lo_x > m_max_x || hi_z < m_min_z || lo_z > m_max_z) {
        continue;
      }
      auto const index = static_cast<int>(m_triangles.size());
      m_triangles.push_back(tri);
      for (int bx = bucket(std::max(lo_x, m_min_x), m_min_x);
           bx <= bucket(std::min(hi_x, m_max_x), m_min_x);
           ++bx) {
        for (int bz = bucket(std::max(lo_z, m_min_z), m_min_z);
             bz <= bucket(std::min(hi_z, m_max_z), m_min_z);
             ++bz) {
          m_buckets[key(bx, bz)].push_back(index);
        }
      }
    }
  }

  [[nodiscard]] auto top_at(float x, float z) const -> std::optional<float> {
    auto const found = m_buckets.find(key(bucket(x, m_min_x), bucket(z, m_min_z)));
    if (found == m_buckets.end()) {
      return std::nullopt;
    }
    std::optional<float> top;
    for (int index : found->second) {
      auto const height = triangle_height_at(m_triangles[index], x, z);
      if (height.has_value() && (!top.has_value() || *height > *top)) {
        top = height;
      }
    }
    return top;
  }

private:
  static auto bucket(float value, float origin) -> int {
    return static_cast<int>(std::floor((value - origin) / k_bucket_size));
  }
  static auto key(int bx, int bz) -> long long {
    return (static_cast<long long>(bx) << 32) ^ static_cast<unsigned int>(bz);
  }

  float m_min_x;
  float m_max_x;
  float m_min_z;
  float m_max_z;
  std::vector<Triangle> m_triangles;
  std::unordered_map<long long, std::vector<int>> m_buckets;
};

auto shipped_maps() -> QStringList {
  QDir const dir(QStringLiteral("assets/maps"));
  QStringList result;
  for (const QString& file :
       dir.entryList({QStringLiteral("map_*.json")}, QDir::Files, QDir::Name)) {
    result.push_back(dir.filePath(file));
  }
  return result;
}

} // namespace

TEST(BridgeWaterClearanceTest, ShippedBridgeDecksStayAboveTheDrawnWater) {
  const QStringList maps = shipped_maps();
  ASSERT_FALSE(maps.isEmpty());

  int checked_bridges = 0;
  for (const QString& path : maps) {
    SCOPED_TRACE(path.toStdString());
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(path, map, &error))
        << error.toStdString();
    if (map.bridges.empty()) {
      continue;
    }

    Game::Map::TerrainHeightMap terrain(
        map.grid.width, map.grid.height, map.grid.tile_size);
    terrain.apply_biome_variation(map.biome);
    terrain.build_from_features(map.terrain);
    terrain.add_lakes(map.lakes);
    terrain.add_river_segments(map.rivers);
    terrain.add_bridges(map.bridges);
    float const tile = terrain.get_tile_size();

    const auto settings = Render::Ground::make_water_surface_ribbon_settings(terrain);
    std::vector<Render::Ground::LinearFeatureRibbonSegment> segments;
    for (const auto& river : terrain.get_river_segments()) {
      segments.push_back({river.start, river.end, river.width});
    }
    std::vector<std::unique_ptr<Render::GL::Mesh>> water =
        Render::Ground::build_linear_ribbon_meshes(segments, tile, settings);
    for (auto& junction : Render::Ground::build_linear_feature_junction_meshes(
             segments, tile, settings)) {
      water.push_back(std::move(junction.mesh));
    }
    for (const auto& lake : terrain.get_lakes()) {
      water.push_back(Render::Ground::build_lake_surface_mesh(lake, tile));
    }

    int bridge_index = 0;
    for (const auto& bridge : terrain.get_bridges()) {
      SCOPED_TRACE(bridge_index++);
      auto const deck = Render::Ground::build_bridge_mesh(bridge, tile, terrain);
      ASSERT_NE(deck, nullptr);

      QVector3D direction = bridge.end - bridge.start;
      direction.setY(0.0F);
      float const length = direction.length();
      direction.normalize();
      QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
      float const width = std::max(bridge.width, Game::Map::k_min_bridge_width);
      float const landing_run = Game::Map::bridge_visual_landing_run(width);
      float const lateral_reach = width * 0.5F * 0.9F;

      float min_x = std::numeric_limits<float>::max();
      float max_x = std::numeric_limits<float>::lowest();
      float min_z = std::numeric_limits<float>::max();
      float max_z = std::numeric_limits<float>::lowest();
      for (const QVector3D& corner :
           {bridge.start - direction * landing_run + perpendicular * width,
            bridge.start - direction * landing_run - perpendicular * width,
            bridge.end + direction * landing_run + perpendicular * width,
            bridge.end + direction * landing_run - perpendicular * width}) {
        min_x = std::min(min_x, corner.x());
        max_x = std::max(max_x, corner.x());
        min_z = std::min(min_z, corner.z());
        max_z = std::max(max_z, corner.z());
      }
      SurfaceField deck_field(min_x, max_x, min_z, max_z);
      deck_field.add(deck.get());
      SurfaceField water_field(min_x, max_x, min_z, max_z);
      for (const auto& mesh : water) {
        water_field.add(mesh.get());
      }

      float worst_clearance = std::numeric_limits<float>::max();
      QVector3D worst_point;
      for (float along = -landing_run; along <= length + landing_run;
           along += k_sample_spacing) {
        for (float lateral = -lateral_reach; lateral <= lateral_reach;
             lateral += k_sample_spacing) {
          QVector3D const point =
              bridge.start + direction * along + perpendicular * lateral;
          auto const water_top = water_field.top_at(point.x(), point.z());
          if (!water_top.has_value()) {
            continue;
          }
          auto const deck_top = deck_field.top_at(point.x(), point.z());
          if (deck_top.has_value() && *deck_top - *water_top < worst_clearance) {
            worst_clearance = *deck_top - *water_top;
            worst_point = point;
          }
        }
      }
      EXPECT_GE(worst_clearance, k_required_clearance)
          << "water reaches the deck at (" << worst_point.x() << ", " << worst_point.z()
          << ")";
      ++checked_bridges;
    }
  }
  EXPECT_GT(checked_bridges, 0);
}

TEST(BridgeWaterClearanceTest, LandingOverANeighbouringChannelStaysAboveItsWater) {
  Game::Map::TerrainHeightMap terrain(64, 64, 1.0F);
  terrain.add_river_segments({
      {QVector3D(-30.0F, 0.0F, -4.0F), QVector3D(30.0F, 0.0F, -4.0F), 3.0F},
      {QVector3D(-30.0F, 0.0F, 2.0F), QVector3D(30.0F, 0.0F, 2.0F), 3.0F},
  });
  Game::Map::Bridge bridge;
  bridge.start = QVector3D(0.0F, 0.0F, -8.0F);
  bridge.end = QVector3D(0.0F, 0.0F, 0.5F);
  terrain.add_bridges({bridge});
  ASSERT_EQ(terrain.get_bridges().size(), 1U);
  const Game::Map::Bridge& fitted = terrain.get_bridges().front();

  auto const deck = Render::Ground::build_bridge_mesh(fitted, 1.0F, terrain);
  ASSERT_NE(deck, nullptr);
  SurfaceField deck_field(-20.0F, 20.0F, -20.0F, 20.0F);
  deck_field.add(deck.get());

  const auto& rivers = terrain.get_river_segments();
  float const neighbour_level = rivers.back().start.y();
  float const landing_run = Game::Map::bridge_visual_landing_run(fitted.width);
  QVector3D direction = fitted.end - fitted.start;
  direction.setY(0.0F);
  direction.normalize();

  int wet_samples = 0;
  for (float along = 0.0F; along <= landing_run; along += 0.1F) {
    QVector3D const point = fitted.end + direction * along;
    if (!Game::Map::drawn_water_level_at(
             {rivers.back()}, {}, 1.0F, point.x(), point.z())
             .has_value()) {
      continue;
    }
    ++wet_samples;
    auto const deck_top = deck_field.top_at(point.x(), point.z());
    ASSERT_TRUE(deck_top.has_value());
    EXPECT_GE(*deck_top, neighbour_level + Game::Map::k_bridge_water_clearance - 0.03F)
        << "along " << along;
  }
  EXPECT_GT(wet_samples, 0);
}
