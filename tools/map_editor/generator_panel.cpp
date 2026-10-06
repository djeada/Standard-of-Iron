#include "generator_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <cmath>

#include "generation_preview_dialog.h"
#include "map_data.h"

namespace MapEditor {

namespace {

constexpr int k_max_stderr_bytes = 64 * 1024;
constexpr int k_stderr_tail_lines = 12;

auto create_label(const QString& text,
                  const QString& object_name,
                  QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setObjectName(object_name);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  return label;
}

auto status_text(const QString& status) -> QString {
  if (status == QLatin1String("running")) {
    return QStringLiteral("… running");
  }
  if (status == QLatin1String("done")) {
    return QStringLiteral("✓ generated");
  }
  if (status == QLatin1String("locked")) {
    return QStringLiteral("■ kept");
  }
  if (status == QLatin1String("failed")) {
    return QStringLiteral("✗ failed");
  }
  return status;
}

auto stderr_tail(const QByteArray& bytes) -> QString {
  QStringList lines =
      QString::fromUtf8(bytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  if (lines.size() > k_stderr_tail_lines) {
    lines = lines.mid(lines.size() - k_stderr_tail_lines);
  }
  return lines.join(QLatin1Char('\n'));
}

auto read_file(const QString& path) -> std::optional<QByteArray> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  return file.readAll();
}

} // namespace

GeneratorPanel::GeneratorPanel(MapData* map_data, QWidget* parent)
    : QWidget(parent)
    , m_map_data(map_data) {
  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(8, 8, 8, 8);
  m_layout->setSpacing(8);

  m_layout->addWidget(create_label(
      QStringLiteral("Map Generator"), QStringLiteral("panelTitle"), this));
  m_layout->addWidget(create_label(
      QStringLiteral("Pick a preset, adjust a few parameters and press Generate. "
                     "Nothing changes until you accept the preview, and accepting "
                     "is one undo step. Lock the stages you like and Reroll the "
                     "rest; anything you edit by hand becomes authored and is "
                     "kept."),
      QStringLiteral("panelIntro"),
      this));

  m_message = create_label(QString(), QStringLiteral("panelHint"), this);
  m_message->setVisible(false);
  m_layout->addWidget(m_message);

  m_controls = new QWidget(this);
  m_layout->addWidget(m_controls);
  m_layout->addStretch(1);
  setMinimumWidth(260);
  setMaximumWidth(440);

  if (m_map_data != nullptr) {
    connect(
        m_map_data, &MapData::data_changed, this, [this]() { sync_from_document(); });
  }
}

GeneratorPanel::~GeneratorPanel() {
  for (QProcess* process : {m_process, m_describe_process}) {
    if (process != nullptr) {
      process->disconnect(this);
      process->kill();
      process->waitForFinished(2000);
    }
  }
}

void GeneratorPanel::set_repository_root(const QString& root) {
  m_repository_root = root;
}

void GeneratorPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (!m_schema_requested) {
    load_schema();
  }
}

