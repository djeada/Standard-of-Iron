#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "editor_window.h"

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
QMenuBar {
    background-color: #0c1e2a;
    border-bottom: 1px solid #0f2b34;
}
QMenuBar::item {
    background: transparent;
    padding: 6px 10px;
}
QMenuBar::item:selected {
    background-color: #184c7a;
    color: #ffffff;
}
QMenu {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    padding: 4px;
}
QMenu::item {
    padding: 6px 24px 6px 12px;
    border-radius: 4px;
}
QMenu::item:selected {
    background-color: #184c7a;
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
QToolButton[toolCard="true"] {
    background-color: #0c1e2a;
    border: 1px solid #0f2b34;
    border-radius: 6px;
    padding: 8px 10px;
    min-height: 56px;
    text-align: left;
    font-weight: 600;
}
QToolButton[toolCard="true"]:hover {
    background-color: #132d40;
    border-color: #1f8bf5;
}
QToolButton[toolCard="true"]:checked {
    background-color: #1054a0;
    border-color: #1f8bf5;
    color: #ffffff;
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
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit {
    background-color: #0c1e2a;
    color: #eaf6ff;
    border: 1px solid #0f2b34;
    border-radius: 4px;
    padding: 4px 8px;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover,
QLineEdit:hover, QPlainTextEdit:hover {
    border-color: #1f8bf5;
}
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
QLineEdit:focus, QPlainTextEdit:focus {
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
QLabel {
    background-color: transparent;
    color: #eaf6ff;
}
QLabel#panelTitle {
    color: #9fd9ff;
    font-size: 16px;
    font-weight: 700;
}
QLabel#panelIntro {
    color: #b7d2df;
}
QLabel#toolSummary {
    background-color: #0c1e2a;
    border: 1px solid #0f2b34;
    border-radius: 6px;
    color: #9fd9ff;
    padding: 8px 10px;
}
QLabel#panelHint {
    color: #b7d2df;
}
)";

}

auto main(int argc, char* argv[]) -> int {

  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Map Editor");
  QApplication::setApplicationVersion("1.0");
  app.setStyleSheet(QString::fromLatin1(k_dark_stylesheet));

  QCommandLineParser parser;
  parser.setApplicationDescription("Map editor for Standard of Iron game");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("file", "Map file to open (optional)");

  parser.process(app);

  MapEditor::EditorWindow window;
  window.show();

  const QStringList args = parser.positionalArguments();
  if (!args.isEmpty()) {
    const QString& file_path = args.first();
    QTimer::singleShot(0, [&window, file_path]() { window.load_file(file_path); });
  }

  return QApplication::exec();
}
