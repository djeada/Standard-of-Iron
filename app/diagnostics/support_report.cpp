#include "app/diagnostics/support_report.h"

#include <QLatin1Char>
#include <QStringList>

#include <algorithm>

namespace App::Diagnostics {

namespace {

auto or_unknown(const QString& value) -> QString {
  const QString trimmed = value.trimmed();
  return trimmed.isEmpty() ? QStringLiteral("unknown") : trimmed;
}

void append_field(QStringList& lines, const QString& key, const QString& value) {
  lines << QStringLiteral("  %1: %2").arg(key, or_unknown(value));
}

void append_header(QStringList& lines, const QString& title) {
  lines << QString();
  lines << title;
}

} // namespace

auto environment_report(const EnvironmentFacts& facts,
                        const QDateTime& timestamp) -> QByteArray {
  QStringList lines;
  lines << QStringLiteral("%1 support report").arg(facts.product_name);
  lines << QStringLiteral("generated: %1").arg(timestamp.toUTC().toString(Qt::ISODate));

  append_header(lines, QStringLiteral("build"));
  append_field(lines, QStringLiteral("product"), facts.product_name);
  append_field(lines, QStringLiteral("version"), facts.version);
  append_field(lines, QStringLiteral("commit"), facts.commit);
  append_field(lines, QStringLiteral("build type"), facts.build_type);
  append_field(lines, QStringLiteral("qt runtime"), facts.qt_version);
  append_field(lines, QStringLiteral("qt built against"), facts.qt_build_version);

  append_header(lines, QStringLiteral("system"));
  append_field(lines, QStringLiteral("os"), facts.os_name);
  append_field(lines,
               QStringLiteral("kernel"),
               QStringLiteral("%1 %2").arg(or_unknown(facts.kernel_type),
                                           or_unknown(facts.kernel_version)));
  append_field(lines,
               QStringLiteral("cpu"),
               QStringLiteral("%1, %2 cores")
                   .arg(or_unknown(facts.cpu_architecture))
                   .arg(facts.cpu_count));
  append_field(
      lines, QStringLiteral("memory"), QStringLiteral("%1 MiB").arg(facts.memory_mb));
  append_field(lines, QStringLiteral("locale"), facts.locale_name);
  append_field(lines, QStringLiteral("language"), facts.language);

  append_header(lines, QStringLiteral("graphics"));
  if (!facts.graphics.available) {
    lines << QStringLiteral("  adapter: unavailable (no OpenGL context was created)");
  } else {
    append_field(lines, QStringLiteral("adapter"), QStringLiteral("available"));
    append_field(lines, QStringLiteral("vendor"), facts.graphics.vendor);
    append_field(lines, QStringLiteral("renderer"), facts.graphics.renderer);
    append_field(lines, QStringLiteral("version"), facts.graphics.version);
    append_field(lines, QStringLiteral("tier"), facts.graphics.tier);
  }
  append_field(lines, QStringLiteral("quality preset"), facts.graphics.quality_preset);

  append_header(lines, QStringLiteral("display"));
  append_field(lines, QStringLiteral("screens"), QString::number(facts.screen_count));
  if (facts.screen_count > 0) {
    append_field(lines,
                 QStringLiteral("primary screen"),
                 QStringLiteral("%1x%2 @ %3")
                     .arg(facts.primary_screen.width())
                     .arg(facts.primary_screen.height())
                     .arg(facts.primary_device_pixel_ratio, 0, 'f', 2));
  }

  append_header(lines, QStringLiteral("storage"));
  for (const QString& line : facts.storage_lines) {
    lines << QStringLiteral("  %1").arg(line);
  }

  lines << QString();
  return (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
}

auto settings_snapshot(const QList<QPair<QString, QString>>& values) -> QByteArray {
  QList<QPair<QString, QString>> sorted = values;
  std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
    return left.first < right.first;
  });

  QStringList lines;
  lines << QStringLiteral(
      "# Standard of Iron preferences as they were when this bundle was written.");
  lines << QStringLiteral(
      "# Only the values that describe how the game behaves; no paths, no account "
      "identifiers.");
  for (const auto& entry : sorted) {
    lines << QStringLiteral("%1=%2").arg(entry.first, entry.second);
  }
  lines << QString();
  return lines.join(QLatin1Char('\n')).toUtf8();
}

} // namespace App::Diagnostics
