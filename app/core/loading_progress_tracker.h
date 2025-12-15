#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class LoadingProgressTracker : public QObject {
  Q_OBJECT

public:
  enum class LoadingStage {
    NOT_STARTED,
    INITIALIZING,
    LOADING_MAP_DATA,
    LOADING_TERRAIN,
    LOADING_BIOME,
    LOADING_WATER_FEATURES,
    LOADING_ROADS,
    LOADING_ENVIRONMENT,
    LOADING_FOG,
    LOADING_ENTITIES,
    LOADING_AUDIO,
    GENERATING_MINIMAP,
    INITIALIZING_SYSTEMS,
    FINALIZING,
    COMPLETED,
    FAILED
  };
  Q_ENUM(LoadingStage)

  explicit LoadingProgressTracker(QObject *parent = nullptr);

  void start_loading();

  void set_stage(LoadingStage stage, const QString &detail = QString());

  void complete_stage(LoadingStage stage);

  void report_error(const QString &error_message);

  [[nodiscard]] bool is_complete() const;

  [[nodiscard]] bool has_failed() const;

  [[nodiscard]] LoadingStage current_stage() const { return m_current_stage; }

  [[nodiscard]] float progress() const;

  [[nodiscard]] QString stage_name(LoadingStage stage) const;

  [[nodiscard]] QString current_detail() const { return m_current_detail; }

signals:

  void stage_changed(LoadingStage stage, QString detail);

  void progress_changed(float progress);

  void loading_completed();

  void loading_failed(QString error_message);

private:
  LoadingStage m_current_stage;
  QString m_current_detail;
  bool m_failed;

  float stage_to_progress(LoadingStage stage) const;
};
