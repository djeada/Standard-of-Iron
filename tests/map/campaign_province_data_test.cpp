#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>

#include <cmath>
#include <gtest/gtest.h>
#include <numeric>

namespace {

auto repo_root() -> QDir {
  QDir dir = QDir::current();
  for (int depth = 0; depth < 8; ++depth) {
    if (dir.exists(QStringLiteral("assets/campaign_map/provinces.json"))) {
      return dir;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QDir::current();
}

auto read_json(const QString& relative) -> QJsonObject {
  QFile file(repo_root().filePath(relative));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QJsonDocument::fromJson(file.readAll()).object();
}

auto ids_of(const QJsonObject& root, const QString& array_key) -> QSet<QString> {
  QSet<QString> ids;
  for (const auto& value : root.value(array_key).toArray()) {
    const QString id = value.toObject().value(QStringLiteral("id")).toString();
    if (!id.isEmpty()) {
      ids.insert(id);
    }
  }
  return ids;
}

constexpr const char* k_regenerate_hint =
    "assets/campaign_map/provinces.json is missing; run "
    "`python3 tools/map_pipeline/provinces.py` to include this check";

auto generated() -> QJsonObject {
  return read_json(QStringLiteral("assets/campaign_map/provinces.json"));
}

auto drawn_ids() -> QSet<QString> {
  return ids_of(generated(), QStringLiteral("provinces"));
}

auto whitelist() -> QJsonObject {
  return read_json(QStringLiteral("assets/campaign_map/valid_provinces.json"));
}

auto ownership() -> QJsonObject {
  return read_json(QStringLiteral("assets/campaign_map/campaign_state.json"));
}

auto sorted_list(const QSet<QString>& ids) -> std::string {
  QStringList list(ids.begin(), ids.end());
  list.sort();
  return list.join(QStringLiteral(", ")).toStdString();
}

TEST(CampaignProvinceDataTest, TheWhitelistNamesExactlyTheProvincesThatExist) {
  const QSet<QString> drawn = drawn_ids();
  if (drawn.isEmpty()) {
    GTEST_SKIP() << k_regenerate_hint;
  }

  const QSet<QString> listed = ids_of(whitelist(), QStringLiteral("provinces"));
  ASSERT_FALSE(listed.isEmpty()) << "valid_provinces.json could not be read";

  EXPECT_EQ(sorted_list(listed - drawn), std::string())
      << "valid_provinces.json lists provinces the pipeline does not generate - "
         "regenerate provinces.json or drop them from the whitelist";
  EXPECT_EQ(sorted_list(drawn - listed), std::string())
      << "the pipeline generates provinces valid_provinces.json does not list";
}

TEST(CampaignProvinceDataTest, EveryProvinceIsOwnedByExactlyOnePower) {

  const QSet<QString> listed = ids_of(whitelist(), QStringLiteral("provinces"));
  ASSERT_FALSE(listed.isEmpty()) << "valid_provinces.json could not be read";

  const QJsonObject state = ownership();
  const QJsonArray entries = state.value(QStringLiteral("provinces")).toArray();
  ASSERT_FALSE(entries.isEmpty()) << "campaign_state.json could not be read";

  const QSet<QString> known_owners = {
      QStringLiteral("rome"), QStringLiteral("carthage"), QStringLiteral("neutral")};

  QSet<QString> assigned;
  for (const auto& value : entries) {
    const QJsonObject entry = value.toObject();
    const QString id = entry.value(QStringLiteral("id")).toString();
    const QString owner = entry.value(QStringLiteral("owner")).toString();

    EXPECT_TRUE(listed.contains(id))
        << "campaign_state.json assigns " << owner.toStdString() << " to '"
        << id.toStdString() << "', which valid_provinces.json never names";
    EXPECT_TRUE(known_owners.contains(owner))
        << id.toStdString() << " is held by an unknown power '" << owner.toStdString()
        << "'";
    EXPECT_FALSE(assigned.contains(id))
        << id.toStdString() << " is assigned an owner twice";
    assigned.insert(id);
  }

  EXPECT_EQ(sorted_list(listed - assigned), std::string())
      << "these provinces exist but campaign_state.json never says who holds them";
}

TEST(CampaignProvinceDataTest, EveryProvinceIsDrawableAndLabellable) {
  const QJsonArray provinces = generated().value(QStringLiteral("provinces")).toArray();
  if (provinces.isEmpty()) {
    GTEST_SKIP() << k_regenerate_hint;
  }

  for (const auto& value : provinces) {
    const QJsonObject province = value.toObject();
    const std::string id =
        province.value(QStringLiteral("id")).toString().toStdString();

    const QJsonArray triangles = province.value(QStringLiteral("triangles")).toArray();
    EXPECT_GE(triangles.size(), 3) << id << " has no fill geometry";
    EXPECT_EQ(triangles.size() % 3, 0) << id << " has a partial triangle";

    for (const auto& point : triangles) {
      const QJsonArray uv = point.toArray();
      ASSERT_EQ(uv.size(), 2) << id << " has a malformed vertex";
      const double u = uv.at(0).toDouble();
      const double v = uv.at(1).toDouble();
      EXPECT_GE(u, 0.0) << id;
      EXPECT_LE(u, 1.0) << id;
      EXPECT_GE(v, 0.0) << id;
      EXPECT_LE(v, 1.0) << id;
    }

    const QJsonArray label = province.value(QStringLiteral("label_uv")).toArray();
    ASSERT_EQ(label.size(), 2) << id << " has no label anchor";
    EXPECT_GE(label.at(0).toDouble(), 0.0) << id;
    EXPECT_LE(label.at(0).toDouble(), 1.0) << id;
    EXPECT_GE(label.at(1).toDouble(), 0.0) << id;
    EXPECT_LE(label.at(1).toDouble(), 1.0) << id;
  }
}

constexpr double k_weld_tolerance = 5e-5;

using Cell = QPair<qint64, qint64>;

auto cell_of(double u, double v) -> Cell {
  return {static_cast<qint64>(std::floor(u / k_weld_tolerance)),
          static_cast<qint64>(std::floor(v / k_weld_tolerance))};
}

class DisjointSet {
public:
  explicit DisjointSet(int count)
      : m_parent(count) {
    std::iota(m_parent.begin(), m_parent.end(), 0);
  }

  auto find(int index) -> int {
    while (m_parent[index] != index) {
      m_parent[index] = m_parent[m_parent[index]];
      index = m_parent[index];
    }
    return index;
  }

  void unite(int a, int b) {
    const int root_a = find(a);
    const int root_b = find(b);
    if (root_a != root_b) {
      m_parent[root_a] = root_b;
    }
  }

private:
  QVector<int> m_parent;
};

auto piece_count(const QJsonArray& triangles) -> int {
  const int count = triangles.size() / 3;
  if (count <= 0) {
    return 0;
  }

  QHash<Cell, QVector<int>> occupants;
  for (int triangle = 0; triangle < count; ++triangle) {
    for (int corner = 0; corner < 3; ++corner) {
      const QJsonArray uv = triangles.at(triangle * 3 + corner).toArray();
      occupants[cell_of(uv.at(0).toDouble(), uv.at(1).toDouble())].push_back(triangle);
    }
  }

  DisjointSet pieces(count);
  for (int triangle = 0; triangle < count; ++triangle) {
    for (int corner = 0; corner < 3; ++corner) {
      const QJsonArray uv = triangles.at(triangle * 3 + corner).toArray();
      const Cell cell = cell_of(uv.at(0).toDouble(), uv.at(1).toDouble());
      for (qint64 dx = -1; dx <= 1; ++dx) {
        for (qint64 dy = -1; dy <= 1; ++dy) {
          for (const int neighbour :
               occupants.value({cell.first + dx, cell.second + dy})) {
            pieces.unite(triangle, neighbour);
          }
        }
      }
    }
  }

  QSet<int> roots;
  for (int triangle = 0; triangle < count; ++triangle) {
    roots.insert(pieces.find(triangle));
  }
  return roots.size();
}

TEST(CampaignProvinceDataTest, EveryProvinceIsOneUnbrokenStretchOfLand) {
  const QJsonArray provinces = generated().value(QStringLiteral("provinces")).toArray();
  if (provinces.isEmpty()) {
    GTEST_SKIP() << k_regenerate_hint;
  }

  for (const auto& value : provinces) {
    const QJsonObject province = value.toObject();
    const std::string id =
        province.value(QStringLiteral("id")).toString().toStdString();
    const QJsonArray triangles = province.value(QStringLiteral("triangles")).toArray();
    ASSERT_GE(triangles.size(), 3) << id << " has no fill geometry";

    EXPECT_EQ(piece_count(triangles), 1)
        << id
        << " is drawn as several disconnected pieces; a province must be one "
           "contiguous stretch of land, so regenerate provinces.json with "
           "tools/map_pipeline/provinces.py and check its exclave warnings";
  }
}

} // namespace
