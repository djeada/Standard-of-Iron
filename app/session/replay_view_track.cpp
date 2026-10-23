#include "app/session/replay_view_track.h"

#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QTextStream>
#include <QWindow>

#include <algorithm>

#include "game/session/selection_service.h"
#include "scene/camera.h"

namespace App::Session {

namespace {

constexpr std::uint64_t k_sample_interval_ticks = 2;

auto vector_json(const QVector3D& value) -> QJsonArray {
  return {value.x(), value.y(), value.z()};
}

auto vector_from_json(const QJsonValue& value, QVector3D& out) -> bool {
  const QJsonArray array = value.toArray();
  if (array.size() != 3) {
    return false;
  }
  out = QVector3D(static_cast<float>(array[0].toDouble()),
                  static_cast<float>(array[1].toDouble()),
                  static_cast<float>(array[2].toDouble()));
  return true;
}

} // namespace

ReplayViewTrack::ReplayViewTrack() = default;

ReplayViewTrack::~ReplayViewTrack() {
  finish();
}

auto ReplayViewTrack::sidecar_path(const QString& replay_path) -> QString {
  return replay_path + QStringLiteral(".view.jsonl");
}

auto ReplayViewTrack::begin_recording(const QString& path) -> bool {
  finish();
  auto file = std::make_unique<QFile>(path);
  if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    return false;
  }
  m_file = std::move(file);
  m_last_tick.reset();
  m_last_selection.clear();
  m_selection_written = false;
  return true;
}

void ReplayViewTrack::note_pointer(QPointF normalized, int buttons) {
  m_pointer_x.store(static_cast<float>(normalized.x()), std::memory_order_relaxed);
  m_pointer_y.store(static_cast<float>(normalized.y()), std::memory_order_relaxed);
  m_pointer_buttons.store(buttons, std::memory_order_relaxed);
}

void ReplayViewTrack::record(std::uint64_t tick,
                             const Render::GL::Camera& camera,
                             const std::vector<Engine::Core::EntityID>& selection) {
  if (!m_file) {
    return;
  }
  if (!m_selection_written || selection != m_last_selection) {
    QJsonArray units;
    for (const auto id : selection) {
      units.append(static_cast<qint64>(id));
    }
    QJsonObject line;
    line["t"] = static_cast<qint64>(tick);
    line["sel"] = units;
    m_file->write(QJsonDocument(line).toJson(QJsonDocument::Compact));
    m_file->write("\n");
    m_last_selection = selection;
    m_selection_written = true;
  }

  const float pointer_x = m_pointer_x.load(std::memory_order_relaxed);
  const float pointer_y = m_pointer_y.load(std::memory_order_relaxed);
  const int pointer_buttons = m_pointer_buttons.load(std::memory_order_relaxed);
  const bool pointer_changed = pointer_buttons != m_last_pointer_buttons;
  if (m_last_tick.has_value() && tick < *m_last_tick + k_sample_interval_ticks &&
      !pointer_changed) {
    return;
  }
  QJsonObject line;
  line["t"] = static_cast<qint64>(tick);
  line["p"] = vector_json(camera.get_position());
  line["g"] = vector_json(camera.get_target());
  if (pointer_x >= 0.0F && pointer_y >= 0.0F) {
    line["c"] = QJsonArray{pointer_x, pointer_y, pointer_buttons};
  }
  m_file->write(QJsonDocument(line).toJson(QJsonDocument::Compact));
  m_file->write("\n");
  m_last_tick = tick;
  m_last_pointer_buttons = pointer_buttons;
}

void ReplayViewTrack::finish() {
  if (m_file) {
    m_file->flush();
    m_file->close();
    m_file.reset();
  }
}

