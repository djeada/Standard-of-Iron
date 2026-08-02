#include "wave_archetype_catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace Game::Mission {

namespace {

Q_LOGGING_CATEGORY(wave_archetype_logger, "soi.mission.waves")

constexpr const char* k_overlay_relative_path = "assets/data/waves/archetypes.json";

auto resolve_data_path(const QString& relative) -> QString {
  const QString direct = QDir::current().filePath(relative);
  if (QFile::exists(direct)) {
    return direct;
  }

  const QString app_dir = QCoreApplication::applicationDirPath();
  if (!app_dir.isEmpty()) {
    const QString from_app = QDir(app_dir).filePath(relative);
    if (QFile::exists(from_app)) {
      return from_app;
    }
    const QString parent = QDir(app_dir).filePath("../" + relative);
    if (QFile::exists(parent)) {
      return QDir(parent).canonicalPath();
    }
  }

  const QString resource_path = QStringLiteral(":/") + relative;
  if (QFile::exists(resource_path)) {
    return resource_path;
  }

  return {};
}

auto make(const char* id,
          const char* label,
          std::vector<WaveComposition> composition) -> WaveArchetype {
  WaveArchetype archetype;
  archetype.id = QString::fromLatin1(id);
  archetype.label = QString::fromLatin1(label);
  archetype.composition = std::move(composition);
  return archetype;
}

auto entry(const char* type, int count, bool elite = false) -> WaveComposition {
  WaveComposition composition;
  composition.type = QString::fromLatin1(type);
  composition.count = count;
  composition.elite = elite;
  return composition;
}

} // namespace

WaveArchetypeCatalog::WaveArchetypeCatalog() {
  load_built_ins();
  apply_overlay();
}

auto WaveArchetypeCatalog::instance() -> WaveArchetypeCatalog& {
  static WaveArchetypeCatalog catalog;
  return catalog;
}

void WaveArchetypeCatalog::reload() {
  m_archetypes.clear();
  load_built_ins();
  apply_overlay();
}

void WaveArchetypeCatalog::load_built_ins() {
  m_archetypes.push_back(make("probe",
                              QT_TRANSLATE_NOOP("WaveArchetype", "Probing column"),
                              {entry("swordsman", 4), entry("archer", 2)}));
  m_archetypes.push_back(
      make("assault",
           QT_TRANSLATE_NOOP("WaveArchetype", "Assault line"),
           {entry("spearman", 8), entry("swordsman", 6), entry("archer", 4)}));
  m_archetypes.push_back(make("cavalry_flank",
                              QT_TRANSLATE_NOOP("WaveArchetype", "Cavalry flank"),
                              {entry("horse_swordsman", 6), entry("horse_archer", 3)}));
  m_archetypes.push_back(
      make("skirmish_screen",
           QT_TRANSLATE_NOOP("WaveArchetype", "Skirmish screen"),
           {entry("archer", 6), entry("horse_archer", 3), entry("spearman", 3)}));
  m_archetypes.push_back(
      make("siege_column",
           QT_TRANSLATE_NOOP("WaveArchetype", "Siege column"),
           {entry("catapult", 1), entry("spearman", 6), entry("swordsman", 4)}));
  m_archetypes.push_back(make("elite_guard",
                              QT_TRANSLATE_NOOP("WaveArchetype", "Elite guard"),
                              {entry("swordsman", 8, true),
                               entry("spearman", 6),
                               entry("archer", 4),
                               entry("horse_swordsman", 3)}));
}

void WaveArchetypeCatalog::apply_overlay() {
  const QString path = resolve_data_path(QString::fromLatin1(k_overlay_relative_path));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCWarning(wave_archetype_logger) << "wave archetype overlay unreadable:" << path;
    return;
  }

  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  file.close();
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
    qCWarning(wave_archetype_logger)
        << "wave archetype overlay is not a JSON object:" << path
        << parse_error.errorString();
    return;
  }

  const QJsonArray archetypes = doc.object().value("archetypes").toArray();
  for (const auto& value : archetypes) {
    const QJsonObject obj = value.toObject();
    const QString id = obj.value("id").toString().trimmed().toLower();
    if (id.isEmpty()) {
      continue;
    }

    WaveArchetype archetype;
    archetype.id = id;
    archetype.label = obj.value("label").toString();
    const QJsonArray composition = obj.value("composition").toArray();
    for (const auto& comp_value : composition) {
      const QJsonObject comp_obj = comp_value.toObject();
      WaveComposition comp;
      comp.type = comp_obj.value("type").toString().trimmed().toLower();
      comp.count = std::max(1, comp_obj.value("count").toInt(1));
      comp.elite = comp_obj.value("elite").toBool(false);
      comp.title = comp_obj.value("title").toString();
      if (!comp.type.isEmpty()) {
        archetype.composition.push_back(std::move(comp));
      }
    }
    if (archetype.composition.empty()) {
      qCWarning(wave_archetype_logger)
          << "wave archetype" << id << "declares no usable composition; ignored";
      continue;
    }

    const auto existing = std::find_if(
        m_archetypes.begin(),
        m_archetypes.end(),
        [&id](const WaveArchetype& candidate) { return candidate.id == id; });
    if (existing == m_archetypes.end()) {
      m_archetypes.push_back(std::move(archetype));
    } else {
      qCInfo(wave_archetype_logger)
          << "wave archetype" << id << "overridden by" << path;
      *existing = std::move(archetype);
    }
  }
}

auto WaveArchetypeCatalog::find(const QString& id) const -> const WaveArchetype* {
  const QString key = id.trimmed().toLower();
  const auto match = std::find_if(
      m_archetypes.begin(), m_archetypes.end(), [&key](const WaveArchetype& candidate) {
        return candidate.id == key;
      });
  return match == m_archetypes.end() ? nullptr : &*match;
}

auto WaveArchetypeCatalog::ids() const -> std::vector<QString> {
  std::vector<QString> result;
  result.reserve(m_archetypes.size());
  for (const auto& archetype : m_archetypes) {
    result.push_back(archetype.id);
  }
  return result;
}

auto WaveArchetypeCatalog::expand(const QString& id, float strength) const
    -> std::vector<WaveComposition> {
  const WaveArchetype* archetype = find(id);
  if (archetype == nullptr) {
    qCWarning(wave_archetype_logger) << "unknown wave archetype" << id;
    return {};
  }
  return scale_wave_composition(archetype->composition, strength);
}

auto difficulty_strength_multiplier(const QString& difficulty) -> float {
  const QString key = difficulty.trimmed().toLower();
  if (key.isEmpty() || key == QLatin1String("normal") ||
      key == QLatin1String("medium")) {
    return 1.0F;
  }
  if (key == QLatin1String("easy") || key == QLatin1String("recruit")) {
    return 0.75F;
  }
  if (key == QLatin1String("hard")) {
    return 1.2F;
  }
  if (key == QLatin1String("very_hard") || key == QLatin1String("brutal") ||
      key == QLatin1String("legendary")) {
    return 1.4F;
  }
  return 1.0F;
}

auto scale_wave_composition(const std::vector<WaveComposition>& source,
                            float multiplier) -> std::vector<WaveComposition> {
  std::vector<WaveComposition> result;
  result.reserve(source.size());
  const float clamped = std::clamp(multiplier, 0.1F, 8.0F);
  for (const auto& comp : source) {
    WaveComposition scaled = comp;
    const float raw = static_cast<float>(std::max(1, comp.count)) * clamped;
    scaled.count = std::max(1, static_cast<int>(std::lround(raw)));
    result.push_back(std::move(scaled));
  }
  return result;
}

} // namespace Game::Mission
