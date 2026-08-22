# Standard of Iron — RPG Combat UX Issues

## 1. Attacks Are Commands Instead of Direct Combat Input

**Problem:** Player combat ultimately becomes an `RpgCommanderActionComponent` containing an action ID, melee sequence number, target ID, action duration, normalized action time, hit events, and a weapon-trace-active flag. The player is effectively requesting an attack action and then waiting for that action to execute.

**Player feels:**
“I pressed attack, now the character is doing something.”

Rather than:

“I am controlling the attack.”

**Do:** Make direct combat input produce continuous combat intent—aim direction, windup direction, strike direction, weapon velocity, guard direction—while keeping action states underneath only for synchronization and rules.

---

## 2. Melee Range Is Abstract Instead of Matching the Weapon

**Problem:** Melee validity is fundamentally based on `AttackComponent::melee_range`. RPG enemy positioning even calculates an ideal engagement distance from that numeric reach plus the commander's combat radius.
**Player feels:**
“My sword clearly touched him but nothing happened.”

or:

“He hit me even though the weapon didn't really reach me.”

**Do:** Make the weapon sweep authoritative for melee contact. Use blade/shaft traces through the actual animated weapon trajectory, with the numeric melee range used only for targeting/chase decisions.

---

## 3. Targeting Is Entity-Centric Instead of Swing-Centric

**Problem:** RPG targeting stores an explicit lock target, aim candidate, recent-hit target and even a soldier slot within a formation. An active action also holds an `active_target_id` and soldier slot.

**Player feels:**
“The game decided who I was attacking.”

This becomes especially obvious in crowds where the sword visually moves through several enemies but combat logic is strongly organized around target identities.

**Do:** Separate:

- **soft target/lock** for camera and aim assistance;
- **actual hit determination** from the weapon's swept volume.

Lock-on should help the player aim. It should not determine what the blade physically hit.

---

## 4. Only One Buffered Attack State Exists

**Problem:** Both RPG action state and the normal combat state use an `input_buffered` boolean. The state can tell that another input exists, but that representation cannot preserve a sequence of several distinct inputs or their timing/direction.

**Player feels:**
“I clicked three times and I don't know which input it accepted.”

Rapid input can conceptually collapse into:

```text
no input
↓
buffered = true
```

rather than retaining meaningful combat intentions.

**Do:** Replace the boolean with a very small timestamped action-intent buffer, for example 2–3 entries containing action type, direction, press/release timing and target/aim information.

---

## 5. Cancel Windows Control Responsiveness

**Problem:** The RPG action state explicitly tracks `cancel_window_active`, `action_running`, `action_completed`, and `input_buffered`.

This means responsiveness is heavily dependent on predetermined points where the current action permits another action.

**Player feels:**
“My character isn't responding.”

Even when the input technically has been received, the game can be waiting for an authored action state to reach the appropriate transition.

**Do:** Define interruption rules around physical phases:

- windup → freely redirectable;
- early strike → limited redirect;
- committed strike → locked briefly;
- follow-through → movement/guard/dodge recovery available progressively.

Do not make responsiveness depend primarily on animation-completion boundaries.

---

## 6. Combat Has Too Many Independent Timing Layers

**Problem:** You currently have normal attack cooldowns, melee cooldowns, combat-animation durations, RPG `action_duration`, normalized RPG action time, stamina costs, ability cooldowns, dodge state, guard state and hit-pause state. The normal combat animation alone has Advance, WindUp, Strike, Impact, Recover and Reposition phases.

**Player feels:**
“Why can't I attack right now?”

There can be several technically valid answers, but the player experiences only one thing: ignored input.

**Do:** Create one authoritative combat-action timeline and derive animation, stamina, recovery and damage windows from it. Avoid separate timer systems representing roughly the same attack.

---

## 7. Cooldown Is Driving Animation Timing

**Problem:** `CombatStateComponent::cooldown_fit_scale()` scales the combat animation cycle to fit the attack cooldown. The complete cycle contains Advance, WindUp, Strike, Impact, Recover and Reposition.

**Player feels:**
“The attack feels slow/floaty for no physical reason.”

A gameplay balance number is influencing the apparent physical motion of the attack.

**Do:** Separate:

```text
physical swing duration
```

from:

```text
when another full-power attack may begin
```

Cooldown should not simply stretch the entire body animation.

---

## 8. Stamina Can Produce Silent Weak Attacks

**Problem:** Combat defines light/heavy attack costs and a low-stamina damage penalty of `0.7`, with low stamina beginning below 20.

**Player feels:**
“Why did that identical-looking sword hit suddenly do much less damage?”

If animation and impact remain similar while damage changes substantially, the rule is difficult to read.

**Do:** Either prevent the attack when stamina is insufficient or visibly change the resulting attack: slower acceleration, weaker follow-through, different sound, reduced hit reaction and clear stamina feedback.

---

## 9. RPG Health and RTS Health Are Two Sources of Truth

