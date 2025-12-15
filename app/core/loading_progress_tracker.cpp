#include "loading_progress_tracker.h"

LoadingProgressTracker::LoadingProgressTracker(QObject *parent)
    : QObject(parent), m_current_stage(LoadingStage::NOT_STARTED),
      m_failed(false) {}

void LoadingProgressTracker::start_loading() {
  m_current_stage = LoadingStage::INITIALIZING;
  m_current_detail = "";
  m_failed = false;
  emit stage_changed(m_current_stage, m_current_detail);
  emit progress_changed(progress());
}

void LoadingProgressTracker::set_stage(LoadingStage stage,
                                       const QString &detail) {
  if (m_failed || m_current_stage == LoadingStage::COMPLETED) {
    return;
  }

  m_current_stage = stage;
  m_current_detail = detail;
  emit stage_changed(stage, detail);
  emit progress_changed(progress());

  if (stage == LoadingStage::COMPLETED) {
    emit loading_completed();
  }
}

void LoadingProgressTracker::complete_stage(LoadingStage stage) {

  auto next_stage = static_cast<int>(stage) + 1;
  if (next_stage < static_cast<int>(LoadingStage::COMPLETED)) {
    set_stage(static_cast<LoadingStage>(next_stage));
  }
}

void LoadingProgressTracker::report_error(const QString &error_message) {
  m_failed = true;
  m_current_stage = LoadingStage::FAILED;
  m_current_detail = error_message;
  emit stage_changed(m_current_stage, error_message);
  emit loading_failed(error_message);
}

bool LoadingProgressTracker::is_complete() const {
  return m_current_stage == LoadingStage::COMPLETED;
}

bool LoadingProgressTracker::has_failed() const { return m_failed; }

float LoadingProgressTracker::progress() const {
  return stage_to_progress(m_current_stage);
}

QString LoadingProgressTracker::stage_name(LoadingStage stage) const {
  switch (stage) {
  case LoadingStage::NOT_STARTED:
    return "Not Started";
  case LoadingStage::INITIALIZING:
    return "Initializing...";
  case LoadingStage::LOADING_MAP_DATA:
    return "Loading Map Data...";
  case LoadingStage::LOADING_TERRAIN:
    return "Loading Terrain...";
  case LoadingStage::LOADING_BIOME:
    return "Loading Biome...";
  case LoadingStage::LOADING_WATER_FEATURES:
    return "Loading Water Features...";
  case LoadingStage::LOADING_ROADS:
    return "Loading Roads...";
  case LoadingStage::LOADING_ENVIRONMENT:
    return "Loading Environment...";
  case LoadingStage::LOADING_FOG:
    return "Loading Fog...";
  case LoadingStage::LOADING_ENTITIES:
    return "Loading Units & Buildings...";
  case LoadingStage::LOADING_AUDIO:
    return "Loading Audio...";
  case LoadingStage::GENERATING_MINIMAP:
    return "Generating Minimap...";
  case LoadingStage::INITIALIZING_SYSTEMS:
    return "Initializing Game Systems...";
  case LoadingStage::FINALIZING:
    return "Finalizing...";
  case LoadingStage::COMPLETED:
    return "Complete";
  case LoadingStage::FAILED:
    return "Failed";
  default:
    return "Unknown";
  }
}

float LoadingProgressTracker::stage_to_progress(LoadingStage stage) const {

  const float total_stages = 13.0F;

  switch (stage) {
  case LoadingStage::NOT_STARTED:
    return 0.0F;
  case LoadingStage::INITIALIZING:
    return 1.0F / total_stages;
  case LoadingStage::LOADING_MAP_DATA:
    return 2.0F / total_stages;
  case LoadingStage::LOADING_TERRAIN:
    return 3.0F / total_stages;
  case LoadingStage::LOADING_BIOME:
    return 4.0F / total_stages;
  case LoadingStage::LOADING_WATER_FEATURES:
    return 5.0F / total_stages;
  case LoadingStage::LOADING_ROADS:
    return 6.0F / total_stages;
  case LoadingStage::LOADING_ENVIRONMENT:
    return 7.0F / total_stages;
  case LoadingStage::LOADING_FOG:
    return 8.0F / total_stages;
  case LoadingStage::LOADING_ENTITIES:
    return 9.0F / total_stages;
  case LoadingStage::LOADING_AUDIO:
    return 10.0F / total_stages;
  case LoadingStage::GENERATING_MINIMAP:
    return 11.0F / total_stages;
  case LoadingStage::INITIALIZING_SYSTEMS:
    return 12.0F / total_stages;
  case LoadingStage::FINALIZING:
    return 13.0F / total_stages;
  case LoadingStage::COMPLETED:
    return 1.0F;
  case LoadingStage::FAILED:
    return 0.0F;
  default:
    return 0.0F;
  }
}
