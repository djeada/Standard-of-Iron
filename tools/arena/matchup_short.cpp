#include "matchup_short.h"

#include <QRegularExpression>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

#include "game/map/environment_lighting.h"

namespace Arena::Matchup {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

constexpr float k_unit_spacing = 4.4F;
constexpr float k_rank_spacing = 4.2F;

constexpr float k_army_separation = 7.5F;

constexpr int k_max_files = 6;

constexpr float k_camera_pitch = 38.0F;
constexpr float k_camera_fov = 28.0F;

constexpr float k_camera_yaw = 0.0F;

auto normalized_troop_name(const QString& raw) -> QString {
  QString name = raw.trimmed().toLower();
  name.replace(QLatin1Char('-'), QLatin1Char('_'));
  name.replace(QLatin1Char(' '), QLatin1Char('_'));

  static const QList<QPair<QString, QString>> k_aliases = {
      {QStringLiteral("swordsmen"), QStringLiteral("swordsman")},
      {QStringLiteral("swordman"), QStringLiteral("swordsman")},
      {QStringLiteral("swordmen"), QStringLiteral("swordsman")},
      {QStringLiteral("spearmen"), QStringLiteral("spearman")},
      {QStringLiteral("horsemen"), QStringLiteral("horse_swordsman")},
      {QStringLiteral("knights"), QStringLiteral("horse_swordsman")},
      {QStringLiteral("knight"), QStringLiteral("horse_swordsman")},
      {QStringLiteral("wolves"), QStringLiteral("wolf")},
  };
  for (const auto& [written, canonical] : k_aliases) {
    if (name == written) {
      return canonical;
    }
    if (name.endsWith(QStringLiteral("_") + written)) {
      name.chop(written.size());
      name += canonical;
      return name;
    }
  }
  if (name.endsWith(QLatin1Char('s')) && name.size() > 3) {
    QString singular = name;
    singular.chop(1);
    Troop probe{};
    if (Game::Units::try_parse_troop_type(singular, probe)) {
      return singular;
    }
  }
  return name;
}

auto nation_from_words(const QStringList& words,
                       int& consumed) -> std::optional<Nation> {
  static const QList<QPair<QString, Nation>> k_nations = {
      {QStringLiteral("iron_sepulcher"), Nation::IronSepulcher},
      {QStringLiteral("sepulcher"), Nation::IronSepulcher},
      {QStringLiteral("undead"), Nation::IronSepulcher},
      {QStringLiteral("carthage"), Nation::Carthage},
      {QStringLiteral("carthaginian"), Nation::Carthage},
      {QStringLiteral("roman_republic"), Nation::RomanRepublic},
      {QStringLiteral("roman"), Nation::RomanRepublic},
      {QStringLiteral("rome"), Nation::RomanRepublic},
  };

  for (int span = std::min(2, static_cast<int>(words.size())); span >= 1; --span) {
    QString phrase = words.mid(0, span).join(QLatin1Char('_')).toLower();
    phrase.replace(QLatin1Char('-'), QLatin1Char('_'));
    for (const auto& [name, nation] : k_nations) {
      if (phrase == name) {
        consumed = span;
        return nation;
      }
    }
  }
  consumed = 0;
  return std::nullopt;
}

auto nation_equivalent(Nation nation, Troop troop) -> Troop {
  if (nation != Nation::IronSepulcher) {

    switch (troop) {
    case Troop::SkeletonSwordsman:
      return Troop::Swordsman;
    case Troop::SkeletonArcher:
      return Troop::Archer;
    case Troop::GravePriest:
      return Troop::Healer;
    default:
      return troop;
    }
  }
  switch (troop) {
  case Troop::Swordsman:
  case Troop::Spearman:
  case Troop::MountedKnight:
  case Troop::HorseSpearman:
    return Troop::SkeletonSwordsman;
  case Troop::Archer:
  case Troop::HorseArcher:
    return Troop::SkeletonArcher;
  case Troop::Healer:
    return Troop::GravePriest;
  default:
    return troop;
  }
}

auto parse_side(const QString& text, QString* error) -> std::optional<Side> {
  const QStringList raw = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);

