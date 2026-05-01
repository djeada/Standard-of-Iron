#include <QApplication>
#include <QSurfaceFormat>

#include "arena_window.h"

namespace {

constexpr auto k_dark_stylesheet = R"(
QWidget {
    background-color: #071018;
    color: #eaf6ff;
    font-size: 12px;
}
QMainWindow {
    background-color: #071018;
}
QToolBar {
    background-color: #0c1e2a;
    border: none;
    border-bottom: 1px solid #0f2b34;
    spacing: 4px;
    padding: 2px 4px;
}
QToolBar::separator {
    background-color: #0f2b34;
    width: 1px;
    margin: 3px 2px;
}
QToolButton {
    background-color: transparent;
    color: #eaf6ff;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
}
QToolButton:hover {
    background-color: #184c7a;
    border-color: #1f8bf5;
}
QToolButton:pressed {
    background-color: #1b74d1;
}
QToolButton:checked {
    background-color: #1b74d1;
    border-color: #1f8bf5;
}
QGroupBox {
    background-color: #0c1e2a;
    border: 1px solid #0f2b34;
    border-radius: 6px;
    margin-top: 8px;
    padding-top: 8px;
    color: #86a7b6;
    font-weight: bold;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 4px;
    left: 8px;
    color: #86a7b6;
}
QPushButton {
    background-color: #0f2430;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 5px 10px;
    min-height: 24px;
}
QPushButton:hover {
    background-color: #184c7a;
    border-color: #1f8bf5;
}
QPushButton:pressed {
    background-color: #1b74d1;
}
QPushButton:disabled {
    background-color: #1a2a32;
    color: #4f6a75;
    border-color: #0f2b34;
}
QComboBox {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
    min-height: 22px;
}
QComboBox:hover {
    border-color: #1f8bf5;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    selection-background-color: #1f8bf5;
    selection-color: #eaf6ff;
}
QSpinBox, QDoubleSpinBox {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
    min-height: 22px;
}
QSpinBox:hover, QDoubleSpinBox:hover {
    border-color: #1f8bf5;
}
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: #0f2430;
    border: none;
    width: 16px;
}
QSlider::groove:horizontal {
    background-color: #0f2430;
    border: none;
    height: 4px;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background-color: #1f8bf5;
    border: none;
    width: 12px;
    height: 12px;
    border-radius: 6px;
    margin: -4px 0;
}
QSlider::handle:horizontal:hover {
    background-color: #9fd9ff;
}
QCheckBox {
    color: #eaf6ff;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid #0f2b34;
    border-radius: 3px;
    background-color: #0c1e2a;
}
QCheckBox::indicator:checked {
    background-color: #1f8bf5;
    border-color: #1b74d1;
}
QCheckBox::indicator:hover {
    border-color: #1f8bf5;
}
QLabel {
    background-color: transparent;
    color: #eaf6ff;
}
QScrollBar:vertical {
    background-color: #071018;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background-color: #0f2430;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background-color: #184c7a;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background-color: #071018;
    height: 8px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background-color: #0f2430;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollArea {
    border: none;
    background-color: transparent;
}
QTabWidget::pane {
    border: 1px solid #0f2b34;
    background-color: #071018;
}
QTabBar::tab {
    background-color: #0c1e2a;
    color: #86a7b6;
    border: 1px solid #0f2b34;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    padding: 5px 12px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background-color: #071018;
    color: #eaf6ff;
}
QTabBar::tab:hover:!selected {
    background-color: #184c7a;
    color: #eaf6ff;
}
QStatusBar {
    background-color: #0c1e2a;
    color: #86a7b6;
    border-top: 1px solid #0f2b34;
}
QStatusBar::item {
    border: none;
}
)";

} // namespace

auto main(int argc, char **argv) -> int {
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Arena");
  QApplication::setApplicationVersion("1.0");
  app.setStyleSheet(QString::fromLatin1(k_dark_stylesheet));

  ArenaWindow window;
  window.resize(1600, 900);
  window.show();

  return app.exec();
}
