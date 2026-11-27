#include "app/diagnostics/support_bundle.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <array>
#include <utility>

namespace App::Diagnostics {

namespace {

constexpr quint32 k_local_header_signature = 0x04034b50U;
constexpr quint32 k_central_header_signature = 0x02014b50U;
constexpr quint32 k_end_of_central_signature = 0x06054b50U;
constexpr quint16 k_version_needed = 20;
constexpr quint16 k_utf8_name_flag = 0x0800;
constexpr quint16 k_method_store = 0;

auto crc_table() -> const std::array<quint32, 256>& {
  static const std::array<quint32, 256> table = [] {
    std::array<quint32, 256> rows{};
    for (quint32 index = 0; index < 256; ++index) {
      quint32 value = index;
      for (int bit = 0; bit < 8; ++bit) {
        value = (value & 1U) != 0U ? (value >> 1U) ^ 0xEDB88320U : value >> 1U;
      }
      rows[index] = value;
    }
    return rows;
  }();
  return table;
}

auto crc32_of(const QByteArray& data) -> quint32 {
  const auto& table = crc_table();
  quint32 value = 0xFFFFFFFFU;
  for (const char raw : data) {
    const auto byte = static_cast<quint8>(raw);
    value = (value >> 8U) ^ table[(value ^ byte) & 0xFFU];
  }
  return value ^ 0xFFFFFFFFU;
}

void put_u16(QByteArray& out, quint16 value) {
  out.append(static_cast<char>(value & 0xFFU));
  out.append(static_cast<char>((value >> 8U) & 0xFFU));
}

void put_u32(QByteArray& out, quint32 value) {
  put_u16(out, static_cast<quint16>(value & 0xFFFFU));
  put_u16(out, static_cast<quint16>((value >> 16U) & 0xFFFFU));
}

auto dos_stamp(const QDateTime& timestamp) -> std::pair<quint16, quint16> {
  const QDate date = timestamp.date();
  const QTime time = timestamp.time();
  const int year = std::clamp(date.year(), 1980, 2107);
  const auto dos_time = static_cast<quint16>(
      (time.hour() << 11) | (time.minute() << 5) | (time.second() / 2));
  const auto dos_date =
      static_cast<quint16>(((year - 1980) << 9) | (date.month() << 5) | date.day());
  return {dos_time, dos_date};
}

auto archive_name(const BundleEntry& entry) -> QByteArray {
  QString name = entry.name;
  name.replace(QLatin1Char('\\'), QLatin1Char('/'));
  while (name.startsWith(QLatin1Char('/'))) {
    name.remove(0, 1);
  }
  return name.toUtf8();
}

auto replace_all(QString& text,
                 const QString& needle,
                 const QString& replacement) -> void {
  if (needle.isEmpty() || needle == replacement) {
    return;
  }
  text.replace(needle, replacement);
}

auto shorten_paths(QString& text,
                   const QStringList& paths,
                   const QString& replacement) -> void {
  QStringList ordered = paths;
  // Longest first: a saves directory lives inside the app data directory, and
  // shortening the outer prefix first would leave the inner one unmatched.
  std::sort(
      ordered.begin(), ordered.end(), [](const QString& left, const QString& right) {
        return left.size() > right.size();
      });
  for (const QString& path : ordered) {
    if (path.size() < 4) {
      continue;
    }
    replace_all(text, path, replacement);
    replace_all(text, QDir::toNativeSeparators(path), replacement);
  }
}

auto redact_ipv4(QString& text) -> void {
  static const QRegularExpression pattern(
      QStringLiteral("\\b(?:(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)\\.){3}"
                     "(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)\\b"));
  auto matches = pattern.globalMatch(text);
  QList<std::pair<int, int>> spans;
  while (matches.hasNext()) {
    const auto match = matches.next();
    const QString address = match.captured(0);
    // Loopback and the unspecified address say nothing about the reporter's
    // network, and a bundle without them cannot explain a local-server bug.
    if (address.startsWith(QStringLiteral("127.")) ||
        address == QStringLiteral("0.0.0.0")) {
      continue;
    }
    spans.append({match.capturedStart(0), match.capturedLength(0)});
  }
  for (int index = spans.size() - 1; index >= 0; --index) {
    text.replace(
        spans[index].first, spans[index].second, QString::fromLatin1(k_placeholder_ip));
  }
}

auto redact_ipv6(QString& text) -> void {
  static const QRegularExpression pattern(
      QStringLiteral("\\b(?:[0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4}\\b"));
  auto matches = pattern.globalMatch(text);
  QList<std::pair<int, int>> spans;
  while (matches.hasNext()) {
    const auto match = matches.next();
    // Only the compressed form is unambiguous: "12:34:56" is a wall clock and no
    // amount of context in a log line makes it safe to rewrite.
    if (!match.captured(0).contains(QStringLiteral("::"))) {
      continue;
    }
    spans.append({match.capturedStart(0), match.capturedLength(0)});
  }
  for (int index = spans.size() - 1; index >= 0; --index) {
    text.replace(
        spans[index].first, spans[index].second, QString::fromLatin1(k_placeholder_ip));
  }
}

struct SecretPattern {
  QRegularExpression pattern;
  QString replacement;
};

auto secret_patterns() -> const QList<SecretPattern>& {
  static const QList<SecretPattern> patterns = [] {
    const auto literal = [](const char* text) {
      return QString::fromLatin1(text);
    };
    QList<SecretPattern> built;
    // An HTTP header first: the whole value is the credential, whatever scheme
    // precedes it. Matching only the first word after the colon would leave the
    // base64 that follows "Basic" in the bundle, so the rule runs to the end of
    // the line and replaces everything there.
    built.append({QRegularExpression(
                      QStringLiteral("(?i)\\bauthorization\\b\\s*[:=]\\s*[^\\r\\n]+")),
                  QStringLiteral("authorization=<secret>")});
    built.append(
        {QRegularExpression(QStringLiteral("\\bgh[pousr]_[A-Za-z0-9]{20,}\\b")),
         literal(k_placeholder_secret)});
    built.append(
        {QRegularExpression(QStringLiteral("\\bgithub_pat_[A-Za-z0-9_]{20,}\\b")),
         literal(k_placeholder_secret)});
    built.append({QRegularExpression(QStringLiteral("\\bsk-[A-Za-z0-9_-]{20,}\\b")),
                  literal(k_placeholder_secret)});
    built.append({QRegularExpression(QStringLiteral("\\bAKIA[0-9A-Z]{16}\\b")),
                  literal(k_placeholder_secret)});
    built.append(
        {QRegularExpression(QStringLiteral("\\bxox[abprs]-[A-Za-z0-9-]{10,}\\b")),
         literal(k_placeholder_secret)});
    built.append(
        {QRegularExpression(QStringLiteral("\\beyJ[A-Za-z0-9_-]{8,}\\.[A-Za-z0-9_-]{8,}"
                                           "\\.[A-Za-z0-9_-]{8,}\\b")),
         literal(k_placeholder_secret)});
    // key=value and key: value pairs, with the key kept so the reader still
    // knows which credential went missing.
    built.append(
        {QRegularExpression(QStringLiteral("(?i)\\b(authorization|bearer|token|"
                                           "access[_-]?token|api[_-]?key|secret|"
                                           "password|passwd)\\b\\s*[:=]\\s*\\S+")),
         QStringLiteral("\\1=") + literal(k_placeholder_secret)});
    return built;
  }();
  return patterns;
}

} // namespace

auto redact_text(const QString& text, const RedactionTargets& targets) -> QString {
  if (text.isEmpty()) {
    return text;
  }

  QString out = text;

  // Data and save directories before the home directory: both of them live
  // inside it, and shortening `/Users/me` first would turn the more specific
  // prefix into `<home>/Library/...` and lose the distinction.
  shorten_paths(out, targets.data_paths, QString::fromLatin1(k_placeholder_path));
  shorten_paths(out, targets.extra_paths, QString::fromLatin1(k_placeholder_path));
  shorten_paths(out, targets.home_paths, QString::fromLatin1(k_placeholder_home));

  // Before the user and host names: an address that carries the user name would
  // otherwise be half rewritten and stop matching.
  {
    static const QRegularExpression email(
        QStringLiteral("\\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}\\b"));
    out.replace(email, QString::fromLatin1(k_placeholder_email));
  }

  redact_ipv4(out);
  redact_ipv6(out);

  for (const auto& secret : secret_patterns()) {
    out.replace(secret.pattern, secret.replacement);
  }

  if (targets.host_name.size() >= 3) {
    replace_all(out, targets.host_name, QString::fromLatin1(k_placeholder_host));
  }
  if (targets.user_name.size() >= 3) {
    const QRegularExpression user_pattern(
        QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(targets.user_name)));
    out.replace(user_pattern, QString::fromLatin1(k_placeholder_user));
  }

