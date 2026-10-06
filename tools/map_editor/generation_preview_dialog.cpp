#include "generation_preview_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "map_canvas.h"
#include "map_data.h"

namespace MapEditor {

namespace {

constexpr int k_object_role = Qt::UserRole + 1;

auto status_glyph(Generator::CheckStatus status) -> QString {
  switch (status) {
  case Generator::CheckStatus::Pass:
    return QStringLiteral("✓");
  case Generator::CheckStatus::Warn:
    return QStringLiteral("⚠");
  case Generator::CheckStatus::Fail:
    return QStringLiteral("✗");
  }
  return {};
}

auto status_color(Generator::CheckStatus status) -> QColor {
  switch (status) {
  case Generator::CheckStatus::Pass:
    return {120, 200, 120};
  case Generator::CheckStatus::Warn:
    return {230, 190, 90};
  case Generator::CheckStatus::Fail:
    return {235, 95, 85};
  }
  return {};
}

auto metric_text(const QJsonValue& value) -> QString {
  if (value.isDouble()) {
    const double number = value.toDouble();
    return number == static_cast<double>(static_cast<qint64>(number))
               ? QString::number(static_cast<qint64>(number))
               : QString::number(number, 'f', 2);
  }
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("yes") : QStringLiteral("no");
  }
  return value.toVariant().toString();
}

auto make_tree(const QStringList& headers, QWidget* parent) -> QTreeWidget* {
  auto* tree = new QTreeWidget(parent);
  tree->setHeaderLabels(headers);
  tree->setRootIsDecorated(true);
  tree->setAlternatingRowColors(true);
  tree->header()->setStretchLastSection(true);
  return tree;
}

} // namespace