void GeneratorPanel::load_schema() {
  m_schema_requested = true;
  const QString fixture = qEnvironmentVariable("SOI_MAPGEN_SCHEMA");
  if (!fixture.isEmpty()) {
    if (const auto bytes = read_file(fixture)) {
      QString error;
      if (!apply_schema(*bytes, &error)) {
        show_error(QStringLiteral("SOI_MAPGEN_SCHEMA is not a generator schema: %1")
                       .arg(error));
      }
    } else {
      show_error(
          QStringLiteral("SOI_MAPGEN_SCHEMA could not be read: %1").arg(fixture));
    }
    return;
  }

  const QString script = Generator::script_path(m_repository_root);
  const auto python = Generator::find_python();
  if (script.isEmpty()) {
    show_error(QStringLiteral(
        "The generator needs scripts/soi-mapgen.py from a source checkout. Run the "
        "editor from the repository (or its build/bin) to use this tab."));
    return;
  }
  if (!python.has_value()) {
    show_error(QStringLiteral(
        "The generator is a Python tool and no python3 was found on PATH. Install "
        "Python 3 and reopen this tab."));
    return;
  }

  m_message->setText(QStringLiteral("Loading generator presets…"));
  m_message->setVisible(true);
  m_describe_process = new QProcess(this);
  m_describe_process->setWorkingDirectory(m_repository_root);
  connect(m_describe_process,
          &QProcess::finished,
          this,
          [this](int exit_code, QProcess::ExitStatus status) {
            QProcess* process = m_describe_process;
            m_describe_process = nullptr;
            process->deleteLater();
            if (status != QProcess::NormalExit || exit_code != 0) {
              show_error(QStringLiteral("soi-mapgen.py --describe failed:\n%1")
                             .arg(stderr_tail(process->readAllStandardError())));
              return;
            }
            QString error;
            if (!apply_schema(process->readAllStandardOutput(), &error)) {
              show_error(QStringLiteral(
                             "soi-mapgen.py --describe printed an unusable schema: %1")
                             .arg(error));
            }
          });
  connect(m_describe_process,
          &QProcess::errorOccurred,
          this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && m_describe_process != nullptr) {
              show_error(QStringLiteral("Could not start Python: %1")
                             .arg(m_describe_process->errorString()));
              m_describe_process->deleteLater();
              m_describe_process = nullptr;
            }
          });
  QStringList arguments = python->prefix_arguments;
  arguments << script << QStringLiteral("--describe");
  m_describe_process->start(python->program, arguments);
}

auto GeneratorPanel::apply_schema(const QByteArray& describe_json,
                                  QString* error) -> bool {
  auto schema = Generator::parse_schema(describe_json, error);
  if (!schema.has_value()) {
    return false;
  }
  if (schema->presets.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("the schema lists no presets");
    }
    return false;
  }
  m_schema = std::move(schema);
  m_message->setVisible(false);
  build_controls();
  apply_preset(m_schema->presets.front().id);

  if (m_pending_prefill.has_value()) {
    const QJsonObject pending = *m_pending_prefill;
    m_pending_prefill.reset();
    prefill_from(pending);
  } else if (m_map_data != nullptr && !m_map_data->generation().isEmpty()) {
    m_last_document_generation = m_map_data->generation();
    prefill_from(m_last_document_generation);
  }
  return true;
}

void GeneratorPanel::show_error(const QString& message) {
  m_message->setText(message);
  m_message->setStyleSheet(QStringLiteral("color: #e8806e;"));
  m_message->setVisible(true);
}

