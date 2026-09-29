#include "app/persistence/save_slot_controller.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

#include <utility>

#include "app/viewmodels/save_slots_view_model.h"
#include "game/audio/audio_cues.h"
#include "game/core/world.h"
#include "game/systems/persistence/save_load_service.h"

namespace App::Core {

namespace {

auto engine_text(const char* source) -> QString {
  return QCoreApplication::translate("GameEngine", source);
}

} // namespace

SaveSlotController::SaveSlotController(Game::Systems::SaveLoadService* service,
                                       App::ViewModels::SaveSlotsViewModel* slot_model,
                                       Engine::Core::World* world,
                                       SaveSlotHooks hooks,
                                       QObject* context)
    : m_service(service)
    , m_slots(slot_model)
    , m_world(world)
    , m_hooks(std::move(hooks))
    , m_context(context)
    , m_orchestrator(SaveOrchestrator::Callbacks{
          .simulation_running = m_hooks.simulation_running,
          .capture = m_hooks.capture,
          .deliver = [this](const QString& slot, const SaveToSlotEffects& effects) {
            QMetaObject::invokeMethod(
                m_context,
                [this, slot, effects]() { finish_request(slot, effects); },
                Qt::QueuedConnection);
          }}) {
}

void SaveSlotController::connect_service_signals() {
  if (m_service == nullptr) {
    return;
  }

  QObject::connect(
      m_service,
      &Game::Systems::SaveLoadService::save_progress,
      m_context,
      [this](
          quint64 job_id, const QString& slot_name, int percent, const QString& stage) {
        if (job_id != m_active_job) {
          return;
        }
        m_slots->set_save_progress(true, percent, stage, slot_name);
      });

  QObject::connect(m_service,
                   &Game::Systems::SaveLoadService::save_finished,
                   m_context,
                   [this](quint64 job_id,
                          const QString& slot_name,
                          bool success,
                          const QString& error) {
                     if (job_id == m_active_job) {
                       m_active_job = 0;
                       m_slots->set_save_progress(
                           false, success ? 100 : 0, QString(), m_progress_slot);
                     }
                     if (!success) {
                       m_hooks.report_error(error);
                       Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
                     } else {
                       Game::Audio::play_cue(Game::Audio::Cue::k_state_save_complete);
                     }
                     emit m_slots->save_completed(slot_name, success, error);
                   });

  QObject::connect(m_service,
                   &Game::Systems::SaveLoadService::save_slots_changed,
                   m_slots,
                   &App::ViewModels::SaveSlotsViewModel::save_slots_changed);

  QObject::connect(
      &m_autosave_timer, &QTimer::timeout, m_context, [this]() { autosave(); });
  restart_autosave_timer();
}

void SaveSlotController::connect_view_model() {
  using ViewModel = App::ViewModels::SaveSlotsViewModel;
  QObject::connect(m_slots,
                   &ViewModel::error_occurred,
                   m_context,
                   [this](const QString& message) { m_hooks.report_error(message); });
  QObject::connect(m_slots, &ViewModel::autosave_interval_changed, m_context, [this] {
    restart_autosave_timer();
  });
  QObject::connect(m_slots,
                   &ViewModel::save_requested,
                   m_context,
                   [this](const QString& slot) { save_game_to_slot(slot); });
  QObject::connect(
      m_slots, &ViewModel::quicksave_requested, m_context, [this] { quicksave(); });
  QObject::connect(
      m_slots, &ViewModel::autosave_requested, m_context, [this] { autosave(); });
  QObject::connect(m_slots, &ViewModel::cancel_save_requested, m_context, [this] {
    cancel_active_save();
  });
}

void SaveSlotController::restart_autosave_timer() {
  const int minutes = m_slots->autosave_interval_minutes();
  if (minutes <= 0) {
    m_autosave_timer.stop();
    return;
  }
  m_autosave_timer.setInterval(minutes * 60 * 1000);
  m_autosave_timer.start();
}

void SaveSlotController::shutdown() {
  m_autosave_timer.stop();
  if (m_service != nullptr) {
    m_service->wait_for_pending_saves();
    m_service->disconnect(m_context);
    m_service->shutdown();
  }
}

void SaveSlotController::clear_progress() {
  m_slots->set_save_progress(false, 0, QString(), QString());
  m_progress_slot.clear();
}

void SaveSlotController::begin_save(const QString& slot_name,
                                    Game::Systems::Save::SlotKind kind,
                                    int autosave_retention) {
  if ((m_service == nullptr) || m_world == nullptr) {
    m_hooks.report_error(engine_text("Save: not initialized"));
    return;
  }

  if (m_active_job != 0 || m_orchestrator.queued()) {

    emit m_slots->save_completed(
        slot_name, false, engine_text("A save is already in progress."));
    return;
  }

  m_hooks.leave_commander_mode();

  m_progress_slot = slot_name;
  m_slots->set_save_progress(true, 0, engine_text("Queued"), slot_name);

  if (queue_capture(slot_name, kind, autosave_retention)) {
    return;
  }

  SaveToSlotEffects effects;
  {
    const std::unique_lock<std::recursive_mutex> capture_lock = m_hooks.lock_frame();
    effects = m_hooks.capture(slot_name, kind, autosave_retention);
  }
  finish_request(slot_name, effects);
}

auto SaveSlotController::queue_capture(const QString& slot_name,
                                       Game::Systems::Save::SlotKind kind,
                                       int autosave_retention) -> bool {
  return m_orchestrator.queue(slot_name, kind, autosave_retention);
}

void SaveSlotController::drain_pending_capture() {
  m_orchestrator.drain();
}

void SaveSlotController::finish_request(const QString& slot_name,
                                        const SaveToSlotEffects& effects) {
  if (!effects.queued) {
    clear_progress();
    m_hooks.report_error(effects.error);
    return;
  }

  m_active_job = effects.job_id;
  m_progress_slot = slot_name;
  m_slots->set_save_progress(true, 0, engine_text("Queued"), slot_name);

  m_screenshot_target_slot = slot_name;
  m_screenshot_requested.store(true, std::memory_order_release);
}

void SaveSlotController::save_game_to_slot(const QString& slot_name) {
  begin_save(slot_name, Game::Systems::Save::SlotKind::Manual, 0);
}

void SaveSlotController::quicksave() {
  begin_save(QStringLiteral("quicksave"), Game::Systems::Save::SlotKind::Quicksave, 0);
}

void SaveSlotController::autosave() {
  if ((m_service == nullptr) || m_world == nullptr || !m_hooks.autosave_allowed() ||
      m_active_job != 0) {
    return;
  }

  const int retention = m_slots->autosave_slot_count();
  begin_save(m_service->next_autosave_slot(retention),
             Game::Systems::Save::SlotKind::Autosave,
             retention);
}

void SaveSlotController::cancel_active_save() {
  if (m_orchestrator.cancel_queued()) {
    clear_progress();
    return;
  }

  if (m_active_job == 0 || (m_service == nullptr)) {
    return;
  }
  m_service->cancel_save(m_active_job);
  m_slots->set_save_progress(true,
                             m_slots->save_progress_percent(),
                             engine_text("Cancelling..."),
                             m_progress_slot);
}

void SaveSlotController::attach_screenshot(const QImage& image) {
  const QString slot_name = m_screenshot_target_slot;
  m_screenshot_target_slot.clear();
  if (slot_name.isEmpty() || (m_service == nullptr) || image.isNull()) {
    return;
  }

  const QByteArray png = Game::Systems::Save::encode_preview(image);
  if (png.isEmpty()) {
    qWarning() << "GameEngine: failed to encode save preview for" << slot_name;
    return;
  }

  m_service->attach_screenshot(slot_name, png);
}

} // namespace App::Core
