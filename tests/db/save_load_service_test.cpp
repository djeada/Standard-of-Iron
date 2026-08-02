#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>

#include <gtest/gtest.h>

#include "systems/save_format.h"
#include "systems/save_load_service.h"

using namespace Game::Systems;

namespace {

auto make_world_document(int entity_count) -> QJsonDocument {
  QJsonArray entities;
  for (int i = 0; i < entity_count; ++i) {
    QJsonObject entity;
    entity["id"] = i + 1;
    entity["unit_type"] = QStringLiteral("archer");
    entities.append(entity);
  }

  QJsonObject world;
  world["entities"] = entities;
  world["nextEntityId"] = entity_count + 1;
  return QJsonDocument(world);
}

auto make_request(const QString& slot_name) -> SaveRequest {
  SaveRequest request;
  request.slot_name = slot_name;
  request.title = slot_name;
  request.map_name = QStringLiteral("River Crossing");
  request.map_path = QStringLiteral("assets/maps/river.json");
  request.mode = QStringLiteral("skirmish");
  request.difficulty = QStringLiteral("normal");
  request.metadata = QJsonObject{{"map_name", "River Crossing"}};
  request.world = make_world_document(200);
  return request;
}

void wait_for_saves(SaveLoadService& service) {
  QElapsedTimer timer;
  timer.start();
  while (service.pending_save_count() > 0 && timer.elapsed() < 30000) {
    QCoreApplication::processEvents();
  }
  ASSERT_TRUE(service.wait_for_pending_saves(30000));
  QCoreApplication::processEvents();
}

} // namespace

class SaveLoadServiceTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    QStandardPaths::setTestModeEnabled(true);

    s_saved_application_name = QCoreApplication::applicationName();
    const QString base = s_saved_application_name.isEmpty()
                             ? QStringLiteral("StandardOfIron")
                             : s_saved_application_name;
    QCoreApplication::setApplicationName(QStringLiteral("%1-save-tests-%2")
                                             .arg(base)
                                             .arg(QCoreApplication::applicationPid()));
  }

  static void TearDownTestSuite() {
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .removeRecursively();
    QCoreApplication::setApplicationName(s_saved_application_name);
    QStandardPaths::setTestModeEnabled(false);
  }

  void SetUp() override {
    QDir(SaveLoadService::saves_directory()).removeRecursively();
    service = std::make_unique<SaveLoadService>();
  }

  void TearDown() override {
    if (service) {
      service->shutdown();
      service.reset();
    }
    QDir(SaveLoadService::saves_directory()).removeRecursively();
  }

  std::unique_ptr<SaveLoadService> service;

  static QString s_saved_application_name;
};

QString SaveLoadServiceTest::s_saved_application_name;

TEST_F(SaveLoadServiceTest, BackgroundSaveReportsProgressAndCompletes) {
  int progress_events = 0;
  int finished_events = 0;
  quint64 finished_job = 0;
  QString finished_slot;
  bool finished_success = false;
  QString finished_error;

  QObject::connect(
      service.get(),
      &SaveLoadService::save_progress,
      service.get(),
      [&](quint64, const QString&, int, const QString&) { ++progress_events; });
  QObject::connect(service.get(),
                   &SaveLoadService::save_finished,
                   service.get(),
                   [&](quint64 job_id,
                       const QString& slot_name,
                       bool success,
                       const QString& error) {
                     ++finished_events;
                     finished_job = job_id;
                     finished_slot = slot_name;
                     finished_success = success;
                     finished_error = error;
                   });

  const quint64 job_id = service->begin_save(make_request("slot_a"));
  ASSERT_NE(job_id, 0U);

  wait_for_saves(*service);

  ASSERT_EQ(finished_events, 1);
  EXPECT_EQ(finished_job, job_id);
  EXPECT_EQ(finished_slot, QString("slot_a"));
  EXPECT_TRUE(finished_success) << finished_error.toStdString();

  EXPECT_GT(progress_events, 1);
  EXPECT_TRUE(service->slot_exists("slot_a"));
  EXPECT_TRUE(service->verify_save_slot("slot_a"));
}

TEST_F(SaveLoadServiceTest, CancelledSaveLeavesPreviousSaveIntact) {
  SaveRequest first = make_request("slot_b");
  first.title = QStringLiteral("Original");
  ASSERT_NE(service->begin_save(first), 0U);
  wait_for_saves(*service);
  ASSERT_TRUE(service->slot_exists("slot_b"));

  SaveRequest second = make_request("slot_b");
  second.title = QStringLiteral("Replacement");
  const quint64 job_id = service->begin_save(second);
  ASSERT_NE(job_id, 0U);
  service->cancel_save(job_id);
  wait_for_saves(*service);

  EXPECT_TRUE(service->slot_exists("slot_b"));
  EXPECT_TRUE(service->verify_save_slot("slot_b"));
}

