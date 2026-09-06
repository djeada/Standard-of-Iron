#pragma once

#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>

#include "app/persistence/save_load_coordinator.h"
#include "game/systems/save_format.h"

namespace App::Core {

class SaveOrchestrator {
public:
  using Capture = std::function<SaveToSlotEffects(
      const QString& slot, Game::Systems::Save::SlotKind kind, int autosave_retention)>;

  using Deliver =
      std::function<void(const QString& slot, const SaveToSlotEffects& effects)>;

  using SimulationRunning = std::function<bool()>;

  struct Callbacks {
    SimulationRunning simulation_running;
    Capture capture;
    Deliver deliver;
  };

  explicit SaveOrchestrator(Callbacks callbacks)
      : m_callbacks(std::move(callbacks)) {}

  [[nodiscard]] auto queue(const QString& slot,
                           Game::Systems::Save::SlotKind kind,
                           int autosave_retention) -> bool;

  void drain();

  auto cancel_queued() -> bool;

  [[nodiscard]] auto queued() const -> bool;

  [[nodiscard]] auto last_capture_us() const -> std::uint64_t {
    return m_last_capture_us.load(std::memory_order_acquire);
  }

private:
  struct Request {
    QString slot;
    Game::Systems::Save::SlotKind kind = Game::Systems::Save::SlotKind::Manual;
    int autosave_retention = 0;
    bool valid = false;
  };

  Callbacks m_callbacks;
  mutable std::mutex m_mutex;
  Request m_request;
  std::atomic<std::uint64_t> m_last_capture_us{0};
};

} // namespace App::Core
