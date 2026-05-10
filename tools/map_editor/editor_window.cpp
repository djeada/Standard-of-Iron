#include "editor_window.h"
#include "json_edit_dialog.h"
#include "resize_dialog.h"
#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace MapEditor {

EditorWindow::EditorWindow(QWidget *parent) : QMainWindow(parent) {
  m_map_data = new MapData(this);

  setupUI();
  setupMenus();

  connect(m_map_data, &MapData::modifiedChanged, this,
          &EditorWindow::onModifiedChanged);
  connect(m_map_data, &MapData::undoRedoChanged, this,
          &EditorWindow::onUndoRedoChanged);
  connect(m_map_data, &MapData::dataChanged, this,
          &EditorWindow::updateDimensionsLabel);

  setWindowTitle("Standard of Iron - Map Editor");
  resize(1400, 900);

  newMap();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setupUI() {
  auto *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  auto *mainLayout = new QHBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  m_tool_panel = new ToolPanel(this);
  connect(m_tool_panel, &ToolPanel::toolSelected, this,
          &EditorWindow::onToolSelected);

  m_canvas = new MapCanvas(this);
  m_canvas->setMapData(m_map_data);
  connect(m_canvas, &MapCanvas::elementDoubleClicked, this,
          &EditorWindow::onElementDoubleClicked);
  connect(m_canvas, &MapCanvas::gridDoubleClicked, this,
          &EditorWindow::onGridDoubleClicked);
  connect(m_canvas, &MapCanvas::toolCleared, this,
          &EditorWindow::onToolCleared);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(m_tool_panel);
  splitter->addWidget(m_canvas);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);

  mainLayout->addWidget(splitter);

  m_status_label = new QLabel("Ready", this);
  m_dimensions_label = new QLabel("", this);
  m_dimensions_label->setToolTip(
      "Double-click on empty canvas area to edit dimensions");
  statusBar()->addWidget(m_status_label);
  statusBar()->addPermanentWidget(m_dimensions_label);
}

void EditorWindow::setupMenus() {

  auto *fileMenu = menuBar()->addMenu("&File");

  auto *newAction = new QAction("&New", this);
  newAction->setShortcut(QKeySequence::New);
  connect(newAction, &QAction::triggered, this, &EditorWindow::newMap);
  fileMenu->addAction(newAction);

  auto *openAction = new QAction("&Open...", this);
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &EditorWindow::openMap);
  fileMenu->addAction(openAction);

  fileMenu->addSeparator();

  auto *saveAction = new QAction("&Save", this);
  saveAction->setShortcut(QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, &EditorWindow::saveMap);
  fileMenu->addAction(saveAction);

  auto *saveAsAction = new QAction("Save &As...", this);
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  connect(saveAsAction, &QAction::triggered, this, &EditorWindow::saveMapAs);
  fileMenu->addAction(saveAsAction);

  fileMenu->addSeparator();

  auto *exitAction = new QAction("E&xit", this);
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QWidget::close);
  fileMenu->addAction(exitAction);

  auto *editMenu = menuBar()->addMenu("&Edit");

  m_undo_action = new QAction("&Undo", this);
  m_undo_action->setShortcut(QKeySequence::Undo);
  m_undo_action->setEnabled(false);
  connect(m_undo_action, &QAction::triggered, this, &EditorWindow::undo);
  editMenu->addAction(m_undo_action);

  m_redo_action = new QAction("&Redo", this);
  m_redo_action->setShortcut(QKeySequence::Redo);
  m_redo_action->setEnabled(false);
  connect(m_redo_action, &QAction::triggered, this, &EditorWindow::redo);
  editMenu->addAction(m_redo_action);

  editMenu->addSeparator();

  auto *resizeAction = new QAction("&Resize Map...", this);
  connect(resizeAction, &QAction::triggered, this, &EditorWindow::resizeMap);
  editMenu->addAction(resizeAction);

  auto *toolbar = addToolBar("Main");
  toolbar->addAction(newAction);
  toolbar->addAction(openAction);
  toolbar->addAction(saveAction);
  toolbar->addSeparator();
  toolbar->addAction(m_undo_action);
  toolbar->addAction(m_redo_action);
  toolbar->addSeparator();
  toolbar->addAction(resizeAction);
}