  QStringList words;
  std::optional<int> count;
  for (const QString& word : raw) {
    const QString lowered = word.toLower();
    if (lowered == QStringLiteral("unit") || lowered == QStringLiteral("units") ||
        lowered == QStringLiteral("x")) {
      continue;
    }
    bool numeric = false;
    const int value = lowered.toInt(&numeric);
    if (numeric && !count.has_value()) {
      count = value;
      continue;
    }
    words.push_back(lowered);
  }

  if (!count.has_value()) {
    if (error != nullptr) {
      *error = QStringLiteral("'%1' names no unit count; write something like "
                              "'2 carthage swordsman'")
                   .arg(text.trimmed());
    }
    return std::nullopt;
  }

  Side side;
  side.count = *count;
  if (side.count < 1 || side.count > k_max_side_count) {
    if (error != nullptr) {
      *error = QStringLiteral("a side needs between 1 and %1 units, not %2")
                   .arg(k_max_side_count)
                   .arg(side.count);
    }
    return std::nullopt;
  }

  int consumed = 0;
  if (auto leading = nation_from_words(words, consumed); leading.has_value()) {
    side.nation = *leading;
    side.nation_named = true;
    words.erase(words.begin(), words.begin() + consumed);
  } else if (words.size() >= 2) {
    for (int span = std::min(2, static_cast<int>(words.size())); span >= 1; --span) {
      const QStringList tail = words.mid(words.size() - span);
      int tail_consumed = 0;
      if (auto trailing = nation_from_words(tail, tail_consumed);
          trailing.has_value() && tail_consumed == span) {
        side.nation = *trailing;
        side.nation_named = true;
        words.erase(words.end() - span, words.end());
        break;
      }
    }
  }

  if (words.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("'%1' names no unit").arg(text.trimmed());
    }
    return std::nullopt;
  }

  const QString troop_name = normalized_troop_name(words.join(QLatin1Char('_')));
  if (!Game::Units::try_parse_troop_type(troop_name, side.troop)) {
    if (error != nullptr) {
      *error = QStringLiteral("unknown unit '%1'").arg(words.join(QLatin1Char(' ')));
    }
    return std::nullopt;
  }

  if (side.nation_named) {
    side.troop = nation_equivalent(side.nation, side.troop);
  }
  return side;
}

auto files_for(int count) -> int {
  return std::clamp(count, 1, k_max_files);
}

auto ranks_for(int count) -> int {
  const int files = files_for(count);
  return (count + files - 1) / files;
}

auto block_half_width(int count) -> float {
  return (static_cast<float>(files_for(count) - 1) * k_unit_spacing) * 0.5F;
}

auto block_depth(int count) -> float {
  return static_cast<float>(ranks_for(count) - 1) * k_rank_spacing;
}

auto side_group_name(const QString& prefix, int rank) -> QString {
  return QStringLiteral("%1_rank_%2").arg(prefix).arg(rank);
}

void push_blocks(ArenaScenarioDefinition& scenario,
                 const Side& side,
                 const QString& prefix,
                 int owner_id,
                 float front_x,
                 float direction) {
  const int files = files_for(side.count);
  const int ranks = ranks_for(side.count);
  const float half_width = block_half_width(side.count);

  int remaining = side.count;
  for (int rank = 0; rank < ranks; ++rank) {
    const int in_rank = std::min(files, remaining);
    remaining -= in_rank;

    ArenaScenarioGroup group;
    group.name = side_group_name(prefix, rank);
    group.troop_type = side.troop;
    group.nation_id = side.nation;
    group.owner_id = owner_id;
    group.count = in_rank;

    group.origin = QVector3D(
        front_x + (direction * static_cast<float>(rank) * k_rank_spacing),
        0.0F,
        -half_width + ((static_cast<float>(files - in_rank) * k_unit_spacing) * 0.5F));
    group.spacing = QVector3D(0.0F, 0.0F, k_unit_spacing);
    group.facing_degrees = owner_id == 1 ? 0.0F : 180.0F;
    scenario.groups.push_back(std::move(group));
  }
}

auto plural_label(const Side& side) -> QString {
  QString name = Game::Units::troop_typeToQString(side.troop);
  name.replace(QLatin1Char('_'), QLatin1Char(' '));
  if (side.count == 1) {
    return name;
  }
  if (name.endsWith(QStringLiteral("man"))) {
    name.chop(3);
    name += QStringLiteral("men");
    return name;
  }
  if (name.endsWith(QStringLiteral("wolf"))) {
    name.chop(4);
    name += QStringLiteral("wolves");
    return name;
  }
  return name + QStringLiteral("s");
}

} // namespace

