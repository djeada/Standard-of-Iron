#include "arena_audio_recorder.h"

#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/audio_system.h"
#include "game/core/ambient_session.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"

namespace Arena::Promo {

namespace {

constexpr float k_ambient_check_seconds = 2.0F;
constexpr float k_combat_radius = 15.0F;
constexpr int k_wav_channels = 2;

constexpr float k_score_rotation_seconds = 120.0F;

constexpr float k_reel_true_peak_ceiling = 0.794F;

constexpr float k_reel_delivered_peak_dbfs = -1.0F;

} // namespace

AudioRecorder::AudioRecorder() = default;

AudioRecorder::~AudioRecorder() {
  stop();
}

auto AudioRecorder::start(Engine::Core::World* world,
                          Game::Systems::NationRegistry& nations) -> bool {
  if (m_running || world == nullptr) {
    return m_running;
  }
  qputenv("SOI_AUDIO_OFFLINE", "1");
  auto& audio = AudioSystem::get_instance();
  if (!audio.initialize()) {
    qWarning() << "AudioRecorder: the audio system refused to start offline";
    return false;
  }

  audio.apply_offline_reference_mix();
  m_world = world;
  m_handler = std::make_unique<Game::Audio::AudioEventHandler>(world);
  if (!m_handler->initialize()) {
    qWarning() << "AudioRecorder: the audio event handler refused to start";
    m_handler.reset();
    return false;
  }
  m_handler->set_local_owner_id(0);
  AudioResourceLoader::load_audio_cues();
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Startup);
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  m_coordinator = std::make_unique<AudioCoordinator>(m_handler.get(), nations);
  m_coordinator->configure_audio_manifest_mappings(0);
  m_coordinator->apply_mission_ambience(nullptr, QString{}, 0);

  m_ambient_state = resting_state();
  Engine::Core::EventManager::instance().publish(Engine::Core::AmbientStateChangedEvent(
      m_ambient_state, Engine::Core::AmbientState::PEACEFUL));
  m_ambient_timer = 0.0F;
  m_score_timer = 0.0F;
  m_sample_carry = 0.0;
  m_running = true;
  qInfo() << "AudioRecorder: game audio running offline at" << k_sample_rate << "Hz";
  return true;
}

void AudioRecorder::play_music_bed(const QString& track_id, float volume) {
  if (!m_running || track_id.isEmpty() || volume <= 0.0F) {
    return;
  }
  m_scored_bed = true;
  AudioResourceLoader::ensure_audio_resource_loaded(track_id);

  AudioSystem::get_instance().play_sound(
      track_id.toStdString(), volume, true, 1, AudioCategory::MUSIC);
  qInfo().noquote() << QStringLiteral("AudioRecorder: score bed %1 at %2")
                           .arg(track_id, QString::number(volume, 'f', 2));
}

void AudioRecorder::play_one_shot(const QString& track_id, float volume) {
  if (!m_running || track_id.isEmpty() || volume <= 0.0F) {
    return;
  }
  AudioResourceLoader::ensure_audio_resource_loaded(track_id);
  AudioSystem::get_instance().play_sound(
      track_id.toStdString(), volume, false, 1, AudioCategory::MUSIC);
  qInfo().noquote() << QStringLiteral("AudioRecorder: report sound %1 at %2")
                           .arg(track_id, QString::number(volume, 'f', 2));
}

void AudioRecorder::advance(float seconds, bool record) {
  if (!m_running || seconds <= 0.0F) {
    return;
  }
  update_ambient_state(seconds);
  rotate_score(seconds);

  m_sample_carry += static_cast<double>(seconds) * k_sample_rate;
  const auto frames = static_cast<unsigned>(std::floor(m_sample_carry));
  m_sample_carry -= frames;
  if (frames == 0U) {
    return;
  }
  m_scratch.resize(static_cast<std::size_t>(frames) * k_wav_channels);
  AudioSystem::get_instance().render_offline(m_scratch.data(), frames);
  if (record) {
    m_clip.insert(m_clip.end(), m_scratch.begin(), m_scratch.end());
  }
}

