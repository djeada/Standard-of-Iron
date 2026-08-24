#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>
#include <set>

#include "tools/arena/arena_scenario.h"
#include "tools/arena/arena_scenarios.h"

namespace {

constexpr char k_manifest_path[] = "tools/arena/rpg_gate_manifest.json";

auto load_manifest() -> QJsonObject {
  QFile file(QString::fromLatin1(k_manifest_path));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  QJsonParseError error{};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
  EXPECT_EQ(error.error, QJsonParseError::NoError)
      << k_manifest_path << ": " << error.errorString().toStdString();
  return document.object();
}

auto registered_rpg_ids() -> std::set<QString> {
  std::set<QString> ids;
  for (const auto& definition : Arena::Scenarios::definitions()) {
    if (definition.id.startsWith(QStringLiteral("rpg_"))) {
      ids.insert(definition.id);
    }
  }
  return ids;
}

auto manifest_ids(const QJsonObject& manifest) -> std::set<QString> {
  std::set<QString> ids;
  for (const auto& value : manifest.value(QStringLiteral("scenarios")).toArray()) {
    ids.insert(value.toObject().value(QStringLiteral("id")).toString());
  }
  return ids;
}

} // namespace

TEST(ArenaRpgGateManifestTest, ManifestIsReadableAndVersioned) {
  const QJsonObject manifest = load_manifest();
  ASSERT_FALSE(manifest.isEmpty())
      << "run test binaries from the repository root; " << k_manifest_path
      << " is resolved relative to the working directory";
  EXPECT_EQ(manifest.value(QStringLiteral("schema_version")).toInt(), 1);
  EXPECT_FALSE(manifest.value(QStringLiteral("scenarios")).toArray().isEmpty());
  EXPECT_FALSE(manifest.value(QStringLiteral("unit_test_filters")).toArray().isEmpty());
}

TEST(ArenaRpgGateManifestTest, EveryRegisteredRpgScenarioIsInTheGate) {
  const std::set<QString> registered = registered_rpg_ids();
  const std::set<QString> listed = manifest_ids(load_manifest());
  ASSERT_FALSE(registered.empty());

  QStringList missing;
  for (const auto& id : registered) {
    if (listed.count(id) == 0) {
      missing.push_back(id);
    }
  }
  EXPECT_TRUE(missing.isEmpty())
      << "these rpg_* scenarios are registered but absent from " << k_manifest_path
      << ", so the gate would never run them: "
      << missing.join(QStringLiteral(", ")).toStdString();
}

TEST(ArenaRpgGateManifestTest, EveryGateEntryNamesARegisteredScenario) {
  const std::set<QString> registered = registered_rpg_ids();
  const std::set<QString> listed = manifest_ids(load_manifest());

  QStringList unknown;
  for (const auto& id : listed) {
    if (registered.count(id) == 0) {
      unknown.push_back(id);
    }
  }
  EXPECT_TRUE(unknown.isEmpty()) << "these gate entries name no registered scenario: "
                                 << unknown.join(QStringLiteral(", ")).toStdString();
}

TEST(ArenaRpgGateManifestTest, EveryEntryCarriesAKnownStatusAndOwningGate) {
  const QJsonObject manifest = load_manifest();
  for (const auto& value : manifest.value(QStringLiteral("scenarios")).toArray()) {
    const QJsonObject entry = value.toObject();
    const QString id = entry.value(QStringLiteral("id")).toString();
    const QString status = entry.value(QStringLiteral("status")).toString();
    EXPECT_TRUE(status == QStringLiteral("required_green") ||
                status == QStringLiteral("expected_red"))
        << id.toStdString() << " has unknown status '" << status.toStdString() << "'";
    EXPECT_FALSE(entry.value(QStringLiteral("gate")).toString().isEmpty())
        << id.toStdString() << " does not name the gate that owns it";
    EXPECT_FALSE(entry.value(QStringLiteral("notes")).toString().isEmpty())
        << id.toStdString() << " does not say what it proves or why it is red";
    if (status == QStringLiteral("expected_red")) {
      EXPECT_FALSE(entry.value(QStringLiteral("issue_codes")).toArray().isEmpty())
          << id.toStdString()
          << " is pinned expected_red without naming the issue codes it fails with";
    }

    const QString reproduction = entry.value(QStringLiteral("reproduction"))
                                     .toString(QStringLiteral("deterministic"));
    EXPECT_TRUE(reproduction == QStringLiteral("deterministic") ||
                reproduction == QStringLiteral("nondeterministic"))
        << id.toStdString() << " has unknown reproduction '"
        << reproduction.toStdString() << "'";
    if (reproduction == QStringLiteral("nondeterministic")) {
      EXPECT_EQ(status, QStringLiteral("expected_red"))
          << id.toStdString()
          << " is nondeterministic, which only describes a pinned defect the gate "
             "cannot provoke on demand";
    }

    if (entry.contains(QStringLiteral("repeats"))) {
      const int repeats = entry.value(QStringLiteral("repeats")).toInt();
      EXPECT_GE(repeats, 1) << id.toStdString() << " asks for " << repeats << " runs";
    }
  }
}

TEST(ArenaRpgGateManifestTest, DefaultsCoverRepeatsAndReproduction) {
  const QJsonObject defaults =
      load_manifest().value(QStringLiteral("scenario_defaults")).toObject();
  EXPECT_GE(defaults.value(QStringLiteral("repeats")).toInt(), 1);
  EXPECT_EQ(defaults.value(QStringLiteral("reproduction")).toString(),
            QStringLiteral("deterministic"))
      << "a scenario is assumed reproducible unless its entry says otherwise";
}
