# Unit balance

This is the model the shipped troop stats are tuned against. It exists so that
adding or editing a unit is a decision about where it sits in the roster rather
than a guess, and so that a stat change that breaks a designed counter shows up
as a failing test instead of a bad match three months later.

Numbers live in `assets/data/troops/base.json` (defaults) and
`assets/data/nations/*.json` (per-nation variants). Counters live in
`game/systems/combat_system/combat_types.h`. Everything is verified by
`tools/balance_sim` and gated by `tests/tools/balance_sim_test.cpp`.

## The yardstick

A squad is one entity with a pooled health pool and a flat attack, so its combat
weight is captured well by the Lanchester square law:

```
power       = sqrt(health x dps)
efficiency  = power / gold cost
```

`dps` is the unit's better of `melee_damage / melee_cooldown` and
`ranged_damage / ranged_cooldown`. Effective damage in a real fight lands near
55% of that, because attacks are gated by strike animations — that gap matters
for time-to-kill, not for comparing units to each other.

Efficiency is the number to hold steady. Health and damage can be traded freely
against each other at constant efficiency, and that trade is what gives a faction
its feel.

## Tiers

| Tier    | Target efficiency | Units                                         |
| ------- | ----------------- | --------------------------------------------- |
| Line    | 2.0 – 2.35        | swordsman, spearman, archer                   |
| Mounted | ~2.0 – 2.1        | horse swordsman, horse spearman, horse archer |
| Elite   | 1.2 – 1.4         | commanders, elephant (1.95, see below)        |
| Siege   | not comparable    | catapult, ballista                            |
| Support | not comparable    | healer, builder, civilian                     |

**Line troops set the baseline.** Everything else is judged against what the same
gold buys in line infantry.

**Elite units are deliberately less cost-efficient than the line.** A commander
is a much stronger single body than any squad, but equal gold spent on line
infantry beats it — verified by the `commander_vs_line` fixture, where three
legionaries beat one commander while losing a squad doing it. Elites earn their
place through auras, reach and the fact that a squad slot is not the constraint.
The elephant sits higher (1.95) because it is a faction-defining unit with a hard
counter; it still loses to equal-cost archers about half the time.

**Siege engines are not measured by this yardstick.** Their value is range denial
(18–21 range against a 7-ish archer), and their weakness is deliberate: they are
overrun by anything that reaches them. They are not, however, allowed to be
free-standing artillery — see the siege note below.

## Counters

These are multipliers, not stat differences, so they survive future retuning:

| Rule                                | Multiplier |
| ----------------------------------- | ---------- |
| Spearmen vs cavalry                 | 2.5x       |
| Infantry melee vs siege             | 3.0x       |
| Archers vs elephant                 | 1.68x      |
| Archers / spearmen from high ground | 1.8x       |

Hold mode adds its own damage and health bonuses on top, which is why a braced
spear line beats a frontal charge outright while spearmen that get caught in the
open do not.

The intended shape:

- Swords beat spears in a straight infantry fight; spears repay their cost
  against cavalry.
- Cavalry loses to a braced spear line, and overruns archers with no melee
  screen.
- Archers out-trade everything at range and lose hard once contacted.
- Infantry overruns unescorted siege in seconds.
- Archers are the elephant answer, but a costly one.

## Siege survivability

Siege engines used to sit at 130–150 health with a 5x infantry melee counter on
top. That combination made them one-shot targets, and combined with a contact bug
it made them _unreachable_ instead — a ballista could not be meleed at all. Both
were fixed together: health moved to 360–420 so a stray volley does not delete
them, and the melee counter dropped to 3x so infantry still overruns them in a
few seconds without the result being an instant kill.

The rule of thumb: a siege engine should survive being noticed, and should not
survive being reached.

## Changing a stat

1. Edit the JSON.
2. `cmake --build build --target balance_sim && ./build/bin/balance_sim`
3. Every fixture must still meet its expectations (exit status `0`).
4. If a fixture's expectation is now wrong rather than the stats, change the
   expectation in `assets/balance/*.json` and say why in its `description`.

Adding a new unit means adding a fixture that places it against equal-cost line
infantry, so its tier is asserted rather than assumed.