void GeneratorPanel::clear_controls() {
  m_parameters.clear();
  m_stage_rows.clear();
  delete m_controls->layout();
  qDeleteAll(m_controls->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
}

void GeneratorPanel::build_controls() {
  clear_controls();
  auto* layout = new QVBoxLayout(m_controls);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* general = new QGroupBox(QStringLiteral("General"), m_controls);
  auto* general_form = new QFormLayout(general);
  m_preset_box = new QComboBox(general);
  for (const Generator::PresetSpec& preset : m_schema->presets) {
    m_preset_box->addItem(preset.label, preset.id);
  }
  connect(m_preset_box, qOverload<int>(&QComboBox::activated), this, [this](int index) {
    apply_preset(m_preset_box->itemData(index).toString());
  });
  general_form->addRow(QStringLiteral("Preset"), m_preset_box);
  m_preset_description = create_label(QString(), QStringLiteral("panelHint"), general);
  general_form->addRow(m_preset_description);

  auto* seed_row = new QWidget(general);
  auto* seed_layout = new QHBoxLayout(seed_row);
  seed_layout->setContentsMargins(0, 0, 0, 0);
  m_seed_box = new QSpinBox(seed_row);
  m_seed_box->setRange(0, INT_MAX);
  m_seed_box->setValue(1);
  m_seed_box->setToolTip(
      QStringLiteral("Same seed + same parameters = the same map, every time."));
  auto* randomize = new QPushButton(QStringLiteral("Randomize"), seed_row);
  connect(randomize, &QPushButton::clicked, this, [this]() {
    m_seed_box->setValue(
        static_cast<int>(QRandomGenerator::global()->bounded(1, INT_MAX)));
  });
  seed_layout->addWidget(m_seed_box, 1);
  seed_layout->addWidget(randomize);
  general_form->addRow(QStringLiteral("Seed"), seed_row);

  auto* size_row = new QWidget(general);
  auto* size_layout = new QHBoxLayout(size_row);
  size_layout->setContentsMargins(0, 0, 0, 0);
  m_width_box = new QSpinBox(size_row);
  m_height_box = new QSpinBox(size_row);
  for (QSpinBox* box : {m_width_box, m_height_box}) {
    box->setRange(200, 1200);
    box->setSingleStep(50);
  }
  size_layout->addWidget(m_width_box);
  size_layout->addWidget(new QLabel(QStringLiteral("×"), size_row));
  size_layout->addWidget(m_height_box);
  general_form->addRow(QStringLiteral("Size"), size_row);
  layout->addWidget(general);

  for (const QString& group : m_schema->parameter_groups()) {
    QFormLayout* form = nullptr;
    if (group == QLatin1String("general")) {
      form = general_form;
    } else {
      const Generator::StageSpec* stage = m_schema->stage(group);
      QString title = stage != nullptr ? stage->label : group;
      if (!title.isEmpty()) {
        title[0] = title[0].toUpper();
      }
      auto* box = new QGroupBox(title, m_controls);
      form = new QFormLayout(box);
      layout->addWidget(box);
    }
    for (const Generator::ParameterSpec* spec_ptr : m_schema->parameters_for(group)) {
      const Generator::ParameterSpec spec = *spec_ptr;
      ParameterEditor editor;
      editor.spec = spec;
      QWidget* field = nullptr;
      switch (spec.type) {
      case Generator::ParameterType::Float: {
        auto* row = new QWidget(m_controls);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        const double step = spec.step > 0.0 ? spec.step : (spec.max - spec.min) / 100.0;
        const int ticks =
            std::max(1, static_cast<int>(std::lround((spec.max - spec.min) / step)));
        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(0, ticks);
        auto* spin = new QDoubleSpinBox(row);
        spin->setRange(spec.min, spec.max);
        spin->setSingleStep(step);
        spin->setDecimals(step < 0.1 ? 2 : 1);
        connect(slider, &QSlider::valueChanged, spin, [spin, spec, step](int tick) {
          const QSignalBlocker blocker(spin);
          spin->setValue(spec.min + tick * step);
        });
        connect(spin,
                qOverload<double>(&QDoubleSpinBox::valueChanged),
                slider,
                [slider, spec, step](double value) {
                  const QSignalBlocker blocker(slider);
                  slider->setValue(
                      static_cast<int>(std::lround((value - spec.min) / step)));
                });
        row_layout->addWidget(slider, 1);
        row_layout->addWidget(spin);
        editor.read = [spin]() {
          return QJsonValue(spin->value());
        };
        editor.write = [spin, slider, spec, step](const QJsonValue& value) {
          const double v = value.toDouble(spec.default_value.toDouble(spec.min));
          spin->setValue(v);
          slider->setValue(static_cast<int>(std::lround((v - spec.min) / step)));
        };
        field = row;
        break;
      }
      case Generator::ParameterType::Int: {
        auto* spin = new QSpinBox(m_controls);
        spin->setRange(static_cast<int>(spec.min), static_cast<int>(spec.max));
        spin->setSingleStep(spec.step > 0.0 ? static_cast<int>(spec.step) : 1);
        editor.read = [spin]() {
          return QJsonValue(spin->value());
        };
        editor.write = [spin, spec](const QJsonValue& value) {
          spin->setValue(
              value.toInt(spec.default_value.toInt(static_cast<int>(spec.min))));
        };
        field = spin;
        break;
      }
      case Generator::ParameterType::Bool: {
        auto* check = new QCheckBox(m_controls);
        editor.read = [check]() {
          return QJsonValue(check->isChecked());
        };
        editor.write = [check, spec](const QJsonValue& value) {
          check->setChecked(value.toBool(spec.default_value.toBool()));
        };
        field = check;
        break;
      }
      case Generator::ParameterType::Choice: {
        auto* combo = new QComboBox(m_controls);
        for (const Generator::ParameterChoice& choice : spec.choices) {
          combo->addItem(choice.label, choice.id);
        }
        editor.read = [combo]() {
          return QJsonValue(combo->currentData().toString());
        };
        editor.write = [combo, spec](const QJsonValue& value) {
          const int index =
              combo->findData(value.toString(spec.default_value.toString()));
          combo->setCurrentIndex(std::max(0, index));
        };
        field = combo;
        break;
      }
      }
      field->setToolTip(spec.help);
      auto* label = new QLabel(spec.label, m_controls);
      label->setToolTip(spec.help);
      form->addRow(label, field);
      m_parameters.insert(spec.key, editor);
    }
  }

  auto* stages_box = new QGroupBox(QStringLiteral("Stages"), m_controls);
  auto* stages_layout = new QGridLayout(stages_box);
  stages_layout->setHorizontalSpacing(8);
  stages_layout->addWidget(
      create_label(QStringLiteral("Locked stages are kept when you press "
                                  "Reroll Unlocked."),
                   QStringLiteral("panelHint"),
                   stages_box),
      0,
      0,
      1,
      2);
  int row = 1;
  for (const Generator::StageSpec& stage : m_schema->stages) {
    StageRow stage_row;
    stage_row.lock =
        new QCheckBox(QStringLiteral("Lock %1").arg(stage.label), stages_box);
    stage_row.status = new QLabel(stages_box);
    stage_row.status->setObjectName(QStringLiteral("panelHint"));
    stage_row.status->setWordWrap(true);
    connect(stage_row.lock, &QCheckBox::toggled, this, [this]() { refresh_locks(); });
    stages_layout->addWidget(stage_row.lock, row, 0);
    stages_layout->addWidget(stage_row.status, row, 1);
    m_stage_rows.insert(stage.id, stage_row);
    ++row;
  }
  stages_layout->setColumnStretch(1, 1);
  layout->addWidget(stages_box);

  auto* buttons = new QGridLayout();
  m_generate_button = new QPushButton(QStringLiteral("Generate"), m_controls);
  m_generate_button->setToolTip(
      QStringLiteral("Generate a whole new map from this seed and preview it."));
  m_reroll_button = new QPushButton(QStringLiteral("Reroll Unlocked"), m_controls);
  m_reroll_button->setToolTip(QStringLiteral(
      "Keep the locked stages of the open map and every authored element, "
      "regenerate the rest with the next seed."));
  m_cancel_button = new QPushButton(QStringLiteral("Cancel"), m_controls);
  m_cancel_button->setVisible(false);
  auto* reset = new QPushButton(QStringLiteral("Reset Parameters"), m_controls);
  auto* save = new QPushButton(QStringLiteral("Save Request…"), m_controls);
  auto* load = new QPushButton(QStringLiteral("Load Request…"), m_controls);
  buttons->addWidget(m_generate_button, 0, 0);
  buttons->addWidget(m_reroll_button, 0, 1);
  buttons->addWidget(m_cancel_button, 1, 0, 1, 2);
  buttons->addWidget(reset, 2, 0);
  buttons->addWidget(save, 3, 0);
  buttons->addWidget(load, 3, 1);
  layout->addLayout(buttons);

  m_run_status = create_label(QString(), QStringLiteral("panelHint"), m_controls);
  layout->addWidget(m_run_status);

  connect(m_generate_button, &QPushButton::clicked, this, [this]() {
    start(Mode::Fresh, m_seed_box->value());
  });
  connect(m_reroll_button, &QPushButton::clicked, this, [this]() {
    m_seed_box->setValue(m_seed_box->value() < INT_MAX ? m_seed_box->value() + 1 : 1);
    start(Mode::Reroll, m_seed_box->value());
  });
  connect(m_cancel_button, &QPushButton::clicked, this, [this]() {
    if (m_process != nullptr) {
      m_cancelled = true;
      m_process->kill();
    }
  });
  connect(reset, &QPushButton::clicked, this, [this]() {
    apply_preset(m_preset_box->currentData().toString());
  });
  connect(save, &QPushButton::clicked, this, [this]() { save_request(); });
  connect(load, &QPushButton::clicked, this, [this]() { load_request(); });

  refresh_locks();
}

void GeneratorPanel::apply_preset(const QString& preset_id) {
  if (!m_schema.has_value()) {
    return;
  }
  const Generator::PresetSpec* preset = m_schema->preset(preset_id);
  if (preset == nullptr) {
    return;
  }
  m_preset_box->setCurrentIndex(std::max(0, m_preset_box->findData(preset_id)));
  m_preset_description->setText(preset->description);
  m_width_box->setValue(preset->width);
  m_height_box->setValue(preset->height);
  Generator::Request request;
  request.preset = preset_id;
  apply_values(Generator::resolved_parameters(*m_schema, request));
}

void GeneratorPanel::apply_values(const QJsonObject& values) {
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (const auto editor = m_parameters.constFind(it.key());
        editor != m_parameters.constEnd()) {
      editor->write(it.value());
    }
  }
}