  return out;
}

auto redact_bytes(const QByteArray& bytes,
                  const RedactionTargets& targets) -> QByteArray {
  return redact_text(QString::fromUtf8(bytes), targets).toUtf8();
}

auto apply_limits(const QList<BundleEntry>& entries,
                  const BundleLimits& limits,
                  QList<BundleEntry>& out) -> bool {
  out.clear();
  qint64 total = 0;
  bool everything_fit = true;
  for (const auto& entry : entries) {
    if (out.size() >= limits.max_entries || total >= limits.max_total_bytes) {
      everything_fit = false;
      break;
    }
    BundleEntry kept = entry;
    const qint64 budget =
        std::min(limits.max_entry_bytes, limits.max_total_bytes - total);
    if (kept.content.size() > budget) {
      everything_fit = false;
      // A screenshot is bytes, not lines: cutting its tail leaves a PNG that no
      // decoder accepts while the export still reports success. It is dropped
      // whole instead, and the collector says so in the readme.
      if (kept.name.endsWith(QStringLiteral(".png"))) {
        continue;
      }
      // The tail, not the head: what a bundle exists to show is the end of a
      // log, the frame that failed last. The marker goes on top so a reader
      // knows the first lines are missing, and it is counted against the budget
      // so the entry cannot grow past it.
      const auto original = static_cast<int>(kept.content.size());
      const auto room = static_cast<int>(budget);
      QByteArray tail = kept.content.right(room);
      QByteArray marker = QStringLiteral("[bundle: dropped the first %1 bytes of %2]\n")
                              .arg(original - tail.size())
                              .arg(entry.name)
                              .toUtf8();
      if (marker.size() >= room) {
        marker.truncate(room);
        tail.clear();
      } else {
        tail = kept.content.right(room - marker.size());
        const int first_newline = tail.indexOf('\n');
        if (first_newline >= 0 && first_newline + 1 < tail.size()) {
          tail.remove(0, first_newline + 1);
        }
      }
      kept.content = marker + tail;
    }
    total += kept.content.size();
    out.append(kept);
  }
  return everything_fit;
}

