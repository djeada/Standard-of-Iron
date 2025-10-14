#pragma once

namespace Game {

struct CameraConfig {
  float defaultDistance = 12.0f;
  float defaultPitch = 45.0f;
  float defaultYaw = 225.0f;
  float orbitStepNormal = 4.0f;
  float orbitStepShift = 8.0f;
};

struct ArrowConfig {
  float arcHeightMultiplier = 0.15f;
  float arcHeightMin = 0.2f;
  float arcHeightMax = 1.2f;
  float speedDefault = 8.0f;
  float speedAttack = 6.0f;
};

struct GameplayConfig {
  float visibilityUpdateInterval = 0.075f;
  float formationSpacingDefault = 1.0f;
  int maxTroopsPerPlayer = 50;
};

class GameConfig {
public:
  static GameConfig &instance() noexcept {
    static GameConfig inst;
    return inst;
  }

  [[nodiscard]] const CameraConfig &camera() const noexcept { return m_camera; }
  [[nodiscard]] const ArrowConfig &arrow() const noexcept { return m_arrow; }
  [[nodiscard]] const GameplayConfig &gameplay() const noexcept {
    return m_gameplay;
  }

  [[nodiscard]] float getCameraDefaultDistance() const noexcept {
    return m_camera.defaultDistance;
  }
  [[nodiscard]] float getCameraDefaultPitch() const noexcept {
    return m_camera.defaultPitch;
  }
  [[nodiscard]] float getCameraDefaultYaw() const noexcept {
    return m_camera.defaultYaw;
  }

  [[nodiscard]] float getArrowArcHeightMultiplier() const noexcept {
    return m_arrow.arcHeightMultiplier;
  }
  [[nodiscard]] float getArrowArcHeightMin() const noexcept {
    return m_arrow.arcHeightMin;
  }
  [[nodiscard]] float getArrowArcHeightMax() const noexcept {
    return m_arrow.arcHeightMax;
  }

  [[nodiscard]] float getArrowSpeedDefault() const noexcept {
    return m_arrow.speedDefault;
  }
  [[nodiscard]] float getArrowSpeedAttack() const noexcept {
    return m_arrow.speedAttack;
  }

  [[nodiscard]] float getVisibilityUpdateInterval() const noexcept {
    return m_gameplay.visibilityUpdateInterval;
  }

  [[nodiscard]] float getFormationSpacingDefault() const noexcept {
    return m_gameplay.formationSpacingDefault;
  }

  [[nodiscard]] float getCameraOrbitStepNormal() const noexcept {
    return m_camera.orbitStepNormal;
  }
  [[nodiscard]] float getCameraOrbitStepShift() const noexcept {
    return m_camera.orbitStepShift;
  }

  void setCameraDefaultDistance(float value) noexcept {
    m_camera.defaultDistance = value;
  }
  void setCameraDefaultPitch(float value) noexcept {
    m_camera.defaultPitch = value;
  }
  void setCameraDefaultYaw(float value) noexcept {
    m_camera.defaultYaw = value;
  }

  void setArrowArcHeightMultiplier(float value) noexcept {
    m_arrow.arcHeightMultiplier = value;
  }
  void setArrowArcHeightMin(float value) noexcept {
    m_arrow.arcHeightMin = value;
  }
  void setArrowArcHeightMax(float value) noexcept {
    m_arrow.arcHeightMax = value;
  }

  void setArrowSpeedDefault(float value) noexcept {
    m_arrow.speedDefault = value;
  }
  void setArrowSpeedAttack(float value) noexcept {
    m_arrow.speedAttack = value;
  }

  void setVisibilityUpdateInterval(float value) noexcept {
    m_gameplay.visibilityUpdateInterval = value;
  }

  void setFormationSpacingDefault(float value) noexcept {
    m_gameplay.formationSpacingDefault = value;
  }

  void setCameraOrbitStepNormal(float value) noexcept {
    m_camera.orbitStepNormal = value;
  }
  void setCameraOrbitStepShift(float value) noexcept {
    m_camera.orbitStepShift = value;
  }

  [[nodiscard]] int getMaxTroopsPerPlayer() const noexcept {
    return m_gameplay.maxTroopsPerPlayer;
  }

  void setMaxTroopsPerPlayer(int value) noexcept {
    m_gameplay.maxTroopsPerPlayer = value;
  }

private:
  GameConfig() = default;

  CameraConfig m_camera;
  ArrowConfig m_arrow;
  GameplayConfig m_gameplay;
};

} // namespace Game