**Problem:** The commander can have both `UnitComponent::health` and separate `RpgHealthComponent::rpg_hp`. During RPG combat, `tick_rpg_combat()` explicitly forces normal unit health back to `1` if RPG HP is still positive.
That is a major architectural warning sign.

**Player feels:**
Damage, death, health bars or systems interacting with the commander can potentially disagree about whether the commander is nearly dead, alive or eligible for normal RTS effects.

**Do:** Have one authoritative health resource. RPG rules can modify how damage is resolved against that health, but should not maintain a second independent life total.

---

## 10. RPG Combat Is Layered on Top of RTS Combat Instead of Replacing It Cleanly

**Problem:** The commander participates in normal `AttackComponent`, `CombatStateComponent`, `UnitComponent` combat state while also gaining RPG health, RPG targeting, RPG action state, commander guard, stamina, engagement and enemy telegraph components.

**Player feels:**
Inconsistent rules depending on which subsystem happened to resolve a situation.

**Do:** Establish a single combat kernel:

```text
Combatant
├─ health
├─ stamina/posture
├─ weapon
├─ target assistance
├─ action state
├─ hit contacts
└─ damage response
```

RTS AI and direct-player control should produce different **intent**, not use fundamentally competing combat-state implementations.

---

## 11. Enemy Combat Is Artificially Choreographed Around the Player

**Problem:** RPG engagement permits three privileged roles:

- front attacker;
- left threat;
- right threat.

Everybody else becomes support.

`refresh_commander_engagement()` explicitly picks those three based on angular sectors around the player.

**Player feels:**
Enemies behave like actors waiting for their turn rather than soldiers trying to kill them.

**Do:** Keep crowd-pressure management, but make it probabilistic and spatial rather than explicit theatrical slots. Let multiple enemies threaten while attack commitment, collision, weapon reach, stamina and friendly obstruction naturally limit simultaneous strikes.

---

## 12. Non-Attacking Enemies Literally Circle on a Fixed Ring

**Problem:** Support enemies are moved around the player on a hardcoded `4.5`-unit support ring at a hardcoded angular speed of `1.4`.

**Player feels:**
“Why are these enemies orbiting me?”

It immediately exposes the combat director.

**Do:** Replace the ring with local tactical positions generated from navigation/collision space:

- seek open flank;
- maintain weapon distance;
- avoid friendlies;
- exploit exposed side;
- retreat/recover;
- advance when another enemy disengages.

There should be no visible geometric circle around the player.

---

## 13. Active Attackers Are Snapped Toward an Ideal Range

**Problem:** Active attackers are scripted toward `ideal_engage_distance`, and reposition whenever they are more than +0.5 or -0.3 away from it.

**Player feels:**
Enemies slide into predetermined duel positions rather than naturally entering striking distance.

**Do:** Use steering forces and attack commitment. An enemy should accelerate toward a strike, overshoot occasionally, collide, reposition and recover naturally instead of maintaining a prescribed ring distance.

---

## 14. Enemy Facing Is Directly Forced Toward the Commander

**Problem:** Active and support enemies both have their transform yaw explicitly set to face the commander during RPG engagement.

**Player feels:**
Enemies pivot unnaturally and remain perfectly oriented toward the player.

**Do:** Give enemies angular acceleration/turn speed. Their attack should depend on whether their body and weapon actually achieve a viable orientation.

---

## 15. Telegraphs Are Timer Objects Rather Than Attack Motion

**Problem:** Enemy telegraphs have fixed phases—WindUp, Active and Recovery—with default durations of 0.45, 0.20 and 0.35 seconds.

**Player feels:**
“I need to learn the game's timing rather than read the enemy.”

**Do:** Make the enemy's weapon/body animation itself the primary telegraph. Internal phase timers can remain, but active hit frames should derive from motion markers/weapon trajectory.

---

## 16. The Telegraph Turns Off Exactly When the Attack Becomes Active

**Problem:** During `WindUp`, `visual_tell_active` is true. When the telegraph transitions into `Active`, the RPG processor immediately sets `visual_tell_active = false`.

**Player feels:**
The warning disappears at precisely the moment danger becomes real.

That can work when the physical weapon animation clearly communicates impact, but with the current stiff character motion it makes readability worse.

**Do:** Transition the visual cue rather than simply disabling it:

```text
windup warning
     ↓
attack flash / weapon trail
     ↓
impact cue
     ↓
recovery readability
```

---

## 17. Attack Direction Has Only Five Discrete Values

**Problem:** Combat exposes only:

- LeftSlash
- RightSlash
- Overhead
- Thrust
- HeavyOverhead

**Player feels:**
Every fight rapidly becomes repetitive and predictable.

**Do:** Keep these as animation/pose families, but feed them continuous direction, elevation and trajectory parameters.

---

## 18. Combo State Is Just a Number

**Problem:** Direct commander state stores `combo_step`, and RPG actions separately store `melee_attack_sequence`.
**Player feels:**
Combo attacks feel like “attack 1 → attack 2 → attack 3,” rather than attacks naturally chaining from where the previous weapon ended.