auto build_archive(const QList<BundleEntry>& entries,
                   const BundleLimits& limits,
                   const QDateTime& timestamp,
                   QByteArray& out) -> BundleWriteResult {
  BundleWriteResult result;

  QList<BundleEntry> limited;
  // Whether anything had to be dropped shows up in the readme and in the file
  // sizes the panel lists; the archive only needs what fits.
  [[maybe_unused]] const bool everything_fit = apply_limits(entries, limits, limited);
  if (limited.isEmpty()) {
    result.error = QStringLiteral("the bundle has no entries");
    return result;
  }

  const auto [dos_time, dos_date] = dos_stamp(timestamp);
  QByteArray body;
  QByteArray directory;
  QStringList written;
  bool duplicate_name = false;
  for (const auto& entry : limited) {
    const QByteArray name = archive_name(entry);
    if (name.isEmpty() || written.contains(QString::fromUtf8(name))) {
      duplicate_name = true;
      continue;
    }
    const quint32 crc = crc32_of(entry.content);
    const auto size = static_cast<quint32>(entry.content.size());
    const auto offset = static_cast<quint32>(body.size());

    put_u32(body, k_local_header_signature);
    put_u16(body, k_version_needed);
    put_u16(body, k_utf8_name_flag);
    put_u16(body, k_method_store);
    put_u16(body, dos_time);
    put_u16(body, dos_date);
    put_u32(body, crc);
    put_u32(body, size);
    put_u32(body, size);
    put_u16(body, static_cast<quint16>(name.size()));
    put_u16(body, 0);
    body.append(name);
    body.append(entry.content);

    put_u32(directory, k_central_header_signature);
    put_u16(directory, k_version_needed);
    put_u16(directory, k_version_needed);
    put_u16(directory, k_utf8_name_flag);
    put_u16(directory, k_method_store);
    put_u16(directory, dos_time);
    put_u16(directory, dos_date);
    put_u32(directory, crc);
    put_u32(directory, size);
    put_u32(directory, size);
    put_u16(directory, static_cast<quint16>(name.size()));
    put_u16(directory, 0);
    put_u16(directory, 0);
    put_u16(directory, 0);
    put_u16(directory, 0);
    put_u32(directory, 0);
    put_u32(directory, offset);
    directory.append(name);

    written.append(QString::fromUtf8(name));
  }

  if (written.isEmpty()) {
    result.error = duplicate_name ? QStringLiteral("every entry repeated a name")
                                  : QStringLiteral("the bundle has no entries");
    return result;
  }

  const auto directory_offset = static_cast<quint32>(body.size());
  const auto directory_size = static_cast<quint32>(directory.size());
  const auto count = static_cast<quint16>(written.size());

  QByteArray archive = body;
  archive.append(directory);
  put_u32(archive, k_end_of_central_signature);
  put_u16(archive, 0);
  put_u16(archive, 0);
  put_u16(archive, count);
  put_u16(archive, count);
  put_u32(archive, directory_size);
  put_u32(archive, directory_offset);
  put_u16(archive, 0);

  out = archive;
  result.ok = true;
  result.bytes = archive.size();
  result.written = written;
  return result;
}

