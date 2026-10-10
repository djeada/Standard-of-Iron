#include "battle_script.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>

#include "game/map/environment_lighting.h"
#include "game/map/ground_type.h"
#include "game/map/map_loader.h"
#include "game/systems/nation_id.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_config.h"
#include "utils/resource_utils.h"

namespace Arena::BattleScript {

namespace {

const QSet<QString> k_ignored_keys = {QStringLiteral("todo"),
                                      QStringLiteral("note"),
                                      QStringLiteral("notes"),
                                      QStringLiteral("comment"),
                                      QStringLiteral("source"),
                                      QStringLiteral("sources"),
                                      QStringLiteral("historical_note")};

const QStringList k_lighting_profiles = {QStringLiteral("mediterranean_summer"),
                                         QStringLiteral("delta_haze"),
                                         QStringLiteral("canyon_dry"),
                                         QStringLiteral("pine_overcast"),
                                         QStringLiteral("alpine_clear"),
                                         QStringLiteral("river_mist"),
                                         QStringLiteral("iberian_high_sun"),
                                         QStringLiteral("arena_neutral"),
                                         QStringLiteral("iron_sepulcher")};

constexpr int k_first_owner_id = 2;

auto child(const QString& path, const QString& key) -> QString {
  return path + QLatin1Char('.') + key;
}

auto index_path(const QString& path, qsizetype index) -> QString {
  return QStringLiteral("%1[%2]").arg(path).arg(index);
}

struct Context {
  std::vector<Diagnostic>* errors = nullptr;
  std::vector<Diagnostic>* warnings = nullptr;
  float geometry{1.0F};

  void error(const QString& path, const QString& message) const {
    errors->push_back({path, message});
  }
  void warn(const QString& path, const QString& message) const {
    warnings->push_back({path, message});
  }

  void check_keys(const QJsonObject& object,
                  const QString& path,
                  std::initializer_list<const char*> allowed) const {
    QSet<QString> known;
    for (const char* key : allowed) {
      known.insert(QString::fromLatin1(key));
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
      const QString& key = it.key();
      if (known.contains(key) || k_ignored_keys.contains(key) ||
          key.startsWith(QLatin1Char('_'))) {
        continue;
      }
      error(child(path, key), QStringLiteral("unknown field '%1'").arg(key));
    }
  }

  [[nodiscard]] auto number(const QJsonObject& object,
                            const QString& path,
                            const char* key,
                            std::optional<float> fallback,
                            float minimum = -1.0e9F,
                            float maximum = 1.0e9F) const -> std::optional<float> {
    const QString name = QString::fromLatin1(key);
    const QJsonValue value = object.value(name);
    if (value.isUndefined() || value.isNull()) {
      if (!fallback.has_value()) {
        error(child(path, name), QStringLiteral("required number is missing"));
      }
      return fallback;
    }
    if (!value.isDouble()) {
      error(child(path, name), QStringLiteral("expected a number"));
      return fallback;
    }
    const auto result = static_cast<float>(value.toDouble());
    if (result < minimum || result > maximum) {
      error(child(path, name),
            QStringLiteral("%1 is outside [%2, %3]")
                .arg(result)
                .arg(minimum)
                .arg(maximum));
      return fallback;
    }
    return result;
  }

  [[nodiscard]] auto
  optional_number(const QJsonObject& object,
                  const QString& path,
                  const char* key,
                  float minimum = -1.0e9F,
                  float maximum = 1.0e9F) const -> std::optional<float> {
    if (!object.contains(QString::fromLatin1(key))) {
      return std::nullopt;
    }
    return number(object, path, key, std::optional<float>{}, minimum, maximum);
  }

  [[nodiscard]] auto string(const QJsonObject& object,
                            const QString& path,
                            const char* key,
                            bool required) const -> QString {
    const QString name = QString::fromLatin1(key);
    const QJsonValue value = object.value(name);
    if (value.isUndefined() || value.isNull()) {
      if (required) {
        error(child(path, name), QStringLiteral("required string is missing"));
      }
      return {};
    }
    if (!value.isString()) {
      error(child(path, name), QStringLiteral("expected a string"));
      return {};
    }
    return value.toString().trimmed();
  }

  [[nodiscard]] auto boolean(const QJsonObject& object,
                             const QString& path,
                             const char* key,
                             bool fallback) const -> bool {
    const QString name = QString::fromLatin1(key);
    const QJsonValue value = object.value(name);
    if (value.isUndefined() || value.isNull()) {
      return fallback;
    }
    if (!value.isBool()) {
      error(child(path, name), QStringLiteral("expected true or false"));
      return fallback;
    }
    return value.toBool();
  }

  [[nodiscard]] auto object(const QJsonObject& parent,
                            const QString& path,
                            const char* key,
                            bool required) const -> std::optional<QJsonObject> {
    const QString name = QString::fromLatin1(key);
    const QJsonValue value = parent.value(name);
    if (value.isUndefined() || value.isNull()) {
      if (required) {
        error(child(path, name), QStringLiteral("required object is missing"));
      }
      return std::nullopt;
    }
    if (!value.isObject()) {
      error(child(path, name), QStringLiteral("expected an object"));
      return std::nullopt;
    }
    return value.toObject();
  }

  [[nodiscard]] auto array(const QJsonObject& parent,
                           const QString& path,
                           const char* key,
                           bool required) const -> QJsonArray {
    const QString name = QString::fromLatin1(key);
    const QJsonValue value = parent.value(name);
    if (value.isUndefined() || value.isNull()) {
      if (required) {
        error(child(path, name), QStringLiteral("required array is missing"));
      }
      return {};
    }
    if (!value.isArray()) {
      error(child(path, name), QStringLiteral("expected an array"));
      return {};
    }
    return value.toArray();
  }

  [[nodiscard]] auto raw_point(const QJsonValue& value,
                               const QString& path) const -> std::optional<QVector3D> {
    if (!value.isArray() || value.toArray().size() != 2 ||
        !value.toArray().at(0).isDouble() || !value.toArray().at(1).isDouble()) {
      error(path, QStringLiteral("expected a point [x, z] in metres"));
      return std::nullopt;
    }
    const QJsonArray pair = value.toArray();
    return QVector3D(static_cast<float>(pair.at(0).toDouble()),
                     0.0F,
                     static_cast<float>(pair.at(1).toDouble()));
  }

  [[nodiscard]] auto point(const QJsonObject& object,
                           const QString& path,
                           const char* key,
                           bool required) const -> std::optional<QVector3D> {
    const QString name = QString::fromLatin1(key);
    const QJsonValue value = object.value(name);
    if (value.isUndefined() || value.isNull()) {
      if (required) {
        error(child(path, name), QStringLiteral("required point is missing"));
      }
      return std::nullopt;
    }
    auto result = raw_point(value, child(path, name));
    if (result.has_value()) {
      *result *= geometry;
    }
    return result;
  }

