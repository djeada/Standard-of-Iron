#pragma once

#include <QString>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Game::Systems::AI {

enum class DoctrineTarget : std::uint8_t {
  Army,
  Barracks,
  Economy,
  Commander,
  Any,
};

enum class DoctrineArm : std::uint8_t {
  Infantry,
  Missile,
  Cavalry,
  Siege,
};

struct DoctrineRecruitment {

  float ranged_share = 0.5F;

  float cavalry_share = 0.0F;

  float siege_share = 0.0F;

  std::vector<std::string> preferred;
};

struct DoctrineWave {

  int size = 6;

  float regroup_seconds = 25.0F;

  float spent_fraction = 0.35F;
  std::vector<DoctrineTarget> target_priority{DoctrineTarget::Army,
                                              DoctrineTarget::Barracks,
                                              DoctrineTarget::Economy,
                                              DoctrineTarget::Any};
};

struct DoctrineGarrison {

  int minimum_units = 2;

  float fraction = 0.25F;
};

struct TownPlanStep {
  std::string building;
  float x = 0.0F;
  float z = 0.0F;

  float rotation = 0.0F;
};

struct TownPlan {
  std::string id;
  std::string display_name;
  std::vector<TownPlanStep> steps;

  [[nodiscard]] auto wall_step_count() const -> int;

  [[nodiscard]] auto step_count(std::string_view building) const -> int;

  [[nodiscard]] auto engine_step_count() const -> int;
};

struct AIDoctrine {
  std::string id;
  std::string strategy = "balanced";
  std::string posture = "field";
  float aggression = 0.5F;
  float defense = 0.5F;
  float harassment = 0.5F;
  const TownPlan* town_plan = nullptr;

  std::string formation;
  DoctrineRecruitment recruitment;
  DoctrineWave wave;
  DoctrineGarrison garrison;
};

auto load_default_ai_doctrine_catalog() -> bool;

void ensure_ai_doctrine_catalog_loaded();

auto load_ai_doctrine_catalog(const QString& doctrines_path,
                              const QString& town_plans_path) -> bool;

void reset_ai_doctrine_catalog();

[[nodiscard]] auto
authored_doctrine(std::string_view commander_id) -> const AIDoctrine*;

[[nodiscard]] auto authored_town_plan(std::string_view plan_id) -> const TownPlan*;

[[nodiscard]] auto ai_doctrine_catalog_loaded() -> bool;

} // namespace Game::Systems::AI
