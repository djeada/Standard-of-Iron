#include "json_edit_dialog.h"

#include <QFontDatabase>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace MapEditor {

JsonEditDialog::JsonEditDialog(const QString& title,
                               const QJsonObject& json,
                               QWidget* parent)
    : QDialog(parent) {
  setup_ui(title, json);
}

void JsonEditDialog::setup_ui(const QString& title, const QJsonObject& json) {
  setWindowTitle(title);
  resize(640, 460);

  auto* layout = new QVBoxLayout(this);

  auto* label =
      new QLabel("Edit JSON properties (changes will be saved to map):", this);
  label->setWordWrap(true);
  layout->addWidget(label);

  m_editor = new QPlainTextEdit(this);
  m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_editor->setPlaceholderText("{\n  \"type\": \"...\"\n}");
  m_editor->setTabStopDistance(
      4 * m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')));

  QJsonDocument doc(json);
  m_editor->setPlainText(doc.toJson(QJsonDocument::Indented));
  layout->addWidget(m_editor);

  connect(m_editor, &QPlainTextEdit::textChanged, this, &JsonEditDialog::validate_json);

  auto* button_layout = new QHBoxLayout();
  auto* cancel_button = new QPushButton("Cancel", this);
  m_ok_button = new QPushButton("OK", this);
  m_ok_button->setDefault(true);
  m_ok_button->setProperty("primary", true);

  button_layout->addStretch();
  button_layout->addWidget(cancel_button);
  button_layout->addWidget(m_ok_button);
  layout->addLayout(button_layout);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_ok_button, &QPushButton::clicked, this, &JsonEditDialog::on_accepted);

  validate_json();
}

void JsonEditDialog::validate_json() {
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(m_editor->toPlainText().toUtf8(), &error);

  m_is_valid = (error.error == QJsonParseError::NoError && doc.isObject());
  m_ok_button->setEnabled(m_is_valid);

  if (!m_is_valid) {
    m_editor->setStyleSheet("QPlainTextEdit { border: 2px solid red; }");
  } else {
    m_editor->setStyleSheet("");
  }
}

void JsonEditDialog::on_accepted() {
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(m_editor->toPlainText().toUtf8(), &error);

  if (error.error == QJsonParseError::NoError && doc.isObject()) {
    m_result = doc.object();
    m_is_valid = true;
    accept();
  } else {
    QMessageBox::warning(
        this, "Invalid JSON", "The JSON is not valid: " + error.errorString());
  }
}

QJsonObject JsonEditDialog::get_json() const {
  return m_result;
}

} // namespace MapEditor