  [[nodiscard]] auto distance(const QJsonObject& object,
                              const QString& path,
                              const char* key,
                              float fallback,
                              float minimum = 0.0F) const -> float {
    const auto value = number(object, path, key, fallback / geometry, minimum);
    return value.value_or(fallback / geometry) * geometry;
  }
};

auto unit_spacing(Game::Units::TroopType troop) -> std::pair<float, float> {
  using Troop = Game::Units::TroopType;
  switch (troop) {
  case Troop::MountedSwordsman:
  case Troop::HorseArcher:
  case Troop::HorseSpearman:
    return {7.5F, 8.0F};
  case Troop::Elephant:
    return {9.0F, 10.0F};
  case Troop::Catapult:
  case Troop::Ballista:
  case Troop::Ram:
  case Troop::SiegeTower:
    return {9.0F, 10.0F};
  case Troop::Archer:
  case Troop::Slinger:
  case Troop::Velites:
    return {6.0F, 6.0F};
  default:
    return {6.0F, 6.0F};
  }
}

auto forward_of(float yaw_degrees) -> QVector3D {
  const float radians = qDegreesToRadians(yaw_degrees);
  return {std::sin(radians), 0.0F, std::cos(radians)};
}

auto right_of(float yaw_degrees) -> QVector3D {
  const QVector3D forward = forward_of(yaw_degrees);
  return {-forward.z(), 0.0F, forward.x()};
}

struct Layout {
  std::vector<QVector2D> local;
  float frontage{0.0F};
  float depth{0.0F};
};

void finish_layout(Layout& layout, float file_spacing, float rank_spacing) {
  if (layout.local.empty()) {
    return;
  }
  float min_x = layout.local.front().x();
  float max_x = min_x;
  float min_y = layout.local.front().y();
  float max_y = min_y;
  for (const auto& p : layout.local) {
    min_x = std::min(min_x, p.x());
    max_x = std::max(max_x, p.x());
    min_y = std::min(min_y, p.y());
    max_y = std::max(max_y, p.y());
  }
  const QVector2D shift((min_x + max_x) * 0.5F, (min_y + max_y) * 0.5F);
  for (auto& p : layout.local) {
    p -= shift;
  }
  layout.frontage = (max_x - min_x) + file_spacing;
  layout.depth = (max_y - min_y) + rank_spacing;
}

auto block_layout(int count,
                  int ranks,
                  float file_spacing,
                  float rank_spacing) -> Layout {
  Layout layout;
  ranks = std::clamp(ranks, 1, std::max(1, count));
  const int files = (count + ranks - 1) / ranks;
  for (int index = 0; index < count; ++index) {
    const int rank = index / files;
    const int file = index % files;
    const int in_rank = std::min(files, count - rank * files);
    const float x =
        (static_cast<float>(file) - static_cast<float>(in_rank - 1) * 0.5F) *
        file_spacing;
    const float y = -static_cast<float>(rank) * rank_spacing;
    layout.local.emplace_back(x, y);
  }
  finish_layout(layout, file_spacing, rank_spacing);
  return layout;
}

auto crescent_layout(int count,
                     int ranks,
                     float file_spacing,
                     float rank_spacing,
                     float bulge) -> Layout {
  Layout layout = block_layout(count, ranks, file_spacing, rank_spacing);
  const float half = std::max(0.5F * (layout.frontage - file_spacing), 0.01F);
  for (auto& p : layout.local) {
    const float t = std::clamp(p.x() / half, -1.0F, 1.0F);
    p.setY(p.y() + bulge * (1.0F - t * t));
  }
  finish_layout(layout, file_spacing, rank_spacing);
  return layout;
}

auto multi_line_layout(int count,
                       const std::vector<float>& split,
                       int ranks_per_line,
                       float line_gap,
                       int lanes,
                       float lane_width,
                       bool stagger,
                       float file_spacing,
                       float rank_spacing) -> Layout {
  Layout layout;
  const int lines = static_cast<int>(split.size());
  float total_weight = 0.0F;
  for (float weight : split) {
    total_weight += std::max(weight, 0.0F);
  }
  std::vector<int> line_counts(static_cast<std::size_t>(lines), 0);
  int assigned = 0;
  for (int line = 0; line < lines; ++line) {
    int share = line == lines - 1
                    ? count - assigned
                    : static_cast<int>(std::lround(
                          static_cast<float>(count) *
                          std::max(split[static_cast<std::size_t>(line)], 0.0F) /
                          std::max(total_weight, 1.0e-4F)));
    share = std::clamp(share, 0, count - assigned);
    line_counts[static_cast<std::size_t>(line)] = share;
    assigned += share;
  }
  const int blocks = std::max(1, lanes + 1);
  ranks_per_line = std::max(1, ranks_per_line);
  int widest_block_files = 1;
  for (int line_count : line_counts) {
    const int files = (line_count + ranks_per_line - 1) / ranks_per_line;
    widest_block_files = std::max(widest_block_files, (files + blocks - 1) / blocks);
  }
  const float block_width = static_cast<float>(widest_block_files - 1) * file_spacing;
  const float pitch = block_width + file_spacing + lane_width;
  const float line_depth = static_cast<float>(ranks_per_line) * rank_spacing;
  for (int line = 0; line < lines; ++line) {
    const int line_count = line_counts[static_cast<std::size_t>(line)];
    if (line_count <= 0) {
      continue;
    }
    const int files = (line_count + ranks_per_line - 1) / ranks_per_line;
    const int files_per_block = (files + blocks - 1) / blocks;
    const float line_y = -static_cast<float>(line) * (line_depth + line_gap);
    const float shift = (stagger && (line % 2 == 1)) ? pitch * 0.5F : 0.0F;
    for (int index = 0; index < line_count; ++index) {
      const int rank = index / files;
      const int file = index % files;
      const int block = std::min(blocks - 1, file / files_per_block);
      const int block_file = file - block * files_per_block;
      const int block_files =
          std::min(files_per_block, files - block * files_per_block);
      const float block_center =
          (static_cast<float>(block) - static_cast<float>(blocks - 1) * 0.5F) * pitch;
      const float x = block_center + shift +
                      (static_cast<float>(block_file) -
                       static_cast<float>(block_files - 1) * 0.5F) *
                          file_spacing;
      const float y = line_y - static_cast<float>(rank) * rank_spacing;
      layout.local.emplace_back(x, y);
    }
  }
  finish_layout(layout, file_spacing, rank_spacing);
  return layout;
}

struct PlannedGroup {
  QString id;
  QString path;
  int army{0};
  Game::Units::TroopType troop{Game::Units::TroopType::Swordsman};
  Game::Systems::NationID nation{Game::Systems::NationID::RomanRepublic};
  int units{0};
  bool commander{false};
  // Set for the historical cameo commanders (#1522), which spawn on a borrowed
  // body and do not take the owner's commander slot.
  QString commander_id;
  bool ambush{false};
  bool hold{false};
  QString formation;
  std::optional<Game::Formation::ArmyFormationIntent> formation_intent;
  QJsonObject deployment;
  QString deployment_path;
  Layout layout;
  float file_spacing{6.0F};
  float rank_spacing{6.0F};
  std::optional<QVector3D> center;
  std::optional<float> facing;
  bool placed{false};
  QString with_group;
  QVector3D with_offset;
};

struct PlannedArmy {
  QString id;
  QString label;
  int owner{0};
  int team{0};
  Game::Systems::NationID nation{Game::Systems::NationID::RomanRepublic};
  float facing{0.0F};
  std::optional<float> historical;
};

auto default_individuals(Game::Units::TroopType troop) -> int {
  return std::max(1,
                  Game::Units::TroopConfig::instance().get_individuals_per_unit(troop));
}

auto troop_names_hint() -> QString {
  return QStringLiteral("swordsman, spearman, archer, slinger, velites, "
                        "horse_swordsman, horse_spearman, horse_archer, elephant, "
                        "healer, catapult, ballista");
}

auto parse_nation(const Context& context,
                  const QJsonObject& object,
                  const QString& path,
                  std::optional<Game::Systems::NationID> fallback)
    -> std::optional<Game::Systems::NationID> {
  const QString value = context.string(object, path, "nation", !fallback.has_value());
  if (value.isEmpty()) {
    return fallback;
  }
  Game::Systems::NationID parsed{};
  if (!Game::Systems::try_parse_nation_id(value, parsed)) {
    context.error(child(path, QStringLiteral("nation")),
                  QStringLiteral("unknown nation '%1' (expected roman_republic, "
                                 "carthage, gauls, iberians or iron_sepulcher)")
                      .arg(value));
    return fallback;
  }
  return parsed;
}

void compute_layout(const Context& context, PlannedGroup& group) {
  const auto& deployment = group.deployment;
  const QString& path = group.deployment_path;
  const auto spacing = unit_spacing(group.troop);
  group.file_spacing =
      context.number(deployment, path, "file_spacing", spacing.first, 0.5F, 100.0F)
          .value_or(spacing.first);
  group.rank_spacing =
      context.number(deployment, path, "rank_spacing", spacing.second, 0.5F, 100.0F)
          .value_or(spacing.second);
  QString shape = context.string(deployment, path, "shape", false);
  if (shape.isEmpty()) {
    shape = QStringLiteral("line");
  }
  const int ranks = static_cast<int>(
      context.number(deployment, path, "ranks", 1.0F, 1.0F, 500.0F).value_or(1.0F));
  if (shape == QStringLiteral("line") || shape == QStringLiteral("block")) {
    group.layout =
        block_layout(group.units, ranks, group.file_spacing, group.rank_spacing);
  } else if (shape == QStringLiteral("column")) {
    const int files = static_cast<int>(
        context.number(deployment, path, "files", 4.0F, 1.0F, 500.0F).value_or(4.0F));
    group.layout =
        block_layout(group.units,
                     (group.units + std::max(1, files) - 1) / std::max(1, files),
                     group.file_spacing,
                     group.rank_spacing);
  } else if (shape == QStringLiteral("crescent")) {
    const float bulge = context.distance(deployment, path, "bulge", 20.0F, -1.0e6F);
    group.layout = crescent_layout(
        group.units, ranks, group.file_spacing, group.rank_spacing, bulge);
  } else if (shape == QStringLiteral("multi_line")) {
    std::vector<float> split;
    const QJsonArray split_array = context.array(deployment, path, "split", false);
    for (qsizetype i = 0; i < split_array.size(); ++i) {
      if (!split_array.at(i).isDouble() || split_array.at(i).toDouble() <= 0.0) {
        context.error(index_path(child(path, QStringLiteral("split")), i),
                      QStringLiteral("expected a positive share"));
        continue;
      }
      split.push_back(static_cast<float>(split_array.at(i).toDouble()));
    }
    const int lines = static_cast<int>(
        context.number(deployment, path, "lines", 3.0F, 1.0F, 12.0F).value_or(3.0F));
    if (split.empty()) {
      split.assign(static_cast<std::size_t>(lines), 1.0F);
    } else if (deployment.contains(QStringLiteral("lines")) &&
               static_cast<int>(split.size()) != lines) {
      context.error(
          child(path, QStringLiteral("split")),
          QStringLiteral("%1 shares for %2 lines").arg(split.size()).arg(lines));
    }
    const int lanes = static_cast<int>(
        context.number(deployment, path, "lanes", 0.0F, 0.0F, 200.0F).value_or(0.0F));
    const float lane_width =
        context.distance(deployment, path, "lane_width", lanes > 0 ? 8.0F : 0.0F);
    const float line_gap = context.distance(deployment, path, "line_gap", 12.0F);
    const bool stagger = context.boolean(deployment, path, "stagger", false);
    group.layout = multi_line_layout(group.units,
                                     split,
                                     ranks,
                                     line_gap,
                                     lanes,
                                     lane_width,
                                     stagger,
                                     group.file_spacing,
                                     group.rank_spacing);
  } else {
    context.error(child(path, QStringLiteral("shape")),
                  QStringLiteral("unknown deployment shape '%1' (expected line, "
                                 "block, column, crescent or multi_line)")
                      .arg(shape));
    group.layout = block_layout(group.units, 1, group.file_spacing, group.rank_spacing);
  }
}

auto anchor_kinds() -> std::array<const char*, 4> {
  return {"behind", "ahead_of", "left_of", "right_of"};
}

auto try_place(const Context& context,
               PlannedGroup& group,
               const std::map<QString, PlannedGroup*>& by_id,
               const PlannedArmy& army,
               bool report) -> bool {
  const auto& deployment = group.deployment;
  const QString& path = group.deployment_path;
  if (const auto center = context.point(deployment, path, "center", false);
      center.has_value()) {
    group.center = *center;
    group.facing =
        context.number(deployment, path, "facing", army.facing, -720.0F, 720.0F)
            .value_or(army.facing);
    return true;
  }
  const auto anchor = context.object(deployment, path, "anchor", false);
  if (!anchor.has_value()) {
    if (report) {
      context.error(path,
                    QStringLiteral("a deployment needs a 'center' or an 'anchor'"));
    }
    return false;
  }
  const QString anchor_path = child(path, QStringLiteral("anchor"));
  QString kind;
  QString reference;
  for (const char* candidate : anchor_kinds()) {
    const QString value = anchor->value(QString::fromLatin1(candidate)).toString();
    if (!value.isEmpty()) {
      if (!kind.isEmpty() && report) {
        context.error(anchor_path, QStringLiteral("an anchor names one relation"));
      }
      kind = QString::fromLatin1(candidate);
      reference = value;
    }
  }
  if (kind.isEmpty()) {
    if (report) {
      context.error(anchor_path,
                    QStringLiteral("expected one of behind, ahead_of, left_of, "
                                   "right_of"));
    }
    return false;
  }
  const auto found = by_id.find(reference);
  if (found == by_id.end()) {
    if (report) {
      context.error(child(anchor_path, kind),
                    QStringLiteral("unknown group '%1'").arg(reference));
    }
    return false;
  }
  const PlannedGroup& other = *found->second;
  if (!other.placed) {
    return false;
  }
  const float gap = context.distance(*anchor, anchor_path, "gap", 10.0F, -1.0e6F);
  const float reference_facing = other.facing.value_or(army.facing);
  const QVector3D forward = forward_of(reference_facing);
  const QVector3D right = right_of(reference_facing);
  QVector3D center = *other.center;
  if (kind == QStringLiteral("behind")) {
    center -= forward * (other.layout.depth * 0.5F + gap + group.layout.depth * 0.5F);
  } else if (kind == QStringLiteral("ahead_of")) {
    center += forward * (other.layout.depth * 0.5F + gap + group.layout.depth * 0.5F);
  } else if (kind == QStringLiteral("left_of")) {
    center -=
        right * (other.layout.frontage * 0.5F + gap + group.layout.frontage * 0.5F);
  } else {
    center +=
        right * (other.layout.frontage * 0.5F + gap + group.layout.frontage * 0.5F);
  }
  if (const auto shift = context.point(*anchor, anchor_path, "offset", false);
      shift.has_value()) {
    center += right * shift->x() + forward * shift->z();
  }
  group.center = center;
  group.facing =
      context.number(deployment, path, "facing", reference_facing, -720.0F, 720.0F)
          .value_or(reference_facing);
  context.check_keys(*anchor,
                     anchor_path,
                     {"behind", "ahead_of", "left_of", "right_of", "gap", "offset"});
  return true;
}

void place_positions(const PlannedGroup& group, ArenaScenarioGroup& out) {
  const float facing = group.facing.value_or(0.0F);
  const QVector3D forward = forward_of(facing);
  const QVector3D right = right_of(facing);
  out.positions.clear();
  out.positions.reserve(group.layout.local.size());
  for (const auto& local : group.layout.local) {
    out.positions.push_back(*group.center + right * local.x() + forward * local.y());
  }
  out.count = static_cast<int>(out.positions.size());
  out.origin = *group.center;
  out.facing_degrees = facing;
}

auto make_step(QString name,
               ScenarioTrigger trigger,
               ScenarioCommandKind command,
               QString group = {}) -> ArenaScenarioStep {
  ArenaScenarioStep step;
  step.name = std::move(name);
  step.trigger = std::move(trigger);
  step.command = command;
  step.group = std::move(group);
  return step;
}

void read_fog_bank_values(const Context& context,
                          const QJsonObject& object,
                          const QString& path,
                          ArenaFogBankChange& out) {
  out.density = context.optional_number(object, path, "density", 0.0F, 1.0F);
  if (object.contains(QStringLiteral("radius"))) {
    out.radius = context.distance(object, path, "radius", 1.0F, 0.5F);
  }
  if (object.contains(QStringLiteral("height"))) {
    out.ceiling = context.distance(object, path, "height", 1.0F, 0.0F);
  }
  out.start = context.point(object, path, "center", false);
  if (!out.start.has_value()) {
    out.start = context.point(object, path, "from", false);
  }
  out.end = context.point(object, path, "to", false);
}

auto read_weather_change(const Context& context,
                         const QJsonObject& object,
                         const QString& path,
                         const QSet<QString>& bank_ids,
                         bool timed) -> ArenaWeatherChange {
  ArenaWeatherChange change;
  if (timed) {
    context.check_keys(object,
                       path,
                       {"at",
                        "name",
                        "duration",
                        "fog_density",
                        "exposure",
                        "rain",
                        "storm",
                        "snow",
                        "wind_strength",
                        "wind_direction",
                        "hour",
                        "fog_banks"});
  } else {
    context.check_keys(object,
                       path,
                       {"type",
                        "delay",
                        "name",
                        "duration",
                        "fog_density",
                        "exposure",
                        "rain",
                        "storm",
                        "snow",
                        "wind_strength",
                        "wind_direction",
                        "hour",
                        "fog_banks"});
  }
  change.name = context.string(object, path, "name", false);
  change.duration_seconds =
      context.number(object, path, "duration", 0.0F, 0.0F, 36000.0F).value_or(0.0F);
  change.fog_density = context.optional_number(object, path, "fog_density", 0.0F, 1.0F);
  change.exposure = context.optional_number(object, path, "exposure", 0.05F, 4.0F);
  change.rain = context.optional_number(object, path, "rain", 0.0F, 1.0F);
  change.storm = context.optional_number(object, path, "storm", 0.0F, 1.0F);
  change.snow = context.optional_number(object, path, "snow", 0.0F, 1.0F);
  change.wind_strength =
      context.optional_number(object, path, "wind_strength", 0.0F, 1.0F);
  change.wind_direction_deg =
      context.optional_number(object, path, "wind_direction", -360.0F, 360.0F);
  change.hour = context.optional_number(object, path, "hour", 0.0F, 24.0F);
  const QString banks_path = child(path, QStringLiteral("fog_banks"));
  const QJsonArray banks = context.array(object, path, "fog_banks", false);
  for (qsizetype i = 0; i < banks.size(); ++i) {
    const QString bank_path = index_path(banks_path, i);
    if (!banks.at(i).isObject()) {
      context.error(bank_path, QStringLiteral("expected an object"));
      continue;
    }
    const QJsonObject bank = banks.at(i).toObject();
    context.check_keys(
        bank, bank_path, {"id", "density", "radius", "height", "center", "from", "to"});
    ArenaFogBankChange target;
    target.id = context.string(bank, bank_path, "id", true);
    if (!target.id.isEmpty() && !bank_ids.contains(target.id)) {
      context.error(child(bank_path, QStringLiteral("id")),
                    QStringLiteral("unknown fog bank '%1'").arg(target.id));
    }
    read_fog_bank_values(context, bank, bank_path, target);
    change.fog_banks.push_back(std::move(target));
  }
  return change;
}

void compile_terrain(const Context& context,
                     const QJsonObject& root,
                     const LoadOptions& options,
                     ArenaScenarioDefinition& scenario,
                     std::vector<FordSegment>& fords) {
  const auto terrain = context.object(root, QStringLiteral("$"), "terrain", false);
  const QString path = QStringLiteral("$.terrain");
  scenario.terrain_grid_extent = 256;
  scenario.arena_floor_half_extent = 100.0F;
  scenario.terrain_height_scale_override = 0.35F;
  if (!terrain.has_value()) {
    return;
  }
  const auto& object = *terrain;
  context.check_keys(object,
                     path,
                     {"map",
                      "grid_extent",
                      "flat_extent",
                      "ground_type",
                      "height_scale",
                      "seed",
                      "snow",
                      "boundary_mountains",
                      "scatter",
                      "props",
                      "rivers",
                      "lakes",
                      "hills",
                      "bridges",
                      "fords"});
  const QString map = context.string(object, path, "map", false);
  if (!map.isEmpty()) {
    if (object.size() > 1) {
      for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.key() != QStringLiteral("map") && it.key() != QStringLiteral("fords") &&
            !k_ignored_keys.contains(it.key())) {
          context.error(child(path, it.key()),
                        QStringLiteral("a battle on an existing map takes its terrain "
                                       "from the map; drop this field"));
        }
      }
    }
    QString resolved = map;
    const QFileInfo authored(map);
    if (authored.isRelative() && !options.base_directory.isEmpty() &&
        !map.startsWith(QStringLiteral("assets/")) &&
        QFileInfo::exists(QDir(options.base_directory).filePath(map))) {
      resolved = QDir(options.base_directory).filePath(map);
    }
    Game::Map::MapDefinition definition;
    QString error;
    if (!Game::Map::MapLoader::load_from_json_file(
            Utils::Resources::resolve_resource_path(resolved), definition, &error)) {
      context.error(child(path, QStringLiteral("map")),
                    QStringLiteral("cannot load map '%1': %2").arg(map, error));
      return;
    }
    if (context.geometry != 1.0F) {
      context.warn(child(path, QStringLiteral("map")),
                   QStringLiteral("a scale override cannot shrink an existing map; "
                                  "deployment geometry is scaled, the map is not"));
    }
    scenario.campaign_map_path = resolved;
    scenario.terrain_grid_extent = definition.grid.width;
    scenario.arena_floor_half_extent = static_cast<float>(definition.grid.width) * 0.5F;
    scenario.terrain_height_scale_override = 0.0F;
    scenario.environment = definition.environment;
    scenario.environment.time_mode = Game::Map::TimeMode::Locked;
  } else {
    const float extent =
        context.number(object, path, "grid_extent", 256.0F, 64.0F, 1024.0F)
            .value_or(256.0F);
    scenario.terrain_grid_extent =
        static_cast<int>(std::lround(extent * std::max(context.geometry, 0.25F)));
    scenario.terrain_grid_extent = std::clamp(scenario.terrain_grid_extent, 64, 1024);
    scenario.arena_floor_half_extent = std::min(
        context.distance(object,
                         path,
                         "flat_extent",
                         static_cast<float>(scenario.terrain_grid_extent) * 0.4F),
        static_cast<float>(scenario.terrain_grid_extent) * 0.5F - 4.0F);
    scenario.terrain_height_scale_override =
        context.number(object, path, "height_scale", 0.35F, 0.01F, 20.0F)
            .value_or(0.35F);
    const QString ground = context.string(object, path, "ground_type", false);
    if (!ground.isEmpty()) {
      Game::Map::GroundType parsed{};
      if (!Game::Map::try_parse_ground_type(ground, parsed)) {
        context.error(child(path, QStringLiteral("ground_type")),
                      QStringLiteral("unknown ground type '%1' (see "
                                     "assets/maps/GROUND_TYPES.md)")
                          .arg(ground));
      } else {
        scenario.ground_type = ground;
      }
    }
    if (const auto seed = context.optional_number(object, path, "seed", 1.0F, 2.0e9F)) {
      scenario.terrain_seed_override = static_cast<int>(*seed);
    }
    scenario.terrain_snowbound = context.boolean(object, path, "snow", false);
    scenario.suppress_boundary_mountains =
        !context.boolean(object, path, "boundary_mountains", true);
    scenario.suppress_terrain_scatter = !context.boolean(object, path, "scatter", true);
    scenario.suppress_procedural_props = !context.boolean(object, path, "props", true);

    const QJsonArray rivers = context.array(object, path, "rivers", false);
    for (qsizetype r = 0; r < rivers.size(); ++r) {
      const QString river_path = index_path(child(path, QStringLiteral("rivers")), r);
      if (!rivers.at(r).isObject()) {
        context.error(river_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject river = rivers.at(r).toObject();
      context.check_keys(river, river_path, {"points", "width"});
      const float width = context.distance(river, river_path, "width", 8.0F, 0.5F);
      const QJsonArray points = context.array(river, river_path, "points", true);
      if (points.size() < 2) {
        context.error(child(river_path, QStringLiteral("points")),
                      QStringLiteral("a river needs at least two points"));
        continue;
      }
      std::optional<QVector3D> previous;
      for (qsizetype p = 0; p < points.size(); ++p) {
        auto point = context.raw_point(
            points.at(p), index_path(child(river_path, QStringLiteral("points")), p));
        if (!point.has_value()) {
          continue;
        }
        *point *= context.geometry;
        if (previous.has_value()) {
          Game::Map::RiverSegment segment;
          segment.start = *previous;
          segment.end = *point;
          segment.width = width;
          scenario.rivers.push_back(segment);
        }
        previous = point;
      }
    }
    const QJsonArray lakes = context.array(object, path, "lakes", false);
    for (qsizetype l = 0; l < lakes.size(); ++l) {
      const QString lake_path = index_path(child(path, QStringLiteral("lakes")), l);
      if (!lakes.at(l).isObject()) {
        context.error(lake_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject lake_object = lakes.at(l).toObject();
      context.check_keys(
          lake_object, lake_path, {"center", "width", "depth", "rotation"});
      Game::Map::Lake lake;
      lake.center =
          context.point(lake_object, lake_path, "center", true).value_or(QVector3D());
      lake.width = context.distance(lake_object, lake_path, "width", 40.0F, 1.0F);
      lake.depth = context.distance(lake_object, lake_path, "depth", 40.0F, 1.0F);
      lake.rotation_deg =
          context.number(lake_object, lake_path, "rotation", 0.0F, -360.0F, 360.0F)
              .value_or(0.0F);
      scenario.lakes.push_back(lake);
    }
    const QJsonArray hills = context.array(object, path, "hills", false);
    for (qsizetype h = 0; h < hills.size(); ++h) {
      const QString hill_path = index_path(child(path, QStringLiteral("hills")), h);
      if (!hills.at(h).isObject()) {
        context.error(hill_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject hill = hills.at(h).toObject();
      context.check_keys(hill, hill_path, {"center", "radius", "height", "plateau"});
      ArenaScenarioElevationPatch patch;
      patch.center =
          context.point(hill, hill_path, "center", true).value_or(QVector3D());
      patch.radius = context.distance(hill, hill_path, "radius", 30.0F, 1.0F);
      patch.height =
          context.number(hill, hill_path, "height", 4.0F, 0.0F, 200.0F).value_or(4.0F);
      patch.plateau = context.distance(hill, hill_path, "plateau", 0.0F, 0.0F);
      scenario.elevation_patches.push_back(patch);
    }
    const QJsonArray bridges = context.array(object, path, "bridges", false);
    for (qsizetype b = 0; b < bridges.size(); ++b) {
      const QString bridge_path = index_path(child(path, QStringLiteral("bridges")), b);
      if (!bridges.at(b).isObject()) {
        context.error(bridge_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject bridge_object = bridges.at(b).toObject();
      context.check_keys(bridge_object, bridge_path, {"from", "to", "width"});
      Game::Map::Bridge bridge;
      bridge.start =
          context.point(bridge_object, bridge_path, "from", true).value_or(QVector3D());
      bridge.end =
          context.point(bridge_object, bridge_path, "to", true).value_or(QVector3D());
      bridge.width = context.distance(bridge_object, bridge_path, "width", 4.0F, 1.0F);
      scenario.bridges.push_back(bridge);
    }
  }
  const QJsonArray ford_array = context.array(object, path, "fords", false);
  for (qsizetype f = 0; f < ford_array.size(); ++f) {
    const QString ford_path = index_path(child(path, QStringLiteral("fords")), f);
    if (!ford_array.at(f).isObject()) {
      context.error(ford_path, QStringLiteral("expected an object"));
      continue;
    }
    const QJsonObject ford = ford_array.at(f).toObject();
    context.check_keys(
        ford,
        ford_path,
        {"id", "river", "at", "width", "depth", "speed", "cold", "exposure"});
    FordSegment segment;
    segment.id = context.string(ford, ford_path, "id", false);
    if (segment.id.isEmpty()) {
      segment.id = QStringLiteral("ford_%1").arg(f + 1);
    }
    segment.river = static_cast<int>(
        context.number(ford, ford_path, "river", 0.0F, 0.0F, 1000.0F).value_or(0.0F));
    segment.at = context.point(ford, ford_path, "at", true).value_or(QVector3D());
    segment.width = context
                        .number(ford,
                                ford_path,
                                "width",
                                Game::Map::k_default_ford_length,
                                Game::Map::k_min_ford_length,
                                Game::Map::k_max_ford_length)
                        .value_or(Game::Map::k_default_ford_length);
    auto& profile = segment.profile;
    profile.depth = context
                        .number(ford,
                                ford_path,
                                "depth",
                                profile.depth,
                                Game::Map::k_min_ford_depth,
                                Game::Map::k_max_ford_depth)
                        .value_or(profile.depth);
    profile.speed = context
                        .number(ford,
                                ford_path,
                                "speed",
                                profile.speed,
                                Game::Map::k_min_ford_speed,
                                Game::Map::k_max_ford_speed)
                        .value_or(profile.speed);
    profile.cold = context.number(ford, ford_path, "cold", profile.cold, 0.0F, 1.0F)
                       .value_or(profile.cold);
    profile.exposure = context
                           .number(ford,
                                   ford_path,
                                   "exposure",
                                   profile.exposure,
                                   Game::Map::k_min_ford_exposure,
                                   Game::Map::k_max_ford_exposure)
                           .value_or(profile.exposure);
    fords.push_back(segment);
  }
}

void compile_environment(const Context& context,
                         const QJsonObject& root,
                         ArenaScenarioDefinition& scenario) {
  const QString path = QStringLiteral("$.environment");
  const auto environment =
      context.object(root, QStringLiteral("$"), "environment", false);
  if (!environment.has_value()) {
    return;
  }
  const auto& object = *environment;
  context.check_keys(object,
                     path,
                     {"hour",
                      "lighting_profile",
                      "time_mode",
                      "day_length",
                      "fog_density",
                      "exposure"});
  scenario.environment.start_time =
      context.number(object, path, "hour", scenario.environment.start_time, 0.0F, 24.0F)
          .value_or(scenario.environment.start_time);
  const QString profile = context.string(object, path, "lighting_profile", false);
  if (!profile.isEmpty()) {
    if (!k_lighting_profiles.contains(profile)) {
      context.error(child(path, QStringLiteral("lighting_profile")),
                    QStringLiteral("unknown lighting profile '%1' (one of %2)")
                        .arg(profile, k_lighting_profiles.join(QStringLiteral(", "))));
    } else {
      scenario.environment.lighting_profile = profile;
    }
  }
  const QString mode = context.string(object, path, "time_mode", false);
  if (!mode.isEmpty()) {
    if (mode != QStringLiteral("locked") && mode != QStringLiteral("continuous")) {
      context.error(child(path, QStringLiteral("time_mode")),
                    QStringLiteral("expected locked or continuous"));
    } else {
      scenario.environment.time_mode = Game::Map::parse_time_mode(mode);
    }
  }
  scenario.environment.day_length_seconds =
      context
          .number(object,
                  path,
                  "day_length",
                  scenario.environment.day_length_seconds,
                  10.0F,
                  1.0e6F)
          .value_or(scenario.environment.day_length_seconds);
  if (const auto fog =
          context.optional_number(object, path, "fog_density", 0.0F, 1.0F)) {
    scenario.environment.fog_density_override = *fog;
  }
  if (const auto exposure =
          context.optional_number(object, path, "exposure", 0.05F, 4.0F)) {
    scenario.environment.exposure_override = *exposure;
  }
}

void compile_weather(const Context& context,
                     const QJsonObject& root,
                     ArenaScenarioDefinition& scenario,
                     QSet<QString>& bank_ids) {
  const QString path = QStringLiteral("$.weather");
  const auto weather = context.object(root, QStringLiteral("$"), "weather", false);
  if (!weather.has_value()) {
    return;
  }
  const auto& object = *weather;
  context.check_keys(object,
                     path,
                     {"rain",
                      "storm",
                      "snow",
                      "wind_strength",
                      "wind_direction",
                      "water_mist",
                      "fog_banks",
                      "changes"});
  scenario.weather.rain =
      context.number(object, path, "rain", 0.0F, 0.0F, 1.0F).value_or(0.0F);
  scenario.weather.storm =
      context.number(object, path, "storm", 0.0F, 0.0F, 1.0F).value_or(0.0F);
  scenario.weather.snow =
      context.number(object, path, "snow", 0.0F, 0.0F, 1.0F).value_or(0.0F);
  scenario.precipitation.wind_strength =
      context.number(object, path, "wind_strength", 0.0F, 0.0F, 1.0F).value_or(0.0F);
  scenario.precipitation.wind_direction_deg =
      context.number(object, path, "wind_direction", 45.0F, -360.0F, 360.0F)
          .value_or(45.0F);
  scenario.weather_script.water_mist =
      context.boolean(object, path, "water_mist", true);

  const QString banks_path = child(path, QStringLiteral("fog_banks"));
  const QJsonArray banks = context.array(object, path, "fog_banks", false);
  for (qsizetype i = 0; i < banks.size(); ++i) {
    const QString bank_path = index_path(banks_path, i);
    if (!banks.at(i).isObject()) {
      context.error(bank_path, QStringLiteral("expected an object"));
      continue;
    }
    const QJsonObject bank_object = banks.at(i).toObject();
    context.check_keys(
        bank_object,
        bank_path,
        {"id", "center", "from", "to", "radius", "height", "density", "keys"});
    ArenaFogBank bank;
    bank.id = context.string(bank_object, bank_path, "id", true);
    if (bank_ids.contains(bank.id)) {
      context.error(child(bank_path, QStringLiteral("id")),
                    QStringLiteral("duplicate fog bank '%1'").arg(bank.id));
    }
    bank_ids.insert(bank.id);
    auto start = context.point(bank_object, bank_path, "center", false);
    if (!start.has_value()) {
      start = context.point(bank_object, bank_path, "from", false);
    }
    if (!start.has_value()) {
      context.error(bank_path, QStringLiteral("a fog bank needs a 'center' or 'from'"));
      start = QVector3D();
    }
    bank.start = *start;
    bank.end = context.point(bank_object, bank_path, "to", false).value_or(bank.start);
    bank.radius = context.distance(bank_object, bank_path, "radius", 30.0F, 0.5F);
    bank.ceiling = context.distance(bank_object, bank_path, "height", 4.0F, 0.0F);
    bank.density = context.number(bank_object, bank_path, "density", 0.6F, 0.0F, 1.0F)
                       .value_or(0.6F);
    const QString keys_path = child(bank_path, QStringLiteral("keys"));
    const QJsonArray keys = context.array(bank_object, bank_path, "keys", false);
    float previous_time = -1.0F;
    for (qsizetype k = 0; k < keys.size(); ++k) {
      const QString key_path = index_path(keys_path, k);
      if (!keys.at(k).isObject()) {
        context.error(key_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject key_object = keys.at(k).toObject();
      context.check_keys(key_object,
                         key_path,
                         {"at", "density", "radius", "height", "center", "from", "to"});
      ArenaFogBankKey key;
      key.time_seconds =
          context.number(key_object, key_path, "at", std::nullopt, 0.0F, 1.0e6F)
              .value_or(0.0F);
      if (key.time_seconds < previous_time) {
        context.error(child(key_path, QStringLiteral("at")),
                      QStringLiteral("fog bank keys must be in time order"));
      }
      previous_time = key.time_seconds;
      ArenaFogBankChange values;
      read_fog_bank_values(context, key_object, key_path, values);
      key.density = values.density;
      key.radius = values.radius;
      key.ceiling = values.ceiling;
      key.start = values.start;
      key.end = values.end;
      bank.keys.push_back(key);
    }
    scenario.weather_script.fog_banks.push_back(std::move(bank));
  }

  const QString changes_path = child(path, QStringLiteral("changes"));
  const QJsonArray changes = context.array(object, path, "changes", false);
  for (qsizetype i = 0; i < changes.size(); ++i) {
    const QString change_path = index_path(changes_path, i);
    if (!changes.at(i).isObject()) {
      context.error(change_path, QStringLiteral("expected an object"));
      continue;
    }
    const QJsonObject change_object = changes.at(i).toObject();
    const float at =
        context.number(change_object, change_path, "at", std::nullopt, 0.0F, 1.0e6F)
            .value_or(0.0F);
    scenario.weather_script.changes.push_back(
        read_weather_change(context, change_object, change_path, bank_ids, true));
    scenario.weather_script.timeline.push_back(
        {at, static_cast<int>(scenario.weather_script.changes.size() - 1U)});
  }
}

auto group_list(const Context& context,
                const QJsonObject& object,
                const QString& path,
                const QSet<QString>& known) -> QStringList {
  QStringList groups;
  const QJsonValue single = object.value(QStringLiteral("group"));
  const QJsonValue many = object.value(QStringLiteral("groups"));
  if (!single.isUndefined() && !many.isUndefined()) {
    context.error(path, QStringLiteral("use 'group' or 'groups', not both"));
  }
  if (single.isString()) {
    groups.push_back(single.toString().trimmed());
  } else if (!single.isUndefined()) {
    context.error(child(path, QStringLiteral("group")),
                  QStringLiteral("expected a string"));
  }
  if (many.isArray()) {
    const QJsonArray array = many.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
      if (!array.at(i).isString()) {
        context.error(index_path(child(path, QStringLiteral("groups")), i),
                      QStringLiteral("expected a group id"));
        continue;
      }
      groups.push_back(array.at(i).toString().trimmed());
    }
  } else if (!many.isUndefined()) {
    context.error(child(path, QStringLiteral("groups")),
                  QStringLiteral("expected an array"));
  }
  for (qsizetype i = 0; i < groups.size(); ++i) {
    if (!known.contains(groups.at(i))) {
      context.error(many.isArray()
                        ? index_path(child(path, QStringLiteral("groups")), i)
                        : child(path, QStringLiteral("group")),
                    QStringLiteral("unknown group '%1'").arg(groups.at(i)));
    }
  }
  return groups;
}

auto checked_group(const Context& context,
                   const QJsonObject& object,
                   const QString& path,
                   const char* key,
                   const QSet<QString>& known,
                   bool required) -> QString {
  const QString value = context.string(object, path, key, required);
  if (!value.isEmpty() && !known.contains(value)) {
    context.error(child(path, QString::fromLatin1(key)),
                  QStringLiteral("unknown group '%1'").arg(value));
  }
  return value;
}

} // namespace

auto phase_event_name(const QString& event) -> QString {
  return QString::fromLatin1(k_phase_event_prefix) + event;
}

auto known_formation_names() -> QStringList {
  QStringList names;
  for (const auto intent : Game::Formation::all_intents()) {
    names.push_back(QString::fromLatin1(Game::Formation::intent_to_string(intent)));
  }
  return names;
}

auto resolve_formation(const QString& name)
    -> std::optional<Game::Formation::ArmyFormationIntent> {
  return Game::Formation::try_parse_intent(name);
}

auto known_commander_ids() -> QStringList {
  QStringList ids;
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    ids.push_back(QString::fromStdString(definition.id));
  }
  for (const auto& definition : Game::Units::historical_commander_definitions()) {
    ids.push_back(QString::fromStdString(definition.id));
  }
  return ids;
}

auto resolve_commander(const QString& catalog_id)
    -> const Game::Units::CommanderDefinition* {
  return Game::Units::find_commander_definition(catalog_id.trimmed().toStdString());
}

auto apply_fords(const std::vector<FordSegment>& fords,
                 ArenaScenarioDefinition& scenario) -> QString {
  for (const auto& segment : fords) {
    Game::Map::FordCrossing crossing;
    crossing.id = segment.id;
    crossing.position = segment.at;
    crossing.length = segment.width;
    crossing.profile = segment.profile.clamped();
    scenario.fords.push_back(crossing);
  }
  if (!fords.empty() && scenario.rivers.empty() &&
      scenario.campaign_map_path.isEmpty()) {
    return QStringLiteral("%1 ford(s) declared but the terrain has no river to cross")
        .arg(fords.size());
  }
  return {};
}

auto compile(const QJsonObject& root, const LoadOptions& options) -> CompileResult {
  CompileResult result;
  Context context;
  context.errors = &result.errors;
  context.warnings = &result.warnings;
  const QString path = QStringLiteral("$");

  context.check_keys(root,
                     path,
                     {"schema",
                      "id",
                      "title",
                      "description",
                      "scale",
                      "duration",
                      "graphics_quality",
                      "camera",
                      "terrain",
                      "environment",
                      "weather",
                      "armies",
                      "phases"});
  const QString schema = context.string(root, path, "schema", true);
  if (!schema.isEmpty() && schema != QLatin1String(k_schema)) {
    context.error(
        child(path, QStringLiteral("schema")),
        QStringLiteral("expected '%1', got '%2'").arg(QLatin1String(k_schema), schema));
  }
  ArenaScenarioDefinition scenario;
  scenario.id = context.string(root, path, "id", true);
  static const QRegularExpression k_id_pattern(QStringLiteral("^[a-z0-9_]+$"));
  if (!scenario.id.isEmpty() && !k_id_pattern.match(scenario.id).hasMatch()) {
    context.error(child(path, QStringLiteral("id")),
                  QStringLiteral("use lower-case letters, digits and underscores"));
  }
  scenario.label = context.string(root, path, "title", false);
  if (scenario.label.isEmpty()) {
    scenario.label = scenario.id;
  }
  scenario.description = context.string(root, path, "description", false);
  result.authored_scale =
      context.number(root, path, "scale", k_default_scale, 0.0001F, 1.0F)
          .value_or(k_default_scale);
  result.scale = result.authored_scale;
  if (options.scale_override.has_value()) {
    if (*options.scale_override <= 0.0F || *options.scale_override > 1.0F) {
      context.error(QStringLiteral("--battle-script-scale"),
                    QStringLiteral("scale must be in (0, 1]"));
    } else {
      result.scale = *options.scale_override;
      result.geometry_factor = std::sqrt(result.scale / result.authored_scale);
    }
  }
  context.geometry = result.geometry_factor;
  scenario.duration_seconds =
      context.number(root, path, "duration", 240.0F, 1.0F, 36000.0F).value_or(240.0F);
  const QString quality = context.string(root, path, "graphics_quality", false);
  scenario.graphics_quality = Render::GraphicsQuality::High;
  if (quality == QStringLiteral("ultra")) {
    scenario.graphics_quality = Render::GraphicsQuality::Ultra;
  } else if (quality == QStringLiteral("medium")) {
    scenario.graphics_quality = Render::GraphicsQuality::Medium;
  } else if (quality == QStringLiteral("low")) {
    scenario.graphics_quality = Render::GraphicsQuality::Low;
  } else if (!quality.isEmpty() && quality != QStringLiteral("high")) {
    context.error(child(path, QStringLiteral("graphics_quality")),
                  QStringLiteral("expected low, medium, high or ultra"));
  }
  scenario.select_spawned_units = false;
  scenario.suppress_spawn_anchor = true;
  scenario.suppress_ui_overlays = true;
  scenario.force_full_creature_lod = false;
  scenario.collect_animation_diagnostics = false;
  scenario.environment.lighting_profile = QStringLiteral("mediterranean_summer");
  scenario.environment.start_time = 9.0F;

  std::vector<FordSegment> fords;
  compile_terrain(context, root, options, scenario, fords);
  compile_environment(context, root, scenario);
  QSet<QString> bank_ids;
  compile_weather(context, root, scenario, bank_ids);
  if (const QString ford_note = apply_fords(fords, scenario); !ford_note.isEmpty()) {
    context.warn(QStringLiteral("$.terrain.fords"), ford_note);
  }

  const auto ipu =
      options.individuals_per_unit
          ? options.individuals_per_unit
          : std::function<int(Game::Units::TroopType)>(default_individuals);

  std::vector<PlannedArmy> armies;
  std::vector<PlannedGroup> groups;
  QSet<QString> army_ids;
  QSet<QString> group_ids;
  QSet<int> used_owners;
  const QString armies_path = child(path, QStringLiteral("armies"));
  const QJsonArray army_array = context.array(root, path, "armies", true);
  if (army_array.size() < 2 && root.contains(QStringLiteral("armies"))) {
    context.error(armies_path, QStringLiteral("a battle needs at least two armies"));
  }
  for (qsizetype a = 0; a < army_array.size(); ++a) {
    const QString army_path = index_path(armies_path, a);
    if (!army_array.at(a).isObject()) {
      context.error(army_path, QStringLiteral("expected an object"));
      continue;
    }
    const QJsonObject army_object = army_array.at(a).toObject();
    context.check_keys(army_object,
                       army_path,
                       {"id",
                        "label",
                        "owner",
                        "team",
                        "nation",
                        "facing",
                        "historical_strength",
                        "commanders",
                        "groups"});
    PlannedArmy army;
    army.id = context.string(army_object, army_path, "id", true);
    if (army_ids.contains(army.id)) {
      context.error(child(army_path, QStringLiteral("id")),
                    QStringLiteral("duplicate army '%1'").arg(army.id));
    }
    army_ids.insert(army.id);
    army.label = context.string(army_object, army_path, "label", false);
    if (army.label.isEmpty()) {
      army.label = army.id;
    }
    army.owner =
        static_cast<int>(context
                             .number(army_object,
                                     army_path,
                                     "owner",
                                     static_cast<float>(k_first_owner_id + a),
                                     1.0F,
                                     64.0F)
                             .value_or(static_cast<float>(k_first_owner_id + a)));
    if (used_owners.contains(army.owner)) {
      context.error(
          child(army_path, QStringLiteral("owner")),
          QStringLiteral("owner %1 is already used by another army").arg(army.owner));
    }
    used_owners.insert(army.owner);
    army.team = static_cast<int>(
        context
            .number(
                army_object, army_path, "team", static_cast<float>(a + 1), 1.0F, 64.0F)
            .value_or(static_cast<float>(a + 1)));
    army.nation = parse_nation(context, army_object, army_path, std::nullopt)
                      .value_or(Game::Systems::NationID::RomanRepublic);
    army.facing =
        context.number(army_object, army_path, "facing", 0.0F, -720.0F, 720.0F)
            .value_or(0.0F);
    army.historical = context.optional_number(
        army_object, army_path, "historical_strength", 0.0F, 1.0e7F);
    const int army_index = static_cast<int>(armies.size());
    armies.push_back(army);

    const QString groups_path = child(army_path, QStringLiteral("groups"));
    const QJsonArray group_array =
        context.array(army_object, army_path, "groups", true);
    float historical_sum = 0.0F;
    for (qsizetype g = 0; g < group_array.size(); ++g) {
      const QString group_path = index_path(groups_path, g);
      if (!group_array.at(g).isObject()) {
        context.error(group_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject group_object = group_array.at(g).toObject();
      context.check_keys(group_object,
                         group_path,
                         {"id",
                          "label",
                          "troop",
                          "nation",
                          "historical",
                          "units",
                          "deployment",
                          "formation",
                          "hold",
                          "ambush"});
      PlannedGroup group;
      group.army = army_index;
      group.path = group_path;
      group.id = context.string(group_object, group_path, "id", true);
      if (group_ids.contains(group.id)) {
        context.error(child(group_path, QStringLiteral("id")),
                      QStringLiteral("duplicate group '%1'").arg(group.id));
      }
      group_ids.insert(group.id);
      const QString troop = context.string(group_object, group_path, "troop", true);
      if (!troop.isEmpty()) {
        if (!Game::Units::try_parse_troop_type(troop, group.troop)) {
          context.error(child(group_path, QStringLiteral("troop")),
                        QStringLiteral("unknown troop '%1' (expected one of %2)")
                            .arg(troop, troop_names_hint()));
        } else if (Game::Units::is_commander_troop(group.troop)) {
          context.error(child(group_path, QStringLiteral("troop")),
                        QStringLiteral("commanders go in the army's 'commanders' list "
                                       "by catalog id"));
        }
      }
      group.nation = parse_nation(context, group_object, group_path, army.nation)
                         .value_or(army.nation);
      const auto historical =
          context.optional_number(group_object, group_path, "historical", 1.0F, 1.0e7F);
      const auto units =
          context.optional_number(group_object, group_path, "units", 1.0F, 100000.0F);
      int soldiers = 0;
      if (historical.has_value() == units.has_value()) {
        context.error(group_path,
                      QStringLiteral("give exactly one of 'historical' (soldiers, "
                                     "scaled) or 'units' (game units)"));
      } else if (historical.has_value()) {
        historical_sum += *historical;
        soldiers = static_cast<int>(std::lround(*historical * result.scale));
        const int per_unit = std::max(1, ipu(group.troop));
        group.units =
            std::max(1,
                     static_cast<int>(std::lround(static_cast<float>(soldiers) /
                                                  static_cast<float>(per_unit))));
      } else {
        group.units = static_cast<int>(*units);
        soldiers = group.units * std::max(1, ipu(group.troop));
      }
      group.ambush = context.boolean(group_object, group_path, "ambush", false);
      group.hold = context.boolean(group_object, group_path, "hold", false);
      group.formation = context.string(group_object, group_path, "formation", false);
      if (!group.formation.isEmpty()) {
        group.formation_intent = resolve_formation(group.formation);
        if (!group.formation_intent.has_value()) {
          context.error(child(group_path, QStringLiteral("formation")),
                        QStringLiteral("unknown formation '%1' (known: %2)")
                            .arg(group.formation,
                                 known_formation_names().join(QStringLiteral(", "))));
        }
      }
      if (const auto deployment =
              context.object(group_object, group_path, "deployment", true)) {
        group.deployment = *deployment;
        group.deployment_path = child(group_path, QStringLiteral("deployment"));
        context.check_keys(group.deployment,
                           group.deployment_path,
                           {"shape",
                            "center",
                            "anchor",
                            "facing",
                            "ranks",
                            "files",
                            "file_spacing",
                            "rank_spacing",
                            "bulge",
                            "lines",
                            "split",
                            "line_gap",
                            "lanes",
                            "lane_width",
                            "stagger"});
      }
      compute_layout(context, group);
      result.groups.push_back(
          {group.id,
           army.id,
           Game::Units::troop_typeToQString(group.troop),
           Game::Systems::nation_id_to_qstring(group.nation),
           historical.has_value() ? static_cast<int>(*historical) : 0,
           soldiers,
           group.units,
           false,
           group.ambush});
      groups.push_back(std::move(group));
    }
    if (army.historical.has_value() && historical_sum > *army.historical * 1.05F) {
      context.warn(child(army_path, QStringLiteral("historical_strength")),
                   QStringLiteral("groups add up to %1, more than the stated %2")
                       .arg(historical_sum)
                       .arg(*army.historical));
    }

    const QString commanders_path = child(army_path, QStringLiteral("commanders"));
    const QJsonArray commanders =
        context.array(army_object, army_path, "commanders", false);
    if (commanders.size() > 1) {
      context.error(commanders_path,
                    QStringLiteral("an army fields one commander: the game allows one "
                                   "living commander per owner, and an owner whose "
                                   "commander dies collapses. Give each commander his "
                                   "own allied army (same 'team') for his contingent"));
    }
    for (qsizetype c = 0; c < commanders.size(); ++c) {
      const QString commander_path = index_path(commanders_path, c);
      if (!commanders.at(c).isObject()) {
        context.error(commander_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject commander = commanders.at(c).toObject();
      context.check_keys(commander,
                         commander_path,
                         {"id",
                          "catalog_id",
                          "historical_name",
                          "position",
                          "with",
                          "offset",
                          "facing",
                          "ambush",
                          "hold"});
      PlannedGroup group;
      group.army = army_index;
      group.path = commander_path;
      group.commander = true;
      group.units = 1;
      group.id = context.string(commander, commander_path, "id", true);
      if (group_ids.contains(group.id)) {
        context.error(child(commander_path, QStringLiteral("id")),
                      QStringLiteral("duplicate group '%1'").arg(group.id));
      }
      group_ids.insert(group.id);
      const QString catalog_id =
          context.string(commander, commander_path, "catalog_id", true);
      const auto* definition =
          catalog_id.isEmpty() ? nullptr : resolve_commander(catalog_id);
      if (!catalog_id.isEmpty() && definition == nullptr) {
        context.error(
            child(commander_path, QStringLiteral("catalog_id")),
            QStringLiteral("unknown commander '%1' (catalog: %2)")
                .arg(catalog_id, known_commander_ids().join(QStringLiteral(", "))));
      }
      if (definition != nullptr) {
        group.troop = definition->troop_type;
        group.nation = definition->nation_id;
        if (Game::Units::is_historical_commander_id(definition->id)) {
          group.commander_id = QString::fromStdString(definition->id);
        }
      }
      group.ambush = context.boolean(commander, commander_path, "ambush", false);
      group.hold = context.boolean(commander, commander_path, "hold", false);
      group.layout = block_layout(1, 1, 1.0F, 1.0F);
      if (const auto position =
              context.point(commander, commander_path, "position", false)) {
        group.center = *position;
        group.facing =
            context
                .number(
                    commander, commander_path, "facing", army.facing, -720.0F, 720.0F)
                .value_or(army.facing);
      } else {
        group.with_group = context.string(commander, commander_path, "with", false);
        if (group.with_group.isEmpty()) {
          context.error(commander_path,
                        QStringLiteral("a commander needs a 'position' or 'with'"));
        }
        group.with_offset = context.point(commander, commander_path, "offset", false)
                                .value_or(QVector3D());
      }
      result.groups.push_back({group.id,
                               army.id,
                               Game::Units::troop_typeToQString(group.troop),
                               Game::Systems::nation_id_to_qstring(group.nation),
                               1,
                               1,
                               1,
                               true,
                               group.ambush});
      groups.push_back(std::move(group));
    }
  }

  std::map<QString, PlannedGroup*> by_id;
  for (auto& group : groups) {
    if (!group.id.isEmpty()) {
      by_id[group.id] = &group;
    }
  }
  for (std::size_t pass = 0; pass <= groups.size(); ++pass) {
    bool progress = false;
    for (auto& group : groups) {
      if (group.placed || group.commander) {
        continue;
      }
      if (group.deployment.isEmpty()) {
        group.placed = true;
        group.center = QVector3D();
        continue;
      }
      if (try_place(context,
                    group,
                    by_id,
                    armies[static_cast<std::size_t>(group.army)],
                    false)) {
        group.placed = true;
        progress = true;
      }
    }
    if (!progress) {
      break;
    }
  }
  for (auto& group : groups) {
    if (group.commander || group.placed) {
      continue;
    }
    if (!try_place(context,
                   group,
                   by_id,
                   armies[static_cast<std::size_t>(group.army)],
                   true)) {
      context.error(
          group.deployment_path,
          QStringLiteral("deployment of '%1' could not be resolved (an anchor "
                         "cycle or an anchor to an unplaced group)")
              .arg(group.id));
    }
    group.placed = true;
    if (!group.center.has_value()) {
      group.center = QVector3D();
    }
  }
  for (auto& group : groups) {
    if (!group.commander || group.center.has_value()) {
      continue;
    }
    const auto found = by_id.find(group.with_group);
    if (found == by_id.end() || found->second->commander) {
      context.error(child(group.path, QStringLiteral("with")),
                    QStringLiteral("unknown group '%1'").arg(group.with_group));
      group.center = QVector3D();
      group.facing = armies[static_cast<std::size_t>(group.army)].facing;
      continue;
    }
    const PlannedGroup& host = *found->second;
    const float facing = host.facing.value_or(0.0F);
    const QVector3D forward = forward_of(facing);
    const QVector3D right = right_of(facing);
    QVector3D offset = group.with_offset;
    if (offset.isNull()) {
      offset = QVector3D(0.0F, 0.0F, -(host.layout.depth * 0.5F + 6.0F));
    }
    group.center = *host.center + right * offset.x() + forward * offset.z();
    group.facing = facing;
  }

  QSet<QString> ambush_ids;
  for (const auto& group : groups) {
    const auto& army = armies[static_cast<std::size_t>(group.army)];
    ArenaScenarioGroup out;
    out.name = group.id;
    out.troop_type = group.troop;
    out.nation_id = group.nation;
    out.owner_id = army.owner;
    out.spawn_at_start = !group.ambush;
    out.keep_troop_speed = true;
    out.commander_id = group.commander_id;
    if (group.ambush) {
      ambush_ids.insert(group.id);
    }
    if (group.center.has_value()) {
      place_positions(group, out);
    }
    scenario.groups.push_back(std::move(out));
  }

  for (const auto& army : armies) {
    scenario.owner_teams.push_back({army.owner, army.team});
    QVector3D sum;
    int count = 0;
    float reach = 16.0F;
    for (const auto& group : scenario.groups) {
      if (group.owner_id != army.owner || !group.spawn_at_start) {
        continue;
      }
      sum += group.origin;
      ++count;
    }
    const QVector3D home = count > 0 ? sum / static_cast<float>(count) : QVector3D();
    for (const auto& group : scenario.groups) {
      if (group.owner_id == army.owner) {
        reach = std::max(reach, (group.origin - home).length() + 20.0F);
      }
    }
    scenario.battle_sides.push_back({army.owner, army.id, home, reach});
  }

  for (const auto& group : groups) {
    const QString deploy = QStringLiteral("deploy:%1").arg(group.id);
    if (group.hold) {
      auto step = make_step(deploy + QStringLiteral("/hold"),
                            {ScenarioTriggerKind::AtTime, 0.0F},
                            ScenarioCommandKind::Hold,
                            group.id);
      step.enabled = true;
      scenario.steps.push_back(std::move(step));
    }
    if (group.formation_intent.has_value() && !group.ambush) {
      auto step = make_step(deploy + QStringLiteral("/formation"),
                            {ScenarioTriggerKind::AtTime, 0.0F},
                            ScenarioCommandKind::FormArmy,
                            group.id);
      step.formation.groups = {group.id};
      step.formation.intent = *group.formation_intent;
      step.formation.anchor = group.center.value_or(QVector3D());
      step.formation.facing_degrees = group.facing.value_or(0.0F);
      step.formation.frontage = group.layout.frontage;
      scenario.steps.push_back(std::move(step));
    }
  }

  QSet<QString> phase_ids;
  const QString phases_path = child(path, QStringLiteral("phases"));
  const QJsonArray phases = context.array(root, path, "phases", false);
  for (qsizetype p = 0; p < phases.size(); ++p) {
    if (phases.at(p).isObject()) {
      const QString id = phases.at(p).toObject().value(QStringLiteral("id")).toString();
      if (!id.isEmpty()) {
        if (phase_ids.contains(id)) {
          context.error(child(index_path(phases_path, p), QStringLiteral("id")),
                        QStringLiteral("duplicate phase '%1'").arg(id));
        }
        phase_ids.insert(id);
      }
    }
  }
  QSet<QString> event_names;
  for (qsizetype p = 0; p < phases.size(); ++p) {
    const QString phase_path = index_path(phases_path, p);
    if (!phases.at(p).isObject()) {
      context.error(phase_path, QStringLiteral("expected an object"));
      continue;
    }
    const QJsonObject phase = phases.at(p).toObject();
    context.check_keys(
        phase, phase_path, {"id", "label", "event", "trigger", "actions"});
    const QString id = context.string(phase, phase_path, "id", true);
    QString event = context.string(phase, phase_path, "event", false);
    if (event.isEmpty()) {
      event = id;
    }
    if (event_names.contains(event)) {
      context.error(child(phase_path, QStringLiteral("event")),
                    QStringLiteral("duplicate phase event '%1'").arg(event));
    }
    event_names.insert(event);
    const QString label = context.string(phase, phase_path, "label", false);
    const QString marker = QStringLiteral("phase:%1").arg(id);

    ScenarioTrigger trigger;
    const QString trigger_path = child(phase_path, QStringLiteral("trigger"));
    if (const auto trigger_object =
            context.object(phase, phase_path, "trigger", true)) {
      const auto& t = *trigger_object;
      const QString type = context.string(t, trigger_path, "type", true);
      if (type == QStringLiteral("start")) {
        context.check_keys(t, trigger_path, {"type", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::AtTime;
      } else if (type == QStringLiteral("time")) {
        context.check_keys(t, trigger_path, {"type", "at", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::AtTime;
        trigger.time_seconds =
            context.number(t, trigger_path, "at", std::nullopt, 0.0F, 1.0e6F)
                .value_or(0.0F);
      } else if (type == QStringLiteral("contact")) {
        context.check_keys(
            t,
            trigger_path,
            {"type", "group", "with", "distance", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::FirstContact;
        trigger.group =
            checked_group(context, t, trigger_path, "group", group_ids, true);
        trigger.target_group =
            checked_group(context, t, trigger_path, "with", group_ids, true);
        trigger.distance =
            context.number(t, trigger_path, "distance", 3.0F, 0.1F, 1000.0F)
                .value_or(3.0F);
      } else if (type == QStringLiteral("strength_below")) {
        context.check_keys(
            t, trigger_path, {"type", "group", "fraction", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::GroupStrengthBelow;
        trigger.group =
            checked_group(context, t, trigger_path, "group", group_ids, true);
        trigger.threshold =
            context.number(t, trigger_path, "fraction", std::nullopt, 0.001F, 1.0F)
                .value_or(0.5F);
      } else if (type == QStringLiteral("destroyed")) {
        context.check_keys(t, trigger_path, {"type", "group", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::GroupDestroyed;
        trigger.group =
            checked_group(context, t, trigger_path, "group", group_ids, true);
      } else if (type == QStringLiteral("area")) {
        context.check_keys(
            t,
            trigger_path,
            {"type", "group", "center", "radius", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::GroupEnteredArea;
        trigger.group =
            checked_group(context, t, trigger_path, "group", group_ids, true);
        trigger.position =
            context.point(t, trigger_path, "center", true).value_or(QVector3D());
        trigger.distance = context.distance(t, trigger_path, "radius", 20.0F, 0.5F);
      } else if (type == QStringLiteral("phase")) {
        context.check_keys(
            t, trigger_path, {"type", "phase", "delay", "after", "fallback_at"});
        trigger.kind = ScenarioTriggerKind::StepExecuted;
        const QString other = context.string(t, trigger_path, "phase", true);
        if (!other.isEmpty() && !phase_ids.contains(other)) {
          context.error(child(trigger_path, QStringLiteral("phase")),
                        QStringLiteral("unknown phase '%1'").arg(other));
        }
        if (other == id) {
          context.error(child(trigger_path, QStringLiteral("phase")),
                        QStringLiteral("a phase cannot wait on itself"));
        }
        trigger.step = QStringLiteral("phase:%1").arg(other);
        trigger.time_seconds =
            context.number(t, trigger_path, "delay", 0.0F, 0.0F, 1.0e6F).value_or(0.0F);
      } else if (!type.isEmpty()) {
        context.error(child(trigger_path, QStringLiteral("type")),
                      QStringLiteral("unknown trigger '%1' (expected start, time, "
                                     "contact, strength_below, destroyed, area or "
                                     "phase)")
                          .arg(type));
      }
      const QString after = context.string(t, trigger_path, "after", false);
      if (!after.isEmpty()) {
        if (!phase_ids.contains(after)) {
          context.error(child(trigger_path, QStringLiteral("after")),
                        QStringLiteral("unknown phase '%1'").arg(after));
        }
        trigger.after_step = QStringLiteral("phase:%1").arg(after);
      }
      if (const auto fallback =
              context.optional_number(t, trigger_path, "fallback_at", 0.0F, 1.0e6F)) {
        trigger.fallback_seconds = *fallback;
      }
    }
    auto marker_step = make_step(marker, trigger, ScenarioCommandKind::Marker);
    marker_step.event = phase_event_name(event);
    scenario.steps.push_back(std::move(marker_step));
    result.phases.push_back({id, phase_event_name(event), label});

    const QString actions_path = child(phase_path, QStringLiteral("actions"));
    const QJsonArray actions = context.array(phase, phase_path, "actions", false);
    int action_index = 0;
    for (qsizetype a = 0; a < actions.size(); ++a) {
      const QString action_path = index_path(actions_path, a);
      if (!actions.at(a).isObject()) {
        context.error(action_path, QStringLiteral("expected an object"));
        continue;
      }
      const QJsonObject action = actions.at(a).toObject();
      const QString type = context.string(action, action_path, "type", true);
      const float delay =
          context.number(action, action_path, "delay", 0.0F, 0.0F, 1.0e6F)
              .value_or(0.0F);
      ScenarioTrigger after_marker;
      after_marker.kind = ScenarioTriggerKind::StepExecuted;
      after_marker.step = marker;
      after_marker.time_seconds = delay;
      const auto next_name = [&]() {
        return QStringLiteral("%1/%2").arg(marker).arg(action_index++);
      };
      if (type == QStringLiteral("weather")) {
        scenario.weather_script.changes.push_back(
            read_weather_change(context, action, action_path, bank_ids, false));
        auto step =
            make_step(next_name(), after_marker, ScenarioCommandKind::SetWeather);
        step.weather_change =
            static_cast<int>(scenario.weather_script.changes.size() - 1U);
        scenario.steps.push_back(std::move(step));
        continue;
      }
      if (type == QStringLiteral("formation")) {
        context.check_keys(action,
                           action_path,
                           {"type",
                            "delay",
                            "group",
                            "groups",
                            "formation",
                            "at",
                            "facing",
                            "frontage"});
        const QStringList members = group_list(context, action, action_path, group_ids);
        const QString name = context.string(action, action_path, "formation", true);
        const auto intent = name.isEmpty() ? std::nullopt : resolve_formation(name);
        if (!name.isEmpty() && !intent.has_value()) {
          context.error(
              child(action_path, QStringLiteral("formation")),
              QStringLiteral("unknown formation '%1' (known: %2)")
                  .arg(name, known_formation_names().join(QStringLiteral(", "))));
        }
        auto step = make_step(next_name(),
                              after_marker,
                              ScenarioCommandKind::FormArmy,
                              members.isEmpty() ? QString() : members.front());
        step.formation.groups = members;
        step.formation.intent =
            intent.value_or(Game::Formation::ArmyFormationIntent::FactionDefault);
        step.formation.anchor =
            context.point(action, action_path, "at", true).value_or(QVector3D());
        step.formation.facing_degrees =
            context.number(action, action_path, "facing", 0.0F, -720.0F, 720.0F)
                .value_or(0.0F);
        step.formation.frontage =
            context.distance(action, action_path, "frontage", 0.0F);
        scenario.steps.push_back(std::move(step));
        continue;
      }
      if (type == QStringLiteral("move")) {
        context.check_keys(
            action,
            action_path,
            {"type", "delay", "group", "groups", "to", "by", "keep_shape", "run"});
      } else if (type == QStringLiteral("attack") || type == QStringLiteral("charge")) {
        context.check_keys(
            action, action_path, {"type", "delay", "group", "groups", "target"});
      } else if (type == QStringLiteral("hold")) {
        context.check_keys(
            action, action_path, {"type", "delay", "group", "groups", "enabled"});
      } else if (type == QStringLiteral("stop")) {
        context.check_keys(action, action_path, {"type", "delay", "group", "groups"});
      } else if (type == QStringLiteral("wheel")) {
        context.check_keys(action,
                           action_path,
                           {"type", "delay", "group", "groups", "degrees", "pivot"});
      } else if (type == QStringLiteral("reveal")) {
        context.check_keys(
            action, action_path, {"type", "delay", "group", "groups", "target"});
      } else {
        if (!type.isEmpty()) {
          context.error(child(action_path, QStringLiteral("type")),
                        QStringLiteral("unknown action '%1' (expected move, attack, "
                                       "charge, hold, stop, wheel, reveal, formation "
                                       "or weather)")
                            .arg(type));
        }
        continue;
      }
      const QStringList members = group_list(context, action, action_path, group_ids);
      if (members.isEmpty()) {
        context.error(action_path,
                      QStringLiteral("an order needs a 'group' or 'groups'"));
      }
      QString target;
      if (type == QStringLiteral("attack") || type == QStringLiteral("charge")) {
        target = checked_group(context, action, action_path, "target", group_ids, true);
      } else if (type == QStringLiteral("reveal")) {
        target =
            checked_group(context, action, action_path, "target", group_ids, false);
      }
      std::optional<QVector3D> destination;
      bool keep_shape = true;
      bool run = false;
      bool by_offset = false;
      if (type == QStringLiteral("move")) {
        by_offset = action.contains(QStringLiteral("by"));
        if (by_offset == action.contains(QStringLiteral("to"))) {
          context.error(action_path,
                        QStringLiteral("a move needs exactly one of 'to' (a point) or "
                                       "'by' (an offset)"));
        }
        destination = by_offset ? context.point(action, action_path, "by", false)
                                : context.point(action, action_path, "to", false);
        keep_shape = context.boolean(action, action_path, "keep_shape", true);
        if (by_offset && !keep_shape) {
          context.error(child(action_path, QStringLiteral("by")),
                        QStringLiteral("an offset move keeps the group's shape"));
        }
        run = context.boolean(action, action_path, "run", false);
        if (run && keep_shape) {
          context.error(child(action_path, QStringLiteral("run")),
                        QStringLiteral("a run reforms at the destination; set "
                                       "keep_shape false"));
        }
      }
      float degrees = 0.0F;
      QString pivot_side;
      std::optional<QVector3D> pivot_point;
      if (type == QStringLiteral("wheel")) {
        degrees =
            context
                .number(action, action_path, "degrees", std::nullopt, -360.0F, 360.0F)
                .value_or(0.0F);
        const QJsonValue pivot = action.value(QStringLiteral("pivot"));
        if (pivot.isString()) {
          pivot_side = pivot.toString();
          if (pivot_side != QStringLiteral("left") &&
              pivot_side != QStringLiteral("right") &&
              pivot_side != QStringLiteral("center")) {
            context.error(child(action_path, QStringLiteral("pivot")),
                          QStringLiteral("expected left, right, center or a point"));
          }
        } else if (pivot.isArray()) {
          pivot_point = context.point(action, action_path, "pivot", true);
        } else {
          pivot_side = QStringLiteral("center");
        }
      }
      for (const QString& member : members) {
        ArenaScenarioStep step =
            make_step(next_name(), after_marker, ScenarioCommandKind::Stand, member);
        if (type == QStringLiteral("move")) {
          step.command = keep_shape ? ScenarioCommandKind::ShapeMove
                         : run      ? ScenarioCommandKind::Run
                                    : ScenarioCommandKind::FormationMove;
          step.destination = destination.value_or(QVector3D());
          step.destination_is_offset = by_offset;
          step.enabled = true;
        } else if (type == QStringLiteral("attack")) {
          step.command = ScenarioCommandKind::AttackMove;
          step.target_group = target;
        } else if (type == QStringLiteral("charge")) {
          step.command = ScenarioCommandKind::Charge;
          step.target_group = target;
        } else if (type == QStringLiteral("hold")) {
          step.command = ScenarioCommandKind::Hold;
          step.enabled = context.boolean(action, action_path, "enabled", true);
        } else if (type == QStringLiteral("stop")) {
          step.command = ScenarioCommandKind::Stop;
        } else if (type == QStringLiteral("wheel")) {
          step.command = ScenarioCommandKind::Wheel;
          step.angle_degrees = -degrees;
          step.pivot_side = pivot_side;
          step.destination = pivot_point.value_or(QVector3D());
        } else if (type == QStringLiteral("reveal")) {
          if (!ambush_ids.contains(member)) {
            context.error(action_path,
                          QStringLiteral("'%1' is not an ambush group; only ambush "
                                         "groups can be revealed")
                              .arg(member));
          }
          step.command = ScenarioCommandKind::SpawnAmbush;
          step.target_group = target;
        }
        scenario.steps.push_back(std::move(step));
      }
    }
  }

  if (!scenario.groups.empty()) {
    for (const auto& group : scenario.groups) {
      if (group.spawn_at_start) {
        ArenaExpectation exists;
        exists.kind = ArenaExpectationKind::GroupExists;
        exists.group = group.name;
        scenario.expectations.push_back(exists);
        break;
      }
    }
  }

  if (const auto camera = context.object(root, path, "camera", false)) {
    const QString camera_path = child(path, QStringLiteral("camera"));
    context.check_keys(*camera, camera_path, {"distance", "tilt", "yaw", "focus"});
    scenario.camera.distance =
        context.distance(*camera, camera_path, "distance", 180.0F, 2.0F);
    scenario.camera.angle =
        context.number(*camera, camera_path, "tilt", 50.0F, 1.0F, 89.0F)
            .value_or(50.0F);
    scenario.camera.yaw =
        context.number(*camera, camera_path, "yaw", 0.0F, -720.0F, 720.0F)
            .value_or(0.0F);
    scenario.camera_focus = context.point(*camera, camera_path, "focus", false);
  } else {
    scenario.camera = {180.0F * context.geometry, 50.0F, 0.0F};
    scenario.camera_focus = QVector3D();
  }

  if (result.errors.empty()) {
    for (const auto& error : validate_scenario(scenario)) {
      context.error(QStringLiteral("$ (compiled %1)").arg(error.field), error.message);
    }
  }
  if (result.errors.empty()) {
    result.scenario = std::move(scenario);
  }
  return result;
}

auto load_file(const QString& path, const LoadOptions& options) -> CompileResult {
  CompileResult result;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    result.errors.push_back({path, QStringLiteral("cannot open battle script")});
    return result;
  }
  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    result.errors.push_back({path,
                             QStringLiteral("invalid JSON at byte %1: %2")
                                 .arg(parse_error.offset)
                                 .arg(parse_error.errorString())});
    return result;
  }
  if (!document.isObject()) {
    result.errors.push_back(
        {QStringLiteral("$"), QStringLiteral("expected an object")});
    return result;
  }
  LoadOptions resolved = options;
  if (resolved.base_directory.isEmpty()) {
    resolved.base_directory = QFileInfo(path).absolutePath();
  }
  return compile(document.object(), resolved);
}

auto format_diagnostics(const CompileResult& result) -> QString {
  QStringList lines;
  for (const auto& error : result.errors) {
    lines.push_back(QStringLiteral("error: %1: %2").arg(error.path, error.message));
  }
  for (const auto& warning : result.warnings) {
    lines.push_back(
        QStringLiteral("warning: %1: %2").arg(warning.path, warning.message));
  }
  return lines.join(QLatin1Char('\n'));
}

auto summary_text(const CompileResult& result) -> QString {
  QStringList lines;
  if (result.scenario.has_value()) {
    lines.push_back(QStringLiteral("battle script '%1' at scale %2 (authored %3)")
                        .arg(result.scenario->id)
                        .arg(result.scale)
                        .arg(result.authored_scale));
  }
  int total_units = 0;
  int total_soldiers = 0;
  for (const auto& group : result.groups) {
    total_units += group.units;
    total_soldiers += group.soldiers;
    lines.push_back(
        QStringLiteral("  %1 [%2] %3 %4: %5 historical -> %6 soldiers -> %7 "
                       "unit(s)%8")
            .arg(group.id, group.army, group.nation, group.troop)
            .arg(group.historical)
            .arg(group.soldiers)
            .arg(group.units)
            .arg(group.ambush ? QStringLiteral(" (ambush)") : QString()));
  }
  lines.push_back(QStringLiteral("  total: %1 units, %2 soldiers")
                      .arg(total_units)
                      .arg(total_soldiers));
  for (const auto& phase : result.phases) {
    lines.push_back(QStringLiteral("  phase %1 -> event %2%3")
                        .arg(phase.id, phase.event)
                        .arg(phase.label.isEmpty()
                                 ? QString()
                                 : QStringLiteral(" (%1)").arg(phase.label)));
  }
  return lines.join(QLatin1Char('\n'));
}

auto register_file(const QString& path,
                   const LoadOptions& options,
                   QString* error) -> std::optional<QString> {
  auto result = load_file(path, options);
  if (!result.ok()) {
    if (error != nullptr) {
      *error = QStringLiteral("battle script %1 is invalid:\n%2")
                   .arg(path, format_diagnostics(result));
    }
    return std::nullopt;
  }
  if (!result.warnings.empty()) {
    qWarning().noquote() << format_diagnostics(result);
  }
  qInfo().noquote() << summary_text(result);
  QString id = result.scenario->id;
  Scenarios::register_runtime_definition(std::move(*result.scenario));
  return id;
}

} // namespace Arena::BattleScript