GenerationPreviewDialog::GenerationPreviewDialog(const QByteArray& candidate,
                                                 const Generator::Report& report,
                                                 const QString& title,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_report(report) {
  setWindowTitle(title);
  resize(1280, 780);

  m_candidate = new MapData(this);
  QString load_error;
  m_candidate_loaded = m_candidate->load_from_bytes(candidate, &load_error);

  auto* layout = new QVBoxLayout(this);
  auto* splitter = new QSplitter(Qt::Horizontal, this);

  m_canvas = new MapCanvas(splitter);
  m_canvas->set_read_only(true);
  m_canvas->set_map_data(m_candidate);
  splitter->addWidget(m_canvas);

  auto* side = new QWidget(splitter);
  auto* side_layout = new QVBoxLayout(side);
  side_layout->setContentsMargins(6, 0, 0, 0);

  m_summary = new QLabel(side);
  m_summary->setWordWrap(true);
  m_summary->setObjectName(QStringLiteral("panelIntro"));
  side_layout->addWidget(m_summary);

  auto* tabs = new QTabWidget(side);
  m_checks = make_tree({QStringLiteral(""), QStringLiteral("Check")}, tabs);
  m_checks->setColumnWidth(0, 44);
  m_stages = make_tree({QStringLiteral("Stage"),
                        QStringLiteral("Status"),
                        QStringLiteral("Time"),
                        QStringLiteral("Forced by")},
                       tabs);
  m_metrics = make_tree({QStringLiteral("Metric"), QStringLiteral("Value")}, tabs);
  m_metrics->setRootIsDecorated(false);
  tabs->addTab(m_checks, QStringLiteral("Validation"));
  tabs->addTab(m_stages, QStringLiteral("Stages"));
  tabs->addTab(m_metrics, QStringLiteral("Metrics"));
  side_layout->addWidget(tabs, 1);

  m_override = new QCheckBox(QStringLiteral("Developer override: accept a map that "
                                            "failed validation"),
                             side);
  m_override->setVisible(!m_report.ok);
  connect(m_override, &QCheckBox::toggled, this, [this]() { refresh_accept(); });
  side_layout->addWidget(m_override);

  splitter->addWidget(side);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  layout->addWidget(splitter, 1);

  auto* buttons = new QDialogButtonBox(this);
  m_accept_button =
      buttons->addButton(QStringLiteral("Accept"), QDialogButtonBox::AcceptRole);
  m_accept_button->setToolTip(
      QStringLiteral("Replace the open document with this map. One undo step."));
  auto* reroll =
      buttons->addButton(QStringLiteral("Reroll"), QDialogButtonBox::ActionRole);
  reroll->setToolTip(QStringLiteral("Same settings, next seed."));
  auto* adjust =
      buttons->addButton(QStringLiteral("Adjust…"), QDialogButtonBox::ActionRole);
  adjust->setToolTip(QStringLiteral("Close the preview and change the parameters."));
  auto* cancel = buttons->addButton(QDialogButtonBox::Cancel);
  connect(m_accept_button, &QPushButton::clicked, this, [this]() {
    choose(Choice::Accept);
  });
  connect(reroll, &QPushButton::clicked, this, [this]() { choose(Choice::Reroll); });
  connect(adjust, &QPushButton::clicked, this, [this]() { choose(Choice::Adjust); });
  connect(cancel, &QPushButton::clicked, this, [this]() { choose(Choice::Cancel); });
  layout->addWidget(buttons);

  connect(m_checks,
          &QTreeWidget::itemActivated,
          this,
          [this](QTreeWidgetItem* item, int) { on_check_activated(item); });
  connect(m_checks,
          &QTreeWidget::itemClicked,
          this,
          [this](QTreeWidgetItem* item, int) { on_check_activated(item); });

  const int fails = m_report.count(Generator::CheckStatus::Fail);
  const int warns = m_report.count(Generator::CheckStatus::Warn);
  QString summary;
  if (!m_candidate_loaded) {
    summary = QStringLiteral("The candidate could not be loaded: %1").arg(load_error);
  } else if (m_report.ok) {
    summary = QStringLiteral("Generation complete. %1 check(s) passed, %2 warning(s).")
                  .arg(m_report.count(Generator::CheckStatus::Pass))
                  .arg(warns);
  } else {
    summary = QStringLiteral("Validation failed: %1 hard failure(s), %2 warning(s). "
                             "Fix the parameters or reroll.")
                  .arg(fails)
                  .arg(warns);
  }
  m_summary->setText(summary);

  populate_checks();
  populate_stages();
  populate_metrics();
  refresh_accept();
}

auto GenerationPreviewDialog::accept_enabled() const -> bool {
  return m_accept_button->isEnabled();
}

void GenerationPreviewDialog::showEvent(QShowEvent* event) {
  QDialog::showEvent(event);
  if (!m_fitted) {
    m_fitted = true;
    QTimer::singleShot(0, m_canvas, [canvas = m_canvas]() { canvas->zoom_to_fit(); });
  }
}

void GenerationPreviewDialog::choose(Choice choice) {
  m_choice = choice;
  if (choice == Choice::Accept) {
    accept();
  } else {
    reject();
  }
}

void GenerationPreviewDialog::populate_checks() {
  for (const Generator::ReportCheck& check : m_report.checks) {
    auto* item = new QTreeWidgetItem(m_checks);
    item->setText(0, status_glyph(check.status));
    item->setForeground(0, status_color(check.status));
    item->setText(1, check.message);
    item->setToolTip(1, check.id);
    for (const Generator::ReportObject& object : check.objects) {
      auto* child = new QTreeWidgetItem(item);
      QString text = object.kind;
      if (object.index >= 0) {
        text += QStringLiteral(" #%1").arg(object.index);
      }
      if (object.has_position) {
        text += QStringLiteral(" at (%1, %2)")
                    .arg(object.x, 0, 'f', 0)
                    .arg(object.z, 0, 'f', 0);
        child->setData(1, k_object_role, QPointF(object.x, object.z));
      }
      child->setText(1, text);
    }
    if (!check.objects.isEmpty()) {
      if (const QTreeWidgetItem* first = item->child(0);
          first->data(1, k_object_role).isValid()) {
        item->setData(1, k_object_role, first->data(1, k_object_role));
      }
      item->setToolTip(1, check.id + QStringLiteral(" — click to frame it on the map"));
    }
    if (check.status != Generator::CheckStatus::Pass) {
      item->setExpanded(true);
    }
  }
  m_checks->resizeColumnToContents(0);
}

void GenerationPreviewDialog::populate_stages() {
  for (const Generator::ReportStage& stage : m_report.stages) {
    auto* item = new QTreeWidgetItem(m_stages);
    item->setText(0, stage.id);
    item->setText(1, stage.status);
    item->setText(2, QStringLiteral("%1 s").arg(stage.seconds, 0, 'f', 1));
    item->setText(3, stage.forced_by);
    if (stage.status == QLatin1String("failed")) {
      item->setForeground(1, status_color(Generator::CheckStatus::Fail));
    }
    for (const QString& note : stage.notes) {
      auto* child = new QTreeWidgetItem(item);
      child->setText(0, note);
      child->setFirstColumnSpanned(true);
    }
  }
  m_stages->resizeColumnToContents(0);
}

void GenerationPreviewDialog::populate_metrics() {
  QStringList keys = m_report.metrics.keys();
  keys.sort();
  for (const QString& key : keys) {
    auto* item = new QTreeWidgetItem(m_metrics);
    QString label = key;
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    item->setText(0, label);
    item->setText(1, metric_text(m_report.metrics.value(key)));
  }
  m_metrics->resizeColumnToContents(0);
}

void GenerationPreviewDialog::on_check_activated(QTreeWidgetItem* item) {
  if (item == nullptr) {
    return;
  }
  const QVariant position = item->data(1, k_object_role);
  if (position.isValid()) {
    m_canvas->center_on(position.toPointF());
  }
}

void GenerationPreviewDialog::refresh_accept() {
  const bool allowed = m_candidate_loaded && (m_report.ok || m_override->isChecked());
  m_accept_button->setEnabled(allowed);
}

} // namespace MapEditor
