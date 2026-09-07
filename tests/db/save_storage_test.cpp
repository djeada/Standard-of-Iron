#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <memory>
#include <numbers>

#include "map/campaign_definition.h"
#include "systems/save_format.h"
#include "systems/save_storage.h"

using namespace Game::Systems;

namespace {

auto make_record(const QString& slot_name,
                 const QString& title,
                 const QByteArray& world = QByteArray("{\"entities\":[]}"))
    -> Save::Record {
  Save::Record record;
  record.slot_name = slot_name;
  record.title = title;
  record.map_name = QStringLiteral("Test Map");
  record.map_path = QStringLiteral("assets/maps/test.json");
  record.mode = QStringLiteral("skirmish");
  record.difficulty = QStringLiteral("normal");
  record.world = Save::pack(world);
  return record;
}

} // namespace

class SaveStorageTest : public ::testing::Test {
protected:
  void SetUp() override {
    storage = std::make_unique<SaveStorage>(":memory:");
    QString error;
    ASSERT_TRUE(storage->initialize(&error))
        << "Failed to initialize: " << error.toStdString();
  }

  void TearDown() override { storage.reset(); }

  std::unique_ptr<SaveStorage> storage;
};

TEST_F(SaveStorageTest, SaveAndLoadSlotRoundTrips) {
  QJsonObject metadata;
  metadata["player_name"] = "TestPlayer";
  metadata["game_time"] = 3600;

  Save::Record record = make_record("save_load_test", "Original Title");
  record.metadata = metadata;
  record.screenshot = QByteArray("screenshot-bytes");
  record.play_time_seconds = 42.5;

  QString error;
  ASSERT_TRUE(storage->write_slot(record, &error))
      << "Save failed: " << error.toStdString();

  Save::Record loaded;
  ASSERT_TRUE(storage->read_slot("save_load_test", loaded, &error))
      << "Load failed: " << error.toStdString();

  EXPECT_EQ(loaded.title, QString("Original Title"));
  EXPECT_EQ(loaded.screenshot, record.screenshot);
  EXPECT_DOUBLE_EQ(loaded.play_time_seconds, 42.5);
  EXPECT_EQ(loaded.metadata["player_name"].toString(), QString("TestPlayer"));
  EXPECT_EQ(loaded.metadata["game_time"].toInt(), 3600);

  QByteArray world;
  ASSERT_TRUE(Save::unpack(loaded.world, world, &error))
      << "Unpack failed: " << error.toStdString();
  EXPECT_EQ(world, QByteArray("{\"entities\":[]}"));
}

TEST_F(SaveStorageTest, WorldStateIsStoredCompressed) {
  const QByteArray world(256 * 1024, 'A');
  QString error;
  ASSERT_TRUE(storage->write_slot(make_record("compressed", "Big", world), &error));

  Save::Record loaded;
  ASSERT_TRUE(storage->read_slot("compressed", loaded, &error));
  EXPECT_EQ(loaded.world.compression, Save::Compression::Zlib);
  EXPECT_EQ(loaded.world.raw_size, world.size());
  EXPECT_LT(loaded.world.blob.size(), world.size());

  QByteArray restored;
  ASSERT_TRUE(Save::unpack(loaded.world, restored, &error));
  EXPECT_EQ(restored, world);
}

TEST_F(SaveStorageTest, CampaignAndSkirmishStateAreStoredSeparately) {
  Save::Record campaign = make_record("campaign_slot", "Campaign Save");
  campaign.mode = QStringLiteral("campaign");
  campaign.campaign_id = QStringLiteral("second_punic_war");
  campaign.mission_id = QStringLiteral("cannae");
  campaign.difficulty = QStringLiteral("hard");
  campaign.kind = Save::SlotKind::Manual;

  Save::Record skirmish = make_record("skirmish_slot", "Skirmish Save");
  skirmish.mission_id = QStringLiteral("assets/maps/river.json");
  skirmish.kind = Save::SlotKind::Autosave;

  QString error;
  ASSERT_TRUE(storage->write_slot(campaign, &error)) << error.toStdString();
  ASSERT_TRUE(storage->write_slot(skirmish, &error)) << error.toStdString();

  Save::Record loaded_campaign;
  ASSERT_TRUE(storage->read_slot("campaign_slot", loaded_campaign, &error));
  EXPECT_EQ(loaded_campaign.mode, QString("campaign"));
  EXPECT_EQ(loaded_campaign.campaign_id, QString("second_punic_war"));
  EXPECT_EQ(loaded_campaign.mission_id, QString("cannae"));
  EXPECT_EQ(loaded_campaign.difficulty, QString("hard"));
  EXPECT_EQ(loaded_campaign.kind, Save::SlotKind::Manual);

  Save::Record loaded_skirmish;
  ASSERT_TRUE(storage->read_slot("skirmish_slot", loaded_skirmish, &error));
  EXPECT_EQ(loaded_skirmish.mode, QString("skirmish"));
  EXPECT_TRUE(loaded_skirmish.campaign_id.isEmpty());
  EXPECT_EQ(loaded_skirmish.kind, Save::SlotKind::Autosave);
}

