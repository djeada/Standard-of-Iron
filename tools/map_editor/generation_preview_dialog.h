#pragma once

#include <QByteArray>
#include <QDialog>

#include "generator_client.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace MapEditor {

class MapCanvas;
class MapData;

class GenerationPreviewDialog : public QDialog {
  Q_OBJECT

public:
  enum class Choice {
    Cancel,
    Accept,
    Reroll,
    Adjust
  };

  GenerationPreviewDialog(const QByteArray& candidate,
                          const Generator::Report& report,
                          const QString& title,
                          QWidget* parent = nullptr);

  [[nodiscard]] auto choice() const -> Choice { return m_choice; }
  [[nodiscard]] auto candidate_loaded() const -> bool { return m_candidate_loaded; }
  [[nodiscard]] auto accept_enabled() const -> bool;
  [[nodiscard]] auto map_data() const -> const MapData* { return m_candidate; }

protected:
  void showEvent(QShowEvent* event) override;

private:
  void choose(Choice choice);
  void populate_checks();
  void populate_stages();
  void populate_metrics();
  void on_check_activated(QTreeWidgetItem* item);
  void refresh_accept();

  Generator::Report m_report;
  MapData* m_candidate = nullptr;
  MapCanvas* m_canvas = nullptr;
  QLabel* m_summary = nullptr;
  QTreeWidget* m_checks = nullptr;
  QTreeWidget* m_stages = nullptr;
  QTreeWidget* m_metrics = nullptr;
  QCheckBox* m_override = nullptr;
  QPushButton* m_accept_button = nullptr;
  Choice m_choice = Choice::Cancel;
  bool m_candidate_loaded = false;
  bool m_fitted = false;
};

} // namespace MapEditor
