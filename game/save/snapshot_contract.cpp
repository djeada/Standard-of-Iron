#include "snapshot_contract.h"

#include <array>
#include <cstring>

namespace Game::Save {

namespace {

using enum FieldClass;

constexpr std::array k_fields = std::to_array<FieldSpec>({

    {"session.world",
     AuthoritativeSerialized,
     "The entities themselves; everything else is described relative to them."},
    {"session.terrain",
     AuthoritativeSerialized,
     "Height field and authored props can be edited mid-match, so the map file "
     "is not a substitute."},
    {"session.owners",
     AuthoritativeSerialized,
     "Who is playing, their teams and colours; capture changes it during a match."},
    {"session.economy",
     AuthoritativeSerialized,
     "Held and harvested resources per owner."},
    {"session.nations",
     AuthoritativeSerialized,
     "Player-to-nation assignment. The nation catalog itself is content, not "
     "match state, and is reloaded from disk."},
    {"session.stats",
     AuthoritativeSerialized,
     "Kills, losses and start time feed the end-of-match screen and cannot be "
     "recomputed from a finished world."},
    {"session.clock",
     AuthoritativeSerialized,
     "Tick count is the match's time base; play time and any replay seek "
     "depend on restoring it exactly."},
    {"session.rng",
     AuthoritativeSerialized,
     "Seed plus draw count. Without both, a restored match diverges from the "
     "one that was saved."},
    {"session.visibility",
     AuthoritativeSerialized,
     "Explored terrain is player knowledge; recomputing it would hand back "
     "fog the player had already cleared."},
    {"session.troop_counts",
     DerivedRebuilt,
     "A tally of live units; rebuilt by walking the restored world."},
    {"session.building_collision",
     DerivedRebuilt,
     "Footprint index rebuilt from the restored buildings."},
    {"session.commands",
     DerivedRebuilt,
     "Orders not yet executed are dropped on save; a restored match starts "
     "with an empty queue rather than replaying stale intent."},

    {"campaign.campaign_id", CampaignLevel, "Which campaign the save belongs to."},
    {"campaign.mission_id", CampaignLevel, "Which mission within the campaign."},
    {"campaign.difficulty", CampaignLevel, "Chosen difficulty."},
    {"campaign.play_time_seconds",
     CampaignLevel,
     "Shown in the load menu, so it lives in the header and is readable "
     "without decompressing a world."},
    {"campaign.screenshot", CampaignLevel, "Slot preview image."},

    {"presentation.camera",
     PresentationOnly,
     "Restored as a convenience, but a wrong camera costs the player nothing."},
    {"presentation.cursor_mode", PresentationOnly, "UI mode of the pointer."},
    {"presentation.follow_selection", PresentationOnly, "Camera follow toggle."},
    {"presentation.selected_player_id",
     PresentationOnly,
     "Which player the spectator UI is looking at."},

    {"TransformComponent", AuthoritativeSerialized, "Position and orientation."},
    {"RenderableComponent",
     AuthoritativeSerialized,
     "Names the visual asset; the choice is authored, not derived."},
    {"UnitComponent", AuthoritativeSerialized, "Health, owner, type."},
    {"MovementComponent", AuthoritativeSerialized, "Path and destination."},
    {"MovementFactsComponent",
     DerivedRebuilt,
     "One tick of movement stages: desired, steered, accepted, progress. "
     "Republished every Movement phase from the route and the world."},
    {"PlayerOrderIntentComponent",
     AuthoritativeSerialized,
     "Distinguishes a player order from an automatic reaction."},
    {"AttackComponent", AuthoritativeSerialized, "Cooldowns and melee lock."},
    {"AttackTargetComponent",
     AuthoritativeSerialized,
     "Current target and chase flag."},
    {"CombatStateComponent",
     AuthoritativeSerialized,
     "Combat phase, timers and the swing being executed."},
    {"CommanderBodyControlComponent",
     DerivedRebuilt,
     "Live melee steering: the aim travel going into the swing and where the "
     "last one left the blade. Rebuilt from the first tick of direct control, "
     "and a save cannot be taken mid-input anyway."},
    {"HitFeedbackComponent",
     AuthoritativeSerialized,
     "Trauma drives both camera shake and stagger, so it is simulation state."},
    {"PatrolComponent", AuthoritativeSerialized, "Waypoints and progress."},
    {"BuildingComponent", AuthoritativeSerialized, "Marks an entity as a building."},
    {"ProductionComponent", AuthoritativeSerialized, "Queue, timer and rally point."},
    {"MoraleComponent", AuthoritativeSerialized, "Current morale."},
    {"UndeadComponent", AuthoritativeSerialized, "Undead state."},
    {"CursedStatusComponent", AuthoritativeSerialized, "Curse timers."},
    {"BurningStatusComponent", AuthoritativeSerialized, "Burn timers."},
    {"CaptureComponent", AuthoritativeSerialized, "Capture progress."},
    {"HomeComponent", AuthoritativeSerialized, "Manpower and villager state."},
    {"FarmComponent",
     AuthoritativeSerialized,
     "How far the crop has grown; a restored farm must not ripen for free."},
    {"HealerComponent", AuthoritativeSerialized, "Healing cooldowns."},
    {"CommanderComponent", AuthoritativeSerialized, "Commander identity and aura."},
    {"CommanderGuardComponent", AuthoritativeSerialized, "Guard assignment."},
    {"GuardModeComponent", AuthoritativeSerialized, "Guard anchor and toggle."},
    {"HoldModeComponent", AuthoritativeSerialized, "Hold toggle and exit cooldown."},
    {"FormationModeComponent", AuthoritativeSerialized, "Formation slot assignment."},
    {"ArmyFormationMembershipComponent",
     AuthoritativeSerialized,
     "Which army-formation group owns this unit, and which slot it holds."},
    {"UnitLayoutStateComponent",
     AuthoritativeSerialized,
     "Internal soldier layout, transition phase and progress."},
    {"StaminaComponent", AuthoritativeSerialized, "Stamina pool and run request."},
    {"SpecialAttackComponent", AuthoritativeSerialized, "Special ability cooldowns."},
    {"CatapultLoadingComponent", AuthoritativeSerialized, "Reload progress."},
    {"ElephantComponent", AuthoritativeSerialized, "Elephant state."},
    {"ElephantPanicComponent", AuthoritativeSerialized, "Panic timer."},
    {"ElephantStompImpactComponent", AuthoritativeSerialized, "Pending stomp impact."},
    {"FirePatchComponent", AuthoritativeSerialized, "Burning ground."},
    {"StructureFireComponent",
     AuthoritativeSerialized,
     "Ignition progress and burn timers for a structure set alight by "
     "incendiary damage."},
    {"CivilianDeliveryComponent", AuthoritativeSerialized, "Delivery assignment."},
    {"ResourceCarryComponent",
     AuthoritativeSerialized,
     "Gathered goods a worker is still hauling. Dropping it on save would either "
     "delete the load or hand it to the player for free."},
    {"SettlementResidentComponent",
     AuthoritativeSerialized,
     "Which settlement a villager belongs to and the errand they are on; "
     "recomputing it would teleport the town's daily life back to its start."},
    {"WildlifeComponent",
     AuthoritativeSerialized,
     "Species, behaviour, herd or pack membership and the anchor an animal roams "
     "around; rebuilding it would scatter every herd on load."},
    {"BuilderProductionComponent", AuthoritativeSerialized, "Construction queue."},
    {"WallConstructionSiteComponent", AuthoritativeSerialized, "Unbuilt wall segment."},
    {"DismantleSiteComponent",
     AuthoritativeSerialized,
     "How far a builder crew has got taking a building down; rebuilding it would "
     "hand the work back to the player for free."},
    {"WallSegmentComponent", AuthoritativeSerialized, "Built wall segment."},
    {"GateComponent", AuthoritativeSerialized, "Gate leaf position and manual mode."},
    {"TerrainContextComponent",
     AuthoritativeSerialized,
     "Cached terrain sample the movement system depends on being consistent."},
    {"RpgHealthComponent", AuthoritativeSerialized, "Commander health in FPV."},
    {"AIControlledComponent", AuthoritativeSerialized, "Marks an AI-driven unit."},

    {"MovementIntentComponent",
     DerivedRebuilt,
     "Recomputed each tick from the movement order."},
    {"CohortMembershipComponent",
     DerivedRebuilt,
     "The cohort system regroups units after load."},
    {"EngagementSlotComponent",
     DerivedRebuilt,
     "Slot assignment around a target, recomputed when combat resumes."},
    {"TargetCommitmentComponent",
     DerivedRebuilt,
     "Re-established by the targeting system."},
    {"FormationContactComponent",
     DerivedRebuilt,
     "Contact set recomputed from positions."},
    {"RpgEngagementComponent",
     DerivedRebuilt,
     "Rebuilt when the commander re-enters combat."},
    {"CommanderAuraBuffComponent",
     DerivedRebuilt,
     "Recomputed every tick from nearby commanders."},

    {"MotionPresentationComponent",
     PresentationOnly,
     "Interpolation state for the renderer, refilled on the first frame."},
    {"CreaturePresentationComponent",
     PresentationOnly,
     "Animation snapshot published for the renderer."},
    {"FormationPresentationComponent", PresentationOnly, "Formation banner state."},
    {"FormationRosterPresentationComponent",
     PresentationOnly,
     "Stable visual soldier identities and survivor assignment."},
    {"FormationHitPresentationComponent",
     PresentationOnly,
     "Per-soldier visual hit reaction selected from a unit damage event."},
    {"DeathAnimationComponent", PresentationOnly, "Death animation progress."},
    {"SoldierCasualtyAnimationComponent", PresentationOnly, "Casualty animation."},
    {"BloodStainComponent",
     PresentationOnly,
     "Ground decal left after a death; purely cosmetic."},
    {"StructureDamagePresentationComponent",
     PresentationOnly,
     "Short-lived facade chip, dust, and impact cues."},
    {"RpgContactPresentationComponent",
     PresentationOnly,
     "Short-lived exact weapon, block, and dodge contact cues."},
    {"ShowcaseRoutineComponent",
     PresentationOnly,
     "Scripted pose loop for the humanoid showcase; never part of a battle."},
    {"CommanderSignaturePresentationComponent",
     PresentationOnly,
     "Short-lived burst marking where a commander's signature move landed."},
    {"StockpileComponent",
     PresentationOnly,
     "Pile heights drawn on the barracks stone yard, resampled each tick from "
     "the owner's resources."},
    {"AssaultWaveComponent",
     AuthoritativeSerialized,
     "Marks a scripted assault wave. The AI keeps these units attacking whatever "
     "its posture is, so a save that lost the flag would turn a live wave "
     "passive."},
    {"ShowcaseRoutineComponent",
     PresentationOnly,
     "Scripted animation routine the arena showcase attaches to a posed "
     "creature. No mission ever creates one."},
    {"ConstructionPreviewComponent",
     PresentationOnly,
     "Placement ghost. Entities carrying it are skipped entirely when a world "
     "is written, so a save never contains a half-placed building."},

    {"PendingRemovalComponent",
     DerivedRebuilt,
     "Marks an entity the cleanup system removes this tick."},
    {"StaggerComponent", DerivedRebuilt, "Stagger reaction resolved within the tick."},
    {"SpearBraceComponent", DerivedRebuilt, "Brace resolved within the tick."},
    {"MountedChargeComponent", DerivedRebuilt, "Charge resolved within the tick."},
    {"ElephantKnockbackCooldownComponent",
     DerivedRebuilt,
     "Knockback cooldown re-established on contact."},
    {"RpgCommanderTargetComponent",
     DerivedRebuilt,
     "FPV aim state, re-acquired when direct control resumes."},
    {"RpgCommanderActionComponent",
     DerivedRebuilt,
     "FPV action in flight, re-issued by input."},
    {"CombatIntentQueueComponent",
     DerivedRebuilt,
     "The last couple of presses the player made and why the previous one was "
     "refused. Input in flight: a restored match starts from whatever the "
     "player presses next, not from an intention they had before saving."},
    {"RpgCommanderAimComponent",
     DerivedRebuilt,
     "Where the FPV commander is looking and how far the bow is drawn. Both are "
     "written from live input every frame, and the weapon stance is re-derived "
     "from the commander's own weapons when direct control resumes."},
});

} // namespace

auto field_class_name(FieldClass classification) -> const char* {
  switch (classification) {
  case AuthoritativeSerialized:
    return "authoritative-serialized";
  case DerivedRebuilt:
    return "derived-rebuilt";
  case PresentationOnly:
    return "presentation-only";
  case CampaignLevel:
    return "campaign-level";
  }
  return "unknown";
}

auto fields() -> std::span<const FieldSpec> {
  return {k_fields.data(), k_fields.size()};
}

auto find(const char* name) -> const FieldSpec* {
  if (name == nullptr) {
    return nullptr;
  }
  for (const auto& spec : k_fields) {
    if (std::strcmp(spec.name, name) == 0) {
      return &spec;
    }
  }
  return nullptr;
}

} // namespace Game::Save
