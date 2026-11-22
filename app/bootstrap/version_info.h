#pragma once

#include <QString>
#include <QStringList>

#include <string_view>

namespace App::Bootstrap {

inline constexpr char k_product_name[] = "Standard of Iron";

struct VersionDetails {
  QString version;
  QString commit;
  QString build_type;
  QString qt_version;
};

inline auto format_version_text(const VersionDetails& details) -> QString {
  QStringList lines;
  lines << QStringLiteral("%1 %2").arg(QString::fromLatin1(k_product_name),
                                       details.version);
  if (!details.commit.isEmpty() && details.commit != QStringLiteral("unknown")) {
    lines << QStringLiteral("Commit: %1").arg(details.commit);
  }
  if (!details.build_type.isEmpty()) {
    lines << QStringLiteral("Build type: %1").arg(details.build_type);
  }
  if (!details.qt_version.isEmpty()) {
    lines << QStringLiteral("Qt: %1").arg(details.qt_version);
  }
  return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

inline auto version_requested_from_argv(int argc, char* argv[]) -> bool {
  for (int index = 1; index < argc; ++index) {
    if (argv[index] == nullptr) {
      continue;
    }
    const std::string_view arg(argv[index]);
    if (arg == "--") {
      return false;
    }
    if (arg == "--version" || arg == "-v") {
      return true;
    }
  }
  return false;
}

auto current_version_details() -> VersionDetails;
auto print_version() -> int;

} // namespace App::Bootstrap