void AudioRecorder::begin_clip() {
  m_clip.clear();
}

auto AudioRecorder::clip_seconds() const -> float {
  return static_cast<float>(m_clip.size() / k_wav_channels) /
         static_cast<float>(k_sample_rate);
}

auto AudioRecorder::write_clip(const QString& wav_path) -> bool {
  QFile file(wav_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "AudioRecorder: cannot write" << wav_path;
    return false;
  }
  const auto sample_count = static_cast<quint32>(m_clip.size());
  const quint32 data_bytes = sample_count * sizeof(qint16);
  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);
  out.writeRawData("RIFF", 4);
  out << static_cast<quint32>(36 + data_bytes);
  out.writeRawData("WAVE", 4);
  out.writeRawData("fmt ", 4);
  out << static_cast<quint32>(16) << static_cast<quint16>(1)
      << static_cast<quint16>(k_wav_channels) << static_cast<quint32>(k_sample_rate)
      << static_cast<quint32>(k_sample_rate * k_wav_channels * sizeof(qint16))
      << static_cast<quint16>(k_wav_channels * sizeof(qint16))
      << static_cast<quint16>(16);
  out.writeRawData("data", 4);
  out << data_bytes;
  std::vector<qint16> pcm(m_clip.size());
  for (std::size_t index = 0; index < m_clip.size(); ++index) {
    const float clamped = std::clamp(m_clip[index], -1.0F, 1.0F);
    pcm[index] = static_cast<qint16>(std::lround(clamped * 32767.0F));
  }
  out.writeRawData(reinterpret_cast<const char*>(pcm.data()),
                   static_cast<int>(pcm.size() * sizeof(qint16)));
  return out.status() == QDataStream::Ok;
}

void AudioRecorder::stop() {
  if (!m_running) {
    return;
  }
  if (m_coordinator != nullptr) {
    m_coordinator->stop_mission_ambience();
  }
  m_coordinator.reset();
  if (m_handler != nullptr) {
    m_handler->shutdown();
  }
  m_handler.reset();
  AudioSystem::get_instance().shutdown();
  m_world = nullptr;
  m_running = false;
  m_scored_bed = false;
}

auto AudioRecorder::measure_loudness(const QStringList& wav_paths,
                                     QString* error) -> std::optional<float> {
  if (wav_paths.isEmpty()) {
    return std::nullopt;
  }
  const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if (ffmpeg.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg was not found on PATH");
    }
    return std::nullopt;
  }

  const QString list_path = wav_paths.front() + QStringLiteral(".concat.txt");
  QFile list(list_path);
  if (!list.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = QStringLiteral("cannot write %1").arg(list_path);
    }
    return std::nullopt;
  }
  for (const QString& wav_path : wav_paths) {

    const QString absolute = QFileInfo(wav_path).absoluteFilePath();
    const QString quoted =
        QString(absolute).replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    list.write(QStringLiteral("file '%1'\n").arg(quoted).toUtf8());
  }
  list.close();

  QProcess probe;
  probe.start(ffmpeg,
              {QStringLiteral("-hide_banner"),
               QStringLiteral("-nostats"),
               QStringLiteral("-f"),
               QStringLiteral("concat"),
               QStringLiteral("-safe"),
               QStringLiteral("0"),
               QStringLiteral("-i"),
               list_path,
               QStringLiteral("-map"),
               QStringLiteral("0:a:0"),
               QStringLiteral("-af"),
               QStringLiteral("ebur128=framelog=quiet"),
               QStringLiteral("-f"),
               QStringLiteral("null"),
               QStringLiteral("-")});
  probe.waitForFinished(-1);
  const QString report = QString::fromUtf8(probe.readAllStandardError());
  QFile::remove(list_path);

  if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
    if (error != nullptr) {
      const QString tail = report.trimmed().section(QLatin1Char('\n'), -3);
      *error = QStringLiteral("ffmpeg loudness pass exited with status %1: %2")
                   .arg(QString::number(probe.exitCode()), tail);
    }
    return std::nullopt;
  }

  const int summary_at = report.lastIndexOf(QStringLiteral("Summary:"));
  static const QRegularExpression integrated(
      QStringLiteral("\\bI:\\s*(-?\\d+(?:\\.\\d+)?)\\s*LUFS"));
  const QRegularExpressionMatch match =
      integrated.match(report, summary_at < 0 ? 0 : summary_at);
  if (!match.hasMatch()) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg reported no integrated loudness");
    }
    return std::nullopt;
  }
  bool ok = false;
  const float lufs = match.captured(1).toFloat(&ok);
  if (!ok || !std::isfinite(lufs)) {
    if (error != nullptr) {
      *error =
          QStringLiteral("unreadable integrated loudness '%1'").arg(match.captured(1));
    }
    return std::nullopt;
  }
  return lufs;
}