void GeneratorPanel::prefill_from(const QJsonObject& generation) {
  if (!m_schema.has_value()) {
    m_pending_prefill = generation;
    return;
  }
  const Generator::Request request = Generator::request_from_json(generation);
  if (m_schema->preset(request.preset) != nullptr) {
    apply_preset(request.preset);
  }
  m_seed_box->setValue(static_cast<int>(std::clamp<qint64>(request.seed, 0, INT_MAX)));
  m_width_box->setValue(request.width);
  m_height_box->setValue(request.height);
  apply_values(request.parameters);
  for (auto it = m_stage_rows.begin(); it != m_stage_rows.end(); ++it) {
    const QSignalBlocker blocker(it->lock);
    it->lock->setChecked(request.locks.contains(it.key()));
  }
  refresh_locks();
}

auto GeneratorPanel::requested_locks() const -> QStringList {
  QStringList locks;
  if (!m_schema.has_value()) {
    return locks;
  }
  for (const Generator::StageSpec& stage : m_schema->stages) {
    if (const auto row = m_stage_rows.constFind(stage.id);
        row != m_stage_rows.constEnd() && row->lock->isChecked()) {
      locks.append(stage.id);
    }
  }
  return locks;
}

auto GeneratorPanel::stage_label(const QString& id) const -> QString {
  const Generator::StageSpec* stage =
      m_schema.has_value() ? m_schema->stage(id) : nullptr;
  return stage != nullptr ? stage->label : id;
}

