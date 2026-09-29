#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>

#include <gtest/gtest.h>
#include <memory>
#include <mutex>

#include "app/persistence/save_slot_controller.h"
#include "app/viewmodels/save_slots_view_model.h"
#include "core/world.h"
#include "systems/persistence/save_load_service.h"

using App::Core::SaveSlotController;
using App::Core::SaveSlotHooks;
using App::Core::SaveToSlotEffects;
using Game::Systems::SaveLoadService;

namespace {

class SaveSlotControllerTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    QStandardPaths::setTestModeEnabled(true);
    s_saved_application_name = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(
        QStringLiteral("StandardOfIron-slot-controller-%1")
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
    slot_model = std::make_unique<App::ViewModels::SaveSlotsViewModel>(service.get());
    controller = std::make_unique<SaveSlotController>(
        service.get(),
        slot_model.get(),
        &world,
        SaveSlotHooks{
            .simulation_running = []() { return false; },
            .capture =
                [this](const QString&, Game::Systems::Save::SlotKind, int) {
                  ++captures;
                  SaveToSlotEffects effects;
                  effects.error = QStringLiteral("no world in this fixture");
                  return effects;
                },
            .lock_frame = [this]() { return std::unique_lock(frame_mutex); },
            .report_error = [this](const QString& message) { errors << message; },
            .leave_commander_mode = [this]() { ++left_commander_mode; },
            .autosave_allowed =
                []() {
                  return true;
                }},
        &context);
    controller->connect_view_model();
    controller->connect_service_signals();
  }

  void TearDown() override {
    controller->shutdown();
    controller.reset();
    slot_model.reset();
    service.reset();
    QDir(SaveLoadService::saves_directory()).removeRecursively();
  }

  QObject context;
  Engine::Core::World world;
  std::recursive_mutex frame_mutex;
  std::unique_ptr<SaveLoadService> service;
  std::unique_ptr<App::ViewModels::SaveSlotsViewModel> slot_model;
  std::unique_ptr<SaveSlotController> controller;
  int captures = 0;
  int left_commander_mode = 0;
  QStringList errors;

  static QString s_saved_application_name;
};

QString SaveSlotControllerTest::s_saved_application_name;

} // namespace

TEST_F(SaveSlotControllerTest, AutosaveCapturesOnceAndClearsProgressWhenRefused) {
  controller->autosave();

  EXPECT_EQ(captures, 1);
  EXPECT_EQ(left_commander_mode, 1);
  ASSERT_EQ(errors.size(), 1);
  EXPECT_EQ(errors.front(), QStringLiteral("no world in this fixture"));
  EXPECT_FALSE(slot_model->save_in_progress());
}

TEST_F(SaveSlotControllerTest, TheAutosaveSignalFromTheViewModelReachesTheController) {
  emit slot_model->autosave_requested();
  EXPECT_EQ(captures, 1);
}

TEST_F(SaveSlotControllerTest, QuicksaveUsesTheSameCapturePath) {
  controller->quicksave();
  EXPECT_EQ(captures, 1);
  EXPECT_FALSE(slot_model->save_in_progress());
}
