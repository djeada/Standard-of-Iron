#include "troop_catalog.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <utility>

namespace Game::Units {

namespace {

struct TroopDefaults {
  Game::Units::TroopType unit_type;
  const char* display_name;

  int cost;
  int wood;
  int stone;
  int iron;
  float build_time;
  int priority;
  bool is_melee;

  int health;
  float speed;
  float vision_range;
  float ranged_range;
  int ranged_damage;
  float ranged_cooldown;
  float melee_range;
  int melee_damage;
  float melee_cooldown;
  bool can_ranged;
  bool can_melee;

  float render_scale;
  float selection_ring_size;
  float selection_ring_ground_offset;
  float formation_spacing;
  const char* renderer_id;

  int individuals_per_unit;
  int max_units_per_row;
};

constexpr std::array<TroopDefaults, 23> k_troop_defaults{{
    {.unit_type = TroopType::Archer,
     .display_name = QT_TRANSLATE_NOOP("Units", "Archer"),
     .cost = 50,
     .wood = 6,
     .stone = 0,
     .iron = 0,
     .build_time = 5.0F,
     .priority = 10,
     .is_melee = false,
     .health = 410,
     .speed = 3.0F,
     .vision_range = 16.0F,
     .ranged_range = 7.5F,
     .ranged_damage = 25,
     .ranged_cooldown = 1.0F,
     .melee_range = 1.5F,
     .melee_damage = 6,
     .melee_cooldown = 0.8F,
     .can_ranged = true,
     .can_melee = true,
     .render_scale = 0.5F,
     .selection_ring_size = 1.2F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/archer",
     .individuals_per_unit = 20,
     .max_units_per_row = 5},
    {.unit_type = TroopType::Swordsman,
     .display_name = QT_TRANSLATE_NOOP("Units", "Swordsman"),
     .cost = 90,
     .wood = 10,
     .stone = 0,
     .iron = 4,
     .build_time = 7.0F,
     .priority = 10,
     .is_melee = true,
     .health = 1000,
     .speed = 2.1F,
     .vision_range = 14.0F,
     .ranged_range = 1.6F,
     .ranged_damage = 6,
     .ranged_cooldown = 1.9F,
     .melee_range = 1.6F,
     .melee_damage = 26,
     .melee_cooldown = 0.62F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.6F,
     .selection_ring_size = 1.1F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 1.05F,
     .renderer_id = "troops/roman/swordsman",
     .individuals_per_unit = 15,
     .max_units_per_row = 5},
    {.unit_type = TroopType::Spearman,
     .display_name = QT_TRANSLATE_NOOP("Units", "Spearman"),
     .cost = 75,
     .wood = 8,
     .stone = 0,
     .iron = 2,
     .build_time = 6.0F,
     .priority = 5,
     .is_melee = true,
     .health = 900,
     .speed = 2.5F,
     .vision_range = 15.0F,
     .ranged_range = 2.5F,
     .ranged_damage = 8,
     .ranged_cooldown = 1.5F,
     .melee_range = 2.5F,
     .melee_damage = 19,
     .melee_cooldown = 0.75F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.55F,
     .selection_ring_size = 1.4F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 1.05F,
     .renderer_id = "troops/roman/spearman",
     .individuals_per_unit = 24,
     .max_units_per_row = 6},
    {.unit_type = TroopType::MountedKnight,
     .display_name = QT_TRANSLATE_NOOP("Units", "Mounted Knight"),
     .cost = 145,
     .wood = 16,
     .stone = 0,
     .iron = 10,
     .build_time = 10.0F,
     .priority = 15,
     .is_melee = true,
     .health = 1800,
     .speed = 4.0F,
     .vision_range = 16.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 5,
     .ranged_cooldown = 2.0F,
     .melee_range = 2.0F,
     .melee_damage = 37,
     .melee_cooldown = 0.72F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.8F,
     .selection_ring_size = 2.0F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/horse_swordsman",
     .individuals_per_unit = 9,
     .max_units_per_row = 3},
    {.unit_type = TroopType::HorseArcher,
     .display_name = QT_TRANSLATE_NOOP("Units", "Horse Archer"),
     .cost = 120,
     .wood = 18,
     .stone = 0,
     .iron = 8,
     .build_time = 9.0F,
     .priority = 12,
     .is_melee = false,
     .health = 1500,
     .speed = 4.0F,
     .vision_range = 18.0F,
     .ranged_range = 8.5F,
     .ranged_damage = 42,
     .ranged_cooldown = 1.1F,
     .melee_range = 1.8F,
     .melee_damage = 9,
     .melee_cooldown = 0.9F,
     .can_ranged = true,
     .can_melee = true,
     .render_scale = 0.75F,
     .selection_ring_size = 1.8F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/horse_archer",
     .individuals_per_unit = 10,
     .max_units_per_row = 3},
    {.unit_type = TroopType::HorseSpearman,
     .display_name = QT_TRANSLATE_NOOP("Units", "Horse Spearman"),
     .cost = 135,
     .wood = 18,
     .stone = 0,
     .iron = 9,
     .build_time = 9.5F,
     .priority = 13,
     .is_melee = true,
     .health = 1700,
     .speed = 4.0F,
     .vision_range = 16.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 5,
     .ranged_cooldown = 2.0F,
     .melee_range = 3.0F,
     .melee_damage = 36,
     .melee_cooldown = 0.8F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.78F,
     .selection_ring_size = 1.9F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/horse_spearman",
     .individuals_per_unit = 9,
     .max_units_per_row = 3},
    {.unit_type = TroopType::Healer,
     .display_name = QT_TRANSLATE_NOOP("Units", "Healer"),
     .cost = 75,
     .wood = 6,
     .stone = 2,
     .iron = 0,
     .build_time = 7.0F,
     .priority = 8,
     .is_melee = false,
     .health = 380,
     .speed = 2.5F,
     .vision_range = 9.5F,
     .ranged_range = 8.0F,
     .ranged_damage = 14,
     .ranged_cooldown = 1.5F,
     .melee_range = 1.5F,
     .melee_damage = 1,
     .melee_cooldown = 1.5F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.55F,
     .selection_ring_size = 1.2F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/healer",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::SkeletonSwordsman,
     .display_name = QT_TRANSLATE_NOOP("Units", "Skeleton Swordsman"),
     .cost = 0,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 0.0F,
     .priority = 10,
     .is_melee = true,
     .health = 950,
     .speed = 2.2F,
     .vision_range = 14.5F,
     .ranged_range = 1.5F,
     .ranged_damage = 1,
     .ranged_cooldown = 1.8F,
     .melee_range = 1.6F,
     .melee_damage = 24,
     .melee_cooldown = 0.75F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.56F,
     .selection_ring_size = 1.05F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/iron_sepulcher/skeleton_swordsman",
     .individuals_per_unit = 18,
     .max_units_per_row = 6},
    {.unit_type = TroopType::SkeletonArcher,
     .display_name = QT_TRANSLATE_NOOP("Units", "Skeleton Archer"),
     .cost = 0,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 0.0F,
     .priority = 8,
     .is_melee = false,
     .health = 420,
     .speed = 2.3F,
     .vision_range = 17.2F,
     .ranged_range = 7.8F,
     .ranged_damage = 26,
     .ranged_cooldown = 0.95F,
     .melee_range = 1.35F,
     .melee_damage = 4,
     .melee_cooldown = 1.1F,
     .can_ranged = true,
     .can_melee = true,
     .render_scale = 0.52F,
     .selection_ring_size = 1.05F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/iron_sepulcher/skeleton_archer",
     .individuals_per_unit = 18,
     .max_units_per_row = 6},
    {.unit_type = TroopType::GravePriest,
     .display_name = QT_TRANSLATE_NOOP("Units", "Grave Priest"),
     .cost = 0,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 0.0F,
     .priority = 20,
     .is_melee = false,
     .health = 900,
     .speed = 2.0F,
     .vision_range = 16.0F,
     .ranged_range = 8.6F,
     .ranged_damage = 30,
     .ranged_cooldown = 2.3F,
     .melee_range = 1.25F,
     .melee_damage = 3,
     .melee_cooldown = 1.6F,
     .can_ranged = true,
     .can_melee = true,
     .render_scale = 0.6F,
     .selection_ring_size = 0.7F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/iron_sepulcher/grave_priest",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::Catapult,
     .display_name = QT_TRANSLATE_NOOP("Units", "Catapult"),
     .cost = 250,
     .wood = 90,
     .stone = 0,
     .iron = 35,
     .build_time = 15.0F,
     .priority = 5,
     .is_melee = false,
     .health = 420,
     .speed = 1.0F,
     .vision_range = 20.0F,
     .ranged_range = 18.0F,
     .ranged_damage = 150,
     .ranged_cooldown = 4.5F,
     .melee_range = 1.5F,
     .melee_damage = 1,
     .melee_cooldown = 2.0F,
     .can_ranged = true,
     .can_melee = false,
     .render_scale = 1.2F,
     .selection_ring_size = 1.25F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/catapult",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::Ballista,
     .display_name = QT_TRANSLATE_NOOP("Units", "Ballista"),
     .cost = 200,
     .wood = 75,
     .stone = 0,
     .iron = 45,
     .build_time = 12.0F,
     .priority = 6,
     .is_melee = false,
     .health = 360,
     .speed = 1.5F,
     .vision_range = 22.0F,
     .ranged_range = 21.0F,
     .ranged_damage = 60,
     .ranged_cooldown = 2.6F,
     .melee_range = 1.5F,
     .melee_damage = 1,
     .melee_cooldown = 2.0F,
     .can_ranged = true,
     .can_melee = false,
     .render_scale = 1.0F,
     .selection_ring_size = 1.0F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/ballista",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::Elephant,
     .display_name = QT_TRANSLATE_NOOP("Units", "War Elephant"),
     .cost = 350,
     .wood = 24,
     .stone = 0,
     .iron = 16,
     .build_time = 20.0F,
     .priority = 2,
     .is_melee = true,
     .health = 6500,
     .speed = 2.2F,
     .vision_range = 16.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 0,
     .ranged_cooldown = 2.0F,
     .melee_range = 3.5F,
     .melee_damage = 105,
     .melee_cooldown = 1.55F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 2.0F,
     .selection_ring_size = 1.5F,
     .selection_ring_ground_offset = 0.6F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/carthage/elephant",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::RomanLegionOrganizer,
     .display_name = QT_TRANSLATE_NOOP("Units", "Quintus Fabius Maximus"),
     .cost = 340,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 30.0F,
     .priority = 20,
     .is_melee = true,
     .health = 3500,
     .speed = 2.0F,
     .vision_range = 18.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 4,
     .ranged_cooldown = 2.0F,
     .melee_range = 2.4F,
     .melee_damage = 58,
     .melee_cooldown = 0.9F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.756F,
     .selection_ring_size = 1.9F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/commanders/fabius_maximus",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::RomanVeteranConsul,
     .display_name = QT_TRANSLATE_NOOP("Units", "Publius Cornelius Scipio"),
     .cost = 360,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 31.0F,
     .priority = 20,
     .is_melee = true,
     .health = 3300,
     .speed = 2.15F,
     .vision_range = 18.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 4,
     .ranged_cooldown = 2.0F,
     .melee_range = 1.8F,
     .melee_damage = 66,
     .melee_cooldown = 0.85F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.756F,
     .selection_ring_size = 1.85F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/commanders/scipio_africanus",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::RomanFieldCommander,
     .display_name = QT_TRANSLATE_NOOP("Units", "Marcus Claudius Marcellus"),
     .cost = 320,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 27.0F,
     .priority = 20,
     .is_melee = false,
     .health = 2800,
     .speed = 2.35F,
     .vision_range = 18.0F,
     .ranged_range = 12.0F,
     .ranged_damage = 80,
     .ranged_cooldown = 1.35F,
     .melee_range = 1.8F,
     .melee_damage = 30,
     .melee_cooldown = 1.0F,
     .can_ranged = true,
     .can_melee = true,
     .render_scale = 0.756F,
     .selection_ring_size = 1.8F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/commanders/marcellus",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::CarthageSpearCommander,
     .display_name = QT_TRANSLATE_NOOP("Units", "Hanno the Great"),
     .cost = 350,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 30.0F,
     .priority = 20,
     .is_melee = true,
     .health = 3800,
     .speed = 2.1F,
     .vision_range = 18.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 4,
     .ranged_cooldown = 2.0F,
     .melee_range = 2.4F,
     .melee_damage = 62,
     .melee_cooldown = 0.9F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.756F,
     .selection_ring_size = 1.82F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/carthage/commanders/hanno_the_great",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::CarthageBowCommander,
     .display_name = QT_TRANSLATE_NOOP("Units", "Hasdrubal Barca"),
     .cost = 330,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 28.0F,
     .priority = 20,
     .is_melee = false,
     .health = 3200,
     .speed = 2.45F,
     .vision_range = 18.0F,
     .ranged_range = 12.0F,
     .ranged_damage = 78,
     .ranged_cooldown = 1.35F,
     .melee_range = 1.8F,
     .melee_damage = 30,
     .melee_cooldown = 1.0F,
     .can_ranged = true,
     .can_melee = true,
     .render_scale = 0.756F,
     .selection_ring_size = 1.88F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/carthage/commanders/hasdrubal_barca",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::CarthageSwordCommander,
     .display_name = QT_TRANSLATE_NOOP("Units", "Hannibal Barca"),
     .cost = 430,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 34.0F,
     .priority = 20,
     .is_melee = true,
     .health = 4200,
     .speed = 2.0F,
     .vision_range = 18.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 4,
     .ranged_cooldown = 2.0F,
     .melee_range = 1.8F,
     .melee_damage = 66,
     .melee_cooldown = 0.9F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.756F,
     .selection_ring_size = 1.95F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/carthage/commanders/hannibal_barca",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::Builder,
     .display_name = QT_TRANSLATE_NOOP("Units", "Builder"),
     .cost = 60,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 6.0F,
     .priority = 4,
     .is_melee = true,
     .health = 720,
     .speed = 2.0F,
     .vision_range = 10.0F,
     .ranged_range = 1.5F,
     .ranged_damage = 2,
     .ranged_cooldown = 2.0F,
     .melee_range = 1.5F,
     .melee_damage = 5,
     .melee_cooldown = 1.0F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.5F,
     .selection_ring_size = 1.0F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/builder",
     .individuals_per_unit = 12,
     .max_units_per_row = 4},
    {.unit_type = TroopType::Civilian,
     .display_name = QT_TRANSLATE_NOOP("Units", "Civilian"),
     .cost = 8,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 5.0F,
     .priority = 1,
     .is_melee = true,
     .health = 35,
     .speed = 2.3F,
     .vision_range = 10.0F,
     .ranged_range = 1.0F,
     .ranged_damage = 1,
     .ranged_cooldown = 2.0F,
     .melee_range = 1.2F,
     .melee_damage = 2,
     .melee_cooldown = 1.2F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 0.48F,
     .selection_ring_size = 0.95F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "troops/roman/civilian",
     .individuals_per_unit = 1,
     .max_units_per_row = 5},
    {.unit_type = TroopType::Sheep,
     .display_name = QT_TRANSLATE_NOOP("Units", "Sheep"),
     .cost = 0,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 0.0F,
     .priority = 0,
     .is_melee = false,
     .health = 28,
     .speed = 1.5F,
     .vision_range = 0.0F,
     .ranged_range = 0.0F,
     .ranged_damage = 0,
     .ranged_cooldown = 0.0F,
     .melee_range = 0.0F,
     .melee_damage = 0,
     .melee_cooldown = 0.0F,
     .can_ranged = false,
     .can_melee = false,
     .render_scale = 1.0F,
     .selection_ring_size = 0.85F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "wildlife/sheep",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
    {.unit_type = TroopType::Wolf,
     .display_name = QT_TRANSLATE_NOOP("Units", "Wolf"),
     .cost = 0,
     .wood = 0,
     .stone = 0,
     .iron = 0,
     .build_time = 0.0F,
     .priority = 0,
     .is_melee = true,
     .health = 55,
     .speed = 3.1F,
     .vision_range = 14.0F,
     .ranged_range = 0.0F,
     .ranged_damage = 0,
     .ranged_cooldown = 0.0F,
     .melee_range = 1.2F,
     .melee_damage = 7,
     .melee_cooldown = 1.1F,
     .can_ranged = false,
     .can_melee = true,
     .render_scale = 1.0F,
     .selection_ring_size = 0.95F,
     .selection_ring_ground_offset = 0.0F,
     .formation_spacing = 0.75F,
     .renderer_id = "wildlife/wolf",
     .individuals_per_unit = 1,
     .max_units_per_row = 1},
}};

[[nodiscard]] auto to_troop_class(const TroopDefaults& defaults) -> TroopClass {
  using Game::Systems::ResourceType;

  TroopClass troop_class{};
  troop_class.unit_type = defaults.unit_type;
  troop_class.display_name = defaults.display_name;

  troop_class.production.cost = defaults.cost;
  troop_class.production.build_time = defaults.build_time;
  troop_class.production.priority = defaults.priority;
  troop_class.production.is_melee = defaults.is_melee;
  troop_class.production.resource_costs.set(ResourceType::Wood, defaults.wood);
  troop_class.production.resource_costs.set(ResourceType::Stone, defaults.stone);
  troop_class.production.resource_costs.set(ResourceType::Iron, defaults.iron);

  troop_class.combat.health = defaults.health;
  troop_class.combat.max_health = defaults.health;
  troop_class.combat.speed = defaults.speed;
  troop_class.combat.vision_range = defaults.vision_range;
  troop_class.combat.ranged_range = defaults.ranged_range;
  troop_class.combat.ranged_damage = defaults.ranged_damage;
  troop_class.combat.ranged_cooldown = defaults.ranged_cooldown;
  troop_class.combat.melee_range = defaults.melee_range;
  troop_class.combat.melee_damage = defaults.melee_damage;
  troop_class.combat.melee_cooldown = defaults.melee_cooldown;
  troop_class.combat.can_ranged = defaults.can_ranged;
  troop_class.combat.can_melee = defaults.can_melee;

  troop_class.visuals.render_scale = defaults.render_scale;
  troop_class.visuals.selection_ring_size = defaults.selection_ring_size;
  troop_class.visuals.selection_ring_ground_offset =
      defaults.selection_ring_ground_offset;
  troop_class.visuals.formation_spacing = defaults.formation_spacing;
  troop_class.visuals.renderer_id = defaults.renderer_id;

  troop_class.individuals_per_unit = defaults.individuals_per_unit;
  troop_class.max_units_per_row = defaults.max_units_per_row;
  return troop_class;
}

} // namespace

auto TroopCatalog::instance() -> TroopCatalog& {
  static TroopCatalog inst;
  return inst;
}

TroopCatalog::TroopCatalog() {
  register_defaults();
}

void TroopCatalog::register_class(TroopClass troop_class) {
  m_classes[troop_class.unit_type] = std::move(troop_class);
}

void TroopCatalog::reset_to_defaults() {
  m_classes.clear();
  m_abilities.clear();
  register_defaults();
}

void TroopCatalog::register_ability(AbilityDefinition ability) {
  for (auto& existing : m_abilities) {
    if (existing.id == ability.id) {
      existing = std::move(ability);
      return;
    }
  }
  m_abilities.push_back(std::move(ability));
}

auto TroopCatalog::get_ability(const std::string& ability_id) const
    -> const AbilityDefinition* {
  for (const auto& ability : m_abilities) {
    if (ability.id == ability_id) {
      return &ability;
    }
  }
  return nullptr;
}

auto TroopCatalog::get_class(Game::Units::TroopType type) const -> const TroopClass* {
  auto it = m_classes.find(type);
  if (it != m_classes.end()) {
    return &it->second;
  }
  return nullptr;
}

auto TroopCatalog::get_class_or_fallback(Game::Units::TroopType type) const
    -> const TroopClass& {
  auto it = m_classes.find(type);
  if (it != m_classes.end()) {
    return it->second;
  }
  return m_fallback;
}

void TroopCatalog::clear() {
  m_classes.clear();
  m_abilities.clear();
}

void TroopCatalog::register_defaults() {
  m_fallback.display_name = QT_TRANSLATE_NOOP("Units", "Unknown Troop");
  m_fallback.visuals.renderer_id = "troops/unknown";

  for (const auto& defaults : k_troop_defaults) {
    register_class(to_troop_class(defaults));
  }
}

} // namespace Game::Units