void GeneratorPanel::refresh_locks() {
  if (!m_schema.has_value()) {
    return;
  }
  const QMap<QString, QStringList> forced =
      Generator::forced_stages(*m_schema, requested_locks());
  for (auto it = m_stage_rows.begin(); it != m_stage_rows.end(); ++it) {
    const auto forcing = forced.constFind(it.key());
    if (forcing != forced.constEnd()) {
      QStringList labels;
      for (const QString& id : *forcing) {
        labels.append(stage_label(id));
      }
      const QString why = QStringLiteral("Regenerates anyway: it is built from %1, "
                                         "which is unlocked.")
                              .arg(labels.join(QStringLiteral(", ")));
      it->lock->setToolTip(why);
      it->status->setText(
          QStringLiteral("forced by %1").arg(labels.join(QStringLiteral(", "))));
      it->lock->setEnabled(false);
    } else {
      it->lock->setToolTip(it->lock->isChecked()
                               ? QStringLiteral("Kept as it is on Reroll Unlocked.")
                               : QStringLiteral("Regenerated on Reroll Unlocked."));
      it->status->clear();
      it->lock->setEnabled(true);
    }
  }
}

auto GeneratorPanel::current_request() const -> Generator::Request {
  Generator::Request request;
  if (!m_schema.has_value()) {
    return request;
  }
  request.seed = m_seed_box->value();
  request.preset = m_preset_box->currentData().toString();
  request.width = m_width_box->value();
  request.height = m_height_box->value();
  for (auto it = m_parameters.constBegin(); it != m_parameters.constEnd(); ++it) {
    request.parameters[it.key()] = it->read();
  }
  request.locks = Generator::effective_locks(*m_schema, requested_locks());
  return request;
}

