#include "theme.h"

#include <QString>
#include <qglobal.h>
#include <qjsengine.h>
#include <qjsonarray.h>
#include <qobject.h>
#include <qqmlengine.h>

Theme* Theme::m_instance = nullptr;

Theme::Theme(QObject* parent)
    : QObject(parent) {
}

auto Theme::instance() -> Theme* {
  if (m_instance == nullptr) {
    m_instance = new Theme();
  }
  return m_instance;
}

auto Theme::create(QQmlEngine* engine, QJSEngine* scriptEngine) -> Theme* {
  Q_UNUSED(engine)
  Q_UNUSED(scriptEngine)
  return instance();
}

QVariantList Theme::playerColors() {
  QVariantList colors;
  colors.append(QVariantMap{{"name", "Red"}, {"hex", "#E74C3C"}});
  colors.append(QVariantMap{{"name", "Blue"}, {"hex", "#3498DB"}});
  colors.append(QVariantMap{{"name", "Brown"}, {"hex", "#8B4513"}});
  colors.append(QVariantMap{{"name", "Green"}, {"hex", "#2ECC71"}});
  colors.append(QVariantMap{{"name", "Yellow"}, {"hex", "#F1C40F"}});
  colors.append(QVariantMap{{"name", "Orange"}, {"hex", "#E67E22"}});
  colors.append(QVariantMap{{"name", "Purple"}, {"hex", "#9B59B6"}});
  colors.append(QVariantMap{{"name", "Cyan"}, {"hex", "#1ABC9C"}});
  colors.append(QVariantMap{{"name", "Pink"}, {"hex", "#E91E63"}});
  return colors;
}

QVariantList Theme::teamIcons() {
  QVariantList icons;
  icons << "⚪" << "①" << "②" << "③" << "④" << "⑤" << "⑥" << "⑦" << "⑧";
  return icons;
}

QVariantList Theme::factions() {
  QVariantList factions_data;
  factions_data.append(QVariantMap{{"id", 0}, {"name", "Standard"}});
  factions_data.append(QVariantMap{{"id", 1}, {"name", "Romans"}});
  factions_data.append(QVariantMap{{"id", 2}, {"name", "Egyptians"}});
  factions_data.append(QVariantMap{{"id", 3}, {"name", "Barbarians"}});
  return factions_data;
}

QVariantMap Theme::unitIcons() {
  QVariantMap icons;
  icons["archer"] = "🏹";
  icons["swordsman"] = "⚔️";
  icons["warrior"] = "⚔️";
  icons["spearman"] = "🛡️";
  icons["cavalry"] = "🐎";
  icons["default"] = "👤";
  return icons;
}

QVariantMap Theme::nationEmblems() {
  QVariantMap emblems;
  constexpr auto k_resource_prefix =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      "qrc:/StandardOfIron/assets/visuals/emblems/";
#else
      "qrc:/assets/visuals/emblems/";
#endif
  emblems["roman_republic"] = QString::fromLatin1(k_resource_prefix) + "rome.png";
  emblems["carthage"] = QString::fromLatin1(k_resource_prefix) + "cartaghe.png";
  return emblems;
}

