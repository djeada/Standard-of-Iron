#include "app/models/loading_tips.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>
#include <chrono>
#include <utility>

#include "game/util/asset_text.h"

namespace {

Q_LOGGING_CATEGORY(loading_tips_logger, "soi.loading.tips")

constexpr const char* k_asset_context = "LoadingTips";
constexpr const char* k_relative_path = "assets/data/loading_tips.json";

auto resolve_data_path(const QString& relative) -> QString {
  const QString direct = QDir::current().filePath(relative);
  if (QFile::exists(direct)) {
    return direct;
  }

  const QString app_dir = QCoreApplication::applicationDirPath();
  if (app_dir.isEmpty()) {
    return {};
  }
  const QString from_app = QDir(app_dir).filePath(relative);
  if (QFile::exists(from_app)) {
    return from_app;
  }
  const QString parent = QDir(app_dir).filePath("../" + relative);
  if (QFile::exists(parent)) {
    return parent;
  }
  return {};
}

} // namespace

LoadingTips::LoadingTips(QObject* parent)
    : QObject(parent) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  m_rng.seed(static_cast<std::mt19937::result_type>(now));
}

auto LoadingTips::instance() -> LoadingTips* {
  static LoadingTips tips;
  return &tips;
}

auto LoadingTips::create(QQmlEngine* engine, QJSEngine* script_engine) -> LoadingTips* {
  Q_UNUSED(engine)
  Q_UNUSED(script_engine)
  auto* tips = instance();
  QQmlEngine::setObjectOwnership(tips, QQmlEngine::CppOwnership);
  return tips;
}

void LoadingTips::load_from_json(const QByteArray& payload) {
  m_sources.clear();
  m_tags.clear();
  m_deck.clear();
  m_last_drawn = -1;
  m_loaded = true;

  QJsonParseError error{};
  const auto document = QJsonDocument::fromJson(payload, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    qCWarning(loading_tips_logger)
        << "loading tips are not a JSON object:" << error.errorString();
    emit tips_changed();
    return;
  }

  const auto entries = document.object().value(QStringLiteral("tips")).toArray();
  m_sources.reserve(static_cast<std::size_t>(entries.size()));
  m_tags.reserve(static_cast<std::size_t>(entries.size()));
  for (const auto entry : entries) {
    const QJsonObject tip = entry.toObject();
    const QString text = tip.value(QStringLiteral("text")).toString().trimmed();
    if (text.isEmpty()) {
      continue;
    }
    QStringList tags;
    for (const auto tag : tip.value(QStringLiteral("tags")).toArray()) {
      const QString name = tag.toString().trimmed().toLower();
      if (!name.isEmpty()) {
        tags.append(name);
      }
    }
    m_sources.push_back(text);
    m_tags.push_back(std::move(tags));
  }

  emit tips_changed();
}

void LoadingTips::ensure_loaded() {
  if (m_loaded) {
    return;
  }
  m_loaded = true;

  const QString path = resolve_data_path(QString::fromLatin1(k_relative_path));
  if (path.isEmpty()) {
    qCWarning(loading_tips_logger) << "no loading tips found at" << k_relative_path;
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(loading_tips_logger) << "could not open loading tips at" << path;
    return;
  }
  load_from_json(file.readAll());
}

int LoadingTips::count() {
  ensure_loaded();
  return static_cast<int>(m_sources.size());
}

QStringList LoadingTips::source_texts() {
  ensure_loaded();
  QStringList out;
  out.reserve(static_cast<int>(m_sources.size()));
  for (const auto& source : m_sources) {
    out.append(source);
  }
  return out;
}

QStringList LoadingTips::tags_of(const QString& source_text) {
  ensure_loaded();
  for (std::size_t i = 0; i < m_sources.size(); ++i) {
    if (m_sources[i] == source_text) {
      return m_tags[i];
    }
  }
  return {};
}

void LoadingTips::set_preferred_tags(QStringList tags) {
  QStringList lowered;
  for (const QString& tag : tags) {
    const QString name = tag.trimmed().toLower();
    if (!name.isEmpty() && !lowered.contains(name)) {
      lowered.append(name);
    }
  }
  if (lowered == m_preferred_tags) {
    return;
  }
  m_preferred_tags = std::move(lowered);
  m_deck.clear();
}

void LoadingTips::prefer_for_load(const QString& map_path,
                                  const QString& mission_id,
                                  bool mission_has_undead) {
  set_preferred_tags(tags_for_load(map_path, mission_id, mission_has_undead));
}

QStringList LoadingTips::tags_for_load(const QString& map_path,
                                       const QString& mission_id,
                                       bool mission_has_undead) {
  QStringList tags;
  const QString map = map_path.toLower();
  const QString mission = mission_id.toLower();
  const bool undead = mission_has_undead || map.contains(QStringLiteral("sepulcher")) ||
                      mission.contains(QStringLiteral("sepulcher")) ||
                      map.contains(QStringLiteral("zama")) ||
                      mission.contains(QStringLiteral("zama"));
  if (undead) {
    tags.append(QStringLiteral("undead"));
  }
  return tags;
}

bool LoadingTips::is_preferred(int index) const {
  if (m_preferred_tags.isEmpty() || index < 0 ||
      static_cast<std::size_t>(index) >= m_tags.size()) {
    return false;
  }
  for (const QString& tag : m_tags[static_cast<std::size_t>(index)]) {
    if (m_preferred_tags.contains(tag)) {
      return true;
    }
  }
  return false;
}

void LoadingTips::reseed(quint32 seed) {
  m_rng.seed(static_cast<std::mt19937::result_type>(seed));
  m_deck.clear();
  m_last_drawn = -1;
}

void LoadingTips::refill_deck() {
  m_deck.resize(m_sources.size());
  for (std::size_t i = 0; i < m_sources.size(); ++i) {
    m_deck[i] = static_cast<int>(i);
  }
  std::shuffle(m_deck.begin(), m_deck.end(), m_rng);
  std::stable_partition(
      m_deck.begin(), m_deck.end(), [this](int index) { return !is_preferred(index); });

  if (m_deck.size() > 1U && m_deck.back() == m_last_drawn) {
    std::swap(m_deck.back(), m_deck.front());
  }
}

QString LoadingTips::next() {
  ensure_loaded();
  if (m_sources.empty()) {
    return {};
  }
  if (m_deck.empty()) {
    refill_deck();
  }

  const int index = m_deck.back();
  m_deck.pop_back();
  m_last_drawn = index;
  return Game::Util::tr_asset(k_asset_context,
                              m_sources[static_cast<std::size_t>(index)]);
}