TEST_F(SaveStorageTest, OverwriteKeepsLatestContent) {
  QString error;
  Save::Record first = make_record("overwrite", "First", QByteArray("state-one"));
  first.metadata = QJsonObject{{"version", 1}};
  ASSERT_TRUE(storage->write_slot(first, &error));

  Save::Record second = make_record("overwrite", "Second", QByteArray("state-two"));
  second.metadata = QJsonObject{{"version", 2}};
  ASSERT_TRUE(storage->write_slot(second, &error));

  Save::Record loaded;
  ASSERT_TRUE(storage->read_slot("overwrite", loaded, &error));
  EXPECT_EQ(loaded.title, QString("Second"));
  EXPECT_EQ(loaded.metadata["version"].toInt(), 2);

  QByteArray world;
  ASSERT_TRUE(Save::unpack(loaded.world, world, &error));
  EXPECT_EQ(world, QByteArray("state-two"));
}

TEST_F(SaveStorageTest, LoadNonExistentSlotFails) {
  Save::Record loaded;
  QString error;
  EXPECT_FALSE(storage->read_slot("nonexistent_slot", loaded, &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(SaveStorageTest, EmptySlotNameIsRejected) {
  QString error;
  EXPECT_FALSE(storage->write_slot(make_record("", "No Slot"), &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(SaveStorageTest, ListSlotsExposesModeAndSizes) {
  QString error;
  ASSERT_TRUE(storage->write_slot(make_record("slot1", "Title 1"), &error));
  ASSERT_TRUE(storage->write_slot(make_record("slot2", "Title 2"), &error));

  const QVariantList slot_list = storage->list_slots(&error);
  ASSERT_TRUE(error.isEmpty()) << error.toStdString();
  ASSERT_EQ(slot_list.size(), 2);

  bool found = false;
  for (const QVariant& entry : slot_list) {
    const QVariantMap slot = entry.toMap();
    if (slot["slot_name"].toString() != "slot1") {
      continue;
    }
    found = true;
    EXPECT_EQ(slot["title"].toString(), QString("Title 1"));
    EXPECT_EQ(slot["mode"].toString(), QString("skirmish"));
    EXPECT_EQ(slot["kind"].toString(), QString("manual"));
    EXPECT_GT(slot["stored_size"].toLongLong(), 0);
    EXPECT_GT(slot["uncompressed_size"].toLongLong(), 0);
  }
  EXPECT_TRUE(found);
}

TEST_F(SaveStorageTest, SlotNamesByKindAreOrderedOldestFirst) {
  QString error;
  for (int i = 1; i <= 3; ++i) {
    Save::Record record =
        make_record(QStringLiteral("autosave_%1").arg(i), QStringLiteral("Auto"));
    record.kind = Save::SlotKind::Autosave;
    record.updated_at = QStringLiteral("2026-01-0%1T00:00:00.000").arg(i);
    ASSERT_TRUE(storage->write_slot(record, &error)) << error.toStdString();
  }
  Save::Record const manual = make_record("manual_slot", "Manual");
  ASSERT_TRUE(storage->write_slot(manual, &error));

  const QStringList autosaves = storage->slot_names_by_kind(Save::SlotKind::Autosave);
  ASSERT_EQ(autosaves.size(), 3);
  EXPECT_EQ(autosaves.at(0), QString("autosave_1"));
  EXPECT_EQ(autosaves.at(2), QString("autosave_3"));
}

TEST_F(SaveStorageTest, VerifySlotDetectsCorruptedPayload) {
  QString error;
  Save::Record record = make_record("corrupt", "Corrupt", QByteArray(4096, 'x'));

  record.world.blob = record.world.blob.left(record.world.blob.size() / 2);
  ASSERT_TRUE(storage->write_slot(record, &error));

  EXPECT_FALSE(storage->verify_slot("corrupt", &error));
  EXPECT_TRUE(error.contains("corrupted")) << error.toStdString();
}

TEST_F(SaveStorageTest, DeleteSlot) {
  QString error;
  ASSERT_TRUE(storage->write_slot(make_record("delete_test", "Title"), &error));
  EXPECT_TRUE(storage->slot_exists("delete_test"));

  EXPECT_TRUE(storage->delete_slot("delete_test", &error))
      << "Delete failed: " << error.toStdString();
  EXPECT_FALSE(storage->slot_exists("delete_test"));
  EXPECT_EQ(storage->list_slots(&error).size(), 0);
}

TEST_F(SaveStorageTest, DeleteNonExistentSlotFails) {
  QString error;
  EXPECT_FALSE(storage->delete_slot("nonexistent_delete", &error));
  EXPECT_FALSE(error.isEmpty());
}

TEST_F(SaveStorageTest, ComplexMetadataSurvivesRoundTrip) {
  QJsonObject nested;
  nested["nested_field"] = "nested_value";

  QJsonArray array;
  array.append(1);
  array.append(2);
  array.append(3);

  QJsonObject metadata;
  metadata["int_value"] = 42;
  metadata["double_value"] = std::numbers::pi;
  metadata["string_value"] = "test_string";
  metadata["bool_value"] = true;
  metadata["nested"] = nested;
  metadata["array"] = array;

  Save::Record record = make_record("complex_metadata", "Complex");
  record.metadata = metadata;

  QString error;
  ASSERT_TRUE(storage->write_slot(record, &error));

  Save::Record loaded;
  ASSERT_TRUE(storage->read_slot("complex_metadata", loaded, &error));
  EXPECT_EQ(loaded.metadata["int_value"].toInt(), 42);
  EXPECT_DOUBLE_EQ(loaded.metadata["double_value"].toDouble(), std::numbers::pi);
  EXPECT_EQ(loaded.metadata["string_value"].toString(), QString("test_string"));
  EXPECT_TRUE(loaded.metadata["bool_value"].toBool());
  EXPECT_EQ(loaded.metadata["nested"].toObject()["nested_field"].toString(),
            QString("nested_value"));
  EXPECT_EQ(loaded.metadata["array"].toArray().size(), 3);
}

TEST_F(SaveStorageTest, LargeSaveRoundTrips) {
  const QByteArray world(1024 * 1024, 'A');
  Save::Record record = make_record("large_data", "Large", world);
  record.screenshot = QByteArray(512 * 1024, 'B');

  QString error;
  ASSERT_TRUE(storage->write_slot(record, &error))
      << "Failed to save large data: " << error.toStdString();

  Save::Record loaded;
  ASSERT_TRUE(storage->read_slot("large_data", loaded, &error));
  EXPECT_EQ(loaded.screenshot.size(), 512 * 1024);

  QByteArray restored;
  ASSERT_TRUE(Save::unpack(loaded.world, restored, &error));
  EXPECT_EQ(restored.size(), 1024 * 1024);
}

TEST_F(SaveStorageTest, MultipleSavesAndDeletes) {
  QString error;
  for (int i = 0; i < 10; i++) {
    ASSERT_TRUE(storage->write_slot(
        make_record(QString("slot_%1").arg(i), QString("Title %1").arg(i)), &error));
  }

  EXPECT_EQ(storage->list_slots(&error).size(), 10);

  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(storage->delete_slot(QString("slot_%1").arg(i), &error));
  }

  const QVariantList remaining = storage->list_slots(&error);
  EXPECT_EQ(remaining.size(), 5);
  for (const QVariant& entry : remaining) {
    const int slot_num = entry.toMap()["slot_name"].toString().mid(5).toInt();
    EXPECT_GE(slot_num, 5);
    EXPECT_LT(slot_num, 10);
  }
}

TEST(SaveStorageSchemaTest, DatabaseWithAForeignSchemaIsRebuiltFromScratch) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString database_path = temp_dir.filePath(QStringLiteral("saves.sqlite"));

  {
    QSqlDatabase legacy = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), QStringLiteral("legacy_schema_fixture"));
    legacy.setDatabaseName(database_path);
    ASSERT_TRUE(legacy.open());

    QSqlQuery query(legacy);
    ASSERT_TRUE(query.exec(
        QStringLiteral("CREATE TABLE saves (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "slot_name TEXT UNIQUE NOT NULL, title TEXT NOT NULL, "
                       "world_state BLOB NOT NULL)")))
        << query.lastError().text().toStdString();
    ASSERT_TRUE(query.exec(
        QStringLiteral("INSERT INTO saves (slot_name, title, world_state) "
                       "VALUES ('old_slot', 'Legacy Save', 'legacy-bytes')")));
    ASSERT_TRUE(query.exec(QStringLiteral("PRAGMA user_version = 3")));
    legacy.close();
  }
  QSqlDatabase::removeDatabase(QStringLiteral("legacy_schema_fixture"));

  SaveStorage storage(database_path);
  QString error;
  ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();

  EXPECT_TRUE(storage.list_slots(&error).isEmpty());
  EXPECT_FALSE(storage.slot_exists("old_slot"));

  ASSERT_TRUE(storage.write_slot(make_record("fresh", "Fresh Save"), &error))
      << error.toStdString();
  EXPECT_TRUE(storage.verify_slot("fresh", &error)) << error.toStdString();
}

TEST(SaveStorageSchemaTest, ReopeningACurrentDatabaseKeepsExistingSaves) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString database_path = temp_dir.filePath(QStringLiteral("saves.sqlite"));

  QString error;
  {
    SaveStorage storage(database_path);
    ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();
    ASSERT_TRUE(storage.write_slot(make_record("kept", "Kept Save"), &error))
        << error.toStdString();
  }

  SaveStorage const reopened(database_path);
  ASSERT_TRUE(reopened.initialize(&error)) << error.toStdString();
  EXPECT_TRUE(reopened.slot_exists("kept"));

  Save::Record loaded;
  ASSERT_TRUE(reopened.read_slot("kept", loaded, &error)) << error.toStdString();
  EXPECT_EQ(loaded.title, QString("Kept Save"));
}

TEST_F(SaveStorageTest, ScreenshotCanBeAttachedAfterTheSaveIsWritten) {
  QString error;
  ASSERT_TRUE(storage->write_slot(make_record("with_preview", "Preview"), &error));

  Save::Record before;
  ASSERT_TRUE(storage->read_slot("with_preview", before, &error));
  EXPECT_TRUE(before.screenshot.isEmpty());

  const QByteArray preview("fake-png-bytes");
  ASSERT_TRUE(storage->update_screenshot("with_preview", preview, &error))
      << error.toStdString();

  Save::Record after;
  ASSERT_TRUE(storage->read_slot("with_preview", after, &error));
  EXPECT_EQ(after.screenshot, preview);

  EXPECT_EQ(after.title, before.title);
  EXPECT_EQ(after.world.raw_checksum, before.world.raw_checksum);
  EXPECT_TRUE(storage->verify_slot("with_preview", &error)) << error.toStdString();

  const QVariantList slot_list = storage->list_slots(&error);
  ASSERT_EQ(slot_list.size(), 1);
  EXPECT_EQ(slot_list.at(0).toMap()["thumbnail"].toString(),
            QString::fromLatin1(preview.toBase64()));
}

TEST_F(SaveStorageTest, AttachingAScreenshotToAMissingSlotFails) {
  QString error;
  EXPECT_FALSE(storage->update_screenshot("nope", QByteArray("png"), &error));
  EXPECT_FALSE(error.isEmpty());
}

namespace {

void write_legacy_v2_database(const QString& database_path) {
  const QString connection = QStringLiteral("legacy_v2_fixture");
  {
    QSqlDatabase legacy =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    legacy.setDatabaseName(database_path);
    ASSERT_TRUE(legacy.open());

    QSqlQuery query(legacy);
    ASSERT_TRUE(query.exec(QStringLiteral(
        "CREATE TABLE saves (slot_name TEXT PRIMARY KEY NOT NULL, title TEXT NOT "
        "NULL, map_name TEXT NOT NULL, map_path TEXT NOT NULL, mode TEXT NOT NULL, "
        "campaign_id TEXT NOT NULL, mission_id TEXT NOT NULL, difficulty TEXT NOT "
        "NULL, kind TEXT NOT NULL, play_time_seconds REAL NOT NULL, created_at TEXT "
        "NOT NULL, updated_at TEXT NOT NULL, format_version INTEGER NOT NULL, "
        "compression TEXT NOT NULL, world_raw_size INTEGER NOT NULL, "
        "world_raw_checksum TEXT NOT NULL, world_blob_checksum TEXT NOT NULL, "
        "metadata BLOB NOT NULL, world_state BLOB NOT NULL, screenshot BLOB)")))
        << query.lastError().text().toStdString();
    ASSERT_TRUE(query.exec(QStringLiteral(
        "CREATE TABLE campaign_progress (campaign_id TEXT PRIMARY KEY NOT NULL, "
        "completed INTEGER NOT NULL DEFAULT 0, unlocked INTEGER NOT NULL DEFAULT 0, "
        "completed_at TEXT)")));
    ASSERT_TRUE(query.exec(QStringLiteral(
        "CREATE TABLE campaign_missions (campaign_id TEXT NOT NULL, mission_id TEXT "
        "NOT NULL, order_index INTEGER NOT NULL, unlocked INTEGER NOT NULL DEFAULT "
        "0, completed INTEGER NOT NULL DEFAULT 0, completed_at TEXT, PRIMARY KEY "
        "(campaign_id, mission_id))")));
    ASSERT_TRUE(query.exec(QStringLiteral(
        "CREATE TABLE mission_results (mission_id TEXT NOT NULL, mode TEXT NOT NULL, "
        "campaign_id TEXT NOT NULL, completed INTEGER NOT NULL DEFAULT 0, "
        "completion_time REAL, difficulty TEXT, result TEXT, completed_at TEXT, "
        "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, PRIMARY KEY (mission_id, "
        "mode, campaign_id))")));

    const Save::Payload payload = Save::pack(QByteArray("{\"entities\":[]}"));
    QSqlQuery insert(legacy);
    ASSERT_TRUE(insert.prepare(QStringLiteral(
        "INSERT INTO saves VALUES ('old_campaign_save', 'Before Cannae', 'Apulia', "
        "'assets/maps/apulia.json', 'campaign', 'second_punic_war', 'cannae', "
        "'normal', 'manual', 900.0, '2026-01-01T00:00:00.000Z', "
        "'2026-01-01T00:00:00.000Z', 1, :compression, :raw_size, :raw_checksum, "
        ":blob_checksum, '{}', :blob, NULL)")));
    insert.bindValue(QStringLiteral(":compression"),
                     Save::compression_to_string(payload.compression));
    insert.bindValue(QStringLiteral(":raw_size"), payload.raw_size);
    insert.bindValue(QStringLiteral(":raw_checksum"), payload.raw_checksum);
    insert.bindValue(QStringLiteral(":blob_checksum"), payload.blob_checksum);
    insert.bindValue(QStringLiteral(":blob"), payload.blob);
    ASSERT_TRUE(insert.exec()) << insert.lastError().text().toStdString();

    ASSERT_TRUE(query.exec(QStringLiteral(
        "INSERT INTO campaign_missions VALUES ('second_punic_war', 'ticinus', 0, 1, "
        "1, '2026-01-01T00:00:00.000Z')")));
    ASSERT_TRUE(query.exec(QStringLiteral(
        "INSERT INTO campaign_missions VALUES ('second_punic_war', 'cannae', 1, 1, "
        "0, NULL)")));
    ASSERT_TRUE(query.exec(QStringLiteral("PRAGMA user_version = 2")));
    legacy.close();
  }
  QSqlDatabase::removeDatabase(connection);
}

auto quarantined_siblings(const QString& database_path) -> QStringList {
  const QFileInfo info(database_path);
  return QDir(info.absolutePath())
      .entryList({info.fileName() + QStringLiteral(".unreadable-*")}, QDir::Files);
}

} // namespace

TEST(SaveStorageSafetyTest, UpgradingFromTheOldSchemaKeepsSavesAndCampaignProgress) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString database_path = temp_dir.filePath(QStringLiteral("saves.sqlite"));
  write_legacy_v2_database(database_path);

  SaveStorage storage(database_path);
  QString error;
  ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();

  EXPECT_TRUE(storage.quarantined_path().isEmpty())
      << "a database we can upgrade must not be moved aside";
  EXPECT_TRUE(storage.slot_exists("old_campaign_save"))
      << "the upgrade dropped the player's save";

  const QVariantList missions =
      storage.get_campaign_mission_progress(QStringLiteral("second_punic_war"), &error);
  ASSERT_EQ(missions.size(), 2) << "the upgrade dropped campaign progress";
  EXPECT_TRUE(missions.at(0).toMap()[QStringLiteral("completed")].toBool());

  const QVariantList listing = storage.list_slots(&error);
  ASSERT_EQ(listing.size(), 1);
  EXPECT_EQ(listing.at(0).toMap()[QStringLiteral("snapshot_version")].toInt(), 2)
      << "a row carried over from schema 2 was written by snapshot 2";
}

