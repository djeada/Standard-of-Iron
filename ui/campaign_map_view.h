#pragma once

#include <QHash>
#include <QPointF>
#include <QQuickFramebufferObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector2D>
#include <QVector4D>
#include <vector>

class CampaignMapView : public QQuickFramebufferObject {
  Q_OBJECT
  Q_PROPERTY(
      float orbitYaw READ orbitYaw WRITE setOrbitYaw NOTIFY orbitYawChanged)
  Q_PROPERTY(float orbitPitch READ orbitPitch WRITE setOrbitPitch NOTIFY
                 orbitPitchChanged)
  Q_PROPERTY(float orbitDistance READ orbitDistance WRITE setOrbitDistance
                 NOTIFY orbitDistanceChanged)
  Q_PROPERTY(float panU READ panU WRITE setPanU NOTIFY panUChanged)
  Q_PROPERTY(float panV READ panV WRITE setPanV NOTIFY panVChanged)
  Q_PROPERTY(QString hoverProvinceId READ hoverProvinceId WRITE
                 setHoverProvinceId NOTIFY hoverProvinceIdChanged)
  Q_PROPERTY(QVariantList provinceLabels READ provinceLabels NOTIFY
                 provinceLabelsChanged)
  Q_PROPERTY(int currentMission READ currentMission WRITE setCurrentMission
                 NOTIFY currentMissionChanged)
public:
  struct ProvinceVisual {
    QString owner;
    QVector4D color;
    bool has_color = false;
  };

  CampaignMapView();

  [[nodiscard]] auto createRenderer() const -> Renderer * override;

  Q_INVOKABLE QString provinceAtScreen(float x, float y);
  Q_INVOKABLE QVariantMap provinceInfoAtScreen(float x, float y);
  Q_INVOKABLE QPointF screenPosForUv(float u, float v);
  Q_INVOKABLE QVariantList provinceLabels();
  Q_INVOKABLE void applyProvinceState(const QVariantList &states);
  Q_INVOKABLE QPointF hannibalIconPosition();

  [[nodiscard]] auto provinceStateVersion() const -> int {
    return m_province_state_version;
  }
  [[nodiscard]] auto
  provinceOverrides() const -> const QHash<QString, ProvinceVisual> & {
    return m_province_overrides;
  }

  [[nodiscard]] auto orbitYaw() const -> float { return m_orbit_yaw; }
  void setOrbitYaw(float yaw);

  [[nodiscard]] auto orbitPitch() const -> float { return m_orbit_pitch; }
  void setOrbitPitch(float pitch);

  [[nodiscard]] auto orbitDistance() const -> float { return m_orbit_distance; }
  void setOrbitDistance(float distance);

  [[nodiscard]] auto panU() const -> float { return m_pan_u; }
  void setPanU(float pan);

  [[nodiscard]] auto panV() const -> float { return m_pan_v; }
  void setPanV(float pan);

  [[nodiscard]] auto hoverProvinceId() const -> QString {
    return m_hover_province_id;
  }
  void setHoverProvinceId(const QString &province_id);

  [[nodiscard]] auto currentMission() const -> int { return m_current_mission; }
  void setCurrentMission(int mission);

signals:
  void orbitYawChanged();
  void orbitPitchChanged();
  void orbitDistanceChanged();
  void panUChanged();
  void panVChanged();
  void hoverProvinceIdChanged();
  void provinceLabelsChanged();
  void currentMissionChanged();

private:
  float m_orbit_yaw = 180.0F;
  float m_orbit_pitch = 90.0F;
  float m_orbit_distance = 1.2F;
  float m_pan_u = 0.0F;
  float m_pan_v = 0.0F;
  QString m_hover_province_id;
  int m_current_mission = 7;

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

  bool m_hannibal_paths_loaded = false;
  std::vector<std::vector<QVector2D>> m_hannibal_paths;
  void load_hannibal_paths();

  QHash<QString, ProvinceVisual> m_province_overrides;
  int m_province_state_version = 0;

  friend class CampaignMapRenderer;
};
