#pragma once

#include <QPointF>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector2D>
#include <vector>

class CampaignMapView : public QQuickFramebufferObject {
  Q_OBJECT
  Q_PROPERTY(
      float orbitYaw READ orbitYaw WRITE setOrbitYaw NOTIFY orbitYawChanged)
  Q_PROPERTY(float orbitPitch READ orbitPitch WRITE setOrbitPitch NOTIFY
                 orbitPitchChanged)
  Q_PROPERTY(float orbitDistance READ orbitDistance WRITE setOrbitDistance
                 NOTIFY orbitDistanceChanged)
  Q_PROPERTY(QString hoverProvinceId READ hoverProvinceId WRITE
                 setHoverProvinceId NOTIFY hoverProvinceIdChanged)
  Q_PROPERTY(QVariantList provinceLabels READ provinceLabels NOTIFY
                 provinceLabelsChanged)
public:
  CampaignMapView();

  [[nodiscard]] auto createRenderer() const -> Renderer * override;

  Q_INVOKABLE QString provinceAtScreen(float x, float y);
  Q_INVOKABLE QVariantMap provinceInfoAtScreen(float x, float y);
  Q_INVOKABLE QPointF screenPosForUv(float u, float v);
  Q_INVOKABLE QVariantList provinceLabels();

  [[nodiscard]] auto orbitYaw() const -> float { return m_orbit_yaw; }
  void setOrbitYaw(float yaw);

  [[nodiscard]] auto orbitPitch() const -> float { return m_orbit_pitch; }
  void setOrbitPitch(float pitch);

  [[nodiscard]] auto orbitDistance() const -> float { return m_orbit_distance; }
  void setOrbitDistance(float distance);

  [[nodiscard]] auto hoverProvinceId() const -> QString {
    return m_hover_province_id;
  }
  void setHoverProvinceId(const QString &province_id);

signals:
  void orbitYawChanged();
  void orbitPitchChanged();
  void orbitDistanceChanged();
  void hoverProvinceIdChanged();
  void provinceLabelsChanged();

private:
  float m_orbit_yaw = 180.0F;
  float m_orbit_pitch = 55.0F;
  float m_orbit_distance = 2.4F;
  QString m_hover_province_id;

  struct ProvinceHit {
    QString id;
    QString name;
    QString owner;
    std::vector<QVector2D> triangles;
  };

  bool m_provinces_loaded = false;
  std::vector<ProvinceHit> m_provinces;

  void load_provinces_for_hit_test();

  bool m_province_labels_loaded = false;
  QVariantList m_province_labels;
  void load_province_labels();
};
