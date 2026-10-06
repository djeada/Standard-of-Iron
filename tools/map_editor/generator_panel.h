#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QProcess>
#include <QVector>
#include <QWidget>

#include <functional>
#include <memory>
#include <optional>

#include "generator_client.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QTemporaryDir;
class QVBoxLayout;

namespace MapEditor {

class MapData;

class GeneratorPanel : public QWidget {
  Q_OBJECT

public:
  explicit GeneratorPanel(MapData* map_data, QWidget* parent = nullptr);
  ~GeneratorPanel() override;

  void set_repository_root(const QString& root);
  void load_schema();
  auto apply_schema(const QByteArray& describe_json, QString* error = nullptr) -> bool;
  void prefill_from(const QJsonObject& generation);

  [[nodiscard]] auto schema_loaded() const -> bool { return m_schema.has_value(); }
  [[nodiscard]] auto current_request() const -> Generator::Request;
  [[nodiscard]] auto is_running() const -> bool { return m_process != nullptr; }

signals:
  void feedback(const QString& message, bool success);
  void document_replaced();

protected:
  void showEvent(QShowEvent* event) override;

private:
  enum class Mode {
    Fresh,
    Reroll
  };

  struct ParameterEditor {
    Generator::ParameterSpec spec;
    std::function<QJsonValue()> read;
    std::function<void(const QJsonValue&)> write;
  };

  struct StageRow {
    QCheckBox* lock = nullptr;
    QLabel* status = nullptr;
  };

  void build_controls();
  void clear_controls();
  void show_error(const QString& message);
  void apply_preset(const QString& preset_id);
  void apply_values(const QJsonObject& values);
  void refresh_locks();
  void set_busy(bool busy);
  void start(Mode mode, qint64 seed);
  void on_progress();
  void on_finished(int exit_code, QProcess::ExitStatus status);
  void show_candidate(const QByteArray& candidate, const Generator::Report& report);
  void save_request();
  void load_request();
  void sync_from_document();
  [[nodiscard]] auto requested_locks() const -> QStringList;
  [[nodiscard]] auto stage_label(const QString& id) const -> QString;

  MapData* m_map_data = nullptr;
  QString m_repository_root;
  std::optional<Generator::Schema> m_schema;
  std::optional<QJsonObject> m_pending_prefill;
  QJsonObject m_last_document_generation;
  bool m_schema_requested = false;

  QVBoxLayout* m_layout = nullptr;
  QLabel* m_message = nullptr;
  QWidget* m_controls = nullptr;
  QComboBox* m_preset_box = nullptr;
  QLabel* m_preset_description = nullptr;
  QSpinBox* m_seed_box = nullptr;
  QSpinBox* m_width_box = nullptr;
  QSpinBox* m_height_box = nullptr;
  QMap<QString, ParameterEditor> m_parameters;
  QMap<QString, StageRow> m_stage_rows;
  QPushButton* m_generate_button = nullptr;
  QPushButton* m_reroll_button = nullptr;
  QPushButton* m_cancel_button = nullptr;
  QLabel* m_run_status = nullptr;
  QString m_generator_error;

  QProcess* m_describe_process = nullptr;
  QProcess* m_process = nullptr;
  std::unique_ptr<QTemporaryDir> m_run_dir;
  Generator::ProgressParser m_progress;
  QByteArray m_stderr;
  Mode m_mode = Mode::Fresh;
  Generator::Request m_running_request;
  bool m_cancelled = false;
};

} // namespace MapEditor
