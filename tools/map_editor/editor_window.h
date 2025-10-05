#pragma once

#include <QMainWindow>
#include <QWidget>

namespace MapEditor {

class EditorWindow : public QMainWindow {
  Q_OBJECT

public:
  EditorWindow(QWidget *parent = nullptr);
  ~EditorWindow();

private slots:
  void newMap();
  void openMap();
  void saveMap();

private:
  void setupUI();
  void setupMenus();

  QWidget *m_renderWidget;
};

} // namespace MapEditor