auto ReplayViewTrack::load(const QString& path) -> bool {
  clear();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream in(&file);
  while (!in.atEnd()) {
    const QString text = in.readLine().trimmed();
    if (text.isEmpty()) {
      continue;
    }
    const QJsonObject line = QJsonDocument::fromJson(text.toUtf8()).object();
    const auto tick = static_cast<std::uint64_t>(line.value("t").toDouble(-1.0));
    if (!line.value("t").isDouble()) {
      continue;
    }
    if (line.contains("sel")) {
      ReplayViewSelection entry{.tick = tick};
      for (const QJsonValue id : line.value("sel").toArray()) {
        entry.units.push_back(static_cast<Engine::Core::EntityID>(id.toDouble()));
      }
      m_selections.push_back(std::move(entry));
      continue;
    }
    ReplayViewSample sample{.tick = tick};
    if (!vector_from_json(line.value("p"), sample.position) ||
        !vector_from_json(line.value("g"), sample.target)) {
      continue;
    }
    if (const QJsonArray pointer = line.value("c").toArray(); pointer.size() == 3) {
      sample.pointer = ReplayViewPointer{
          .position = QPointF(pointer[0].toDouble(), pointer[1].toDouble()),
          .buttons = pointer[2].toInt()};
    }
    m_samples.push_back(sample);
  }
  const auto by_tick = [](const auto& a, const auto& b) {
    return a.tick < b.tick;
  };
  std::stable_sort(m_samples.begin(), m_samples.end(), by_tick);
  std::stable_sort(m_selections.begin(), m_selections.end(), by_tick);
  return loaded();
}

void ReplayViewTrack::clear() {
  m_samples.clear();
  m_selections.clear();
  m_applied_selection = nullptr;
  m_applied_pointer.reset();
}

auto ReplayViewTrack::sample_at(std::uint64_t tick) const
    -> std::optional<ReplayViewSample> {
  if (m_samples.empty()) {
    return std::nullopt;
  }
  const auto next = std::upper_bound(
      m_samples.begin(),
      m_samples.end(),
      tick,
      [](std::uint64_t value, const ReplayViewSample& s) { return value < s.tick; });
  if (next == m_samples.begin()) {
    return m_samples.front();
  }
  const auto& before = *(next - 1);
  if (next == m_samples.end() || before.tick == tick) {
    return before;
  }
  const auto& after = *next;
  const float t = static_cast<float>(tick - before.tick) /
                  static_cast<float>(after.tick - before.tick);
  ReplayViewSample blended{.tick = tick,
                           .position =
                               before.position + (after.position - before.position) * t,
                           .target = before.target + (after.target - before.target) * t,
                           .pointer = before.pointer};
  if (before.pointer.has_value() && after.pointer.has_value()) {
    blended.pointer->position =
        before.pointer->position +
        (after.pointer->position - before.pointer->position) * static_cast<double>(t);
  }
  return blended;
}

auto ReplayViewTrack::selection_at(std::uint64_t tick) const
    -> const std::vector<Engine::Core::EntityID>* {
  const auto next = std::upper_bound(
      m_selections.begin(),
      m_selections.end(),
      tick,
      [](std::uint64_t value, const ReplayViewSelection& s) { return value < s.tick; });
  if (next == m_selections.begin()) {
    return nullptr;
  }
  return &(next - 1)->units;
}

void ReplayViewTrack::apply(std::uint64_t tick,
                            Render::GL::Camera& camera,
                            Game::Session::SelectionService& selection) {
  if (const auto sample = sample_at(tick)) {
    camera.look_at(sample->position, sample->target, QVector3D(0.0F, 1.0F, 0.0F));
    m_applied_pointer = sample->pointer;
  }
  const auto* units = selection_at(tick);
  if (units == nullptr || units == m_applied_selection) {
    return;
  }
  m_applied_selection = units;
  selection.clear_selection();
  for (const auto id : *units) {
    selection.select_unit(id);
  }
}

ReplayPointerTap::ReplayPointerTap(ReplayViewTrack& track, QObject* parent)
    : QObject(parent)
    , m_track(track) {
}

auto ReplayPointerTap::eventFilter(QObject* watched, QEvent* event) -> bool {
  const auto type = event->type();
  if (type == QEvent::MouseMove || type == QEvent::MouseButtonPress ||
      type == QEvent::MouseButtonRelease) {
    auto* window = qobject_cast<QWindow*>(watched);
    const auto* mouse = dynamic_cast<QMouseEvent*>(event);
    if (window != nullptr && mouse != nullptr && window->width() > 0 &&
        window->height() > 0) {
      const QPointF at = mouse->position();
      int buttons = 0;
      if ((mouse->buttons() & Qt::LeftButton) != 0) {
        buttons |= ReplayViewPointer::k_left;
      }
      if ((mouse->buttons() & Qt::RightButton) != 0) {
        buttons |= ReplayViewPointer::k_right;
      }
      m_track.note_pointer(QPointF(at.x() / window->width(), at.y() / window->height()),
                           buttons);
    }
  }
  return QObject::eventFilter(watched, event);
}

} // namespace App::Session
