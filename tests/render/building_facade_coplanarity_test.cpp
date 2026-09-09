#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_geometry_audit.h"
#include "render/entity/building_state.h"

namespace {

using Render::GL::BuildingState;

constexpr std::array<BuildingState, 3> k_states{
    {BuildingState::Normal, BuildingState::Damaged, BuildingState::Destroyed}};

auto audit_everything(bool include_benign) -> std::vector<Render::GL::FacadeConflict> {
  std::vector<Render::GL::FacadeConflict> all;
  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    for (const BuildingState state : k_states) {
      const auto desc = entry.build(state);
      auto result = Render::GL::audit_building_desc(desc, state);
      all.insert(all.end(), result.conflicts.begin(), result.conflicts.end());
      if (include_benign) {
        all.insert(all.end(), result.benign.begin(), result.benign.end());
      }
    }
  }
  return all;
}

} // namespace

TEST(BuildingFacadeCoplanarity, NoArchetypeHasTiedVisibleFaces) {
  const auto conflicts = audit_everything(false);

  if (!conflicts.empty()) {
    std::ostringstream report;
    report << conflicts.size() << " coplanar building surfaces:\n";
    for (const auto& conflict : conflicts) {
      report << Render::GL::describe_facade_conflict(conflict) << "\n";
    }
    ADD_FAILURE() << report.str();
  }
}

TEST(BuildingFacadeCoplanarity, CarthageHomeDoorwayIsLayered) {
  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    if (entry.name != "carthage_home") {
      continue;
    }
    for (const BuildingState state : k_states) {
      const auto result = Render::GL::audit_building_desc(entry.build(state), state);
      EXPECT_TRUE(result.conflicts.empty())
          << "carthage_home still ties facade surfaces";
    }
    return;
  }
  ADD_FAILURE() << "carthage_home missing from the building archetype catalog";
}

TEST(BuildingFacadeCoplanarity, Report) {
  if (std::getenv("SOI_BUILDING_AUDIT_REPORT") == nullptr) {
    GTEST_SKIP() << "set SOI_BUILDING_AUDIT_REPORT=1 to print the audit";
  }

  for (const auto& entry : Render::GL::building_archetype_catalog()) {
    for (const BuildingState state : k_states) {
      const auto result = Render::GL::audit_building_desc(entry.build(state), state);
      for (const auto& conflict : result.conflicts) {
        std::cout << "TIE " << Render::GL::describe_facade_conflict(conflict) << "\n";
      }
      for (const auto& part : result.buried) {
        std::cout << "BURIED " << Render::GL::describe_buried_part(part) << "\n";
      }
      std::cout << "SUMMARY " << entry.name << " state=" << static_cast<int>(state)
                << " parts=" << result.analyzed_parts
                << " skipped=" << result.skipped_parts
                << " ties=" << result.conflicts.size()
                << " benign=" << result.benign.size()
                << " buried=" << result.buried.size() << "\n";
    }
  }
}

TEST(BuildingFacadeCoplanarity, CatalogCoversEveryProceduralArchetype) {
  const auto& catalog = Render::GL::building_archetype_catalog();
  EXPECT_GE(catalog.size(), 34U);

  std::size_t analyzed = 0;
  for (const auto& entry : catalog) {
    const auto result = Render::GL::audit_building_desc(
        entry.build(BuildingState::Normal), BuildingState::Normal);
    analyzed += result.analyzed_parts;
    EXPECT_GT(result.analyzed_parts, 0U) << entry.name << " contributed no geometry";
  }
  EXPECT_GT(analyzed, 1000U);
}
