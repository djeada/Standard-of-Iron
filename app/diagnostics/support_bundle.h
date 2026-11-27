#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace App::Diagnostics {

// One file that goes into a support bundle. `name` is the path inside the
// archive and is the only thing a bug report reader has to trust, so it is
// built by the collector and never taken from a user-supplied path.
struct BundleEntry {
  QString name;
  QString description;
  QByteArray content;

  bool operator==(const BundleEntry& other) const {
    return name == other.name && description == other.description &&
           content == other.content;
  }
};

// Hard ceilings, not targets. A bundle has to stay small enough to attach to a
// GitHub issue, so every oversized input is truncated and says so in place.
struct BundleLimits {
  qint64 max_entry_bytes = 128 * 1024;
  qint64 max_total_bytes = 4 * 1024 * 1024;
  int max_entries = 32;
};

// What has to disappear from a log line before it can leave the machine.
struct RedactionTargets {
  // Absolute prefixes (home, app data, saves, temp), longest first. Each is
  // replaced with the placeholder that matches its kind.
  QStringList home_paths;
  QStringList data_paths;
  QStringList extra_paths;
  QString user_name;
  QString host_name;

  bool empty() const {
    return home_paths.isEmpty() && data_paths.isEmpty() && extra_paths.isEmpty() &&
           user_name.isEmpty() && host_name.isEmpty();
  }
};

// Placeholders a redacted bundle uses instead of the real value.
inline constexpr char k_placeholder_home[] = "<home>";
inline constexpr char k_placeholder_path[] = "<path>";
inline constexpr char k_placeholder_user[] = "<user>";
inline constexpr char k_placeholder_host[] = "<host>";
inline constexpr char k_placeholder_ip[] = "<ip>";
inline constexpr char k_placeholder_email[] = "<email>";
inline constexpr char k_placeholder_secret[] = "<secret>";

[[nodiscard]] auto redact_text(const QString& text,
                               const RedactionTargets& targets) -> QString;
[[nodiscard]] auto redact_bytes(const QByteArray& bytes,
                                const RedactionTargets& targets) -> QByteArray;

// Applies the limits above and reports what happened: entries are dropped once
// the count or the byte budget is spent, and every text entry over the per-file
// cap is cut on a line boundary with a marker appended. A binary entry (a PNG)
// is never cut - a truncated image does not decode - so it is dropped whole.
[[nodiscard]] auto apply_limits(const QList<BundleEntry>& entries,
                                const BundleLimits& limits,
                                QList<BundleEntry>& out) -> bool;

struct BundleWriteResult {
  bool ok = false;
  QString path;
  qint64 bytes = 0;
  QStringList written;
  QString error;
};

// A stored (uncompressed) ZIP, built in memory so the byte layout is testable.
// The method is deliberately 0: a bundle is a few hundred kilobytes of text,
// and a format that every unzipper reads without a compression library is worth
// more here than the bytes it would save.
[[nodiscard]] auto build_archive(const QList<BundleEntry>& entries,
                                 const BundleLimits& limits,
                                 const QDateTime& timestamp,
                                 QByteArray& out) -> BundleWriteResult;

[[nodiscard]] auto write_archive(const QString& path,
                                 const QList<BundleEntry>& entries,
                                 const BundleLimits& limits,
                                 const QDateTime& timestamp) -> BundleWriteResult;

// standard-of-iron-support-20261126-120000-a1b2c3d.zip, so two bundles from the
// same machine never collide and a maintainer can tell builds apart by name.
[[nodiscard]] auto default_archive_name(const QDateTime& timestamp,
                                        const QString& version,
                                        const QString& commit) -> QString;

// The text a reader opens first: what this is, what is in it, what was left out.
[[nodiscard]] auto bundle_readme(const QList<BundleEntry>& entries,
                                 const QDateTime& timestamp,
                                 const QString& version,
                                 const QStringList& omitted = {}) -> QByteArray;

} // namespace App::Diagnostics
