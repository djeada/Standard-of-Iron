#include "app/bootstrap/version_info.h"

#include <QTextStream>
#include <QtGlobal>

#ifndef SOI_BUILD_COMMIT
#define SOI_BUILD_COMMIT ""
#endif
#ifndef SOI_BUILD_TYPE
#define SOI_BUILD_TYPE ""
#endif

namespace App::Bootstrap {

auto current_version_details() -> VersionDetails {
  VersionDetails details;
  details.version = QStringLiteral(SOI_VERSION);
  details.commit = QStringLiteral(SOI_BUILD_COMMIT);
  details.build_type = QStringLiteral(SOI_BUILD_TYPE);
  details.qt_version = QString::fromLatin1(qVersion());
  return details;
}

auto print_version() -> int {
  QTextStream out(stdout);
  out << format_version_text(current_version_details());
  out.flush();
  return 0;
}

} // namespace App::Bootstrap