auto parse(const QString& text, QString* error) -> std::optional<Matchup> {
  static const QRegularExpression separator(
      QStringLiteral(R"(\s+(?:vs\.?|versus|v)\s+)"),
      QRegularExpression::CaseInsensitiveOption);

  const QStringList halves = text.split(separator);
  if (halves.size() != 2) {
    if (error != nullptr) {
      *error =
          QStringLiteral("a matchup reads '<count> <nation> <unit> vs <count> <nation> "
                         "<unit>', such as '2 carthage swordsman vs 5 iron sepulcher "
                         "swordsman'; got '%1'")
              .arg(text.trimmed());
    }
    return std::nullopt;
  }

  Matchup matchup;
  auto attacker = parse_side(halves[0], error);
  if (!attacker.has_value()) {
    return std::nullopt;
  }
  auto defender = parse_side(halves[1], error);
  if (!defender.has_value()) {
    return std::nullopt;
  }
  matchup.attacker = *attacker;
  matchup.defender = *defender;

  if (!matchup.attacker.nation_named && !matchup.defender.nation_named) {
    matchup.defender.nation = Nation::Carthage;
  }
  return matchup;
}

auto side_label(const Side& side) -> QString {
  QString nation = Game::Systems::nation_id_to_qstring(side.nation);
  nation.replace(QLatin1Char('_'), QLatin1Char(' '));
  return QStringLiteral("%1 %2 %3")
      .arg(side.count)
      .arg(nation, plural_label(side))
      .toUpper();
}

auto title(const Matchup& matchup) -> QString {
  return QStringLiteral("%1 vs %2")
      .arg(side_label(matchup.attacker), side_label(matchup.defender));
}

auto build_scenario(const Matchup& matchup) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition scenario;
  scenario.id = QString::fromLatin1(k_scenario_id);
  scenario.label = title(matchup);
  scenario.description =
      QStringLiteral("Two lines walk into each other so the fight can be recorded "
                     "whole: %1.")
          .arg(title(matchup));
  scenario.duration_seconds = matchup.fight_seconds + matchup.report_seconds + 10.0F;

  const float attacker_front = -(k_army_separation * 0.5F);
  const float defender_front = k_army_separation * 0.5F;

  const float reach = std::max(block_depth(matchup.attacker.count),
                               block_depth(matchup.defender.count));
  const float span = std::max(block_half_width(matchup.attacker.count),
                              block_half_width(matchup.defender.count));
  scenario.arena_floor_half_extent =
      std::max(22.0F, (k_army_separation * 0.5F) + reach + span + 10.0F);

  scenario.camera.distance = 24.0F;
  scenario.camera.angle = k_camera_pitch;
  scenario.camera.yaw = k_camera_yaw;
  scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  scenario.select_spawned_units = false;
  scenario.suppress_spawn_anchor = true;
  scenario.suppress_ui_overlays = false;
  scenario.environment.start_time = 11.0F;
  scenario.environment.time_mode = Game::Map::TimeMode::Locked;

  push_blocks(
      scenario, matchup.attacker, QStringLiteral("blue"), 1, attacker_front, -1.0F);
  push_blocks(
      scenario, matchup.defender, QStringLiteral("red"), 2, defender_front, 1.0F);

  ArenaScenarioBattleSide blue;
  blue.owner_id = 1;
  blue.label = side_label(matchup.attacker);
  blue.home = QVector3D(attacker_front, 0.0F, 0.0F);
  blue.home_radius = 14.0F;
  ArenaScenarioBattleSide red;
  red.owner_id = 2;
  red.label = side_label(matchup.defender);
  red.home = QVector3D(defender_front, 0.0F, 0.0F);
  red.home_radius = 14.0F;
  scenario.battle_sides = {blue, red};

  const auto advance =
      [&](const QString& prefix, int ranks, const QString& enemy, int enemy_ranks) {
        for (int rank = 0; rank < ranks; ++rank) {
          ArenaScenarioStep step;
          step.name = QStringLiteral("advance_%1").arg(side_group_name(prefix, rank));
          step.trigger = {Trigger::AtTime, 0.4F, {}, {}, 0.0F};
          step.command = Command::AttackMove;
          step.group = side_group_name(prefix, rank);

          step.target_group = side_group_name(enemy, std::min(rank, enemy_ranks - 1));
          step.chase = true;
          scenario.steps.push_back(std::move(step));
        }
      };
  const int attacker_ranks = ranks_for(matchup.attacker.count);
  const int defender_ranks = ranks_for(matchup.defender.count);
  advance(
      QStringLiteral("blue"), attacker_ranks, QStringLiteral("red"), defender_ranks);
  advance(
      QStringLiteral("red"), defender_ranks, QStringLiteral("blue"), attacker_ranks);

  ArenaExpectation decision;
  decision.kind = Expect::BattleReachesDecision;
  scenario.expectations.push_back(decision);

  return scenario;
}