void GeneratorPanel::set_busy(bool busy) {
  for (auto* box : m_controls->findChildren<QGroupBox*>()) {
    box->setEnabled(!busy);
  }
  for (auto* button : m_controls->findChildren<QPushButton*>()) {
    if (button != m_cancel_button) {
      button->setEnabled(!busy);
    }
  }
  m_cancel_button->setVisible(busy);
}

void GeneratorPanel::start(Mode mode, qint64 seed) {
  if (!m_schema.has_value() || m_process != nullptr) {
    return;
  }
  const QString script = Generator::script_path(m_repository_root);
  const auto python = Generator::find_python();
  if (script.isEmpty() || !python.has_value()) {
    show_error(QStringLiteral("The generator script or Python could not be found."));
    return;
  }

  m_run_dir = std::make_unique<QTemporaryDir>();
  if (!m_run_dir->isValid()) {
    show_error(QStringLiteral("Could not create a temporary directory for the run."));
    return;
  }

  m_mode = mode;
  m_running_request = current_request();
  m_running_request.seed = seed;
  if (mode == Mode::Fresh) {
    m_running_request.locks.clear();
  }

  Generator::Invocation invocation;
  invocation.script = script;
  invocation.output_path = m_run_dir->filePath(QStringLiteral("candidate.json"));
  invocation.report_path = m_run_dir->filePath(QStringLiteral("report.json"));
  if (mode == Mode::Reroll) {
    invocation.base_path = m_run_dir->filePath(QStringLiteral("base.json"));
    QFile base(invocation.base_path);
    if (!base.open(QIODevice::WriteOnly) ||
        base.write(m_map_data->to_json_bytes()) < 0) {
      show_error(QStringLiteral("Could not write the open map for the reroll."));
      return;
    }
  }

  for (auto it = m_stage_rows.begin(); it != m_stage_rows.end(); ++it) {
    const bool kept = m_running_request.locks.contains(it.key());
    it->status->setText(kept ? QStringLiteral("■ kept") : QStringLiteral("· queued"));
  }
  m_message->setVisible(false);
  m_progress = Generator::ProgressParser();
  m_generator_error.clear();
  m_stderr.clear();
  m_cancelled = false;
  m_run_status->setText(
      mode == Mode::Fresh
          ? QStringLiteral("Generating seed %1…").arg(seed)
          : QStringLiteral("Rerolling unlocked stages with seed %1…").arg(seed));

  m_process = new QProcess(this);
  m_process->setWorkingDirectory(m_repository_root);
  connect(
      m_process, &QProcess::readyReadStandardOutput, this, [this]() { on_progress(); });
  connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
    m_stderr += m_process->readAllStandardError();
    if (m_stderr.size() > k_max_stderr_bytes) {
      m_stderr = m_stderr.right(k_max_stderr_bytes);
    }
  });
  connect(m_process, &QProcess::finished, this, &GeneratorPanel::on_finished);
  connect(
      m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_process != nullptr) {
          const QString message = m_process->errorString();
          m_process->deleteLater();
          m_process = nullptr;
          set_busy(false);
          show_error(QStringLiteral("Could not start Python: %1").arg(message));
        }
      });
  set_busy(true);
  m_process->start(python->program,
                   python->prefix_arguments +
                       Generator::build_arguments(m_running_request, invocation));
}

