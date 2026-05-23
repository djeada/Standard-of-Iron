#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVector>

namespace MapEditor {

class TerrainProjectionWidget;

class JsonEditDialog : public QDialog {
  Q_OBJECT

public:
  explicit JsonEditDialog(const QString& title,
                          const QJsonObject& json,
                          bool enable_hill_projection = false,
                          QWidget* parent = nullptr);

  [[nodiscard]] QJsonObject get_json() const;
  [[nodiscard]] bool is_valid() const { return m_is_valid; }

private slots:
  void validate_json();
  void on_projection_entrances_changed();
  void on_accepted();

private:
  void setup_ui(const QString& title, const QJsonObject& json);
  void sync_editor_from_model();
  void update_projection_state();
  void apply_projection_to_model_json();

  QPlainTextEdit* m_editor = nullptr;
  TerrainProjectionWidget* m_projection = nullptr;
  QLabel* m_projection_hint_label = nullptr;
  QVector<QPushButton*> m_marker_buttons;
  QPushButton* m_ok_button = nullptr;
  QJsonObject m_model_json;
  QJsonObject m_result;
  bool m_enable_hill_projection = false;
  bool m_syncing_editor = false;
  bool m_syncing_projection = false;
  bool m_is_valid = false;
};

} // namespace MapEditor
