#pragma once

namespace Game {

class GameConfig {
public:
  static GameConfig &instance() {
    static GameConfig inst;
    return inst;
  }

  float getCameraDefaultDistance() const { return m_cameraDefaultDistance; }
  float getCameraDefaultPitch() const { return m_cameraDefaultPitch; }
  float getCameraDefaultYaw() const { return m_cameraDefaultYaw; }

  float getArrowArcHeightMultiplier() const {
    return m_arrowArcHeightMultiplier;
  }
  float getArrowArcHeightMin() const { return m_arrowArcHeightMin; }
  float getArrowArcHeightMax() const { return m_arrowArcHeightMax; }

  float getArrowSpeedDefault() const { return m_arrowSpeedDefault; }
  float getArrowSpeedAttack() const { return m_arrowSpeedAttack; }

  float getVisibilityUpdateInterval() const {
    return m_visibilityUpdateInterval;
  }

  float getFormationSpacingDefault() const {
    return m_formationSpacingDefault;
  }

  float getCameraOrbitStepNormal() const { return m_cameraOrbitStepNormal; }
  float getCameraOrbitStepShift() const { return m_cameraOrbitStepShift; }

  void setCameraDefaultDistance(float value) { m_cameraDefaultDistance = value; }
  void setCameraDefaultPitch(float value) { m_cameraDefaultPitch = value; }
  void setCameraDefaultYaw(float value) { m_cameraDefaultYaw = value; }

  void setArrowArcHeightMultiplier(float value) {
    m_arrowArcHeightMultiplier = value;
  }
  void setArrowArcHeightMin(float value) { m_arrowArcHeightMin = value; }
  void setArrowArcHeightMax(float value) { m_arrowArcHeightMax = value; }

  void setArrowSpeedDefault(float value) { m_arrowSpeedDefault = value; }
  void setArrowSpeedAttack(float value) { m_arrowSpeedAttack = value; }

  void setVisibilityUpdateInterval(float value) {
    m_visibilityUpdateInterval = value;
  }

  void setFormationSpacingDefault(float value) {
    m_formationSpacingDefault = value;
  }

  void setCameraOrbitStepNormal(float value) {
    m_cameraOrbitStepNormal = value;
  }
  void setCameraOrbitStepShift(float value) {
    m_cameraOrbitStepShift = value;
  }

private:
  GameConfig()
      : m_cameraDefaultDistance(12.0f), m_cameraDefaultPitch(45.0f),
        m_cameraDefaultYaw(225.0f), m_arrowArcHeightMultiplier(0.15f),
        m_arrowArcHeightMin(0.2f), m_arrowArcHeightMax(1.2f),
        m_arrowSpeedDefault(8.0f), m_arrowSpeedAttack(6.0f),
        m_visibilityUpdateInterval(0.075f), m_formationSpacingDefault(1.0f),
        m_cameraOrbitStepNormal(4.0f), m_cameraOrbitStepShift(8.0f) {}

  float m_cameraDefaultDistance;
  float m_cameraDefaultPitch;
  float m_cameraDefaultYaw;

  float m_arrowArcHeightMultiplier;
  float m_arrowArcHeightMin;
  float m_arrowArcHeightMax;

  float m_arrowSpeedDefault;
  float m_arrowSpeedAttack;

  float m_visibilityUpdateInterval;

  float m_formationSpacingDefault;

  float m_cameraOrbitStepNormal;
  float m_cameraOrbitStepShift;
};

} // namespace Game