void GeneratorPanel::on_progress() {
  if (m_process == nullptr) {
    return;
  }
  m_progress.feed(m_process->readAllStandardOutput());
  for (const Generator::ProgressEvent& event : m_progress.take_events()) {
    if (event.kind == Generator::ProgressEvent::Kind::Error) {
      m_generator_error = event.message;
      continue;
    }
    if (event.kind != Generator::ProgressEvent::Kind::Stage) {
      continue;
    }
    if (const auto row = m_stage_rows.find(event.stage); row != m_stage_rows.end()) {
      QString text = status_text(event.status);
      if (event.seconds >= 0.0 && event.status != QLatin1String("running")) {
        text += QStringLiteral(" (%1 s)").arg(event.seconds, 0, 'f', 1);
      }
      row->status->setText(text);
    }
    const QString attempt = event.attempt > 1
                                ? QStringLiteral("Attempt %1 · ").arg(event.attempt)
                                : QString();
    m_run_status->setText(
        QStringLiteral("%1%2: %3")
            .arg(attempt, stage_label(event.stage), status_text(event.status)));
  }
}

void GeneratorPanel::on_finished(int exit_code, QProcess::ExitStatus status) {
  if (m_process == nullptr) {
    return;
  }
  on_progress();
  m_progress.finish();
  m_stderr += m_process->readAllStandardError();
  m_process->deleteLater();
  m_process = nullptr;
  set_busy(false);

  if (m_cancelled) {
    m_run_status->setText(QStringLiteral("Generation cancelled."));
    emit feedback(QStringLiteral("Map generation cancelled."), false);
    return;
  }

  const QString output = m_run_dir->filePath(QStringLiteral("candidate.json"));
  const QString report_path = m_run_dir->filePath(QStringLiteral("report.json"));
  const auto report_bytes = read_file(report_path);
  std::optional<Generator::Report> report;
  QString report_error;
  if (report_bytes.has_value()) {
    report = Generator::parse_report(*report_bytes, &report_error);
  }
  const auto candidate = read_file(output);

  if (status != QProcess::NormalExit || exit_code >= 2 || !candidate.has_value() ||
      !report.has_value()) {
    QString why =
        status != QProcess::NormalExit
            ? QStringLiteral("the generator crashed")
            : QStringLiteral("the generator exited with status %1").arg(exit_code);
    if (!m_generator_error.isEmpty()) {
      why += QStringLiteral("\n") + m_generator_error;
    }
    if (report.has_value()) {
      for (const Generator::ReportCheck& check : report->checks) {
        if (check.status == Generator::CheckStatus::Fail) {
          why += QStringLiteral("\n✗ ") + check.message;
        }
      }
    } else if (!report_error.isEmpty()) {
      why += QStringLiteral("\nreport: ") + report_error;
    }
    const QString tail = stderr_tail(m_stderr);
    if (!tail.isEmpty()) {
      why += QStringLiteral("\n\n") + tail;
    }
    show_error(QStringLiteral("No map was produced: %1").arg(why));
    m_run_status->setText(QStringLiteral("Generation failed."));
    emit feedback(QStringLiteral("Map generation failed; see the Generator tab."),
                  false);
    return;
  }

  m_run_status->setText(
      report->ok
          ? QStringLiteral("Candidate ready (seed %1).").arg(m_running_request.seed)
          : QStringLiteral("Candidate ready with validation failures (seed %1).")
                .arg(m_running_request.seed));
  show_candidate(*candidate, *report);
}