auto write_archive(const QString& path,
                   const QList<BundleEntry>& entries,
                   const BundleLimits& limits,
                   const QDateTime& timestamp) -> BundleWriteResult {
  QByteArray archive;
  BundleWriteResult result = build_archive(entries, limits, timestamp, archive);
  result.path = path;
  if (!result.ok) {
    return result;
  }

  if (path.isEmpty()) {
    result.ok = false;
    result.error = QStringLiteral("no destination was given");
    return result;
  }
  const QFileInfo info(path);
  if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
    result.ok = false;
    result.error =
        QStringLiteral("could not create the folder %1").arg(info.absolutePath());
    return result;
  }

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    result.ok = false;
    result.error = QStringLiteral("could not open the bundle for writing: %1")
                       .arg(file.errorString());
    return result;
  }
  if (file.write(archive) != archive.size()) {
    result.ok = false;
    result.error = QStringLiteral("the bundle was only partly written: %1")
                       .arg(file.errorString());
    file.cancelWriting();
    return result;
  }
  if (!file.commit()) {
    result.ok = false;
    result.error =
        QStringLiteral("the bundle could not be saved: %1").arg(file.errorString());
    return result;
  }
  return result;
}

auto default_archive_name(const QDateTime& timestamp,
                          const QString& version,
                          const QString& commit) -> QString {
  QString suffix = commit.trimmed();
  if (suffix.isEmpty() || suffix == QStringLiteral("unknown")) {
    suffix = version.trimmed();
  }
  suffix = suffix.left(7);
  suffix.replace(QRegularExpression(QStringLiteral("[^0-9A-Za-z._-]")), QString());
  if (suffix.isEmpty()) {
    suffix = QStringLiteral("unknown");
  }
  return QStringLiteral("standard-of-iron-support-%1-%2.zip")
      .arg(timestamp.toString(QStringLiteral("yyyyMMdd-hhmmss")), suffix);
}

auto bundle_readme(const QList<BundleEntry>& entries,
                   const QDateTime& timestamp,
                   const QString& version,
                   const QStringList& omitted) -> QByteArray {
  QStringList lines;
  lines << QStringLiteral("Standard of Iron %1 support bundle").arg(version);
  lines << QStringLiteral("Created: %1").arg(timestamp.toUTC().toString(Qt::ISODate));
  lines << QString();
  lines << QStringLiteral(
      "This file was written by the game's Export Diagnostics action. Everything in "
      "it was read from this machine at that moment and nothing was uploaded: the "
      "bundle is an ordinary .zip you attach to a bug report yourself.");
  lines << QString();
  lines << QStringLiteral("What is inside:");
  for (const auto& entry : entries) {
    lines
        << QStringLiteral("  %1  (%2 bytes)").arg(entry.name).arg(entry.content.size());
    if (!entry.description.isEmpty()) {
      lines << QStringLiteral("      %1").arg(entry.description);
    }
  }
  lines << QString();
  if (!omitted.isEmpty()) {
    lines << QStringLiteral("Left out at the size limit:");
    for (const QString& note : omitted) {
      lines << QStringLiteral("  - %1").arg(note);
    }
    lines << QString();
  }
  lines << QStringLiteral(
      "What was left out on purpose: saved games, replay files, account identifiers "
      "and anything that could be used to reach this machine. Paths appear as "
      "<home> and <path>, credentials as <secret>, addresses as <ip> and addresses "
      "of people as <email>. Truncated files say so at the top.");
  lines << QString();
  lines << QStringLiteral(
      "If you are reading this in a bug report, the build line and the graphics "
      "block in system.txt are what usually identify a rendering problem, and "
      "logs/recent.log is the tail of the game's own output.");
  return (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
}

} // namespace App::Diagnostics
