#include "editor_window.h"
#include <QAction>
#include <QLabel>
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace MapEditor {

EditorWindow::EditorWindow(QWidget *parent) : QMainWindow(parent) {
  setupUI();
  setupMenus();

  setWindowTitle("Standard of Iron - Map Editor");
  resize(1200, 800);
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setupUI() {
  auto centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  auto layout = new QVBoxLayout(centralWidget);

  m_renderWidget =
      new QLabel("Map Editor Render Area\n(OpenGL widget would go here)", this);
  static_cast<QLabel *>(m_renderWidget)->setAlignment(Qt::AlignCenter);
  static_cast<QLabel *>(m_renderWidget)
      ->setStyleSheet("QLabel { background-color: #2c3e50; color: white; "
                      "border: 1px solid #34495e; }");
  layout->addWidget(m_renderWidget);
}

void EditorWindow::setupMenus() {
  auto fileMenu = menuBar()->addMenu("&File");

  auto newAction = new QAction("&New", this);
  newAction->setShortcut(QKeySequence::New);
  connect(newAction, &QAction::triggered, this, &EditorWindow::newMap);
  fileMenu->addAction(newAction);

  auto openAction = new QAction("&Open", this);
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &EditorWindow::openMap);
  fileMenu->addAction(openAction);

  auto saveAction = new QAction("&Save", this);
  saveAction->setShortcut(QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, &EditorWindow::saveMap);
  fileMenu->addAction(saveAction);

  fileMenu->addSeparator();

  auto exitAction = new QAction("E&xit", this);
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QWidget::close);
  fileMenu->addAction(exitAction);

  auto toolbar = addToolBar("Main");
  toolbar->addAction(newAction);
  toolbar->addAction(openAction);
  toolbar->addAction(saveAction);
}

void EditorWindow::newMap() {}

void EditorWindow::openMap() {}

void EditorWindow::saveMap() {}

} // namespace MapEditor