TEST_F(SaveStorageTest, ASaveFromAnotherSnapshotVersionIsRefusedButKept) {
  QString error;
  ASSERT_TRUE(storage->write_slot(make_record("mine", "This Build"), &error));
  ASSERT_TRUE(storage->write_slot(make_record("theirs", "Another Build"), &error));

  QSqlQuery bump(QSqlDatabase::database(
      QStringLiteral("SaveStorage_%1")
          .arg(reinterpret_cast<quintptr>(storage.get()), 0, 16)));
  ASSERT_TRUE(bump.exec(QStringLiteral("UPDATE saves SET snapshot_version = %1 "
                                       "WHERE slot_name = 'theirs'")
                            .arg(Save::k_snapshot_version + 7)))
      << bump.lastError().text().toStdString();

  Save::Record loaded;
  EXPECT_FALSE(storage->read_slot("theirs", loaded, &error));
  EXPECT_TRUE(error.contains("different version")) << error.toStdString();

  EXPECT_TRUE(storage->slot_exists("theirs")) << "the refused save was deleted";
  EXPECT_TRUE(storage->read_slot("mine", loaded, &error)) << error.toStdString();

  const QVariantList listing = storage->list_slots(&error);
  ASSERT_EQ(listing.size(), 2);
  for (const QVariant& entry : listing) {
    const QVariantMap slot = entry.toMap();
    EXPECT_EQ(slot[QStringLiteral("loadable")].toBool(),
              slot[QStringLiteral("slot_name")].toString() == QStringLiteral("mine"))
        << "the listing must say which rows this build can open";
  }
}

