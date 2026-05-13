#include "resize_dialog.h"
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace MapEditor {

ResizeDialog::ResizeDialog(int current_width, int current_height,
                           QWidget *parent)
    : QDialog(parent) {
  setup_ui(current_width, current_height);
}

void ResizeDialog::setup_ui(int current_width, int current_height) {
  setWindowTitle("Resize Map");
  resize(300, 150);

  auto *layout = new QVBoxLayout(this);
  layout->setSpacing(8);

  auto *label = new QLabel(
      "Update the playable grid size. Existing elements keep their current "
      "coordinates.",
      this);
  label->setWordWrap(true);
  layout->addWidget(label);

  auto *form_layout = new QFormLayout();

  m_width_spin_box = new QSpinBox(this);
  m_width_spin_box->setRange(10, 1000);
  m_width_spin_box->setValue(current_width);
  form_layout->addRow("Width:", m_width_spin_box);

  m_height_spin_box = new QSpinBox(this);
  m_height_spin_box->setRange(10, 1000);
  m_height_spin_box->setValue(current_height);
  form_layout->addRow("Height:", m_height_spin_box);

  layout->addLayout(form_layout);

  auto *button_layout = new QHBoxLayout();
  auto *cancel_button = new QPushButton("Cancel", this);
  auto *ok_button = new QPushButton("OK", this);
  ok_button->setDefault(true);
  ok_button->setProperty("primary", true);

  button_layout->addStretch();
  button_layout->addWidget(cancel_button);
  button_layout->addWidget(ok_button);
  layout->addLayout(button_layout);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok_button, &QPushButton::clicked, this, &QDialog::accept);
}

int ResizeDialog::new_width() const { return m_width_spin_box->value(); }

int ResizeDialog::new_height() const { return m_height_spin_box->value(); }

} // namespace MapEditor
