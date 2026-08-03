#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <gtest/gtest.h>

// The campaign map is described by three files that have to agree: the
// generated geometry, the id whitelist the docs call the source of truth, and
// the 218 BC ownership table. Nothing in the build checks that, and by the time
// this test was written all three had drifted -- two of them listed a
// `sicily_carthaginian` province that `tools/map_pipeline/provinces.py` has
// never emitted, so an owner assigned to it would have silently painted
// nothing.
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

auto generated() -> QJsonObject {
  return read_json(QStringLiteral("assets/campaign_map/provinces.json"));
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
  const QSet<QString> drawn = ids_of(generated(), QStringLiteral("provinces"));
  ASSERT_FALSE(drawn.isEmpty()) << "provinces.json could not be read";

  const QSet<QString> listed = ids_of(whitelist(), QStringLiteral("provinces"));
  ASSERT_FALSE(listed.isEmpty()) << "valid_provinces.json could not be read";

  EXPECT_EQ(sorted_list(listed - drawn), std::string())
      << "valid_provinces.json lists provinces the pipeline does not generate";
  EXPECT_EQ(sorted_list(drawn - listed), std::string())
      << "the pipeline generates provinces valid_provinces.json does not list";
}

TEST(CampaignProvinceDataTest, EveryProvinceIsOwnedByExactlyOnePower) {
  const QSet<QString> drawn = ids_of(generated(), QStringLiteral("provinces"));
  ASSERT_FALSE(drawn.isEmpty());

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

    EXPECT_TRUE(drawn.contains(id))
        << "campaign_state.json assigns " << owner.toStdString() << " to '"
        << id.toStdString() << "', which the map never draws";
    EXPECT_TRUE(known_owners.contains(owner))
        << id.toStdString() << " is held by an unknown power '" << owner.toStdString()
        << "'";
    EXPECT_FALSE(assigned.contains(id))
        << id.toStdString() << " is assigned an owner twice";
    assigned.insert(id);
  }

  // An unassigned province falls back to the neutral wash, which looks like a
  // deliberate choice and is usually a forgotten one.
  EXPECT_EQ(sorted_list(drawn - assigned), std::string())
      << "these provinces are drawn but campaign_state.json never says who holds them";
}

TEST(CampaignProvinceDataTest, EveryProvinceIsDrawableAndLabellable) {
  const QJsonArray provinces = generated().value(QStringLiteral("provinces")).toArray();
  ASSERT_FALSE(provinces.isEmpty());

  for (const auto& value : provinces) {
    const QJsonObject province = value.toObject();
    const std::string id =
        province.value(QStringLiteral("id")).toString().toStdString();

    const QJsonArray triangles = province.value(QStringLiteral("triangles")).toArray();
    EXPECT_GE(triangles.size(), 3) << id << " has no fill geometry";
    EXPECT_EQ(triangles.size() % 3, 0) << id << " has a partial triangle";

    // Everything the renderer draws lives in the map's UV square. A stray
    // vertex outside it is geometry laid over open ocean.
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

} // namespace
