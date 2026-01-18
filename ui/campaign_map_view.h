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
  Q_PROPERTY(float orbit_yaw READ orbit_yaw WRITE set_orbit_yaw NOTIFY
                 orbit_yaw_changed)
  Q_PROPERTY(float orbit_pitch READ orbit_pitch WRITE set_orbit_pitch NOTIFY
                 orbit_pitch_changed)
  Q_PROPERTY(float orbit_distance READ orbit_distance WRITE set_orbit_distance
                 NOTIFY orbit_distance_changed)
  Q_PROPERTY(float pan_u READ pan_u WRITE set_pan_u NOTIFY pan_u_changed)
  Q_PROPERTY(float pan_v READ pan_v WRITE set_pan_v NOTIFY pan_v_changed)
  Q_PROPERTY(QString hover_province_id READ hover_province_id WRITE
                 set_hover_province_id NOTIFY hover_province_id_changed)
  Q_PROPERTY(QVariantList province_labels READ province_labels NOTIFY
                 province_labels_changed)
  Q_PROPERTY(int current_mission READ current_mission WRITE set_current_mission
                 NOTIFY current_mission_changed)
  Q_PROPERTY(float min_orbit_distance READ min_orbit_distance CONSTANT)
  Q_PROPERTY(float max_orbit_distance READ max_orbit_distance CONSTANT)
public:
  static constexpr float k_min_orbit_distance = 0.3F;
  static constexpr float k_max_orbit_distance = 5.0F;

  struct ProvinceVisual {
    QString owner;
    QVector4D color;
    bool has_color = false;
  };

  CampaignMapView();

  [[nodiscard]] auto createRenderer() const -> Renderer * override;

  Q_INVOKABLE QString province_at_screen(float x, float y);
  Q_INVOKABLE QVariantMap province_info_at_screen(float x, float y);
  Q_INVOKABLE QPointF screen_pos_for_uv(float u, float v);
  Q_INVOKABLE QVariantList province_labels();
  Q_INVOKABLE void apply_province_state(const QVariantList &states);
  Q_INVOKABLE QPointF hannibal_icon_position();

  [[nodiscard]] auto province_state_version() const -> int {
    return m_province_state_version;
  }
  [[nodiscard]] auto
  province_overrides() const -> const QHash<QString, ProvinceVisual> & {
    return m_province_overrides;
  }

  [[nodiscard]] auto orbit_yaw() const -> float { return m_orbit_yaw; }
  void set_orbit_yaw(float yaw);

  [[nodiscard]] auto orbit_pitch() const -> float { return m_orbit_pitch; }
  void set_orbit_pitch(float pitch);

  [[nodiscard]] auto orbit_distance() const -> float {
    return m_orbit_distance;
  }
  void set_orbit_distance(float distance);

  [[nodiscard]] auto min_orbit_distance() const -> float {
    return k_min_orbit_distance;
  }
  [[nodiscard]] auto max_orbit_distance() const -> float {
    return k_max_orbit_distance;
  }

  [[nodiscard]] auto pan_u() const -> float { return m_pan_u; }
  void set_pan_u(float pan);

  [[nodiscard]] auto pan_v() const -> float { return m_pan_v; }
  void set_pan_v(float pan);

  [[nodiscard]] auto hover_province_id() const -> QString {
    return m_hover_province_id;
  }
  void set_hover_province_id(const QString &province_id);

  [[nodiscard]] auto current_mission() const -> int {
    return m_current_mission;
  }
  void set_current_mission(int mission);

signals:
  void orbit_yaw_changed();
  void orbit_pitch_changed();
  void orbit_distance_changed();
  void pan_u_changed();
  void pan_v_changed();
  void hover_province_id_changed();
  void province_labels_changed();
  void current_mission_changed();

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