auto AudioRecorder::measure_true_peak(const QString& media_path,
                                      QString* error) -> std::optional<float> {
  const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if (ffmpeg.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg was not found on PATH");
    }
    return std::nullopt;
  }

  QProcess probe;
  probe.start(ffmpeg,
              {QStringLiteral("-hide_banner"),
               QStringLiteral("-nostats"),
               QStringLiteral("-i"),
               media_path,
               QStringLiteral("-map"),
               QStringLiteral("0:a:0"),
               QStringLiteral("-af"),
               QStringLiteral("ebur128=peak=true:framelog=quiet"),
               QStringLiteral("-f"),
               QStringLiteral("null"),
               QStringLiteral("-")});
  probe.waitForFinished(-1);
  const QString report = QString::fromUtf8(probe.readAllStandardError());
  if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg peak pass exited with status %1")
                   .arg(probe.exitCode());
    }
    return std::nullopt;
  }

  const int peak_at = report.lastIndexOf(QStringLiteral("True peak:"));
  static const QRegularExpression peak(
      QStringLiteral("\\bPeak:\\s*(-?\\d+(?:\\.\\d+)?)\\s*dBFS"));
  const QRegularExpressionMatch match = peak.match(report, peak_at < 0 ? 0 : peak_at);
  if (!match.hasMatch()) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg reported no true peak");
    }
    return std::nullopt;
  }
  bool ok = false;
  const float dbfs = match.captured(1).toFloat(&ok);
  if (!ok || !std::isfinite(dbfs)) {
    if (error != nullptr) {
      *error = QStringLiteral("unreadable true peak '%1'").arg(match.captured(1));
    }
    return std::nullopt;
  }
  return dbfs;
}

auto AudioRecorder::delivered_peak_ceiling_dbfs() -> float {
  return k_reel_delivered_peak_dbfs;
}

auto AudioRecorder::mux(const QString& clip_path,
                        const QString& wav_path,
                        float gain_db,
                        QString* error) -> bool {
  const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if (ffmpeg.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg was not found on PATH");
    }
    return false;
  }
  const QString muxed = clip_path + QStringLiteral(".muxed.mp4");
  QStringList arguments{QStringLiteral("-hide_banner"),
                        QStringLiteral("-loglevel"),
                        QStringLiteral("error"),
                        QStringLiteral("-y"),
                        QStringLiteral("-i"),
                        clip_path,
                        QStringLiteral("-i"),
                        wav_path,
                        QStringLiteral("-map"),
                        QStringLiteral("0:v:0"),
                        QStringLiteral("-map"),
                        QStringLiteral("1:a:0"),
                        QStringLiteral("-c:v"),
                        QStringLiteral("copy")};
  if (gain_db > 0.0F) {
    arguments << QStringLiteral("-filter:a")
              << QStringLiteral("volume=%1dB,alimiter=limit=%2:level=disabled")
                     .arg(QString::number(gain_db, 'f', 2),
                          QString::number(k_reel_true_peak_ceiling, 'f', 3));
  }
  arguments << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a")
            << QStringLiteral("256k") << QStringLiteral("-shortest")
            << QStringLiteral("-movflags") << QStringLiteral("+faststart") << muxed;
  const int status = QProcess::execute(ffmpeg, arguments);
  if (status != 0) {
    if (error != nullptr) {
      *error = QStringLiteral("ffmpeg mux exited with status %1").arg(status);
    }
    QFile::remove(muxed);
    return false;
  }
  if (!QFile::remove(clip_path) || !QFile::rename(muxed, clip_path)) {
    if (error != nullptr) {
      *error =
          QStringLiteral("could not replace %1 with the muxed clip").arg(clip_path);
    }
    return false;
  }
  QFile::remove(wav_path);
  return true;
}

