#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QPushButton>

namespace MapEditor {

/**
 * @brief Dialog for editing element JSON properties
 */
class JsonEditDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonEditDialog(const QString &title, const QJsonObject &json,
                          QWidget *parent = nullptr);

  [[nodiscard]] QJsonObject getJson() const;
  [[nodiscard]] bool isValid() const { return m_isValid; }

private slots:
  void validateJson();
  void onAccepted();

private:
  void setupUI(const QString &title, const QJsonObject &json);

  QPlainTextEdit *m_editor = nullptr;
  QPushButton *m_okButton = nullptr;
  QJsonObject m_result;
  bool m_isValid = false;
};

} // namespace MapEditor
