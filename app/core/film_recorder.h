#pragma once

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

class GameEngine;
class QQuickWindow;
class QTimer;

namespace App::Core {

struct BenchmarkActionFixture;

struct FilmConfig {
  QString directory;
  int fps{60};
  double seconds{8.0};

  double start_seconds{0.0};
  int width{1920};
  int height{1080};

  bool background{true};
};

class FilmRecorder : public QObject {
  Q_OBJECT
public:
  FilmRecorder(GameEngine* engine,
               QQuickWindow* window,
               FilmConfig config,
               std::optional<BenchmarkActionFixture> fixture,
               QObject* parent = nullptr);
  ~FilmRecorder() override;

  void start();

protected:
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
  void pump();
  auto grab_frame() -> bool;
  void finish(int exit_code);
  void enqueue_save(QString path, QImage frame);
  void drain_saves();

  QPointer<GameEngine> m_engine;
  QPointer<QQuickWindow> m_window;
  FilmConfig m_config;
  std::unique_ptr<BenchmarkActionFixture> m_fixture;
  QTimer* m_timer{nullptr};
  double m_clock{0.0};
  double m_previous_clock{0.0};
  int m_frames_written{0};
  int m_warmup_frames{0};
  bool m_started{false};
  bool m_seen_loading{false};

  struct PendingSave {
    QString path;
    QImage frame;
  };
  std::vector<std::thread> m_writers;
  std::deque<PendingSave> m_pending;
  std::mutex m_pending_mutex;
  std::condition_variable m_pending_changed;
  bool m_writers_stop{false};
  bool m_save_failed{false};
};

} // namespace App::Core
