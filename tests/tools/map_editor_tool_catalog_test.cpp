#include <QSet>

#include <gtest/gtest.h>

#include "game/units/spawn_type.h"
#include "tools/map_editor/tool_catalog.h"

namespace {

using MapEditor::ToolPlacement;
using MapEditor::ToolType;

TEST(MapEditorToolCatalogTest, EveryToolHasExactlyOneEntry) {
  QSet<int> seen;
  for (const auto& spec : MapEditor::tool_catalog()) {
    EXPECT_FALSE(seen.contains(static_cast<int>(spec.tool)))
        << "duplicate catalog entry for " << spec.name;
    seen.insert(static_cast<int>(spec.tool));
  }
  for (int value = static_cast<int>(ToolType::Select);
       value <= static_cast<int>(ToolType::Eraser);
       ++value) {
    EXPECT_TRUE(seen.contains(value))
        << "ToolType " << value << " has no catalog entry";
  }
}

TEST(MapEditorToolCatalogTest, EveryToolHasANameDescriptionAndSection) {
  QSet<int> listed_sections;
  for (const auto section : MapEditor::tool_sections()) {
    listed_sections.insert(static_cast<int>(section));
    EXPECT_FALSE(MapEditor::section_title(section).isEmpty());
  }
  for (const auto& spec : MapEditor::tool_catalog()) {
    EXPECT_FALSE(MapEditor::tool_name(spec.tool).isEmpty());
    EXPECT_FALSE(MapEditor::tool_card_label(spec.tool).isEmpty());
    EXPECT_FALSE(MapEditor::tool_description(spec.tool).isEmpty()) << spec.name;
    EXPECT_TRUE(listed_sections.contains(static_cast<int>(spec.section)))
        << spec.name << " sits in a section the panel never shows";
  }
}

TEST(MapEditorToolCatalogTest, StatusLabelNamesEveryTool) {
  EXPECT_EQ(MapEditor::tool_status_label(ToolType::Forest), QStringLiteral("Forest"));
  EXPECT_EQ(MapEditor::tool_status_label(ToolType::WildlifeWolves),
            QStringLiteral("Wolf range"));
  EXPECT_EQ(MapEditor::tool_status_label(ToolType::River),
            QStringLiteral("River (click start, then end)"));
  EXPECT_EQ(MapEditor::tool_status_label(ToolType::TroopCarthageSwordCommander),
            QStringLiteral("Hannibal Barca"));
}

TEST(MapEditorToolCatalogTest, PlacingToolsNameAnElementTypeTheGameAccepts) {
  for (const auto& spec : MapEditor::tool_catalog()) {
    const QString type = MapEditor::element_type_for_tool(spec.tool);
    switch (spec.placement) {
    case ToolPlacement::Troop: {
      Game::Units::SpawnType parsed{};
      ASSERT_TRUE(Game::Units::try_parse_spawn_type(type, parsed)) << spec.name;
      EXPECT_TRUE(Game::Units::is_troop_spawn(parsed)) << spec.name;
      break;
    }
    case ToolPlacement::Structure: {
      if (type == QStringLiteral("village")) {
        break;
      }
      Game::Units::SpawnType parsed{};
      ASSERT_TRUE(Game::Units::try_parse_spawn_type(type, parsed)) << spec.name;
      EXPECT_TRUE(Game::Units::is_building_spawn(parsed)) << spec.name;
      break;
    }
    case ToolPlacement::Terrain:
    case ToolPlacement::WorldProp:
    case ToolPlacement::Wildlife:
    case ToolPlacement::Linear:
    case ToolPlacement::Gate:
      EXPECT_FALSE(type.isEmpty()) << spec.name;
      break;
    case ToolPlacement::None:
    case ToolPlacement::Erase:
    case ToolPlacement::Forest:
    case ToolPlacement::UndeadZone:
      break;
    }
  }
}

TEST(MapEditorToolCatalogTest, FilterMatchesNameTypeAndSection) {
  const auto& wolves = MapEditor::tool_spec(ToolType::WildlifeWolves);
  EXPECT_TRUE(MapEditor::tool_matches_filter(wolves, QStringLiteral("")));
  EXPECT_TRUE(MapEditor::tool_matches_filter(wolves, QStringLiteral("WOLF")));
  EXPECT_TRUE(MapEditor::tool_matches_filter(wolves, QStringLiteral("wildlife")));
  EXPECT_FALSE(MapEditor::tool_matches_filter(wolves, QStringLiteral("barracks")));

  const auto& tower = MapEditor::tool_spec(ToolType::DefenseTower);
  EXPECT_TRUE(MapEditor::tool_matches_filter(tower, QStringLiteral("defense_tower")));
}

} // namespace