void EditorWindow::newMap() {
  if (!maybeSave()) {
    return;
  }

  m_map_data->clear();
  m_current_file_path.clear();
  updateWindowTitle();
  m_status_label->setText("New map created");
}

void EditorWindow::openMap() {
  if (!maybeSave()) {
    return;
  }

  QString filePath = QFileDialog::getOpenFileName(
      this, "Open Map", QString(), "JSON Files (*.json);;All Files (*)");

  if (filePath.isEmpty()) {
    return;
  }

  if (m_map_data->loadFromJson(filePath)) {
    m_current_file_path = filePath;
    updateWindowTitle();
    m_status_label->setText("Loaded: " + filePath);
  } else {
    QMessageBox::critical(this, "Error",
                          "Failed to load map file: " + filePath);
  }
}

bool EditorWindow::loadFile(const QString &filePath) {
  if (m_map_data->loadFromJson(filePath)) {
    m_current_file_path = filePath;
    updateWindowTitle();
    m_status_label->setText("Loaded: " + filePath);
    return true;
  }
  QMessageBox::critical(this, "Error", "Failed to load map file: " + filePath);
  return false;
}

void EditorWindow::saveMap() {
  if (m_current_file_path.isEmpty()) {
    saveMapAs();
  } else {
    if (m_map_data->saveToJson(m_current_file_path)) {
      m_map_data->setModified(false);
      m_status_label->setText("Saved: " + m_current_file_path);
    } else {
      QMessageBox::critical(this, "Error",
                            "Failed to save map file: " + m_current_file_path);
    }
  }
}

void EditorWindow::saveMapAs() {
  QString filePath = QFileDialog::getSaveFileName(
      this, "Save Map As", QString(), "JSON Files (*.json);;All Files (*)");

  if (filePath.isEmpty()) {
    return;
  }

  if (!filePath.endsWith(".json", Qt::CaseInsensitive)) {
    filePath += ".json";
  }

  if (m_map_data->saveToJson(filePath)) {
    m_current_file_path = filePath;
    m_map_data->setModified(false);
    updateWindowTitle();
    m_status_label->setText("Saved: " + filePath);
  } else {
    QMessageBox::critical(this, "Error",
                          "Failed to save map file: " + filePath);
  }
}

void EditorWindow::resizeMap() {
  const GridSettings &grid = m_map_data->grid();
  ResizeDialog dialog(grid.width, grid.height, this);

  if (dialog.exec() == QDialog::Accepted) {
    GridSettings new_grid = grid;
    new_grid.width = dialog.newWidth();
    new_grid.height = dialog.newHeight();
    m_map_data->setGrid(new_grid);
    m_canvas->update();
    m_status_label->setText(
        QString("Map resized to %1x%2").arg(new_grid.width).arg(new_grid.height));
  }
}

void EditorWindow::undo() {
  m_map_data->undo();
  m_status_label->setText("Undo");
}

void EditorWindow::redo() {
  m_map_data->redo();
  m_status_label->setText("Redo");
}

void EditorWindow::onToolSelected(ToolType tool) {
  m_canvas->setCurrentTool(tool);

  QString toolName;
  switch (tool) {
  case ToolType::Select:
    toolName = "Select";
    break;
  case ToolType::Hill:
    toolName = "Hill";
    break;
  case ToolType::Mountain:
    toolName = "Mountain";
    break;
  case ToolType::River:
    toolName = "River (click start, then end)";
    break;
  case ToolType::Road:
    toolName = "Road (click start, then end)";
    break;
  case ToolType::Bridge:
    toolName = "Bridge (click start, then end)";
    break;
  case ToolType::Firecamp:
    toolName = "Firecamp";
    break;
  case ToolType::Barracks:
    toolName = "Barracks (assign to team)";
    break;
  case ToolType::Village:
    toolName = "Village (assign to team)";
    break;
  case ToolType::Eraser:
    toolName = "Eraser";
    break;
  }

  m_status_label->setText("Tool: " + toolName);
}

void EditorWindow::onToolCleared() {
  m_tool_panel->clearSelection();
  m_status_label->setText("Tool: Select");
}

void EditorWindow::onGridDoubleClicked() { resizeMap(); }