TEST(SaveStorageSafetyTest, ADatabaseFromANewerBuildIsMovedAsideNotDeleted) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString database_path = temp_dir.filePath(QStringLiteral("saves.sqlite"));

  {
    SaveStorage storage(database_path);
    QString error;
    ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();
    ASSERT_TRUE(storage.write_slot(make_record("from_the_future", "Later"), &error));
  }
  {
    QSqlDatabase future = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("future_fixture"));
    future.setDatabaseName(database_path);
    ASSERT_TRUE(future.open());
    QSqlQuery query(future);
    ASSERT_TRUE(query.exec(QStringLiteral("PRAGMA user_version = %1")
                               .arg(Save::k_database_schema_version + 5)));
    future.close();
  }
  QSqlDatabase::removeDatabase(QStringLiteral("future_fixture"));

  SaveStorage storage(database_path);
  QString error;
  ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();

  EXPECT_FALSE(storage.quarantined_path().isEmpty())
      << "the newer database should have been moved aside";
  EXPECT_TRUE(QFile::exists(storage.quarantined_path()))
      << "the player's file was deleted instead of preserved";
  EXPECT_FALSE(quarantined_siblings(database_path).isEmpty());
  EXPECT_TRUE(storage.list_slots(&error).isEmpty());
  EXPECT_TRUE(storage.write_slot(make_record("fresh_start", "Fresh"), &error))
      << error.toStdString();
}

