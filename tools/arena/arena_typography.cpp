
#include "arena_typography.h"

#include <cmath>

#include "ui/brand_fonts.h"

namespace Arena::Typography {

namespace {

auto branded(double base, double ui, double tracking) -> QFont {
  QFont font(Ui::BrandFonts::title_family());
  font.setPixelSize(static_cast<int>(std::lround(base * ui)));
  font.setBold(true);

  font.setCapitalization(QFont::AllUppercase);

  font.setLetterSpacing(QFont::AbsoluteSpacing, tracking * ui);
  return font;
}

} // namespace

auto number(double ui) -> QFont {
  return branded(21.0, ui, 1.5);
}

auto small_label(double ui) -> QFont {
  return branded(15.0, ui, 1.2);
}

} // namespace Arena::Typography