void AudioRecorder::update_ambient_state(float seconds) {
  m_ambient_timer += seconds;
  if (m_ambient_timer < k_ambient_check_seconds) {
    return;
  }
  m_ambient_timer = 0.0F;
  const auto next =
      any_side_in_combat() ? Engine::Core::AmbientState::COMBAT : resting_state();
  if (next == m_ambient_state) {
    return;
  }
  const auto previous = m_ambient_state;
  m_ambient_state = next;
  m_score_timer = 0.0F;
  qInfo().noquote() << QStringLiteral("AudioRecorder: ambient state %1 -> %2")
                           .arg(static_cast<int>(previous))
                           .arg(static_cast<int>(next));
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AmbientStateChangedEvent(next, previous));
}

void AudioRecorder::rotate_score(float seconds) {
  if (m_scored_bed || m_handler == nullptr) {
    return;
  }
  m_score_timer += seconds;
  if (m_score_timer < k_score_rotation_seconds) {
    return;
  }
  m_score_timer = 0.0F;
  if (m_handler->rotate_ambient_music(m_ambient_state)) {
    qInfo().noquote() << QStringLiteral("AudioRecorder: score rotated");
  }
}

auto AudioRecorder::resting_state() const -> Engine::Core::AmbientState {
  return has_opposing_forces() ? Engine::Core::AmbientState::TENSE
                               : Engine::Core::AmbientState::PEACEFUL;
}

auto AudioRecorder::has_opposing_forces() const -> bool {
  if (m_world == nullptr) {
    return false;
  }
  int first_owner = 0;
  for (auto [entity_id, unit] : m_world->view<const Engine::Core::UnitComponent>()) {
    if (unit.health <= 0 || unit.owner_id <= 0) {
      continue;
    }
    if (first_owner == 0) {
      first_owner = unit.owner_id;
    } else if (unit.owner_id != first_owner) {
      return true;
    }
  }
  return false;
}

auto AudioRecorder::any_side_in_combat() const -> bool {
  if (m_world == nullptr) {
    return false;
  }
  struct Fighter {
    int owner;
    float x;
    float z;
  };
  std::vector<Fighter> fighters;
  for (const auto entity_id : m_world->entities_with<Engine::Core::UnitComponent>()) {
    auto* entity = m_world->get_entity(entity_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        unit->owner_id <= 0) {
      continue;
    }
    if (entity->has_component<Engine::Core::AttackTargetComponent>() &&
        !entity->has_component<Engine::Core::BuildingComponent>()) {
      return true;
    }
    fighters.push_back({unit->owner_id, transform->position.x, transform->position.z});
  }
  const float radius_sq = k_combat_radius * k_combat_radius;
  for (const auto& a : fighters) {
    for (const auto& b : fighters) {
      if (a.owner == b.owner) {
        continue;
      }
      const float dx = a.x - b.x;
      const float dz = a.z - b.z;
      if (dx * dx + dz * dz < radius_sq) {
        return true;
      }
    }
  }
  return false;
}

} // namespace Arena::Promo
