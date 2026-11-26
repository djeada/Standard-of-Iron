#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>

#include "app/diagnostics/support_bundle.h"

class QImage;

namespace App::Diagnostics {

// The QML side of the support bundle: collects what the game knows about itself,
// shows the reporter exactly what would be in the archive, and writes it when
// they pick a path.
//
// Nothing here uploads, posts or phones home. The only way a bundle leaves the
// machine is the file the reporter attaches to a bug report themselves.
class SupportBundleService : public QObject {
  Q_OBJECT

  // name, description, size and truncated state, one row per file.
  Q_PROPERTY(QVariantList entries READ entries NOTIFY entries_changed)
  // Where the bundle goes unless the reporter edits the field.
  Q_PROPERTY(QString default_path READ default_path NOTIFY default_path_changed)
  Q_PROPERTY(bool include_screenshot READ include_screenshot WRITE
                 set_include_screenshot NOTIFY include_screenshot_changed)
  // One line for the panel: what the last export did, or why it did not.
  Q_PROPERTY(QString message READ message NOTIFY message_changed)
  Q_PROPERTY(bool export_succeeded READ export_succeeded NOTIFY message_changed)

public:
  explicit SupportBundleService(QObject* parent = nullptr);

  // Set by the composition root, which owns the executable's version strings.
  // Left empty in a test, where the report then says "unknown".
  void set_build_info(const QString& version,
                      const QString& commit,
                      const QString& build_type);

  // Set by the composition root once the window exists. Without it the
  // screenshot option stays available and says it is not.
  void set_screenshot_provider(std::function<QImage()> provider);

  [[nodiscard]] auto entries() const -> QVariantList;
  [[nodiscard]] auto default_path() const -> QString;
  [[nodiscard]] auto include_screenshot() const -> bool;
  void set_include_screenshot(bool include);
  [[nodiscard]] auto message() const -> QString;
  [[nodiscard]] auto export_succeeded() const -> bool;

  // Rebuilds the entry list and the preview. Called from the panel and after the
  // screenshot option changes.
  Q_INVOKABLE void refresh();

  // The first `max_characters` of one entry, already redacted. Empty for an
  // entry that is not text.
  Q_INVOKABLE QString preview(const QString& name, int max_characters) const;

  // Writes the archive to `path` (the default path when it is empty).
  Q_INVOKABLE bool export_to(const QString& path);

signals:
  void entries_changed();
  void default_path_changed();
  void include_screenshot_changed();
  void message_changed();

private:
  [[nodiscard]] auto collect() const -> QList<BundleEntry>;
  [[nodiscard]] auto redaction_targets() const -> RedactionTargets;
  void set_message(const QString& text, bool succeeded);

  QList<BundleEntry> m_entries;
  QString m_default_path;
  QString m_message;
  bool m_export_succeeded = false;
  bool m_include_screenshot = false;
  QString m_version;
  QString m_commit;
  QString m_build_type;
  std::function<QImage()> m_screenshot_provider;
};

} // namespace App::Diagnostics
