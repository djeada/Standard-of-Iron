#include <QComboBox>

#include <gtest/gtest.h>
#include <vector>

#include "game/systems/nation_registry.h"
#include "tools/arena/building_panel.h"
#include "tools/arena/unit_panel.h"

namespace {

class ArenaPanelPopulationTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Systems::NationRegistry::instance().clear(); }

  void TearDown() override { Game::Systems::NationRegistry::instance().clear(); }
};

void expect_all_combo_boxes_populated(QWidget& panel) {
  std::vector<QComboBox*> combo_boxes;
  for (QComboBox* combo_box : panel.findChildren<QComboBox*>()) {
    if (combo_box != nullptr) {
      combo_boxes.push_back(combo_box);
    }
  }

  ASSERT_FALSE(combo_boxes.empty());
  for (QComboBox* combo_box : combo_boxes) {
    EXPECT_GT(combo_box->count(), 0);
  }
}

} // namespace

TEST_F(ArenaPanelPopulationTest,
       UnitPanelPopulatesComboBoxesWithoutPreinitializedNationRegistry) {
  UnitPanel panel;

  expect_all_combo_boxes_populated(panel);
  EXPECT_FALSE(panel.selected_nation_id().isEmpty());
  EXPECT_FALSE(panel.selected_unit_type_id().isEmpty());
}

TEST_F(ArenaPanelPopulationTest,
       BuildingPanelPopulatesComboBoxesWithoutPreinitializedNationRegistry) {
  BuildingPanel panel;

  expect_all_combo_boxes_populated(panel);
  EXPECT_FALSE(panel.selected_nation_id().isEmpty());
  EXPECT_FALSE(panel.selected_building_type_id().isEmpty());
}
