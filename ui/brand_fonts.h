#ifndef UI_BRAND_FONTS_H
#define UI_BRAND_FONTS_H

#include <QString>
#include <QStringList>

namespace Ui::BrandFonts {

auto register_bundled() -> QStringList;

auto title_family() -> QString;

} // namespace Ui::BrandFonts

#endif
