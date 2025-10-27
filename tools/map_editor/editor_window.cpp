#include "editor_window.h"
#include <QAction>
#include <QLabel>
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <qaction.h>
#include <qboxlayout.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <qnamespace.h>
#include <qwidget.h>

namespace MapEditor {

EditorWindow::EditorWindow(QWidget *parent) : QMainWindow(parent) {
  setupUI();
  setupMenus();

  setWindowTitle("Standard of Iron - Map Editor");
  resize(1200, 800);
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setupUI() {
  auto *central_widget = new QWidget(this);
  setCentralWidget(central_widget);

  auto *layout = new QVBoxLayout(central_widget);

  m_renderWidget =
      new QLabel("Map Editor Render Area\n(OpenGL widget would go here)", this);
  dynamic_cast<QLabel *>(m_renderWidget)->setAlignment(Qt::AlignCenter);
  dynamic_cast<QLabel *>(m_renderWidget)
      ->setStyleSheet("QLabel { background-color: #2c3e50; color: white; "
                      "border: 1px solid #34495e; }");
  layout->addWidget(m_renderWidget);
}

void EditorWindow::setupMenus() {
  auto *file_menu = menuBar()->addMenu("&File");

  auto *new_action = new QAction("&New", this);
  new_action->setShortcut(QKeySequence::New);
  connect(new_action, &QAction::triggered, this, &EditorWindow::newMap);
  file_menu->addAction(new_action);

  auto *open_action = new QAction("&Open", this);
  open_action->setShortcut(QKeySequence::Open);
  connect(open_action, &QAction::triggered, this, &EditorWindow::openMap);
  file_menu->addAction(open_action);

  auto *save_action = new QAction("&Save", this);
  save_action->setShortcut(QKeySequence::Save);
  connect(save_action, &QAction::triggered, this, &EditorWindow::saveMap);
  file_menu->addAction(save_action);

  file_menu->addSeparator();

  auto *exit_action = new QAction("E&xit", this);
  exit_action->setShortcut(QKeySequence::Quit);
  connect(exit_action, &QAction::triggered, this, &QWidget::close);
  file_menu->addAction(exit_action);

  auto *toolbar = addToolBar("Main");
  toolbar->addAction(new_action);
  toolbar->addAction(open_action);
  toolbar->addAction(save_action);
}

void EditorWindow::newMap() {}

void EditorWindow::openMap() {}

void EditorWindow::saveMap() {}

} // namespace MapEditor