QString Theme::widgetStyleSheet() {
  const auto hex = [](const QColor& color) {
    return color.name(QColor::HexRgb);
  };
  return QStringLiteral(R"(
QWidget {
  background-color: %1;
  color: %2;
  font-family: "Noto Sans", "DejaVu Sans", sans-serif;
  font-size: 12px;
}
QMainWindow, QDialog { background-color: %1; }
QMenuBar, QMenu, QToolBar, QStatusBar {
  background-color: %3;
  color: %2;
  border-color: %4;
}
QMenuBar { border-bottom: 1px solid %4; }
QMenuBar::item, QMenu::item { padding: 6px 10px; }
QMenuBar::item:selected, QMenu::item:selected { background-color: %5; }
QToolBar {
  border: none;
  border-bottom: 1px solid %4;
  spacing: 6px;
  padding: 3px 6px;
}
QToolBar::separator { background: %4; width: 1px; margin: 4px 3px; }
QToolButton, QPushButton {
  background-color: %6;
  color: %2;
  border: 1px solid %4;
  border-radius: 4px;
  padding: 5px 10px;
  min-height: 26px;
}
QToolButton:hover, QPushButton:hover {
  background-color: %5;
  border-color: %7;
}
QToolButton:focus, QPushButton:focus,
QComboBox:focus, QLineEdit:focus, QPlainTextEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus {
  border: 2px solid %8;
}
QToolButton:pressed, QPushButton:pressed {
  background-color: %9;
}
QToolButton:checked {
  background-color: %9;
  border-color: %8;
}
QToolButton:disabled, QPushButton:disabled {
  background-color: %10;
  color: %11;
  border-color: %4;
}
QPushButton[primary="true"] {
  background-color: %9;
  border: 1px solid %8;
  font-weight: 700;
}
QPushButton[destructive="true"] {
  color: %12;
  background: transparent;
  border-color: %12;
}
QPushButton[destructive="true"]:hover {
  color: %2;
  background: %12;
}
QGroupBox {
  background-color: %3;
  border: 1px solid %4;
  border-radius: 5px;
  margin-top: 10px;
  padding-top: 10px;
  font-weight: 600;
}
QGroupBox::title { color: %7; subcontrol-origin: margin; left: 8px; padding: 0 6px; }
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit, QTableWidget {
  background-color: %6;
  color: %2;
  border: 1px solid %4;
  border-radius: 4px;
  padding: 4px 8px;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover,
QLineEdit:hover, QPlainTextEdit:hover { border-color: %7; }
QComboBox QAbstractItemView {
  background-color: %6;
  color: %2;
  border: 1px solid %4;
  selection-background-color: %9;
}
QHeaderView::section {
  background: %3;
  color: %13;
  border: 0;
  border-right: 1px solid %4;
  border-bottom: 1px solid %4;
  padding: 5px;
}
QTabWidget::pane { border: 1px solid %4; background: %1; }
QTabBar::tab {
  background: %3;
  color: %11;
  border: 1px solid %4;
  padding: 6px 13px;
  min-width: 62px;
}
QTabBar::tab:selected {
  background: %1;
  color: %2;
  border-bottom: 2px solid %8;
}
QTabBar::tab:hover:!selected { background: %5; color: %2; }
QSplitter::handle { background: %4; }
QSplitter::handle:hover { background: %7; }
QScrollArea { border: none; background: transparent; }
QScrollBar:vertical, QScrollBar:horizontal { background: %1; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
  background: %4;
  border-radius: 4px;
  min-height: 20px;
  min-width: 20px;
}
QScrollBar::handle:hover { background: %7; }
QStatusBar { border-top: 1px solid %4; color: %11; }
QLabel { background: transparent; }
QLabel#panelTitle { color: %13; font-size: 16px; font-weight: 700; }
QLabel#panelIntro, QLabel#panelHint { color: %11; }
QLabel[status="success"] { color: %14; }
QLabel[status="warning"] { color: %15; }
QLabel[status="error"] { color: %12; }
QLabel[status="muted"] { color: %11; }
QToolTip {
  color: %2;
  background: %3;
  border: 1px solid %7;
  padding: 5px 7px;
}
)")
      .arg(hex(backgroundDeep()),
           hex(textPrimary()),
           hex(panelLeather()),
           hex(borderSubtle()),
           hex(hoverBg()),
           hex(panelIron()),
           hex(accent()),
           hex(selection()),
           hex(selectedBg()),
           hex(disabledBg()),
           hex(textDisabled()),
           hex(danger()),
           hex(accentBright()),
           hex(success()),
           hex(warning()));
}
