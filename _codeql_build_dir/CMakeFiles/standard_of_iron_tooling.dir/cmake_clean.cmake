file(REMOVE_RECURSE
  "StandardOfIron/assets/maps/map_forest.json"
  "StandardOfIron/assets/maps/map_mountain.json"
  "StandardOfIron/assets/maps/map_rivers.json"
  "StandardOfIron/assets/shaders/basic.frag"
  "StandardOfIron/assets/shaders/basic.vert"
  "StandardOfIron/assets/shaders/grid.frag"
  "StandardOfIron/assets/visuals/unit_visuals.json"
  "StandardOfIron/translations/app_de.qm"
  "StandardOfIron/translations/app_en.qm"
  "StandardOfIron/ui/qml/BattleSummary.qml"
  "StandardOfIron/ui/qml/GameView.qml"
  "StandardOfIron/ui/qml/HUD.qml"
  "StandardOfIron/ui/qml/HUDBottom.qml"
  "StandardOfIron/ui/qml/HUDTop.qml"
  "StandardOfIron/ui/qml/HUDVictory.qml"
  "StandardOfIron/ui/qml/LoadGamePanel.qml"
  "StandardOfIron/ui/qml/Main.qml"
  "StandardOfIron/ui/qml/MainMenu.qml"
  "StandardOfIron/ui/qml/MapSelect.qml"
  "StandardOfIron/ui/qml/ProductionPanel.qml"
  "StandardOfIron/ui/qml/SaveGamePanel.qml"
  "StandardOfIron/ui/qml/SettingsPanel.qml"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/standard_of_iron_tooling.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