TEST_F(SaveLoadServiceTest, AutosaveSlotsRotateWithinRetention) {
  const int retention = 3;
  QStringList used;
  for (int i = 0; i < 5; ++i) {
    const QString slot = service->next_autosave_slot(retention);
    used.append(slot);
    SaveRequest request = make_request(slot);
    request.kind = Save::SlotKind::Autosave;
    request.autosave_retention = retention;
    ASSERT_NE(service->begin_save(request), 0U);
    wait_for_saves(*service);
  }

  int autosave_count = 0;
  for (const QVariant& entry : service->get_save_slots()) {
    if (entry.toMap()["kind"].toString() == QLatin1String("autosave")) {
      ++autosave_count;
    }
  }
  EXPECT_EQ(autosave_count, retention);
  EXPECT_EQ(used.at(0), QString("autosave_1"));
  EXPECT_EQ(used.at(2), QString("autosave_3"));

  EXPECT_TRUE(used.at(3).startsWith(QLatin1String("autosave_")));
}

TEST_F(SaveLoadServiceTest, LoweringRetentionPrunesOldAutosaves) {
  for (int i = 1; i <= 4; ++i) {
    SaveRequest request = make_request(QStringLiteral("autosave_%1").arg(i));
    request.kind = Save::SlotKind::Autosave;
    ASSERT_NE(service->begin_save(request), 0U);
    wait_for_saves(*service);
  }

  EXPECT_EQ(service->prune_autosaves(2), 2);
  EXPECT_FALSE(service->slot_exists("autosave_1"));
  EXPECT_TRUE(service->slot_exists("autosave_4"));
}

TEST_F(SaveLoadServiceTest, ExportedSaveCanBeImportedBack) {
  SaveRequest request = make_request("campaign_slot");
  request.mode = QStringLiteral("campaign");
  request.campaign_id = QStringLiteral("second_punic_war");
  request.mission_id = QStringLiteral("trebia");
  ASSERT_NE(service->begin_save(request), 0U);
  wait_for_saves(*service);

  const QString path = SaveLoadService::exports_directory() +
                       QStringLiteral("/campaign_slot.") + Save::package_file_suffix();
  QString error;
  ASSERT_TRUE(service->export_slot("campaign_slot", path, &error))
      << error.toStdString();
  EXPECT_EQ(service->list_exported_packages().size(), 1);

  ASSERT_TRUE(service->delete_save_slot("campaign_slot"));

  QString imported_slot;
  ASSERT_TRUE(service->import_package(path, imported_slot, &error))
      << error.toStdString();
  EXPECT_EQ(imported_slot, QString("campaign_slot"));
  EXPECT_TRUE(service->verify_save_slot("campaign_slot"));
}

TEST_F(SaveLoadServiceTest, ImportingTwiceCreatesADistinctSlot) {
  ASSERT_NE(service->begin_save(make_request("shared")), 0U);
  wait_for_saves(*service);

  const QString path = SaveLoadService::exports_directory() +
                       QStringLiteral("/shared.") + Save::package_file_suffix();
  QString error;
  ASSERT_TRUE(service->export_slot("shared", path, &error)) << error.toStdString();

  QString imported_slot;
  ASSERT_TRUE(service->import_package(path, imported_slot, &error))
      << error.toStdString();
  EXPECT_EQ(imported_slot, QString("shared_2"));
}

TEST_F(SaveLoadServiceTest, EmptySlotNameIsRejected) {
  EXPECT_EQ(service->begin_save(make_request("")), 0U);
  EXPECT_FALSE(service->get_last_error().isEmpty());
}

TEST_F(SaveLoadServiceTest, ScreenshotQueuedRightAfterASaveLandsOnThatSave) {

  const quint64 job_id = service->begin_save(make_request("with_preview"));
  ASSERT_NE(job_id, 0U);

  const QByteArray preview("fake-png-bytes");
  service->attach_screenshot("with_preview", preview);

  wait_for_saves(*service);

  bool found = false;
  for (const QVariant& entry : service->get_save_slots()) {
    const QVariantMap slot = entry.toMap();
    if (slot["slot_name"].toString() != QLatin1String("with_preview")) {
      continue;
    }
    found = true;
    EXPECT_EQ(slot["thumbnail"].toString(), QString::fromLatin1(preview.toBase64()));
  }
  EXPECT_TRUE(found);
  EXPECT_TRUE(service->verify_save_slot("with_preview"));
}

TEST_F(SaveLoadServiceTest, EmptyScreenshotIsIgnored) {
  ASSERT_NE(service->begin_save(make_request("no_preview")), 0U);
  service->attach_screenshot("no_preview", QByteArray());
  service->attach_screenshot(QString(), QByteArray("png"));
  wait_for_saves(*service);

  EXPECT_TRUE(service->slot_exists("no_preview"));
}
