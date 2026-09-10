#include "film_recorder.h"

#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "app/core/benchmark_action_fixture.h"
#include "app/core/film_action_dispatch.h"
#include "app/core/game_engine.h"

namespace App::Core {

namespace {

constexpr int k_warmup_frames = 4;

constexpr int k_writer_threads = 3;
constexpr std::size_t k_max_pending_saves = 6;
constexpr int k_png_quality = 85;
constexpr int k_parked_visible_pixels = 8;

} // namespace

FilmRecorder::FilmRecorder(GameEngine* engine,
                           QQuickWindow* window,
                           FilmConfig config,
                           std::optional<BenchmarkActionFixture> fixture,
                           QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_window(window)
    , m_config(std::move(config))
    , m_fixture(fixture.has_value()
                    ? std::make_unique<BenchmarkActionFixture>(std::move(*fixture))
                    : nullptr) {
  m_timer = new QTimer(this);
  m_timer->setInterval(0);
  connect(m_timer, &QTimer::timeout, this, &FilmRecorder::pump);
  for (int i = 0; i < k_writer_threads; ++i) {
    m_writers.emplace_back([this]() {
      for (;;) {
        PendingSave job;
        {
          std::unique_lock<std::mutex> lock(m_pending_mutex);
          m_pending_changed.wait(
              lock, [this]() { return m_writers_stop || !m_pending.empty(); });
          if (m_pending.empty()) {
            return;
          }
          job = std::move(m_pending.front());
          m_pending.pop_front();
        }
        const bool ok = job.frame.save(job.path, "PNG", k_png_quality);
        {
          const std::lock_guard<std::mutex> lock(m_pending_mutex);
          if (!ok) {
            m_save_failed = true;
          }
        }
        m_pending_changed.notify_all();
      }
    });
  }
}

FilmRecorder::~FilmRecorder() {
  drain_saves();
  {
    const std::lock_guard<std::mutex> lock(m_pending_mutex);
    m_writers_stop = true;
  }
  m_pending_changed.notify_all();
  for (auto& writer : m_writers) {
    if (writer.joinable()) {
      writer.join();
    }
  }
}

void FilmRecorder::enqueue_save(QString path, QImage frame) {
  std::unique_lock<std::mutex> lock(m_pending_mutex);
  m_pending_changed.wait(lock,
                         [this]() { return m_pending.size() < k_max_pending_saves; });
  m_pending.push_back({std::move(path), std::move(frame)});
  lock.unlock();
  m_pending_changed.notify_all();
}

void FilmRecorder::drain_saves() {
  std::unique_lock<std::mutex> lock(m_pending_mutex);
  m_pending_changed.wait(lock, [this]() { return m_pending.empty(); });
}

void FilmRecorder::start() {
  if (m_window == nullptr || m_engine == nullptr) {
    finish(20);
    return;
  }
  QDir().mkpath(m_config.directory);
  m_window->setWindowState(Qt::WindowNoState);
  m_window->setWidth(m_config.width);
  m_window->setHeight(m_config.height);
  if (m_config.background) {

    m_window->setFlags(Qt::Window | Qt::FramelessWindowHint |
                       Qt::BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus);
    const QRect screen = m_window->screen() != nullptr
                             ? m_window->screen()->geometry()
                             : QRect(0, 0, m_config.width, m_config.height);
    m_window->setPosition(screen.x() + screen.width() - k_parked_visible_pixels,
                          screen.y() + screen.height() - k_parked_visible_pixels);
    m_window->setVisible(true);

    m_window->installEventFilter(this);
  }
  qInfo().noquote() << QStringLiteral("SOI_FILM: recording %1 fps for %2 s into %3")
                           .arg(m_config.fps)
                           .arg(m_config.seconds, 0, 'f', 2)
                           .arg(m_config.directory);
  m_timer->start();
}

auto FilmRecorder::grab_frame() -> bool {
  const QImage frame = m_window->grabWindow();
  if (frame.isNull()) {
    qCritical() << "SOI_FILM: FAIL - the window produced no frame";
    return false;
  }
  if (m_warmup_frames < k_warmup_frames) {
    ++m_warmup_frames;
    return true;
  }
  if (m_clock + 1e-6 < m_config.start_seconds) {
    return true;
  }
  const QString path = QStringLiteral("%1/frame_%2.png")
                           .arg(m_config.directory)
                           .arg(m_frames_written, 6, 10, QLatin1Char('0'));
  {
    const std::lock_guard<std::mutex> lock(m_pending_mutex);
    if (m_save_failed) {
      qCritical() << "SOI_FILM: FAIL - a frame could not be written under"
                  << m_config.directory;
      return false;
    }
  }
  enqueue_save(path, frame);
  ++m_frames_written;
  return true;
}

auto FilmRecorder::eventFilter(QObject* watched, QEvent* event) -> bool {

  if (m_config.background && m_started && watched == m_window &&
      event->type() == QEvent::UpdateRequest) {
    return true;
  }
  return QObject::eventFilter(watched, event);
}

void FilmRecorder::pump() {
  if (m_engine == nullptr || m_window == nullptr) {
    finish(20);
    return;
  }
  if (m_engine->is_loading()) {
    m_seen_loading = true;
    return;
  }
  if (!m_seen_loading) {
    return;
  }
  if (!m_started) {
    m_started = true;
    qInfo() << "SOI_FILM: match loaded, film clock starts";
  }

  if (m_warmup_frames < k_warmup_frames) {
    if (!grab_frame()) {
      finish(21);
    }
    return;
  }

  const double dt = 1.0 / static_cast<double>(std::max(1, m_config.fps));
  m_previous_clock = m_clock;
  m_clock += dt;

  if (m_fixture != nullptr) {
    for (const auto& action : actions_between(*m_fixture, m_previous_clock, m_clock)) {
      apply_benchmark_action(m_engine, m_window, action);
    }
  }
  const auto step_start = std::chrono::steady_clock::now();
  m_engine->film_step(static_cast<float>(dt));
  const auto step_end = std::chrono::steady_clock::now();

  if (!grab_frame()) {
    finish(21);
    return;
  }
  const auto grab_end = std::chrono::steady_clock::now();
  if (m_frames_written % 30 == 1) {
    qInfo().noquote()
        << QStringLiteral("SOI_FILM: frame %1 step %2 ms grab+save %3 ms")
               .arg(m_frames_written)
               .arg(std::chrono::duration<double, std::milli>(step_end - step_start)
                        .count(),
                    0,
                    'f',
                    1)
               .arg(std::chrono::duration<double, std::milli>(grab_end - step_end)
                        .count(),
                    0,
                    'f',
                    1);
  }

  const int total_frames =
      static_cast<int>(std::lround(m_config.seconds * m_config.fps));
  if (m_frames_written >= total_frames) {
    finish(0);
  }
}

void FilmRecorder::finish(int exit_code) {
  m_timer->stop();
  drain_saves();
  {
    const std::lock_guard<std::mutex> lock(m_pending_mutex);
    if (m_save_failed && exit_code == 0) {
      exit_code = 21;
    }
  }
  QJsonObject manifest;
  manifest.insert(QStringLiteral("fps"), m_config.fps);
  manifest.insert(QStringLiteral("frames"), m_frames_written);
  manifest.insert(QStringLiteral("start_seconds"), m_config.start_seconds);
  manifest.insert(QStringLiteral("seconds"),
                  static_cast<double>(m_frames_written) / std::max(1, m_config.fps));
  manifest.insert(QStringLiteral("width"), m_config.width);
  manifest.insert(QStringLiteral("height"), m_config.height);
  QFile file(m_config.directory + QStringLiteral("/film.json"));
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
  }
  if (exit_code == 0) {
    qInfo().noquote() << QStringLiteral("SOI_FILM: PASS - %1 frames in %2")
                             .arg(m_frames_written)
                             .arg(m_config.directory);
  } else {
    qCritical().noquote() << QStringLiteral("SOI_FILM: FAIL - stopped after %1 frames")
                                 .arg(m_frames_written);
  }
  QGuiApplication::exit(exit_code);
}

} // namespace App::Core
