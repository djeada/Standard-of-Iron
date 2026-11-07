#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

#include <functional>

class QLineEdit;
class QListWidget;
class QLabel;

namespace MapEditor {

struct PaletteEntry {
  QString title;
  QString detail;
  QString category;
  std::function<void()> run;
};

[[nodiscard]] auto palette_score(const QString& query, const QString& text) -> int;

class CommandPalette : public QDialog {
  Q_OBJECT

public:
  explicit CommandPalette(QWidget* parent = nullptr);

  void set_entries(QVector<PaletteEntry> entries);
  void open_with(const QString& text = {});

protected:
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;

private:
  void refilter();
  void activate_current();

  QLineEdit* m_input = nullptr;
  QListWidget* m_list = nullptr;
  QLabel* m_hint = nullptr;
  QVector<PaletteEntry> m_entries;
  QVector<int> m_visible;
};

} // namespace MapEditor
