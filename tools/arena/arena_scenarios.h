#pragma once

#include <QString>

#include <array>

namespace Arena::Scenarios {

inline constexpr char k_sword_duel_id[] = "sword_duel";
inline constexpr char k_spear_duel_id[] = "spear_duel";
inline constexpr char k_bow_exchange_id[] = "bow_exchange";
inline constexpr char k_mounted_charge_id[] = "mounted_charge";
inline constexpr char k_melee_lock_id[] = "melee_lock";
inline constexpr char k_chase_to_attack_id[] = "chase_to_attack";
inline constexpr char k_attack_to_chase_id[] = "attack_to_chase";
inline constexpr char k_target_death_id[] = "target_death";
inline constexpr char k_retargeting_id[] = "retargeting";
inline constexpr char k_hold_guard_exit_id[] = "hold_guard_exit";
inline constexpr char k_lod_switch_id[] = "lod_switch";

struct ScenarioOption {
  QString id;
  QString label;
  QString description;
};

inline auto options() -> const std::array<ScenarioOption, 11>& {
  static const std::array<ScenarioOption, 11> k_options = {{
      {QString::fromLatin1(k_sword_duel_id),
       QStringLiteral("Sword Duel"),
       QStringLiteral("Single-soldier melee duel for baseline sword attack flow.")},
      {QString::fromLatin1(k_spear_duel_id),
       QStringLiteral("Spear Duel"),
       QStringLiteral(
           "Single-soldier spear engagement for thrust timing and recovery.")},
      {QString::fromLatin1(k_bow_exchange_id),
       QStringLiteral("Bow Exchange"),
       QStringLiteral(
           "Explicit ranged attack targets for bow draw, release, and reload.")},
      {QString::fromLatin1(k_mounted_charge_id),
       QStringLiteral("Mounted Charge"),
       QStringLiteral("Mounted melee approach with cavalry-to-cavalry contact.")},
      {QString::fromLatin1(k_melee_lock_id),
       QStringLiteral("Melee Lock"),
       QStringLiteral(
           "Reciprocal melee-lock pair to inspect lock-specific attack ownership.")},
      {QString::fromLatin1(k_chase_to_attack_id),
       QStringLiteral("Chase To Attack"),
       QStringLiteral(
           "Attacker starts out of range and must chase into a melee swing.")},
      {QString::fromLatin1(k_attack_to_chase_id),
       QStringLiteral("Attack To Chase"),
       QStringLiteral(
           "Target disengages after contact so the attacker drops back into chase.")},
      {QString::fromLatin1(k_target_death_id),
       QStringLiteral("Target Death"),
       QStringLiteral(
           "Low-health target dies through the normal damage path during combat.")},
      {QString::fromLatin1(k_retargeting_id),
       QStringLiteral("Retargeting"),
       QStringLiteral(
           "Attacker kills one target, then reacquires a second nearby enemy.")},
      {QString::fromLatin1(k_hold_guard_exit_id),
       QStringLiteral("Hold / Guard Exit"),
       QStringLiteral("Hold and guard units exit their stance under approaching combat "
                      "pressure.")},
      {QString::fromLatin1(k_lod_switch_id),
       QStringLiteral("LOD Switch"),
       QStringLiteral(
           "Dense melee scene with dynamic creature LOD enabled and camera sweep.")},
  }};
  return k_options;
}

inline auto find_option(const QString& scenario_id) -> const ScenarioOption* {
  for (const ScenarioOption& option : options()) {
    if (option.id == scenario_id) {
      return &option;
    }
  }
  return nullptr;
}

} // namespace Arena::Scenarios
