file(REMOVE_RECURSE
  "StandardOfIron/assets/maps/test_map.txt"
  "StandardOfIron/assets/shaders/basic.frag"
  "StandardOfIron/assets/shaders/basic.vert"
  "StandardOfIron/ui/qml/GameView.qml"
  "StandardOfIron/ui/qml/HUD.qml"
  "StandardOfIron/ui/qml/Main.qml"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/standard_of_iron_tooling.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
