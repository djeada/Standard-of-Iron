# Unit Balance Model

This document describes the model used to tune the shipped troop roster. Its purpose is to make balance changes deliberate: adding or editing a unit should be a decision about where that unit belongs in the roster, not a guess. When a stat change breaks an intended counter, automated checks should expose the regression before it becomes a bad match months later.

The authoritative data is split across three places:

- base troop statistics: `assets/data/troops/base.json`;
- per-nation variants: `assets/data/nations/*.json`; and
- combat counter rules: `game/systems/combat_system/combat_types.h`.

The model is exercised by `tools/balance_sim` and gated by `tests/tools/balance_sim_test.cpp`.

## The balance yardstick

A squad is represented as one entity with a pooled health pool and a flat attack value. For comparing combat weight, the project uses a Lanchester-square-law-inspired measure:

```text
power       = sqrt(health × dps)
efficiency  = power / gold cost
```

`dps` is whichever is greater:

```text
melee_damage / melee_cooldown
ranged_damage / ranged_cooldown
```

In an actual fight, effective damage lands near 55% of that theoretical value because attacks are gated by strike animations. That difference matters when estimating time to kill, but it does not materially change relative comparisons between units.

**Efficiency is the value to keep broadly stable within a tier.** Health and damage can be traded against one another at similar efficiency, and that trade is one of the main ways factions acquire a distinct feel.

## Balance tiers

| Tier    | Target efficiency | Units                                         |
| ------- | ----------------- | --------------------------------------------- |
| Line    | 2.0–2.35          | swordsman, spearman, archer                   |
| Mounted | ~2.0–2.1          | horse swordsman, horse spearman, horse archer |
| Elite   | 1.2–1.4           | commanders, elephant (1.95; see below)        |
| Siege   | not comparable    | catapult, ballista                             |
| Support | not comparable    | healer, builder, civilian                     |

### Line troops establish the baseline

Everything else is judged against what the same amount of gold buys in line infantry.

### Elite units trade efficiency for concentrated value

Commanders are deliberately less cost-efficient than line troops. A commander is a much stronger individual body than any single squad, but the same gold spent on line infantry should win the direct comparison. The `commander_vs_line` fixture verifies this: three legionaries defeat one commander while losing a squad in the process.

Elite units justify their cost through effects that raw efficiency does not capture well, including auras, reach, and the fact that squad slots are not the limiting resource.

The elephant is the exception within the elite tier, with an efficiency of 1.95. It is a faction-defining unit with a hard counter and still loses to equal-cost archers roughly half the time.

### Siege engines are measured differently

Siege value comes primarily from range denial: roughly 18–21 range compared with about 7 for an archer. Their weakness is intentional. Anything that reaches a siege engine should overrun it quickly.

That does not mean siege should behave like disposable, free-standing artillery. Its survivability has its own design rule, described below.

## Counter rules

Counters are expressed as multipliers rather than as stat differences so they remain meaningful when base stats are retuned.

| Rule                                | Multiplier |
| ----------------------------------- | ---------- |
| Spearmen vs cavalry                 | 2.5×       |
| Infantry melee vs siege             | 3.0×       |
| Archers vs elephant                 | 1.68×      |
| Archers / spearmen from high ground | 1.8×       |

Hold mode adds damage and health bonuses on top of these values. As a result, a braced spear line should defeat a frontal cavalry charge outright, while spearmen caught in the open should not perform as strongly.

The intended battlefield relationships are:

- swords beat spears in a straight infantry fight, while spears repay their cost against cavalry;
- cavalry loses to a braced spear line but overruns archers that lack a melee screen;
- archers out-trade other troops at range and lose decisively once contacted;
- infantry destroys unescorted siege within seconds; and
- archers are the intended elephant counter, although using them for that purpose remains expensive.

## Siege survivability

Siege engines previously sat at 130–150 health while also taking a 5× infantry-melee counter. That combination made them effectively one-shot targets. At the same time, a contact bug made ballistas impossible to melee at all, so they could paradoxically be both too fragile and unreachable.

The two problems were corrected together. Siege health moved to 360–420 so that a stray volley does not erase an engine, while the infantry-melee counter dropped to 3×. Infantry still overruns siege in a few seconds, but contact no longer means an instantaneous kill.

The design rule is concise:

> A siege engine should survive being noticed, but it should not survive being reached.

## Changing a troop stat

When changing balance data:

1. Edit the relevant JSON.
2. Build and run the balance simulator:

   ```sh
   cmake --build build --target balance_sim && ./build/bin/balance_sim
   ```

3. Confirm that every fixture still meets its expectations and the command exits with status `0`.
4. If a fixture's expectation is now wrong rather than the troop statistics, update the expectation in `assets/balance/*.json` and explain the reason in its `description`.

When adding a new unit, also add a fixture that compares it with equal-cost line infantry. A unit's tier should be asserted by the balance suite rather than assumed from its intended role.

## Lore is part of the balance surface

Each troop in `assets/data/troops/base.json` contains a `lore` block. Its `strengths` and `weaknesses` are written from the same counter relationships documented above, and the in-game unit panel displays them verbatim. See the “Unit details” section of `UI_DESIGN_SYSTEM.md`.

That makes the prose part of the balance contract. If a counter is retuned, the sentence teaching that counter can become incorrect even when the code is working as intended.

`TroopCatalogLoader.ShippedTroopsCarryTheLoreTheInspectPanelDraws` verifies that the text exists, but it cannot verify that the explanation is still strategically true. Maintaining that relationship is the responsibility of the person changing the balance data.

A new unit therefore needs all three of the following:

- tuned statistics;
- a balance fixture; and
- a `lore` block whose English text is translated in all five catalogues before `translations-check` can pass.

Keeping numerical behavior, automated fixtures, and player-facing explanations aligned is what makes the balance model durable rather than merely descriptive.
