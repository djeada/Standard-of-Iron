#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QPushButton>

namespace MapEditor {

class JsonEditDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonEditDialog(const QString& title,
                          const QJsonObject& json,
                          QWidget* parent = nullptr);

  [[nodiscard]] QJsonObject get_json() const;
  [[nodiscard]] bool is_valid() const { return m_is_valid; }

private slots:
  void validate_json();
  void on_accepted();

private:
  void setup_ui(const QString& title, const QJsonObject& json);

  QPlainTextEdit* m_editor = nullptr;
  QPushButton* m_ok_button = nullptr;
  QJsonObject m_result;
  bool m_is_valid = false;
};

} // namespace MapEditor
