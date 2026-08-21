
#include "ui/brand_fonts.h"

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

#include "utils/resource_utils.h"

namespace Ui::BrandFonts {

namespace {

struct Registry {
  QStringList families;
  bool loaded{false};
};

auto registry() -> Registry& {
  static Registry state;
  return state;
}

auto font_directory() -> QString {
  return Utils::Resources::resolve_resource_path(QStringLiteral(":/assets/fonts"));
}

} // namespace

auto register_bundled() -> QStringList {
  Registry& state = registry();
  if (state.loaded) {
    return state.families;
  }

  // QFontDatabase needs the GUI application up. Registering before that
  // silently does nothing, and the failure only shows up as a wrong-looking
  // capture hours later, so refuse loudly instead.
  if (QGuiApplication::instance() == nullptr) {
    qWarning("Ui::BrandFonts::register_bundled() called before QGuiApplication exists");
    return {};
  }

  state.loaded = true;

  const QDir dir(font_directory());
  if (!dir.exists()) {
    qWarning("bundled fonts missing: %s", qUtf8Printable(dir.path()));
    return state.families;
  }

  const QStringList files = dir.entryList(
      {QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files, QDir::Name);
  for (const QString& file : files) {
    const int id = QFontDatabase::addApplicationFont(dir.filePath(file));
    if (id < 0) {
      qWarning("failed to register bundled font: %s", qUtf8Printable(file));
      continue;
    }
    for (const QString& family : QFontDatabase::applicationFontFamilies(id)) {
      if (!state.families.contains(family)) {
        state.families.append(family);
      }
    }
  }

  if (state.families.isEmpty()) {
    qWarning("no bundled fonts registered from %s", qUtf8Printable(dir.path()));
  }
  return state.families;
}

auto title_family() -> QString {
  // The display face first, then the serif that backs it up. The first one the
  // application actually registered wins, so dropping a new
  // StandardIronDisplay-Bold.ttf into assets/fonts/ switches the whole game
  // over with no code change. Function-local so it is built on first use
  // rather than during static initialisation.
  static const QStringList preference = {
      QStringLiteral("Standard Iron Display"),
      QStringLiteral("EB Garamond"),
  };

  const QStringList registered = register_bundled();
  for (const QString& candidate : preference) {
    if (registered.contains(candidate)) {
      return candidate;
    }
  }
  // Nothing bundled resolved. A serif request is a better last resort than a
  // named family the host may not have.
  return QStringLiteral("serif");
}

} // namespace Ui::BrandFonts
