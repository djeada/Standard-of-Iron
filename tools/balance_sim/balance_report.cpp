#include "balance_report.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace Balance {

namespace {

auto median(std::vector<double> values) -> double {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const std::size_t mid = values.size() / 2;
  if (values.size() % 2 == 1) {
    return values[mid];
  }
  return (values[mid - 1] + values[mid]) * 0.5;
}

auto mean(const std::vector<double>& values) -> double {
  if (values.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (double value : values) {
    sum += value;
  }
  return sum / static_cast<double>(values.size());
}

auto percent(double fraction) -> QString {
  return QString::number(fraction * 100.0, 'f', 1) + QStringLiteral("%");
}

} // namespace

auto summarize(const Fixture& fixture,
               std::vector<BattleResult> results) -> FixtureSummary {
  FixtureSummary summary;
  summary.id = fixture.id;
  summary.label = fixture.label;
  summary.battles = static_cast<int>(results.size());
  summary.side_a.label =
      fixture.side_a.label.isEmpty() ? QStringLiteral("A") : fixture.side_a.label;
  summary.side_b.label =
      fixture.side_b.label.isEmpty() ? QStringLiteral("B") : fixture.side_b.label;

  if (results.empty()) {
    return summary;
  }

  int a_wins = 0;
  int b_wins = 0;
  int draws = 0;
  int timeouts = 0;
  int left_wins = 0;
  int decisive = 0;

  std::vector<double> victory_times;
  std::vector<double> contact_distances;
  std::vector<double> survivors_a;
  std::vector<double> survivors_b;
  std::vector<double> fraction_a;
  std::vector<double> fraction_b;
  std::vector<double> ranged_a;
  std::vector<double> ranged_b;
  std::vector<double> melee_a;
  std::vector<double> melee_b;
  std::vector<double> cohesion_a;
  std::vector<double> cohesion_b;

  for (const auto& result : results) {
    switch (result.outcome) {
    case Outcome::SideAWins:
      ++a_wins;
      break;
    case Outcome::SideBWins:
      ++b_wins;
      break;
    case Outcome::MutualElimination:
      ++draws;
      break;
    case Outcome::Timeout:
      ++timeouts;
      break;
    }

    if (result.outcome == Outcome::SideAWins || result.outcome == Outcome::SideBWins) {
      ++decisive;
      victory_times.push_back(result.elapsed_seconds);

      const bool a_on_left = !result.sides_swapped;
      const bool a_won = result.outcome == Outcome::SideAWins;
      if (a_on_left == a_won) {
        ++left_wins;
      }
    }

    if (result.first_contact_distance >= 0.0F) {
      contact_distances.push_back(result.first_contact_distance);
    } else {
      ++summary.battles_without_contact;
    }

    survivors_a.push_back(result.side_a.surviving_units);
    survivors_b.push_back(result.side_b.surviving_units);
    if (result.side_a.starting_units > 0) {
      fraction_a.push_back(static_cast<double>(result.side_a.surviving_units) /
                           result.side_a.starting_units);
    }
    if (result.side_b.starting_units > 0) {
      fraction_b.push_back(static_cast<double>(result.side_b.surviving_units) /
                           result.side_b.starting_units);
    }
    ranged_a.push_back(result.side_a.damage_dealt.ranged);
    ranged_b.push_back(result.side_b.damage_dealt.ranged);
    melee_a.push_back(result.side_a.damage_dealt.melee);
    melee_b.push_back(result.side_b.damage_dealt.melee);
    cohesion_a.push_back(result.side_a.mean_cohesion_radius);
    cohesion_b.push_back(result.side_b.mean_cohesion_radius);

    summary.invalid.friendly_fire_hits += result.invalid.friendly_fire_hits;
    summary.invalid.ranged_shots_while_melee_locked +=
        result.invalid.ranged_shots_while_melee_locked;
    summary.invalid.idle_unit_seconds_in_contact +=
        result.invalid.idle_unit_seconds_in_contact;
  }

  const auto battles = static_cast<double>(results.size());
  summary.a_win_rate = a_wins / battles;
  summary.b_win_rate = b_wins / battles;
  summary.draw_rate = draws / battles;
  summary.timeout_rate = timeouts / battles;
  summary.median_victory_seconds = median(victory_times);
  summary.mean_first_contact_distance = mean(contact_distances);
  summary.left_spawn_win_rate =
      decisive > 0 ? static_cast<double>(left_wins) / decisive : 0.5;
  summary.spawn_side_bias = std::abs(summary.left_spawn_win_rate - 0.5) * 2.0;

  summary.side_a.starting_units = results.front().side_a.starting_units;
  summary.side_b.starting_units = results.front().side_b.starting_units;
  summary.side_a.starting_cost = results.front().side_a.starting_cost;
  summary.side_b.starting_cost = results.front().side_b.starting_cost;
  summary.side_a.median_survivors = median(survivors_a);
  summary.side_b.median_survivors = median(survivors_b);
  summary.side_a.mean_survivor_fraction = mean(fraction_a);
  summary.side_b.mean_survivor_fraction = mean(fraction_b);
  summary.side_a.mean_damage_ranged = mean(ranged_a);
  summary.side_b.mean_damage_ranged = mean(ranged_b);
  summary.side_a.mean_damage_melee = mean(melee_a);
  summary.side_b.mean_damage_melee = mean(melee_b);
  summary.side_a.mean_cohesion_radius = mean(cohesion_a);
  summary.side_b.mean_cohesion_radius = mean(cohesion_b);

  const auto& expect = fixture.expect;
  if (expect.a_win_rate_min && summary.a_win_rate < *expect.a_win_rate_min) {
    summary.expectation_failures.push_back(
        QStringLiteral("side A win rate %1 below minimum %2")
            .arg(percent(summary.a_win_rate), percent(*expect.a_win_rate_min)));
  }
  if (expect.a_win_rate_max && summary.a_win_rate > *expect.a_win_rate_max) {
    summary.expectation_failures.push_back(
        QStringLiteral("side A win rate %1 above maximum %2")
            .arg(percent(summary.a_win_rate), percent(*expect.a_win_rate_max)));
  }
  if (expect.max_timeout_rate && summary.timeout_rate > *expect.max_timeout_rate) {
    summary.expectation_failures.push_back(
        QStringLiteral("timeout rate %1 above maximum %2")
            .arg(percent(summary.timeout_rate), percent(*expect.max_timeout_rate)));
  }
  if (expect.max_spawn_side_bias &&
      summary.spawn_side_bias > *expect.max_spawn_side_bias) {
    summary.expectation_failures.push_back(
        QStringLiteral("spawn-side bias %1 above maximum %2")
            .arg(QString::number(summary.spawn_side_bias, 'f', 3),
                 QString::number(*expect.max_spawn_side_bias, 'f', 3)));
  }

  summary.battle_results = std::move(results);
  return summary;
}

auto render_text_report(const std::vector<FixtureSummary>& summaries) -> QString {
  QString out;
  for (const auto& summary : summaries) {
    out += QStringLiteral("== %1 (%2) ==\n").arg(summary.label, summary.id);
    out += QStringLiteral("  battles           : %1\n").arg(summary.battles);
    out += QStringLiteral("  A %1 [%2 units, %3g]  win %4  median survivors %5\n")
               .arg(summary.side_a.label)
               .arg(summary.side_a.starting_units)
               .arg(summary.side_a.starting_cost)
               .arg(percent(summary.a_win_rate),
                    QString::number(summary.side_a.median_survivors, 'f', 1));
    out += QStringLiteral("  B %1 [%2 units, %3g]  win %4  median survivors %5\n")
               .arg(summary.side_b.label)
               .arg(summary.side_b.starting_units)
               .arg(summary.side_b.starting_cost)
               .arg(percent(summary.b_win_rate),
                    QString::number(summary.side_b.median_survivors, 'f', 1));
    out += QStringLiteral("  draw %1  timeout %2\n")
               .arg(percent(summary.draw_rate), percent(summary.timeout_rate));
    out += QStringLiteral("  median victory    : %1s\n")
               .arg(QString::number(summary.median_victory_seconds, 'f', 1));
    out +=
        QStringLiteral("  first contact     : %1 units (%2 battles never contacted)\n")
            .arg(QString::number(summary.mean_first_contact_distance, 'f', 1))
            .arg(summary.battles_without_contact);
    out += QStringLiteral("  damage A r/m      : %1 / %2\n")
               .arg(QString::number(summary.side_a.mean_damage_ranged, 'f', 0),
                    QString::number(summary.side_a.mean_damage_melee, 'f', 0));
    out += QStringLiteral("  damage B r/m      : %1 / %2\n")
               .arg(QString::number(summary.side_b.mean_damage_ranged, 'f', 0),
                    QString::number(summary.side_b.mean_damage_melee, 'f', 0));
    out += QStringLiteral("  cohesion A/B      : %1 / %2\n")
               .arg(QString::number(summary.side_a.mean_cohesion_radius, 'f', 2),
                    QString::number(summary.side_b.mean_cohesion_radius, 'f', 2));
    out += QStringLiteral("  spawn-side bias   : %1\n")
               .arg(QString::number(summary.spawn_side_bias, 'f', 3));

    if (summary.invalid.friendly_fire_hits > 0 ||
        summary.invalid.ranged_shots_while_melee_locked > 0 ||
        summary.invalid.idle_unit_seconds_in_contact > 0.0) {
      out += QStringLiteral(
                 "  INVALID: friendly-fire %1, ranged-in-melee %2, idle %3 unit-s\n")
                 .arg(summary.invalid.friendly_fire_hits)
                 .arg(summary.invalid.ranged_shots_while_melee_locked)
                 .arg(QString::number(
                     summary.invalid.idle_unit_seconds_in_contact, 'f', 1));
    }

    for (const auto& failure : summary.expectation_failures) {
      out += QStringLiteral("  FAIL: %1\n").arg(failure);
    }
    out += QStringLiteral("\n");
  }
  return out;
}

auto render_json_report(const std::vector<FixtureSummary>& summaries) -> QByteArray {
  QJsonArray fixtures;
  for (const auto& summary : summaries) {
    QJsonObject side_a{
        {"label", summary.side_a.label},
        {"starting_units", summary.side_a.starting_units},
        {"starting_cost", summary.side_a.starting_cost},
        {"median_survivors", summary.side_a.median_survivors},
        {"mean_survivor_fraction", summary.side_a.mean_survivor_fraction},
        {"mean_damage_ranged", summary.side_a.mean_damage_ranged},
        {"mean_damage_melee", summary.side_a.mean_damage_melee},
        {"mean_cohesion_radius", summary.side_a.mean_cohesion_radius},
    };
    QJsonObject side_b{
        {"label", summary.side_b.label},
        {"starting_units", summary.side_b.starting_units},
        {"starting_cost", summary.side_b.starting_cost},
        {"median_survivors", summary.side_b.median_survivors},
        {"mean_survivor_fraction", summary.side_b.mean_survivor_fraction},
        {"mean_damage_ranged", summary.side_b.mean_damage_ranged},
        {"mean_damage_melee", summary.side_b.mean_damage_melee},
        {"mean_cohesion_radius", summary.side_b.mean_cohesion_radius},
    };

    QJsonArray battles;
    for (const auto& battle : summary.battle_results) {
      battles.append(QJsonObject{
          {"seed", static_cast<qint64>(battle.seed)},
          {"sides_swapped", battle.sides_swapped},
          {"outcome", outcome_name(battle.outcome)},
          {"elapsed_seconds", battle.elapsed_seconds},
          {"first_contact_distance", battle.first_contact_distance},
          {"first_contact_time", battle.first_contact_time},
          {"a_surviving_units", battle.side_a.surviving_units},
          {"b_surviving_units", battle.side_b.surviving_units},
      });
    }

    QJsonArray failures;
    for (const auto& failure : summary.expectation_failures) {
      failures.append(failure);
    }

    fixtures.append(QJsonObject{
        {"id", summary.id},
        {"label", summary.label},
        {"battles", summary.battles},
        {"a_win_rate", summary.a_win_rate},
        {"b_win_rate", summary.b_win_rate},
        {"draw_rate", summary.draw_rate},
        {"timeout_rate", summary.timeout_rate},
        {"median_victory_seconds", summary.median_victory_seconds},
        {"mean_first_contact_distance", summary.mean_first_contact_distance},
        {"battles_without_contact", summary.battles_without_contact},
        {"left_spawn_win_rate", summary.left_spawn_win_rate},
        {"spawn_side_bias", summary.spawn_side_bias},
        {"side_a", side_a},
        {"side_b", side_b},
        {"invalid",
         QJsonObject{
             {"friendly_fire_hits",
              static_cast<qint64>(summary.invalid.friendly_fire_hits)},
             {"ranged_shots_while_melee_locked",
              static_cast<qint64>(summary.invalid.ranged_shots_while_melee_locked)},
             {"idle_unit_seconds_in_contact",
              summary.invalid.idle_unit_seconds_in_contact},
         }},
        {"passed", summary.passed()},
        {"expectation_failures", failures},
        {"battle_results", battles},
    });
  }

  return QJsonDocument(QJsonObject{{"fixtures", fixtures}})
      .toJson(QJsonDocument::Indented);
}

auto render_csv_report(const std::vector<FixtureSummary>& summaries) -> QByteArray {
  QString csv = QStringLiteral(
      "fixture,seed,sides_swapped,outcome,elapsed_seconds,first_contact_distance,"
      "a_start_units,a_surviving_units,a_start_cost,b_start_units,b_surviving_units,"
      "b_start_cost,a_damage_ranged,a_damage_melee,b_damage_ranged,b_damage_melee,"
      "a_cohesion,b_cohesion,friendly_fire_hits,ranged_in_melee,idle_unit_seconds\n");

  for (const auto& summary : summaries) {
    for (const auto& battle : summary.battle_results) {
      csv += QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%"
                            "17,%18,%19,%20,%21\n")
                 .arg(summary.id)
                 .arg(battle.seed)
                 .arg(battle.sides_swapped ? 1 : 0)
                 .arg(outcome_name(battle.outcome))
                 .arg(QString::number(battle.elapsed_seconds, 'f', 3))
                 .arg(QString::number(battle.first_contact_distance, 'f', 3))
                 .arg(battle.side_a.starting_units)
                 .arg(battle.side_a.surviving_units)
                 .arg(battle.side_a.starting_cost)
                 .arg(battle.side_b.starting_units)
                 .arg(battle.side_b.surviving_units)
                 .arg(battle.side_b.starting_cost)
                 .arg(QString::number(battle.side_a.damage_dealt.ranged, 'f', 0))
                 .arg(QString::number(battle.side_a.damage_dealt.melee, 'f', 0))
                 .arg(QString::number(battle.side_b.damage_dealt.ranged, 'f', 0))
                 .arg(QString::number(battle.side_b.damage_dealt.melee, 'f', 0))
                 .arg(QString::number(battle.side_a.mean_cohesion_radius, 'f', 3))
                 .arg(QString::number(battle.side_b.mean_cohesion_radius, 'f', 3))
                 .arg(battle.invalid.friendly_fire_hits)
                 .arg(battle.invalid.ranged_shots_while_melee_locked)
                 .arg(QString::number(
                     battle.invalid.idle_unit_seconds_in_contact, 'f', 1));
    }
  }
  return csv.toUtf8();
}

} // namespace Balance
