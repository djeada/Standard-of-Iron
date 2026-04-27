#pragma once

#include <QGroupBox>

class TerrainPanel : public QGroupBox {
  Q_OBJECT

public:
  explicit TerrainPanel(QWidget *parent = nullptr);

signals:
  void seedChanged(int seed);
  void heightScaleChanged(float value);
  void octavesChanged(int value);
  void frequencyChanged(float value);
  void regenerateRequested();
  void wireframeToggled(bool enabled);
  void normalsToggled(bool enabled);
};
