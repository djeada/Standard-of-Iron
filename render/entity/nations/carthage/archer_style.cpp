#include "render/entity/nations/carthage/archer_style.h"

#include <array>

#include "render/equipment/material_palette.h"
#include "render/equipment/sepulcher/palette.h"

namespace Render::GL::Carthage {

void register_carthage_archer_style() {
  ArcherStyleConfig carthage;
  Nation::apply_materials(carthage, Nation::k_carthage_materials);
  carthage.wood_color = Nation::k_carthage_materials.timber;
  carthage.fletching_color = QVector3D(0.90F, 0.82F, 0.28F);
  carthage.bow_string_color = QVector3D(0.32F, 0.30F, 0.26F);
  carthage.cape_color = Nation::k_carthage_materials.cloth;
  carthage.show_helmet = true;
  carthage.show_armor = true;
  carthage.show_shoulder_decor = false;
  carthage.show_cape = true;
  carthage.force_beard = true;
  carthage.attachment_profile.clear();
  carthage.armor_id = "armor_light_carthage";

  ArcherStyleConfig sepulcher = carthage;
  sepulcher.skin_color = Sepulcher::k_bone;
  Nation::apply_materials(sepulcher, Nation::k_sepulcher_materials);
  sepulcher.wood_color = Sepulcher::k_grave_wood;
  sepulcher.fletching_color = QVector3D(0.43F, 0.41F, 0.36F);
  sepulcher.bow_string_color = QVector3D(0.24F, 0.23F, 0.21F);
  sepulcher.cape_color = QVector3D(0.075F, 0.075F, 0.090F);
  sepulcher.show_cape = false;
  sepulcher.show_shoulder_decor = false;
  sepulcher.force_beard = false;

  const std::array<ArcherStyleRegistration, 2> styles{{
      {.key = "carthage", .style = carthage},
      {.key = "iron_sepulcher", .style = sepulcher},
  }};
  ::Render::GL::register_archer_styles(styles);
}

} // namespace Render::GL::Carthage
