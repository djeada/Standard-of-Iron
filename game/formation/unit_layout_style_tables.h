#pragma once

#include <string>
#include <vector>

#include "unit_layout.h"

namespace Game::Formation::style_tables {

[[nodiscard]] auto make_style(std::string id, UnitLayoutShape shape) -> UnitLayoutStyle;

void register_generic_infantry_styles(std::vector<UnitLayoutStyle>& out);
void register_generic_mounted_styles(std::vector<UnitLayoutStyle>& out);
void register_generic_support_styles(std::vector<UnitLayoutStyle>& out);
void register_generic_sepulcher_styles(std::vector<UnitLayoutStyle>& out);
void register_rome_styles(std::vector<UnitLayoutStyle>& out);
void register_carthage_styles(std::vector<UnitLayoutStyle>& out);
void register_sepulcher_styles(std::vector<UnitLayoutStyle>& out);

void register_default_styles(std::vector<UnitLayoutStyle>& out);

} // namespace Game::Formation::style_tables
