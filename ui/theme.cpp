#include "theme.h"

#include <QString>
#include <qglobal.h>
#include <qjsengine.h>
#include <qjsonarray.h>
#include <qobject.h>
#include <qqmlengine.h>

#include <algorithm>
#include <cmath>

#include "preferences.h"

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
  auto* theme = instance();

  QQmlEngine::setObjectOwnership(theme, QQmlEngine::CppOwnership);
  return theme;
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

  return {};
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
  const auto* prefs = UiPreferences::instance();
  const bool high_contrast = prefs->high_contrast();
  const double scale = prefs->ui_scale();

  const QColor surface = high_contrast ? QColor("#050505") : backgroundDeep();
  const QColor raised = high_contrast ? QColor("#181818") : panelIron();
  const QColor panel = high_contrast ? QColor("#111111") : panelLeather();
  const QColor text = high_contrast ? QColor("#ffffff") : textPrimary();
  const QColor muted = high_contrast ? QColor("#c8c8c8") : textDisabled();
  const QColor line = high_contrast ? QColor("#8c8c8c") : borderSubtle();
  const QColor emphasis = high_contrast ? QColor("#f0c674") : accent();

  const auto hex = [](const QColor& color) {
    return color.name(QColor::HexRgb);
  };

  const auto px = [scale](int base) {
    return QString::number(std::max(1, static_cast<int>(std::lround(base * scale)))) +
           QStringLiteral("px");
  };

  return QStringLiteral(R"(
QWidget {
  background-color: %1;
  color: %2;
  font-family: "Noto Sans", "DejaVu Sans", sans-serif;
  font-size: %16;
}
QMainWindow, QDialog { background-color: %1; }
QMenuBar, QMenu, QToolBar, QStatusBar {
  background-color: %3;
  color: %2;
  border-color: %4;
}
QMenuBar { border-bottom: 1px solid %4; }
QMenuBar::item, QMenu::item { padding: %17 %18; }
QMenuBar::item:selected, QMenu::item:selected { background-color: %5; }
QMenu::separator { height: 1px; background: %4; margin: %17 0; }
QToolBar {
  border: none;
  border-bottom: 1px solid %4;
  spacing: %17;
  padding: %17 %18;
}
QToolBar::separator { background: %4; width: 1px; margin: %17 4px; }
QToolButton, QPushButton {
  background-color: %6;
  color: %2;
  border: 1px solid %4;
  border-radius: 4px;
  padding: %17 %18;
  min-height: %19;
}
QToolButton:hover, QPushButton:hover {
  background-color: %5;
  border-color: %7;
}
QToolButton:focus, QPushButton:focus,
QComboBox:focus, QLineEdit:focus, QPlainTextEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus, QCheckBox:focus, QRadioButton:focus,
QListWidget:focus, QListView:focus, QTreeWidget:focus, QTreeView:focus,
QTableWidget:focus, QTableView:focus, QSlider:focus {
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
  margin-top: %18;
  padding-top: %18;
  font-weight: 600;
}
QGroupBox::title { color: %7; subcontrol-origin: margin; left: %17; padding: 0 %17; }
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit, QTextEdit {
  background-color: %6;
  color: %2;
  border: 1px solid %4;
  border-radius: 4px;
  padding: 4px %17;
  min-height: %19;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover,
QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover { border-color: %7; }
QComboBox QAbstractItemView {
  background-color: %6;
  color: %2;
  border: 1px solid %4;
  selection-background-color: %9;
}
QCheckBox, QRadioButton { spacing: %17; padding: 2px; }
QCheckBox::indicator, QRadioButton::indicator {
  width: %19;
  height: %19;
  border: 1px solid %4;
  background: %6;
}
QCheckBox::indicator { border-radius: 3px; }
QRadioButton::indicator { border-radius: 50%; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: %7; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
  background: %7;
  border-color: %8;
}
QCheckBox:disabled, QRadioButton:disabled { color: %11; }
QListWidget, QListView, QTreeWidget, QTreeView, QTableWidget, QTableView {
  background-color: %6;
  alternate-background-color: %3;
  color: %2;
  border: 1px solid %4;
  border-radius: 4px;
}
QListWidget::item, QListView::item, QTreeWidget::item, QTreeView::item {
  padding: 4px %17;
  border: none;
}
QListWidget::item:hover, QListView::item:hover,
QTreeWidget::item:hover, QTreeView::item:hover { background: %5; }
QListWidget::item:selected, QListView::item:selected,
QTreeWidget::item:selected, QTreeView::item:selected,
QTableWidget::item:selected, QTableView::item:selected {
  background: %9;
  color: %2;
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
  padding: %17 %18;
  min-width: 62px;
}
QTabBar::tab:selected {
  background: %1;
  color: %2;
  border-bottom: 2px solid %8;
}
QTabBar::tab:hover:!selected { background: %5; color: %2; }
QSlider::groove:horizontal {
  height: 6px;
  background: %6;
  border: 1px solid %4;
  border-radius: 3px;
}
QSlider::sub-page:horizontal { background: %7; border-radius: 3px; }
QSlider::handle:horizontal {
  width: %19;
  margin: -6px 0;
  background: %13;
  border: 1px solid %8;
  border-radius: 4px;
}
QProgressBar {
  background: %6;
  border: 1px solid %4;
  border-radius: 4px;
  text-align: center;
  color: %2;
}
QProgressBar::chunk { background: %7; border-radius: 3px; }
QDockWidget {
  color: %2;
  titlebar-close-icon: none;
  titlebar-normal-icon: none;
}
QDockWidget::title {
  background: %3;
  padding: %17;
  border-bottom: 1px solid %4;
}
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
QStatusBar::item { border: none; }
QLabel { background: transparent; }
QLabel#panelTitle { color: %13; font-size: %20; font-weight: 700; }
QLabel#panelIntro, QLabel#panelHint { color: %11; }
QLabel#toolSummary {
  background: %3;
  border: 1px solid %4;
  border-radius: 6px;
  color: %13;
  padding: %17 %18;
}
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
      .arg(hex(surface),
           hex(text),
           hex(panel),
           hex(line),
           hex(hoverBg()),
           hex(raised),
           hex(emphasis),
           hex(selection()),
           hex(selectedBg()),
           hex(disabledBg()),
           hex(muted),
           hex(danger()),
           hex(accentBright()),
           hex(success()),
           hex(warning()))
      .arg(px(12), px(6), px(10), px(26), px(16));
}
