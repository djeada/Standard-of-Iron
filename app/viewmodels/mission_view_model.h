#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class CameraViewModel;

class MissionViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool staged READ staged NOTIFY stages_changed)
  Q_PROPERTY(QVariantList stages READ stages NOTIFY stages_changed)
  Q_PROPERTY(int active_index READ active_index NOTIFY stages_changed)
  Q_PROPERTY(QString active_title READ active_title NOTIFY stages_changed)
  Q_PROPERTY(QString active_hint READ active_hint NOTIFY stages_changed)
  Q_PROPERTY(int active_progress READ active_progress NOTIFY stages_changed)
  Q_PROPERTY(int active_required READ active_required NOTIFY stages_changed)
  Q_PROPERTY(bool active_has_target READ active_has_target NOTIFY stages_changed)
  Q_PROPERTY(int completed_count READ completed_count NOTIFY stages_changed)
  Q_PROPERTY(QVariantList markers READ markers NOTIFY stages_changed)
  Q_PROPERTY(bool stages_mirror_victory_conditions READ stages_mirror_victory_conditions
                 NOTIFY stages_changed)

public:
  MissionViewModel(const App::Core::ClientContext& context,
                   App::Core::ClientHost& host,
                   CameraViewModel& camera,
                   QObject* parent = nullptr);

  void set_stages(const QVariantList& stages, bool mirrors_victory_conditions = false);
  void clear();

  [[nodiscard]] auto staged() const -> bool { return !m_stages.isEmpty(); }
  [[nodiscard]] auto stages() const -> QVariantList { return m_stages; }
  [[nodiscard]] auto active_index() const -> int { return m_active_index; }
  [[nodiscard]] auto active_title() const -> QString;
  [[nodiscard]] auto active_hint() const -> QString;
  [[nodiscard]] auto active_progress() const -> int;
  [[nodiscard]] auto active_required() const -> int;
  [[nodiscard]] auto active_has_target() const -> bool;
  [[nodiscard]] auto completed_count() const -> int;
  [[nodiscard]] auto markers() const -> QVariantList { return m_markers; }
  [[nodiscard]] auto stages_mirror_victory_conditions() const -> bool {
    return m_stages_mirror_victory_conditions;
  }

  Q_INVOKABLE void focus_active_stage();

signals:
  void stages_changed();

private:
  [[nodiscard]] auto active_stage() const -> QVariantMap;

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  CameraViewModel& m_camera;

  QVariantList m_stages;
  QVariantList m_markers;
  int m_active_index = -1;
  bool m_stages_mirror_victory_conditions = false;
};

} // namespace App::ViewModels
