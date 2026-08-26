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
  for (const auto& entry : entries) {
    const QString text =
        entry.toObject().value(QStringLiteral("text")).toString().trimmed();
    if (!text.isEmpty()) {
      m_sources.push_back(text);
    }
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
