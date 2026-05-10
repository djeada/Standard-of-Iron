#include "json_edit_dialog.h"
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace MapEditor {

JsonEditDialog::JsonEditDialog(const QString &title, const QJsonObject &json,
                               QWidget *parent)
    : QDialog(parent) {
  setupUI(title, json);
}

void JsonEditDialog::setupUI(const QString &title, const QJsonObject &json) {
  setWindowTitle(title);
  resize(500, 400);

  auto *layout = new QVBoxLayout(this);

  auto *label =
      new QLabel("Edit JSON properties (changes will be saved to map):", this);
  layout->addWidget(label);

  m_editor = new QPlainTextEdit(this);
  m_editor->setFont(QFont("Monospace", 10));

  QJsonDocument doc(json);
  m_editor->setPlainText(doc.toJson(QJsonDocument::Indented));
  layout->addWidget(m_editor);

  connect(m_editor, &QPlainTextEdit::textChanged, this,
          &JsonEditDialog::validateJson);

  auto *buttonLayout = new QHBoxLayout();
  auto *cancelButton = new QPushButton("Cancel", this);
  m_ok_button = new QPushButton("OK", this);
  m_ok_button->setDefault(true);

  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(m_ok_button);
  layout->addLayout(buttonLayout);

  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_ok_button, &QPushButton::clicked, this,
          &JsonEditDialog::onAccepted);

  validateJson();
}

void JsonEditDialog::validateJson() {
  QJsonParseError error;
  QJsonDocument doc =
      QJsonDocument::fromJson(m_editor->toPlainText().toUtf8(), &error);

  m_is_valid = (error.error == QJsonParseError::NoError && doc.isObject());
  m_ok_button->setEnabled(m_is_valid);

  if (!m_is_valid) {
    m_editor->setStyleSheet("QPlainTextEdit { border: 2px solid red; }");
  } else {
    m_editor->setStyleSheet("");
  }
}

void JsonEditDialog::onAccepted() {
  QJsonParseError error;
  QJsonDocument doc =
      QJsonDocument::fromJson(m_editor->toPlainText().toUtf8(), &error);

  if (error.error == QJsonParseError::NoError && doc.isObject()) {
    m_result = doc.object();
    m_is_valid = true;
    accept();
  } else {
    QMessageBox::warning(this, "Invalid JSON",
                         "The JSON is not valid: " + error.errorString());
  }
}

QJsonObject JsonEditDialog::getJson() const { return m_result; }

} // namespace MapEditor