TEST(SaveStorageSafetyTest, AnUnreadableFileIsMovedAsideAndReplaced) {
  QTemporaryDir const temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString database_path = temp_dir.filePath(QStringLiteral("saves.sqlite"));

  {
    QFile rubbish(database_path);
    ASSERT_TRUE(rubbish.open(QIODevice::WriteOnly));
    rubbish.write(QByteArray(8192, '\x7f'));
    rubbish.close();
  }

  SaveStorage storage(database_path);
  QString error;
  ASSERT_TRUE(storage.initialize(&error)) << error.toStdString();
  EXPECT_TRUE(storage.health_error().isEmpty());
  EXPECT_FALSE(storage.quarantined_path().isEmpty());
  EXPECT_TRUE(QFile::exists(storage.quarantined_path()));
  EXPECT_TRUE(storage.write_slot(make_record("after_repair", "Repaired"), &error))
      << error.toStdString();
  EXPECT_TRUE(storage.verify_slot("after_repair", &error)) << error.toStdString();
}

TEST_F(SaveStorageTest, AManualSaveCannotTakeASlotNameTheGameRotates) {
  QString error;
  EXPECT_FALSE(storage->write_slot(make_record("quicksave", "Mine"), &error));
  EXPECT_TRUE(error.contains("reserved")) << error.toStdString();
  EXPECT_FALSE(storage->write_slot(make_record("autosave_3", "Mine"), &error));
  EXPECT_TRUE(error.contains("reserved")) << error.toStdString();

  Save::Record autosave = make_record("autosave_3", "Rotation");
  autosave.kind = Save::SlotKind::Autosave;
  EXPECT_TRUE(storage->write_slot(autosave, &error)) << error.toStdString();
}

