#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <gtest/gtest.h>

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

} // namespace
