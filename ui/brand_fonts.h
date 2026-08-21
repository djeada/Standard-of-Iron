#ifndef UI_BRAND_FONTS_H
#define UI_BRAND_FONTS_H

#include <QString>
#include <QStringList>

namespace Ui::BrandFonts {

// Registers every face bundled under assets/fonts/ with the running
// application, so the game and the Qt Widgets tools all draw with the same
// typography no matter which fonts the host machine happens to have installed.
//
// Idempotent: repeated calls are cheap no-ops. Safe to call from any bootstrap
// path, which is why both main.cpp and UiShell::apply() call it -- the arena
// goes through the latter and the game through the former.
//
// Requires a QGuiApplication to exist. Returns the families it registered.
auto register_bundled() -> QStringList;

// The face titles, headlines and big numbers are set in: the Standard of Iron
// display face when it is present, otherwise the bundled serif. Never a bare
// system family name, which is what made captures machine-dependent.
auto title_family() -> QString;

} // namespace Ui::BrandFonts

#endif
