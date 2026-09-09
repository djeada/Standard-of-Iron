#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "building_archetype_desc.h"
#include "building_state.h"

namespace Render::GL {

struct FacadeConflict {
  std::string archetype;
  BuildingState state{BuildingState::Normal};
  std::size_t part_a{0};
  std::size_t part_b{0};
  std::string name_a;
  std::string name_b;
  QVector3D min_a;
  QVector3D max_a;
  QVector3D min_b;
  QVector3D max_b;
  int axis{2};
  int outward{1};
  float plane{0.0F};
  float separation{0.0F};
  std::array<float, 2> overlap_u{};
  std::array<float, 2> overlap_v{};
  float overlap_area{0.0F};

  float exposure{1.0F};
  bool same_color{false};
};

struct BuriedPart {
  std::string archetype;
  BuildingState state{BuildingState::Normal};
  std::size_t part{0};
  std::string name;
};

struct BuildingGeometryAuditConfig {

  float min_separation{0.002F};

  float min_overlap{0.020F};
  float min_overlap_area{0.0016F};

  float min_exposure{0.12F};

  float buried_exposure{0.02F};

  float ground_y{0.0F};
};

struct BuildingGeometryAuditResult {

  std::vector<FacadeConflict> conflicts;

  std::vector<FacadeConflict> benign;
  std::vector<BuriedPart> buried;
  std::size_t analyzed_parts{0};

  std::size_t skipped_parts{0};
};

auto audit_building_desc(const BuildingArchetypeDesc& desc,
                         BuildingState state,
                         const BuildingGeometryAuditConfig& config = {})
    -> BuildingGeometryAuditResult;

auto describe_facade_conflict(const FacadeConflict& conflict) -> std::string;
auto describe_buried_part(const BuriedPart& part) -> std::string;

} // namespace Render::GL
