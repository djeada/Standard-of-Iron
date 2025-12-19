#pragma once

#include <QQuickFramebufferObject>

class CampaignMapView : public QQuickFramebufferObject {
  Q_OBJECT
  Q_PROPERTY(float orbitYaw READ orbitYaw WRITE setOrbitYaw NOTIFY orbitYawChanged)
  Q_PROPERTY(float orbitPitch READ orbitPitch WRITE setOrbitPitch NOTIFY orbitPitchChanged)
  Q_PROPERTY(float orbitDistance READ orbitDistance WRITE setOrbitDistance NOTIFY orbitDistanceChanged)
public:
  CampaignMapView();

  [[nodiscard]] auto createRenderer() const -> Renderer * override;

  [[nodiscard]] auto orbitYaw() const -> float { return m_orbitYaw; }
  void setOrbitYaw(float yaw);

  [[nodiscard]] auto orbitPitch() const -> float { return m_orbitPitch; }
  void setOrbitPitch(float pitch);

  [[nodiscard]] auto orbitDistance() const -> float { return m_orbitDistance; }
  void setOrbitDistance(float distance);

signals:
  void orbitYawChanged();
  void orbitPitchChanged();
  void orbitDistanceChanged();

private:
  float m_orbitYaw = 180.0F;
  float m_orbitPitch = 55.0F;
  float m_orbitDistance = 2.4F;
};