void EditorWindow::onUndoRedoChanged() {
  m_undo_action->setEnabled(m_map_data->canUndo());
  m_redo_action->setEnabled(m_map_data->canRedo());
}

void EditorWindow::updateDimensionsLabel() {
  const GridSettings &grid = m_map_data->grid();
  m_dimensions_label->setText(
      QString("Map: %1 x %2").arg(grid.width).arg(grid.height));
}

void EditorWindow::onElementDoubleClicked(int elementType, int index) {
  QJsonObject json;
  QString title;

  if (elementType == 0) {

    const auto &terrain = m_map_data->terrainElements();
    if (index < 0 || index >= terrain.size()) {
      return;
    }
    const auto &elem = terrain[index];

    json["type"] = elem.type;
    json["x"] = static_cast<double>(elem.x);
    json["z"] = static_cast<double>(elem.z);
    json["radius"] = static_cast<double>(elem.radius);
    json["width"] = static_cast<double>(elem.width);
    json["depth"] = static_cast<double>(elem.depth);
    json["height"] = static_cast<double>(elem.height);
    json["rotation"] = static_cast<double>(elem.rotation);
    if (!elem.entrances.isEmpty()) {
      json["entrances"] = elem.entrances;
    }
    for (const QString &key : elem.extraFields.keys()) {
      json[key] = elem.extraFields[key];
    }

    title = "Edit Terrain: " + elem.type;
  } else if (elementType == 1) {

    const auto &firecamps = m_map_data->firecamps();
    if (index < 0 || index >= firecamps.size()) {
      return;
    }
    const auto &elem = firecamps[index];

    json["x"] = static_cast<double>(elem.x);
    json["z"] = static_cast<double>(elem.z);
    json["intensity"] = static_cast<double>(elem.intensity);
    json["radius"] = static_cast<double>(elem.radius);
    for (const QString &key : elem.extraFields.keys()) {
      json[key] = elem.extraFields[key];
    }

    title = "Edit Firecamp";
  } else if (elementType == 2) {

    const auto &linear = m_map_data->linearElements();
    if (index < 0 || index >= linear.size()) {
      return;
    }
    const auto &elem = linear[index];

    json["type"] = elem.type;
    json["start"] = QJsonArray{static_cast<double>(elem.start.x()),
                               static_cast<double>(elem.start.y())};
    json["end"] = QJsonArray{static_cast<double>(elem.end.x()),
                             static_cast<double>(elem.end.y())};
    json["width"] = static_cast<double>(elem.width);
    if (elem.type == "bridge") {
      json["height"] = static_cast<double>(elem.height);
    }
    if (elem.type == "road" && !elem.style.isEmpty()) {
      json["style"] = elem.style;
    }
    for (const QString &key : elem.extraFields.keys()) {
      json[key] = elem.extraFields[key];
    }

    title = "Edit " + elem.type;
  } else if (elementType == 3) {

    const auto &structures = m_map_data->structures();
    if (index < 0 || index >= structures.size()) {
      return;
    }
    const auto &elem = structures[index];

    json["type"] = elem.type;
    json["x"] = static_cast<double>(elem.x);
    json["z"] = static_cast<double>(elem.z);
    json["player_id"] = elem.player_id;
    json["max_population"] = elem.max_population;
    if (!elem.nation.isEmpty()) {
      json["nation"] = elem.nation;
    }
    for (const QString &key : elem.extraFields.keys()) {
      json[key] = elem.extraFields[key];
    }

    title = "Edit " + elem.type;
  } else {
    return;
  }

  JsonEditDialog dialog(title, json, this);
  if (dialog.exec() == QDialog::Accepted && dialog.isValid()) {
    QJsonObject newJson = dialog.getJson();

    if (elementType == 0) {
      TerrainElement elem;
      elem.type = newJson["type"].toString();
      elem.x = static_cast<float>(newJson["x"].toDouble());
      elem.z = static_cast<float>(newJson["z"].toDouble());
      elem.radius = static_cast<float>(newJson["radius"].toDouble(10.0));
      elem.width = static_cast<float>(newJson["width"].toDouble(0.0));
      elem.depth = static_cast<float>(newJson["depth"].toDouble(0.0));
      elem.height = static_cast<float>(newJson["height"].toDouble(3.0));
      elem.rotation = static_cast<float>(newJson["rotation"].toDouble(0.0));
      elem.entrances = newJson["entrances"].toArray();

      QStringList knownKeys = {"type",   "x",        "z",
                               "radius", "width",    "depth",
                               "height", "rotation", "entrances"};
      for (const QString &key : newJson.keys()) {
        if (!knownKeys.contains(key)) {
          elem.extraFields[key] = newJson[key];
        }
      }

      m_map_data->updateTerrainElement(index, elem);
    } else if (elementType == 1) {
      FirecampElement elem;
      elem.x = static_cast<float>(newJson["x"].toDouble());
      elem.z = static_cast<float>(newJson["z"].toDouble());
      elem.intensity = static_cast<float>(newJson["intensity"].toDouble(1.0));
      elem.radius = static_cast<float>(newJson["radius"].toDouble(3.0));

      QStringList knownKeys = {"x", "z", "intensity", "radius"};
      for (const QString &key : newJson.keys()) {
        if (!knownKeys.contains(key)) {
          elem.extraFields[key] = newJson[key];
        }
      }

      m_map_data->updateFirecamp(index, elem);
    } else if (elementType == 2) {
      LinearElement elem;
      elem.type = newJson["type"].toString();

      QJsonArray startArr = newJson["start"].toArray();
      QJsonArray endArr = newJson["end"].toArray();
      if (startArr.size() >= 2 && endArr.size() >= 2) {
        elem.start = QVector2D(static_cast<float>(startArr[0].toDouble()),
                               static_cast<float>(startArr[1].toDouble()));
        elem.end = QVector2D(static_cast<float>(endArr[0].toDouble()),
                             static_cast<float>(endArr[1].toDouble()));
      }
      elem.width = static_cast<float>(newJson["width"].toDouble(3.0));
      elem.height = static_cast<float>(newJson["height"].toDouble(0.5));
      elem.style = newJson["style"].toString("default");

      QStringList knownKeys = {"type",  "start",  "end",
                               "width", "height", "style"};
      for (const QString &key : newJson.keys()) {
        if (!knownKeys.contains(key)) {
          elem.extraFields[key] = newJson[key];
        }
      }

      m_map_data->updateLinearElement(index, elem);
    } else if (elementType == 3) {
      StructureElement elem;
      elem.type = newJson["type"].toString();
      elem.x = static_cast<float>(newJson["x"].toDouble());
      elem.z = static_cast<float>(newJson["z"].toDouble());
      elem.player_id = newJson["player_id"].toInt(0);
      elem.max_population = newJson["max_population"].toInt(150);
      elem.nation = newJson["nation"].toString();

      QStringList knownKeys = {"type",          "x",     "z", "player_id",
                               "max_population", "nation"};
      for (const QString &key : newJson.keys()) {
        if (!knownKeys.contains(key)) {
          elem.extraFields[key] = newJson[key];
        }
      }

      m_map_data->updateStructure(index, elem);
    }
  }
}

void EditorWindow::onModifiedChanged(bool modified) {
  Q_UNUSED(modified)
  updateWindowTitle();
}

void EditorWindow::updateWindowTitle() {
  QString title = "Standard of Iron - Map Editor";
  if (!m_current_file_path.isEmpty()) {
    title += " - " + QFileInfo(m_current_file_path).fileName();
  } else {
    title += " - " + m_map_data->name();
  }
  if (m_map_data->isModified()) {
    title += " *";
  }
  setWindowTitle(title);
}

bool EditorWindow::maybeSave() {
  if (!m_map_data->isModified()) {
    return true;
  }

  QMessageBox::StandardButton ret = QMessageBox::warning(
      this, "Unsaved Changes",
      "The map has been modified.\nDo you want to save your changes?",
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

  if (ret == QMessageBox::Save) {
    saveMap();
    return !m_map_data->isModified();
  }
  if (ret == QMessageBox::Cancel) {
    return false;
  }
  return true;
}

void EditorWindow::closeEvent(QCloseEvent *event) {
  if (maybeSave()) {
    event->accept();
  } else {
    event->ignore();
  }
}

} // namespace MapEditor