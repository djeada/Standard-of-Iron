#pragma once

#include <QVector3D>
#include <optional>
#include <string>

struct HorseSwordsmanStyleConfig {
    std::optional<QVector3D> cloth_color;
    std::optional<QVector3D> leather_color;
    std::optional<QVector3D> leather_dark_color;
    std::optional<QVector3D> metal_color;
    std::optional<QVector3D> wood_color;
    std::optional<QVector3D> cape_color;
    std::string shader_id;
    bool show_helmet = true;
    bool show_armor = true;
    bool show_shoulder_decor = false;
    bool show_cape = false;
    bool has_cavalry_shield = false;
};

void register_kingdom_horse_swordsman_style();
