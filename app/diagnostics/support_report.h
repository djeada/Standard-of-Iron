#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QSize>
#include <QString>

namespace App::Diagnostics {

// What the renderer reported the last time it had a context. Never guessed: an
// empty block here is the honest answer for a headless or software-only run.
struct GraphicsFacts {
  bool available = false;
  QString vendor;
  QString renderer;
  QString version;
  QString tier;
  QString quality_preset;
};

struct EnvironmentFacts {
  QString product_name = QStringLiteral("Standard of Iron");
  QString version;
  QString commit;
  QString build_type;
  QString qt_version;
  QString qt_build_version = QStringLiteral(QT_VERSION_STR);

  QString os_name;
  QString kernel_type;
  QString kernel_version;
  QString cpu_architecture;
  int cpu_count = 0;
  qint64 memory_mb = 0;
  QString locale_name;
  QString language;

  GraphicsFacts graphics;

  int screen_count = 0;
  QSize primary_screen;
  qreal primary_device_pixel_ratio = 1.0;

  // One line per directory the game keeps state in, as "saves: present (12
  // files)". Absolute paths stay out of the bundle; only whether the folder is
  // there and how much is in it, which is what a "my saves vanished" report is
  // actually asking.
  QStringList storage_lines;
};

// Plain text, one heading per block, every line of the form "  key: value". The
// shape is stable because maintainers diff these between reports.
[[nodiscard]] auto environment_report(const EnvironmentFacts& facts,
                                      const QDateTime& timestamp) -> QByteArray;

// The settings the game persists, minus everything that names a person or a
// path. Sorted, so two runs with the same preferences produce the same bytes.
[[nodiscard]] auto
settings_snapshot(const QList<QPair<QString, QString>>& values) -> QByteArray;

} // namespace App::Diagnostics