TEST_F(SaveStorageTest, TheRotationRefusesToOverwriteASaveOfADifferentKind) {
  QString error;
  Save::Record manual = make_record("autosave_1", "Hand Written");
  manual.kind = Save::SlotKind::Quicksave;
  ASSERT_TRUE(storage->write_slot(manual, &error)) << error.toStdString();

  Save::Record autosave = make_record("autosave_1", "Rotation");
  autosave.kind = Save::SlotKind::Autosave;
  EXPECT_FALSE(storage->write_slot(autosave, &error));
  EXPECT_TRUE(error.contains("Delete it from the Load menu")) << error.toStdString();

  Save::Record kept;
  ASSERT_TRUE(storage->read_slot("autosave_1", kept, &error));
  EXPECT_EQ(kept.title, QString("Hand Written"));
}

TEST_F(SaveStorageTest, VerifyRejectsAPayloadThatIsNotAWorld) {
  QString error;
  ASSERT_TRUE(storage->write_slot(
      make_record("not_a_world", "Nonsense", QByteArray("not json at all")), &error));
  EXPECT_FALSE(storage->verify_slot("not_a_world", &error));
  EXPECT_TRUE(error.contains("readable world")) << error.toStdString();

  ASSERT_TRUE(storage->write_slot(
      make_record("no_units", "Empty", QByteArray("{\"players\":[]}")), &error));
  EXPECT_FALSE(storage->verify_slot("no_units", &error));
  EXPECT_TRUE(error.contains("units")) << error.toStdString();

  ASSERT_TRUE(storage->write_slot(make_record("good", "Good"), &error));
  EXPECT_TRUE(storage->verify_slot("good", &error)) << error.toStdString();
}

