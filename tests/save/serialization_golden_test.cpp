#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "save/serialization.h"
#include "systems/persistence/save_storage.h"

using namespace Engine::Core;

namespace {

auto golden_path() -> std::filesystem::path {
  return std::filesystem::path("tests") / "save" / "golden" /
         "entity_all_components.json";
}

void add_every_serialized_component(Entity* entity) {
  entity->add_component<TransformComponent>();
  entity->add_component<RenderableComponent>();
  entity->add_component<UnitComponent>();
  entity->add_component<MovementComponent>();
  entity->add_component<PlayerOrderIntentComponent>();
  entity->add_component<AttackComponent>();
  entity->add_component<AttackTargetComponent>();
  entity->add_component<CommanderComponent>();
  entity->add_component<RpgHealthComponent>();
  entity->add_component<MoraleComponent>();
  entity->add_component<UndeadComponent>();
  entity->add_component<CursedStatusComponent>();
  entity->add_component<BurningStatusComponent>();
  entity->add_component<PatrolComponent>();
  entity->add_component<BuildingComponent>();
  entity->add_component<ProductionComponent>();
  entity->add_component<AIControlledComponent>();
  entity->add_component<CaptureComponent>();
  entity->add_component<AssaultWaveComponent>();
  entity->add_component<HoldModeComponent>();
  entity->add_component<GuardModeComponent>();
  entity->add_component<HealerComponent>();
  entity->add_component<SpecialAttackComponent>();
  entity->add_component<FirePatchComponent>();
  entity->add_component<StructureFireComponent>();
  entity->add_component<CommanderGuardComponent>();
  entity->add_component<CatapultLoadingComponent>();
  entity->add_component<ElephantComponent>();
  entity->add_component<ElephantPanicComponent>();
  entity->add_component<ElephantStompImpactComponent>();
  entity->add_component<CombatStateComponent>();
  entity->add_component<HitFeedbackComponent>();
  entity->add_component<BuilderProductionComponent>();
  entity->add_component<WallSegmentComponent>();
  entity->add_component<WallConstructionSiteComponent>();
  entity->add_component<DismantleSiteComponent>();
  entity->add_component<GateComponent>();
  entity->add_component<FormationModeComponent>();
  entity->add_component<ArmyFormationMembershipComponent>();
  entity->add_component<UnitLayoutStateComponent>();
  entity->add_component<StaminaComponent>();
  entity->add_component<TerrainContextComponent>();
  entity->add_component<HomeComponent>();
  entity->add_component<FarmComponent>();
  entity->add_component<CivilianDeliveryComponent>();
  entity->add_component<ResourceCarryComponent>();
  entity->add_component<SettlementResidentComponent>();
  entity->add_component<WildlifeComponent>();
}

auto distinct_numbers(const QJsonValue& value, int& counter) -> QJsonValue {
  if (value.isObject()) {
    QJsonObject out;
    const QJsonObject in = value.toObject();
    for (auto it = in.begin(); it != in.end(); ++it) {
      if (it.key() == "id") {
        out[it.key()] = it.value();
      } else {
        out[it.key()] = distinct_numbers(it.value(), counter);
      }
    }
    return out;
  }
  if (value.isArray()) {
    QJsonArray out;
    for (const auto element : value.toArray()) {
      out.append(distinct_numbers(element, counter));
    }
    return out;
  }
  if (value.isDouble()) {
    ++counter;
    return 1.0 + static_cast<double>(counter % 5) * 0.5;
  }
  if (value.isBool()) {
    return !value.toBool();
  }
  return value;
}

auto pretty(const QJsonObject& object) -> QByteArray {
  return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

auto build_report() -> QJsonObject {
  World world;
  Entity* entity = world.create_entity();
  add_every_serialized_component(entity);

  QJsonObject report;
  const QJsonObject defaults = Serialization::serialize_entity(entity);
  report["defaults"] = defaults;

  int counter = 0;
  const QJsonObject mutated_in = distinct_numbers(defaults, counter).toObject();
  report["mutated_in"] = mutated_in;

  World restored_world;
  Entity* restored = restored_world.create_entity();
  Serialization::deserialize_entity(restored, mutated_in);
  report["mutated_out"] = Serialization::serialize_entity(restored);

  const QJsonDocument world_doc = Serialization::serialize_world(&world);
  QJsonObject world_obj = world_doc.object();
  report["world_envelope_keys"] = QJsonArray::fromStringList(world_obj.keys());
  return report;
}

} // namespace

TEST(SerializationGoldenTest, EveryComponentKeepsItsExactSavedShape) {
  const QByteArray actual = pretty(build_report());

  if (std::getenv("SOI_UPDATE_GOLDEN") != nullptr) {
    QFile out(QString::fromStdString(golden_path().string()));
    ASSERT_TRUE(out.open(QIODevice::WriteOnly));
    out.write(actual);
    out.close();
    GTEST_SKIP() << "golden rewritten";
  }

  QFile golden(QString::fromStdString(golden_path().string()));
  ASSERT_TRUE(golden.open(QIODevice::ReadOnly))
      << "run from the repo root; golden is " << golden_path().string();
  EXPECT_EQ(golden.readAll().toStdString(), actual.toStdString());
}

TEST(SerializationGoldenTest, TheSaveDatabaseKeepsItsExactSchema) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath("schema.sqlite");

  {
    Game::Systems::SaveStorage storage(path);
    QString error;
    ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();
  }

  QString dump;
  {
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "schema_golden_probe");
    database.setDatabaseName(path);
    ASSERT_TRUE(database.open());
    QSqlQuery query(database);
    ASSERT_TRUE(query.exec("SELECT type, name, sql FROM sqlite_master WHERE sql IS "
                           "NOT NULL ORDER BY name"));
    while (query.next()) {
      dump += query.value(0).toString() + "|" + query.value(1).toString() + "|" +
              query.value(2).toString() + "\n";
    }
    QSqlQuery version(database);
    ASSERT_TRUE(version.exec("PRAGMA user_version") && version.next());
    dump += "user_version=" + version.value(0).toString() + "\n";
    database.close();
  }
  QSqlDatabase::removeDatabase("schema_golden_probe");

  const std::filesystem::path golden_file =
      std::filesystem::path("tests") / "save" / "golden" / "save_database_schema.txt";
  QFile golden(QString::fromStdString(golden_file.string()));
  if (std::getenv("SOI_UPDATE_GOLDEN") != nullptr) {
    ASSERT_TRUE(golden.open(QIODevice::WriteOnly));
    golden.write(dump.toUtf8());
    golden.close();
    GTEST_SKIP() << "golden rewritten";
  }
  ASSERT_TRUE(golden.open(QIODevice::ReadOnly));
  EXPECT_EQ(golden.readAll().toStdString(), dump.toUtf8().toStdString());
}
