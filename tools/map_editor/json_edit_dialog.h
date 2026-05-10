#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QPushButton>

namespace MapEditor {

class JsonEditDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonEditDialog(const QString &title, const QJsonObject &json,
                          QWidget *parent = nullptr);

  [[nodiscard]] QJsonObject getJson() const;
  [[nodiscard]] bool isValid() const { return m_is_valid; }

private slots:
  void validateJson();
  void onAccepted();

private:
  void setupUI(const QString &title, const QJsonObject &json);

  QPlainTextEdit *m_editor = nullptr;
  QPushButton *m_ok_button = nullptr;
  QJsonObject m_result;
  bool m_is_valid = false;
};

} // namespace MapEditor