**Do:** Choose the next attack from current weapon pose + requested direction + movement state. A combo should emerge from continuity of motion.

---

## 19. Hit Reaction Movement Is Extremely Small

**Problem:** Generic `HitFeedbackComponent` defines maximum knockback around `0.15`, while RPG stagger handling applies values around `0.12–0.18` for knockback reactions.
**Player feels:**
Weapons lack weight. Enemies absorb hits rather than physically reacting.

**Do:** Separate visual/body displacement from authoritative navigation position. Allow substantially stronger procedural torso recoil, foot displacement, rotation, stumble and weapon deflection without necessarily launching the actual simulation entity.

---

## 20. Hit Feedback Is Primarily Post-Hit State

**Problem:** `HitFeedbackComponent` records reaction intensity, knockback direction and stagger tier after a hit occurs. RPG contact presentation similarly stores short-lived contact entries.

**Player feels:**
Feedback can communicate “the game says a hit happened” rather than making the collision itself visually obvious.

**Do:** Build feedback around the actual contact point:

- weapon trail interruption;
- spark/blood position;
- sound at contact;
- victim reaction originating from impact direction;
- brief attacker resistance/hit-stop;
- camera impulse proportional to impact;
- weapon deflection.

---

## 21. Dodge Invulnerability Is a Boolean State

**Problem:** `RpgHealthComponent` contains `dodge_invincible`, making dodge immunity fundamentally a state flag rather than necessarily depending on the actual body avoiding the weapon.

**Player feels:**
A sword can visually pass through the commander without damage because the game considers them invulnerable—or visually miss while another timing rule decides otherwise.

**Do:** Make dodge primarily positional. Keep a small forgiveness window if desired, but minimize invisible invulnerability and align it tightly with the actual evasive motion.

---

## 22. Guard Is a Rule Check More Than Physical Weapon Interaction

**Problem:** Commander guard stores `active`, a frontal arc threshold, damage multiplier, perfect-guard timing and guard-break timing.

**Player feels:**
Blocking can feel like “I had guard enabled when damage arrived,” rather than the enemy weapon actually contacting the shield/sword.

**Do:** Use shield/weapon collision volumes to determine successful guard, with the frontal arc retained only as a broad validation/fallback rule.

---

## 23. The System Has No Rich Representation of Why an Input Failed

**Problem:** The visible action state mostly exposes whether an action is active, running, completed, buffered or inside a cancel window, alongside stamina/cooldown values. There is no equivalent rich combat rejection state explaining things such as “recovering”, “not enough stamina”, “target invalid”, “outside strike angle”, etc.

**Player feels:**
“I pressed the button and nothing happened.”

**Do:** Every rejected combat intent should produce a reason internally, even if most never become text:

```text
Accepted
Buffered
InsufficientStamina
Recovering
GuardBroken
InvalidWeaponState
NoAmmo
Interrupted
```

Presentation can then give the appropriate tiny audio/animation/UI response.

---

## 24. Player and Enemy Combat Are Built Around State Machines Instead of Contact

**Problem:** The architecture places enormous emphasis on:

```text
target
→ action selection
→ timed phase
→ hit window
→ damage
→ feedback
```

rather than:

```text
player/enemy motion
→ weapon trajectory
→ physical contact
→ damage response
```

The repository explicitly describes commander combat as using “authored melee actions,” and the underlying components reflect that design.

**Player feels:**
Combat is game-state manipulation wearing melee animations.

**Do:** Invert the system so that motion/contact becomes authoritative and state machines support it.

---

# Highest-Priority UX Fixes

1. **Make actual weapon sweeps determine melee hits.**
2. **Replace discrete player attacks with continuous directional melee intent.**
3. **Unify RPG and RTS health/combat state instead of maintaining two combat truths.**
4. **Replace the three-attacker + 4.5-unit enemy ring choreography.**
5. **Make attack timing derive from physical animation/contact rather than cooldown stretching.**
6. **Replace the one-bit input buffer with a small intent queue.**
7. **Make dodge and guard visually/physically correspond to what the enemy weapon actually does.**
8. **Strengthen contact feedback and body reaction substantially.**

## Core Diagnosis

The current flow is approximately:

```text
player input
    ↓
select attack/action
    ↓
action ID / combo step / target
    ↓
timed action state
    ↓
allowed hit window
    ↓
target/range validation
    ↓
damage
    ↓
hit reaction / VFX
```

The combat system players expect from direct-control melee is closer to:

```text
continuous player input
    ↓
body + weapon intent
    ↓
actual weapon motion
    ↓
swept collision/contact
    ↓
block / dodge / armor interaction
    ↓
damage + physical response
    ↓
animation reacts to the result
```

That inversion is the largest improvement you can make. Right now the **animation and timers tell combat what happened**. For convincing RPG melee, **the player's motion and weapon contact should tell the combat system what happened**.
