#include "resize_dialog.h"
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace MapEditor {

ResizeDialog::ResizeDialog(int currentWidth, int currentHeight, QWidget *parent)
    : QDialog(parent) {
  setupUI(currentWidth, currentHeight);
}

void ResizeDialog::setupUI(int currentWidth, int currentHeight) {
  setWindowTitle("Resize Map");
  resize(300, 150);

  auto *layout = new QVBoxLayout(this);

  auto *formLayout = new QFormLayout();

  m_widthSpinBox = new QSpinBox(this);
  m_widthSpinBox->setRange(10, 1000);
  m_widthSpinBox->setValue(currentWidth);
  formLayout->addRow("Width:", m_widthSpinBox);

  m_heightSpinBox = new QSpinBox(this);
  m_heightSpinBox->setRange(10, 1000);
  m_heightSpinBox->setValue(currentHeight);
  formLayout->addRow("Height:", m_heightSpinBox);

  layout->addLayout(formLayout);

  // Buttons
  auto *buttonLayout = new QHBoxLayout();
  auto *cancelButton = new QPushButton("Cancel", this);
  auto *okButton = new QPushButton("OK", this);
  okButton->setDefault(true);

  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(okButton);
  layout->addLayout(buttonLayout);

  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
}

int ResizeDialog::newWidth() const { return m_widthSpinBox->value(); }

int ResizeDialog::newHeight() const { return m_heightSpinBox->value(); }

} // namespace MapEditor
