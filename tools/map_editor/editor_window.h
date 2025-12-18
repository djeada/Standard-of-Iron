#pragma once

#include <QMainWindow>
#include <QWidget>

namespace MapEditor {

class EditorWindow : public QMainWindow {
  Q_OBJECT

public:
  EditorWindow(QWidget *parent = nullptr);
  ~EditorWindow() override;

private slots:
  void newMap();
  void openMap();
  void saveMap();

  void setupUI();
  void setupMenus();

private:
  QWidget *m_renderWidget{};
};

} 