void GeneratorPanel::show_candidate(const QByteArray& candidate,
                                    const Generator::Report& report) {
  const QString title = QStringLiteral("Generated map preview — %1, seed %2")
                            .arg(m_preset_box->currentText())
                            .arg(m_running_request.seed);
  GenerationPreviewDialog dialog(candidate, report, title, window());
  dialog.exec();

  switch (dialog.choice()) {
  case GenerationPreviewDialog::Choice::Accept: {
    QString error;
    const QString description =
        m_mode == Mode::Fresh
            ? QStringLiteral("Generate map (seed %1)").arg(m_running_request.seed)
            : QStringLiteral("Reroll unlocked stages (seed %1)")
                  .arg(m_running_request.seed);
    if (!m_map_data->replace_document(candidate, description, &error)) {
      show_error(QStringLiteral("The candidate could not be applied: %1").arg(error));
      return;
    }
    m_last_document_generation = m_map_data->generation();
    m_run_status->setText(
        QStringLiteral("Accepted seed %1.").arg(m_running_request.seed));
    emit document_replaced();
    emit feedback(description + QStringLiteral(" — accepted. Undo restores the "
                                               "previous map."),
                  true);
    break;
  }
  case GenerationPreviewDialog::Choice::Reroll: {
    const qint64 next =
        m_running_request.seed < INT_MAX ? m_running_request.seed + 1 : 1;
    m_seed_box->setValue(static_cast<int>(next));
    start(m_mode, next);
    break;
  }
  case GenerationPreviewDialog::Choice::Adjust:
    m_run_status->setText(QStringLiteral("Adjust the parameters and generate again."));
    break;
  case GenerationPreviewDialog::Choice::Cancel:
    m_run_status->setText(QStringLiteral("Preview discarded; the map is unchanged."));
    break;
  }
}

void GeneratorPanel::save_request() {
  const QString path =
      QFileDialog::getSaveFileName(this,
                                   QStringLiteral("Save Generation Request"),
                                   QStringLiteral("generation_request.json"),
                                   QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(Generator::request_to_json(current_request()))
                     .toJson(QJsonDocument::Indented)) < 0) {
    emit feedback(QStringLiteral("Could not write %1").arg(path), false);
    return;
  }
  emit feedback(QStringLiteral("Saved generation request to %1").arg(path), true);
}

void GeneratorPanel::load_request() {
  const QString path =
      QFileDialog::getOpenFileName(this,
                                   QStringLiteral("Load Generation Request"),
                                   QString(),
                                   QStringLiteral("JSON (*.json)"));
  if (path.isEmpty()) {
    return;
  }
  const auto bytes = read_file(path);
  const QJsonObject root =
      bytes.has_value() ? QJsonDocument::fromJson(*bytes).object() : QJsonObject();
  const QJsonObject request = root.contains(QStringLiteral("generation"))
                                  ? root.value(QStringLiteral("generation")).toObject()
                                  : root;
  if (!request.contains(QStringLiteral("seed"))) {
    emit feedback(
        QStringLiteral("%1 is neither a generation request nor a generated map.")
            .arg(path),
        false);
    return;
  }
  prefill_from(request);
  emit feedback(QStringLiteral("Loaded generation request from %1").arg(path), true);
}

void GeneratorPanel::sync_from_document() {
  if (m_map_data == nullptr) {
    return;
  }
  const QJsonObject generation = m_map_data->generation();
  if (generation == m_last_document_generation) {
    return;
  }
  m_last_document_generation = generation;
  if (!generation.isEmpty() && !is_running()) {
    prefill_from(generation);
  }
}

} // namespace MapEditor