auto build_spec(const Matchup& matchup) -> Promo::Spec {
  Promo::Spec spec;
  spec.id = QStringLiteral("matchup_%1_%2_%3_vs_%4_%5_%6")
                .arg(matchup.attacker.count)
                .arg(Game::Systems::nation_id_to_qstring(matchup.attacker.nation),
                     Game::Units::troop_typeToQString(matchup.attacker.troop))
                .arg(matchup.defender.count)
                .arg(Game::Systems::nation_id_to_qstring(matchup.defender.nation),
                     Game::Units::troop_typeToQString(matchup.defender.troop));
  spec.title = title(matchup);
  spec.width = matchup.preview ? 540 : 1080;
  spec.height = matchup.preview ? 960 : 1920;
  spec.fps = 30;
  spec.supersample = 1;

  spec.audio = !matchup.preview;

  spec.music_track = QStringLiteral("music.menu.iron_kingdom");
  spec.music_volume = 0.18F;

  spec.report_sound_decided = QStringLiteral("music.stinger.victory_fanfare");
  spec.report_sound_undecided = QStringLiteral("music.stinger.defeat_quiet_field");
  spec.report_sound_volume = 0.45F;
  spec.gameplay_ui = true;

  spec.gameplay_ui_all_owners = true;
  spec.report_card_style = Promo::ReportCardStyle::Matchup;

  const float reach = std::max(block_depth(matchup.attacker.count),
                               block_depth(matchup.defender.count));
  const float span = std::max(block_half_width(matchup.attacker.count),
                              block_half_width(matchup.defender.count));

  const float across_frame = k_army_separation + (2.0F * reach) + 6.0F;
  const float up_frame =
      ((2.0F * span) + 4.0F) * std::sin(qDegreesToRadians(k_camera_pitch));

  const float vertical_half = std::tan(qDegreesToRadians(k_camera_fov) * 0.5F);
  const float horizontal_half = vertical_half * (static_cast<float>(spec.width) /
                                                 static_cast<float>(spec.height));

  const float for_separation = (across_frame * 0.5F) / std::max(0.05F, horizontal_half);
  const float for_line = (up_frame * 0.5F) / std::max(0.05F, vertical_half);
  const float distance =
      std::clamp(std::max(for_separation, for_line) * 1.04F, 12.0F, 120.0F);

  Promo::Shot shot;
  shot.name = QStringLiteral("fight");
  shot.scenario = QString::fromLatin1(k_scenario_id);
  shot.seed = matchup.seed;
  shot.start_seconds = 0.0F;
  shot.duration_seconds =
      matchup.preview ? std::min(matchup.fight_seconds, 6.0F) : matchup.fight_seconds;
  shot.gameplay_ui = true;
  shot.gameplay_ui_all_owners = true;
  shot.report_card_seconds = matchup.report_seconds;

  shot.focus.mode = Promo::FocusMode::GroupPair;
  shot.focus.group = side_group_name(QStringLiteral("blue"), 0);
  shot.focus.second_group = side_group_name(QStringLiteral("red"), 0);
  shot.focus.point = QVector3D(0.0F, 0.0F, 0.0F);
  shot.focus.smoothing = 2.0F;

  Promo::CameraKey open;
  open.time = 0.0F;
  open.distance = distance;
  open.pitch = k_camera_pitch;
  open.yaw = k_camera_yaw;
  open.fov = k_camera_fov;
  open.height = 0.0F;
  open.ease = Promo::Ease::Smooth;
  Promo::CameraKey close = open;
  close.time = shot.duration_seconds;

  close.distance = distance * 0.78F;
  shot.keys = {open, close};

  spec.shots.push_back(std::move(shot));
  return spec;
}

} // namespace Arena::Matchup
