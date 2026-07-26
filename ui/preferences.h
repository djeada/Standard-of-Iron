#ifndef SOI_UI_PREFERENCES_H
#define SOI_UI_PREFERENCES_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class UiPreferences : public QObject {
  Q_OBJECT

  Q_PROPERTY(qreal uiScale READ ui_scale WRITE set_ui_scale NOTIFY ui_scale_changed)
  Q_PROPERTY(bool reducedMotion READ reduced_motion WRITE set_reduced_motion NOTIFY
                 reduced_motion_changed)
  Q_PROPERTY(bool highContrast READ high_contrast WRITE set_high_contrast NOTIFY
                 high_contrast_changed)
  Q_PROPERTY(QString colorVisionMode READ color_vision_mode WRITE set_color_vision_mode
                 NOTIFY color_vision_mode_changed)
  Q_PROPERTY(bool alwaysShowFocus READ always_show_focus WRITE set_always_show_focus
                 NOTIFY always_show_focus_changed)
  Q_PROPERTY(QStringList colorVisionModes READ color_vision_modes CONSTANT)
  Q_PROPERTY(qreal minUiScale READ min_ui_scale CONSTANT)
  Q_PROPERTY(qreal maxUiScale READ max_ui_scale CONSTANT)

public:
  static auto instance() -> UiPreferences*;
  static auto create(QQmlEngine* engine, QJSEngine* scriptEngine) -> UiPreferences*;

  [[nodiscard]] auto ui_scale() const -> qreal { return m_ui_scale; }
  [[nodiscard]] auto reduced_motion() const -> bool { return m_reduced_motion; }
  [[nodiscard]] auto high_contrast() const -> bool { return m_high_contrast; }
  [[nodiscard]] auto color_vision_mode() const -> QString {
    return m_color_vision_mode;
  }
  [[nodiscard]] auto always_show_focus() const -> bool { return m_always_show_focus; }

  [[nodiscard]] static auto color_vision_modes() -> QStringList;
  [[nodiscard]] static auto min_ui_scale() -> qreal;
  [[nodiscard]] static auto max_ui_scale() -> qreal;

  void set_ui_scale(qreal scale);
  void set_reduced_motion(bool enabled);
  void set_high_contrast(bool enabled);
  void set_color_vision_mode(const QString& mode);
  void set_always_show_focus(bool enabled);

  Q_INVOKABLE void reset_to_defaults();

signals:
  void ui_scale_changed();
  void reduced_motion_changed();
  void high_contrast_changed();
  void color_vision_mode_changed();
  void always_show_focus_changed();

private:
  explicit UiPreferences(QObject* parent = nullptr);

  static UiPreferences* m_instance;

  qreal m_ui_scale;
  bool m_reduced_motion;
  bool m_high_contrast;
  QString m_color_vision_mode;
  bool m_always_show_focus;
};

#endif
