#pragma once

#include <QPointF>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVector2D>
#include <vector>

class CampaignMapView : public QQuickFramebufferObject {
  Q_OBJECT
  Q_PROPERTY(float orbitYaw READ orbitYaw WRITE setOrbitYaw NOTIFY orbitYawChanged)
  Q_PROPERTY(float orbitPitch READ orbitPitch WRITE setOrbitPitch NOTIFY orbitPitchChanged)
  Q_PROPERTY(float orbitDistance READ orbitDistance WRITE setOrbitDistance NOTIFY orbitDistanceChanged)
  Q_PROPERTY(QString hoverProvinceId READ hoverProvinceId WRITE setHoverProvinceId NOTIFY hoverProvinceIdChanged)
public:
  CampaignMapView();

  [[nodiscard]] auto createRenderer() const -> Renderer * override;

  Q_INVOKABLE QString provinceAtScreen(float x, float y);
  Q_INVOKABLE QPointF screenPosForUv(float u, float v);

  [[nodiscard]] auto orbitYaw() const -> float { return m_orbitYaw; }
  void setOrbitYaw(float yaw);

  [[nodiscard]] auto orbitPitch() const -> float { return m_orbitPitch; }
  void setOrbitPitch(float pitch);

  [[nodiscard]] auto orbitDistance() const -> float { return m_orbitDistance; }
  void setOrbitDistance(float distance);

  [[nodiscard]] auto hoverProvinceId() const -> QString { return m_hoverProvinceId; }
  void setHoverProvinceId(const QString &province_id);

signals:
  void orbitYawChanged();
  void orbitPitchChanged();
  void orbitDistanceChanged();
  void hoverProvinceIdChanged();

private:
  float m_orbitYaw = 180.0F;
  float m_orbitPitch = 55.0F;
  float m_orbitDistance = 2.4F;
  QString m_hoverProvinceId;

  struct ProvinceHit {
    QString id;
    std::vector<QVector2D> triangles;
  };

  bool m_provincesLoaded = false;
  std::vector<ProvinceHit> m_provinces;

  void loadProvincesForHitTest();
};
