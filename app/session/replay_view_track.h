#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector3D>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "game/core/entity_id.h"

class QFile;

namespace Render::GL {
class Camera;
}

namespace Game::Session {
class SelectionService;
}

namespace App::Session {

struct ReplayViewPointer {
  static constexpr int k_left = 1;
  static constexpr int k_right = 2;
  QPointF position;
  int buttons = 0;
};

struct ReplayViewSample {
  std::uint64_t tick = 0;
  QVector3D position;
  QVector3D target;
  std::optional<ReplayViewPointer> pointer;
};

struct ReplayViewSelection {
  std::uint64_t tick = 0;
  std::vector<Engine::Core::EntityID> units;
};

class ReplayViewTrack {
public:
  ReplayViewTrack();
  ~ReplayViewTrack();
  ReplayViewTrack(const ReplayViewTrack&) = delete;
  auto operator=(const ReplayViewTrack&) -> ReplayViewTrack& = delete;

  [[nodiscard]] static auto sidecar_path(const QString& replay_path) -> QString;

  auto begin_recording(const QString& path) -> bool;
  void record(std::uint64_t tick,
              const Render::GL::Camera& camera,
              const std::vector<Engine::Core::EntityID>& selection);
  void finish();
  [[nodiscard]] auto recording() const -> bool { return m_file != nullptr; }

  void note_pointer(QPointF normalized, int buttons);

  auto load(const QString& path) -> bool;
  void clear();
  [[nodiscard]] auto loaded() const -> bool { return !m_samples.empty(); }

  [[nodiscard]] auto
  sample_at(std::uint64_t tick) const -> std::optional<ReplayViewSample>;
  [[nodiscard]] auto
  selection_at(std::uint64_t tick) const -> const std::vector<Engine::Core::EntityID>*;

  void apply(std::uint64_t tick,
             Render::GL::Camera& camera,
             Game::Session::SelectionService& selection);

  [[nodiscard]] auto pointer() const -> std::optional<ReplayViewPointer> {
    return m_applied_pointer;
  }

private:
  std::unique_ptr<QFile> m_file;
  std::optional<std::uint64_t> m_last_tick;
  std::vector<Engine::Core::EntityID> m_last_selection;
  bool m_selection_written = false;
  std::atomic<float> m_pointer_x{-1.0F};
  std::atomic<float> m_pointer_y{-1.0F};
  std::atomic<int> m_pointer_buttons{0};
  int m_last_pointer_buttons = 0;

  std::vector<ReplayViewSample> m_samples;
  std::vector<ReplayViewSelection> m_selections;
  const std::vector<Engine::Core::EntityID>* m_applied_selection = nullptr;
  std::optional<ReplayViewPointer> m_applied_pointer;
};

class ReplayPointerTap : public QObject {
  Q_OBJECT
public:
  explicit ReplayPointerTap(ReplayViewTrack& track, QObject* parent = nullptr);

protected:
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
  ReplayViewTrack& m_track;
};

} // namespace App::Session
