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
    spacing: 6px;
    padding: 3px 6px;
}
QToolBar::separator {
    background-color: #0f2b34;
    width: 1px;
    margin: 4px 3px;
}
QToolButton {
    background-color: transparent;
    color: #eaf6ff;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 5px 10px;
    min-width: 60px;
}
QToolButton:hover {
    background-color: #184c7a;
    border-color: #1f8bf5;
}
QToolButton:pressed {
    background-color: #1b74d1;
}
QToolButton:checked {
    background-color: #1054a0;
    border-color: #1f8bf5;
    color: #9fd9ff;
}
QGroupBox {
    background-color: #0c1e2a;
    border: 1px solid #0f2b34;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 10px;
    font-weight: bold;
    font-size: 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    left: 8px;
    color: #9fd9ff;
}
QPushButton {
    background-color: #0f2430;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 5px 10px;
    min-height: 26px;
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
QPushButton[primary="true"] {
    background-color: #1054a0;
    color: #dff0ff;
    border: 1px solid #1f8bf5;
    font-weight: bold;
}
QPushButton[primary="true"]:hover {
    background-color: #1568c5;
    border-color: #9fd9ff;
    color: #ffffff;
}
QPushButton[primary="true"]:pressed {
    background-color: #1b74d1;
}
QComboBox {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
    min-height: 24px;
}
QComboBox:hover {
    border-color: #1f8bf5;
}
QComboBox:focus {
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
    outline: none;
}
QSpinBox, QDoubleSpinBox {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
    min-height: 24px;
}
QSpinBox:hover, QDoubleSpinBox:hover {
    border-color: #1f8bf5;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #1f8bf5;
}
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: #0f2430;
    border: none;
    width: 18px;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background-color: #184c7a;
}
QSlider::groove:horizontal {
    background-color: #0f2430;
    border: none;
    height: 4px;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background-color: #1f8bf5;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background-color: #1f8bf5;
    border: 2px solid #071018;
    width: 14px;
    height: 14px;
    border-radius: 7px;
    margin: -5px 0;
}
QSlider::handle:horizontal:hover {
    background-color: #9fd9ff;
    border-color: #071018;
}
QCheckBox {
    color: #eaf6ff;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid #1a4060;
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
    background-color: #1a3a4a;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background-color: #1f8bf5;
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
    background-color: #1a3a4a;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover {
    background-color: #1f8bf5;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollArea {
    border: none;
    background-color: transparent;
}
QSplitter::handle {
    background-color: #0f2b34;
}
QSplitter::handle:horizontal {
    width: 4px;
}
QSplitter::handle:hover {
    background-color: #1f8bf5;
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
    padding: 6px 16px;
    margin-right: 2px;
    min-width: 60px;
}
QTabBar::tab:selected {
    background-color: #071018;
    color: #9fd9ff;
    border-bottom: 2px solid #1f8bf5;
}
QTabBar::tab:hover:!selected {
    background-color: #132030;
    color: #eaf6ff;
}
QStatusBar {
    background-color: #0c1e2a;
    color: #86a7b6;
    border-top: 1px solid #0f2b34;
    padding: 0 4px;
}
QStatusBar::item {
    border: none;
}
)";

}

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