TEST_F(SaveStorageTest, PruningRemovedMissionsNeverDropsAFinishedOne) {
  QString error;
  Game::Campaign::CampaignDefinition campaign;
  campaign.id = QStringLiteral("punic");
  campaign.missions.push_back(
      {.mission_id = QStringLiteral("rhone"), .order_index = 0});
  campaign.missions.push_back({.mission_id = QStringLiteral("alps"), .order_index = 1});
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(campaign, &error))
      << error.toStdString();

  ASSERT_TRUE(
      storage->complete_campaign_mission(campaign.id, QStringLiteral("rhone"), &error)
          .has_value())
      << error.toStdString();

  Game::Campaign::CampaignDefinition broken;
  broken.id = campaign.id;
  ASSERT_TRUE(storage->ensure_campaign_missions_in_db(broken, &error))
      << error.toStdString();

  const QVariantList rows = storage->get_campaign_mission_progress(campaign.id);
  bool kept_the_win = false;
  for (const QVariant& entry : rows) {
    const QVariantMap row = entry.toMap();
    if (row.value(QStringLiteral("mission_id")).toString() == QStringLiteral("rhone")) {
      kept_the_win = row.value(QStringLiteral("completed")).toBool();
    }
    EXPECT_NE(row.value(QStringLiteral("mission_id")).toString(),
              QStringLiteral("alps"))
        << "an unfinished mission the definition dropped should still be pruned";
  }
  EXPECT_TRUE(kept_the_win)
      << "a finished mission was erased because the definition stopped listing it";
}
