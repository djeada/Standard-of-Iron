#pragma once

#include <QDialog>
#include <QSpinBox>

namespace MapEditor {

class ResizeDialog : public QDialog {
  Q_OBJECT

public:
  explicit ResizeDialog(int current_width,
                        int current_height,
                        QWidget* parent = nullptr);

  [[nodiscard]] int new_width() const;
  [[nodiscard]] int new_height() const;

private:
  void setup_ui(int current_width, int current_height);

  QSpinBox* m_width_spin_box = nullptr;
  QSpinBox* m_height_spin_box = nullptr;
};

} // namespace MapEditor
