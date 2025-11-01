#include "horse_swordsman_style.h"
#include <unordered_map>

static std::unordered_map<std::string, HorseSwordsmanStyleConfig> styles;

void register_carthage_horse_swordsman_style() {
    // Example: register a default style
    HorseSwordsmanStyleConfig default_style;
    default_style.cloth_color = QVector3D(0.8F, 0.7F, 0.6F);
    default_style.leather_color = QVector3D(0.5F, 0.3F, 0.2F);
    default_style.metal_color = QVector3D(0.7F, 0.7F, 0.8F);
    default_style.shader_id = "horse_swordsman_carthage";
    default_style.show_helmet = true;
    default_style.show_armor = true;
    default_style.has_cavalry_shield = true;
    styles["default"] = default_style;
}
