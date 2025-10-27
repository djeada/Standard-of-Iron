#pragma once

namespace Game {

struct CameraConfig {
  float defaultDistance = 12.0F;
  float defaultPitch = 45.0F;
  float defaultYaw = 225.0F;
  float orbitStepNormal = 4.0F;
  float orbitStepShift = 8.0F;
};

struct ArrowConfig {
  float arcHeightMultiplier = 0.15F;
  float arcHeightMin = 0.2F;
  float arcHeightMax = 1.2F;
  float speedDefault = 8.0F;
  float speedAttack = 6.0F;
};

struct GameplayConfig {
  float visibility_update_interval = 0.075F;
  float formationSpacingDefault = 1.0F;
  int max_troops_per_player = 50;
};

class GameConfig {
public:
  static auto instance() noexcept -> GameConfig & {
    static GameConfig inst;
    return inst;
  }

  [[nodiscard]] auto camera() const noexcept -> const CameraConfig & {
    return m_camera;
  }
  [[nodiscard]] auto arrow() const noexcept -> const ArrowConfig & {
    return m_arrow;
  }
  [[nodiscard]] auto gameplay() const noexcept -> const GameplayConfig & {
    return m_gameplay;
  }

  [[nodiscard]] auto getCameraDefaultDistance() const noexcept -> float {
    return m_camera.defaultDistance;
  }
  [[nodiscard]] auto getCameraDefaultPitch() const noexcept -> float {
    return m_camera.defaultPitch;
  }
  [[nodiscard]] auto getCameraDefaultYaw() const noexcept -> float {
    return m_camera.defaultYaw;
  }

  [[nodiscard]] auto getArrowArcHeightMultiplier() const noexcept -> float {
    return m_arrow.arcHeightMultiplier;
  }
  [[nodiscard]] auto getArrowArcHeightMin() const noexcept -> float {
    return m_arrow.arcHeightMin;
  }
  [[nodiscard]] auto getArrowArcHeightMax() const noexcept -> float {
    return m_arrow.arcHeightMax;
  }

  [[nodiscard]] auto getArrowSpeedDefault() const noexcept -> float {
    return m_arrow.speedDefault;
  }
  [[nodiscard]] auto getArrowSpeedAttack() const noexcept -> float {
    return m_arrow.speedAttack;
  }

  [[nodiscard]] auto getVisibilityUpdateInterval() const noexcept -> float {
    return m_gameplay.visibility_update_interval;
  }

  [[nodiscard]] auto getFormationSpacingDefault() const noexcept -> float {
    return m_gameplay.formationSpacingDefault;
  }

  [[nodiscard]] auto getCameraOrbitStepNormal() const noexcept -> float {
    return m_camera.orbitStepNormal;
  }
  [[nodiscard]] auto getCameraOrbitStepShift() const noexcept -> float {
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
    m_gameplay.visibility_update_interval = value;
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

  [[nodiscard]] auto getMaxTroopsPerPlayer() const noexcept -> int {
    return m_gameplay.max_troops_per_player;
  }

  void setMaxTroopsPerPlayer(int value) noexcept {
    m_gameplay.max_troops_per_player = value;
  }

private:
  GameConfig() = default;

  CameraConfig m_camera;
  ArrowConfig m_arrow;
  GameplayConfig m_gameplay;
};

} // namespace Game
