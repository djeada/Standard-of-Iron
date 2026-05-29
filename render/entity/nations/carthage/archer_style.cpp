#include "archer_style.h"

#include <array>

namespace Render::GL::Carthage {

void register_carthage_archer_style() {
  ArcherStyleConfig carthage;
  carthage.cloth_color = QVector3D(0.12F, 0.36F, 0.52F);
  carthage.leather_color = QVector3D(0.36F, 0.24F, 0.12F);
  carthage.leather_dark_color = QVector3D(0.22F, 0.16F, 0.10F);
  carthage.metal_color = QVector3D(0.75F, 0.66F, 0.42F);
  carthage.wood_color = QVector3D(0.38F, 0.28F, 0.18F);
  carthage.fletching_color = QVector3D(0.90F, 0.82F, 0.28F);
  carthage.bow_string_color = QVector3D(0.32F, 0.30F, 0.26F);
  carthage.cape_color = QVector3D(0.14F, 0.38F, 0.54F);
  carthage.show_helmet = true;
  carthage.show_armor = true;
  carthage.show_shoulder_decor = false;
  carthage.show_cape = true;
  carthage.force_beard = true;
  carthage.attachment_profile.clear();
  carthage.armor_id = "armor_light_carthage";

  ArcherStyleConfig sepulcher = carthage;
  sepulcher.skin_color = QVector3D(0.81F, 0.83F, 0.78F);
  sepulcher.cloth_color = QVector3D(0.22F, 0.22F, 0.25F);
  sepulcher.leather_color = QVector3D(0.25F, 0.21F, 0.18F);
  sepulcher.leather_dark_color = QVector3D(0.14F, 0.13F, 0.13F);
  sepulcher.metal_color = QVector3D(0.56F, 0.60F, 0.66F);
  sepulcher.wood_color = QVector3D(0.34F, 0.30F, 0.26F);
  sepulcher.fletching_color = QVector3D(0.62F, 0.66F, 0.70F);
  sepulcher.bow_string_color = QVector3D(0.35F, 0.35F, 0.36F);
  sepulcher.cape_color = QVector3D(0.10F, 0.10F, 0.12F);
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
