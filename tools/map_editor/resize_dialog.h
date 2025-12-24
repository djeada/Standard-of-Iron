#pragma once

#include <QDialog>
#include <QSpinBox>

namespace MapEditor {

/**
 * @brief Dialog for resizing the map
 */
class ResizeDialog : public QDialog {
  Q_OBJECT

public:
  explicit ResizeDialog(int currentWidth, int currentHeight,
                        QWidget *parent = nullptr);

  [[nodiscard]] int newWidth() const;
  [[nodiscard]] int newHeight() const;

private:
  void setupUI(int currentWidth, int currentHeight);

  QSpinBox *m_widthSpinBox = nullptr;
  QSpinBox *m_heightSpinBox = nullptr;
};

} // namespace MapEditor
