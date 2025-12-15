#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

/**
 * @brief Tracks loading progress through defined stages with signal emission
 * 
 * This class provides a clean interface for tracking what is being loaded
 * during game initialization. It uses a signal-based (push) approach where
 * loading stages emit progress updates.
 */
class LoadingProgressTracker : public QObject {
  Q_OBJECT

public:
  enum class LoadingStage {
    NOT_STARTED,
    INITIALIZING,
    LOADING_MAP_DATA,
    LOADING_TERRAIN,
    LOADING_BIOME,
    LOADING_WATER_FEATURES,  // Rivers, riverbanks, bridges
    LOADING_ROADS,
    LOADING_ENVIRONMENT,     // Stones, plants, pines, olives, fire camps
    LOADING_FOG,
    LOADING_ENTITIES,        // Units, buildings
    LOADING_AUDIO,
    GENERATING_MINIMAP,
    INITIALIZING_SYSTEMS,    // AI, combat, etc.
    FINALIZING,
    COMPLETED,
    FAILED
  };
  Q_ENUM(LoadingStage)

  explicit LoadingProgressTracker(QObject *parent = nullptr);

  // Start tracking a new loading session
  void start_loading();

  // Update to a specific stage
  void set_stage(LoadingStage stage, const QString &detail = QString());

  // Mark stage as complete and move to next
  void complete_stage(LoadingStage stage);

  // Report error and fail
  void report_error(const QString &error_message);

  // Check if loading is complete
  [[nodiscard]] bool is_complete() const;

  // Check if loading failed
  [[nodiscard]] bool has_failed() const;

  // Get current stage
  [[nodiscard]] LoadingStage current_stage() const { return m_current_stage; }

  // Get current progress (0.0 to 1.0)
  [[nodiscard]] float progress() const;

  // Get human-readable stage name
  [[nodiscard]] QString stage_name(LoadingStage stage) const;

  // Get current stage detail text
  [[nodiscard]] QString current_detail() const { return m_current_detail; }

signals:
  // Emitted when stage changes
  void stage_changed(LoadingStage stage, QString detail);

  // Emitted when progress updates (0.0 to 1.0)
  void progress_changed(float progress);

  // Emitted when loading completes successfully
  void loading_completed();

  // Emitted when loading fails
  void loading_failed(QString error_message);

private:
  LoadingStage m_current_stage;
  QString m_current_detail;
  bool m_failed;

  // Map stages to progress values
  float stage_to_progress(LoadingStage stage) const;
};
