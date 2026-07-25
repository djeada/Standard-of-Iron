#include "mission_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace MapEditor {

namespace {

auto combo(const QStringList& values, QWidget* parent) -> QComboBox* {
  auto* result = new QComboBox(parent);
  result->addItems(values);
  return result;
}

void select_value(QComboBox* box, const QString& value, int fallback = 0) {
  const int index = box->findText(value);
  box->setCurrentIndex(index >= 0 ? index : fallback);
}

auto double_box(double minimum,
                double maximum,
                double value,
                QWidget* parent,
                int decimals = 2) -> QDoubleSpinBox* {
  auto* box = new QDoubleSpinBox(parent);
  box->setRange(minimum, maximum);
  box->setDecimals(decimals);
  box->setValue(value);
  return box;
}

auto int_box(int minimum, int maximum, int value, QWidget* parent) -> QSpinBox* {
  auto* box = new QSpinBox(parent);
  box->setRange(minimum, maximum);
  box->setValue(value);
  return box;
}

auto button_row(QWidget* parent,
                const QString& add_text,
                QPushButton*& add_button,
                QPushButton*& edit_button,
                QPushButton*& remove_button) -> QWidget* {
  auto* row = new QWidget(parent);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  add_button = new QPushButton(add_text, row);
  edit_button = new QPushButton(QStringLiteral("Edit"), row);
  remove_button = new QPushButton(QStringLiteral("Remove"), row);
  layout->addWidget(add_button);
  layout->addWidget(edit_button);
  layout->addWidget(remove_button);
  return row;
}

auto table(const QStringList& headers, QWidget* parent) -> QTableWidget* {
  auto* result = new QTableWidget(0, headers.size(), parent);
  result->setHorizontalHeaderLabels(headers);
  result->setSelectionBehavior(QAbstractItemView::SelectRows);
  result->setSelectionMode(QAbstractItemView::SingleSelection);
  result->setEditTriggers(QAbstractItemView::NoEditTriggers);
  result->verticalHeader()->setVisible(false);
  result->horizontalHeader()->setStretchLastSection(true);
  result->setMinimumHeight(120);
  return result;
}

void set_row(QTableWidget* target, int row, const QStringList& values) {
  target->insertRow(row);
  for (int column = 0; column < values.size(); ++column) {
    target->setItem(row, column, new QTableWidgetItem(values[column]));
  }
}

auto selected_row(QTableWidget* target) -> int {
  const QModelIndexList selected = target->selectionModel()->selectedRows();
  return selected.isEmpty() ? -1 : selected.first().row();
}

auto accepted_dialog(QDialog& dialog) -> bool {
  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  dialog.layout()->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  return dialog.exec() == QDialog::Accepted;
}

auto edit_ai_dialog(const QJsonObject& initial,
                    QWidget* parent) -> std::optional<QJsonObject> {
  QDialog dialog(parent);
  dialog.setWindowTitle(initial.isEmpty() ? QStringLiteral("Add AI Opponent")
                                          : QStringLiteral("Edit AI Opponent"));
  auto* form = new QFormLayout(&dialog);
  auto* id = new QLineEdit(initial.value("id").toString("enemy_1"), &dialog);
  auto* nation = combo(MissionData::supported_nations(), &dialog);
  auto* faction = combo({"roman", "carthaginian", "iron_sepulcher"}, &dialog);
  auto* color = combo(MissionData::supported_colors(), &dialog);
  auto* difficulty = combo(MissionData::supported_difficulties(), &dialog);
  auto* strategy = combo(MissionData::supported_strategies(), &dialog);
  auto* team = int_box(-1, 32, initial.value("team_id").toInt(-1), &dialog);
  team->setSpecialValueText(QStringLiteral("Independent"));
  auto* commander =
      combo(QStringList{QStringLiteral("(default)")} + MissionData::supported_troops(),
            &dialog);
  const QJsonObject personality = initial.value("personality").toObject();
  auto* aggression =
      double_box(0.0, 1.0, personality.value("aggression").toDouble(0.5), &dialog);
  auto* defense =
      double_box(0.0, 1.0, personality.value("defense").toDouble(0.5), &dialog);
  auto* harassment =
      double_box(0.0, 1.0, personality.value("harassment").toDouble(0.5), &dialog);

  select_value(nation, initial.value("nation").toString("carthage"));
  select_value(faction, initial.value("faction").toString("carthaginian"));
  select_value(color, initial.value("color").toString("blue"));
  select_value(difficulty, initial.value("difficulty").toString("normal"));
  select_value(strategy, initial.value("strategy").toString("balanced"));
  select_value(commander, initial.value("commander_troop").toString("(default)"));

  form->addRow(QStringLiteral("ID"), id);
  form->addRow(QStringLiteral("Nation"), nation);
  form->addRow(QStringLiteral("Faction"), faction);
  form->addRow(QStringLiteral("Color"), color);
  form->addRow(QStringLiteral("Difficulty"), difficulty);
  form->addRow(QStringLiteral("Strategy"), strategy);
  form->addRow(QStringLiteral("Team"), team);
  form->addRow(QStringLiteral("Commander"), commander);
  form->addRow(QStringLiteral("Aggression"), aggression);
  form->addRow(QStringLiteral("Defense"), defense);
  form->addRow(QStringLiteral("Harassment"), harassment);

  if (!accepted_dialog(dialog) || id->text().trimmed().isEmpty()) {
    return std::nullopt;
  }

  QJsonObject result = initial;
  result["id"] = id->text().trimmed();
  result["nation"] = nation->currentText();
  result["faction"] = faction->currentText();
  result["color"] = color->currentText();
  result["difficulty"] = difficulty->currentText();
  result["strategy"] = strategy->currentText();
  if (team->value() >= 0) {
    result["team_id"] = team->value();
  } else {
    result.remove("team_id");
  }
  if (commander->currentIndex() > 0) {
    result["commander_troop"] = commander->currentText();
  } else {
    result.remove("commander_troop");
  }
  result["personality"] = QJsonObject{{"aggression", aggression->value()},
                                      {"defense", defense->value()},
                                      {"harassment", harassment->value()}};
  if (!result.contains("starting_units")) {
    result["starting_units"] = QJsonArray{};
  }
  if (!result.contains("starting_buildings")) {
    result["starting_buildings"] = QJsonArray{};
  }
  if (!result.contains("waves")) {
    result["waves"] = QJsonArray{};
  }
  return result;
}

struct ForceLocation {
  int owner = -1;
  bool building = false;
  int index = -1;
};

auto edit_force_dialog(const QJsonArray& ai_setups,
                       const ForceLocation& initial_location,
                       const QJsonObject& initial,
                       QWidget* parent)
    -> std::optional<std::pair<ForceLocation, QJsonObject>> {
  QDialog dialog(parent);
  dialog.setWindowTitle(initial.isEmpty() ? QStringLiteral("Add Starting Force")
                                          : QStringLiteral("Edit Starting Force"));
  auto* form = new QFormLayout(&dialog);
  auto* owner = new QComboBox(&dialog);
  owner->addItem(QStringLiteral("Player"), -1);
  for (qsizetype i = 0; i < ai_setups.size(); ++i) {
    owner->addItem(ai_setups[i].toObject().value("id").toString(), i);
  }
  const int owner_index = owner->findData(initial_location.owner);
  owner->setCurrentIndex(std::max(0, owner_index));

  auto* kind = combo({"Troop", "Structure"}, &dialog);
  kind->setCurrentIndex(initial_location.building ? 1 : 0);
  auto* type = new QComboBox(&dialog);
  auto refill_types = [type](bool building) {
    const QString previous = type->currentText();
    type->clear();
    type->addItems(building ? MissionData::supported_structures()
                            : MissionData::supported_troops());
    select_value(type, previous);
  };
  refill_types(initial_location.building);
  select_value(type, initial.value("type").toString());
  auto* count = int_box(1, 1000, initial.value("count").toInt(1), &dialog);
  auto* population =
      int_box(1, 5000, initial.value("max_population").toInt(150), &dialog);
  const QJsonObject position = initial.value("position").toObject();
  auto* x = double_box(-10000, 10000, position.value("x").toDouble(), &dialog);
  auto* z = double_box(-10000, 10000, position.value("z").toDouble(), &dialog);
  auto* behavior = combo({"strategic", "guard", "hold", "patrol"}, &dialog);
  select_value(behavior, initial.value("behavior").toString("strategic"));

  form->addRow(QStringLiteral("Owner"), owner);
  form->addRow(QStringLiteral("Kind"), kind);
  form->addRow(QStringLiteral("Type"), type);
  form->addRow(QStringLiteral("Count"), count);
  form->addRow(QStringLiteral("Max population"), population);
  form->addRow(QStringLiteral("X"), x);
  form->addRow(QStringLiteral("Z"), z);
  form->addRow(QStringLiteral("Behavior"), behavior);
  count->setVisible(!initial_location.building);
  population->setVisible(initial_location.building);
  behavior->setVisible(!initial_location.building);
  QObject::connect(kind, &QComboBox::currentIndexChanged, &dialog, [=](int index) {
    const bool building = index == 1;
    refill_types(building);
    count->setVisible(!building);
    population->setVisible(building);
    behavior->setVisible(!building);
  });

  if (!accepted_dialog(dialog)) {
    return std::nullopt;
  }

  ForceLocation location;
  location.owner = owner->currentData().toInt();
  location.building = kind->currentIndex() == 1;
  location.index = initial_location.index;
  QJsonObject result{{"type", type->currentText()},
                     {"position", QJsonObject{{"x", x->value()}, {"z", z->value()}}}};
  if (location.building) {
    result["max_population"] = population->value();
  } else {
    result["count"] = count->value();
    result["behavior"] = behavior->currentText();
  }
  return std::make_pair(location, result);
}

auto edit_condition_dialog(const QString& key,
                           const QJsonObject& initial,
                           const QVector<UndeadZoneElement>& zones,
                           QWidget* parent) -> std::optional<QJsonObject> {
  QDialog dialog(parent);
  dialog.setWindowTitle(initial.isEmpty() ? QStringLiteral("Add Objective")
                                          : QStringLiteral("Edit Objective"));
  auto* form = new QFormLayout(&dialog);
  QStringList types;
  if (key == "victory_conditions") {
    types = MissionData::supported_victory_conditions();
  } else if (key == "defeat_conditions") {
    types = MissionData::supported_defeat_conditions();
  } else {
    types = MissionData::supported_optional_objectives();
  }
  auto* type = combo(types, &dialog);
  select_value(type, initial.value("type").toString());
  auto* description = new QPlainTextEdit(&dialog);
  description->setPlainText(initial.value("description").toString());
  description->setMaximumHeight(90);
  auto* duration =
      double_box(1.0, 86400.0, initial.value("duration").toDouble(300.0), &dialog);
  auto* structure = combo(MissionData::supported_structures(), &dialog);
  const QJsonArray structure_types = initial.value("structure_types").toArray();
  select_value(structure,
               !structure_types.isEmpty()
                   ? structure_types.first().toString()
                   : initial.value("structure_type").toString("barracks"));
  auto* min_count = int_box(1, 100, initial.value("min_count").toInt(1), &dialog);
  auto* zone = new QComboBox(&dialog);
  for (const UndeadZoneElement& item : zones) {
    zone->addItem(item.id);
  }
  if (zone->count() == 0) {
    zone->addItem(QStringLiteral("(add an undead zone on the map)"));
  }
  select_value(zone, initial.value("zone_id").toString());
  auto* wave_count = int_box(1, 100, initial.value("wave_count").toInt(1), &dialog);

  form->addRow(QStringLiteral("Type"), type);
  form->addRow(QStringLiteral("Objective text"), description);
  form->addRow(QStringLiteral("Duration (s)"), duration);
  form->addRow(QStringLiteral("Structure"), structure);
  form->addRow(QStringLiteral("Minimum count"), min_count);
  form->addRow(QStringLiteral("Objective zone"), zone);
  form->addRow(QStringLiteral("Wave count"), wave_count);

  auto update_visibility = [=]() {
    const QString selected = type->currentText();
    duration->setVisible(selected == "survive_duration" || selected == "time_pressure");
    const bool uses_structure =
        selected == "control_structures" || selected == "capture_structures" ||
        selected == "lose_structure" || selected == "only_commander_remaining";
    structure->setVisible(uses_structure);
    min_count->setVisible(selected == "control_structures" ||
                          selected == "capture_structures");
    const bool uses_zone = selected == "clear_undead_zone" ||
                           selected == "purify_shrine" ||
                           selected == "survive_undead_wave";
    zone->setVisible(uses_zone);
    wave_count->setVisible(selected == "survive_undead_wave" ||
                           selected == "wave_count");
  };
  QObject::connect(type, &QComboBox::currentTextChanged, &dialog, update_visibility);
  update_visibility();

  if (!accepted_dialog(dialog) || description->toPlainText().trimmed().isEmpty()) {
    return std::nullopt;
  }

  const QString selected = type->currentText();
  QJsonObject result{{"type", selected},
                     {"description", description->toPlainText().trimmed()}};
  if (selected == "survive_duration" || selected == "time_pressure") {
    result["duration"] = duration->value();
  }
  if (selected == "control_structures" || selected == "capture_structures") {
    result["structure_types"] = QJsonArray{structure->currentText()};
    result["min_count"] = min_count->value();
  } else if (selected == "lose_structure" || selected == "only_commander_remaining") {
    result["structure_type"] = structure->currentText();
  }
  if (selected == "clear_undead_zone" || selected == "purify_shrine" ||
      selected == "survive_undead_wave") {
    result["zone_id"] = zone->currentText();
  }
  if (selected == "survive_undead_wave" || selected == "wave_count") {
    result["wave_count"] = wave_count->value();
  }
  return result;
}

struct WaveLocation {
  int ai_index = -1;
  int wave_index = -1;
};

auto edit_wave_dialog(const QJsonArray& ai_setups,
                      const WaveLocation& initial_location,
                      const QJsonObject& initial,
                      QWidget* parent)
    -> std::optional<std::pair<WaveLocation, QJsonObject>> {
  if (ai_setups.isEmpty()) {
    return std::nullopt;
  }
  QDialog dialog(parent);
  dialog.setWindowTitle(initial.isEmpty() ? QStringLiteral("Add Reinforcement Wave")
                                          : QStringLiteral("Edit Reinforcement Wave"));
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout;
  auto* ai = new QComboBox(&dialog);
  for (qsizetype i = 0; i < ai_setups.size(); ++i) {
    ai->addItem(ai_setups[i].toObject().value("id").toString(), i);
  }
  const int ai_combo_index = ai->findData(initial_location.ai_index);
  ai->setCurrentIndex(std::max(0, ai_combo_index));
  auto* timing =
      double_box(0.0, 86400.0, initial.value("timing").toDouble(60.0), &dialog);
  const QJsonObject entry = initial.value("entry_point").toObject();
  auto* x = double_box(-10000, 10000, entry.value("x").toDouble(), &dialog);
  auto* z = double_box(-10000, 10000, entry.value("z").toDouble(), &dialog);
  form->addRow(QStringLiteral("AI opponent"), ai);
  form->addRow(QStringLiteral("Arrival time (s)"), timing);
  form->addRow(QStringLiteral("Entry X"), x);
  form->addRow(QStringLiteral("Entry Z"), z);
  layout->addLayout(form);

  layout->addWidget(new QLabel(QStringLiteral("Composition"), &dialog));
  auto* composition = new QListWidget(&dialog);
  for (const QJsonValue& value : initial.value("composition").toArray()) {
    const QJsonObject item = value.toObject();
    auto* row = new QListWidgetItem(QStringLiteral("%1 × %2")
                                        .arg(item.value("count").toInt())
                                        .arg(item.value("type").toString()),
                                    composition);
    row->setData(Qt::UserRole, item);
  }
  layout->addWidget(composition);
  auto* composition_row = new QWidget(&dialog);
  auto* composition_layout = new QHBoxLayout(composition_row);
  composition_layout->setContentsMargins(0, 0, 0, 0);
  auto* troop = combo(MissionData::supported_troops(), composition_row);
  auto* count = int_box(1, 1000, 1, composition_row);
  auto* add = new QPushButton(QStringLiteral("Add troop"), composition_row);
  auto* remove = new QPushButton(QStringLiteral("Remove troop"), composition_row);
  composition_layout->addWidget(troop);
  composition_layout->addWidget(count);
  composition_layout->addWidget(add);
  composition_layout->addWidget(remove);
  layout->addWidget(composition_row);
  QObject::connect(add, &QPushButton::clicked, &dialog, [=]() {
    const QJsonObject item{{"type", troop->currentText()}, {"count", count->value()}};
    auto* row = new QListWidgetItem(
        QStringLiteral("%1 × %2").arg(count->value()).arg(troop->currentText()),
        composition);
    row->setData(Qt::UserRole, item);
  });
  QObject::connect(remove, &QPushButton::clicked, &dialog, [=]() {
    delete composition->takeItem(composition->currentRow());
  });

  if (!accepted_dialog(dialog) || composition->count() == 0) {
    return std::nullopt;
  }

  QJsonArray composition_array;
  for (int i = 0; i < composition->count(); ++i) {
    composition_array.append(composition->item(i)->data(Qt::UserRole).toJsonObject());
  }
  WaveLocation location{ai->currentData().toInt(), initial_location.wave_index};
  QJsonObject result{
      {"timing", timing->value()},
      {"composition", composition_array},
      {"entry_point", QJsonObject{{"x", x->value()}, {"z", z->value()}}}};
  return std::make_pair(location, result);
}

auto edit_phase_dialog(const QJsonObject& initial,
                       QWidget* parent) -> std::optional<QJsonObject> {
  QDialog dialog(parent);
  dialog.setWindowTitle(initial.isEmpty() ? QStringLiteral("Add Battlefield Phase")
                                          : QStringLiteral("Edit Battlefield Phase"));
  auto* form = new QFormLayout(&dialog);
  const QJsonObject trigger = initial.value("trigger").toObject();
  auto* time = double_box(0.0, 86400.0, trigger.value("time").toDouble(30.0), &dialog);
  auto* text = new QPlainTextEdit(&dialog);
  const QJsonArray actions = initial.value("actions").toArray();
  if (!actions.isEmpty()) {
    text->setPlainText(actions.first().toObject().value("text").toString());
  }
  text->setMaximumHeight(100);
  form->addRow(QStringLiteral("Start time (s)"), time);
  form->addRow(QStringLiteral("Announcement"), text);
  if (!accepted_dialog(dialog) || text->toPlainText().trimmed().isEmpty()) {
    return std::nullopt;
  }
  return QJsonObject{
      {"trigger", QJsonObject{{"type", "timer"}, {"time", time->value()}}},
      {"actions",
       QJsonArray{QJsonObject{{"type", "show_message"},
                              {"text", text->toPlainText().trimmed()}}}}};
}

auto edit_fog_dialog(const FogZoneElement& initial,
                     QWidget* parent) -> std::optional<FogZoneElement> {
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Fog Zone"));
  auto* form = new QFormLayout(&dialog);
  auto* x = double_box(-10000, 10000, initial.x, &dialog);
  auto* z = double_box(-10000, 10000, initial.z, &dialog);
  auto* width = double_box(0.1, 10000, initial.width, &dialog);
  auto* height = double_box(0.1, 10000, initial.height, &dialog);
  auto* density = double_box(0.0, 1.0, initial.density, &dialog);
  form->addRow(QStringLiteral("Center X"), x);
  form->addRow(QStringLiteral("Center Z"), z);
  form->addRow(QStringLiteral("Width"), width);
  form->addRow(QStringLiteral("Height"), height);
  form->addRow(QStringLiteral("Density"), density);
  if (!accepted_dialog(dialog)) {
    return std::nullopt;
  }
  return FogZoneElement{static_cast<float>(x->value()),
                        static_cast<float>(z->value()),
                        static_cast<float>(width->value()),
                        static_cast<float>(height->value()),
                        static_cast<float>(density->value())};
}

auto group(const QString& title, QVBoxLayout*& layout, QWidget* parent) -> QGroupBox* {
  auto* result = new QGroupBox(title, parent);
  layout = new QVBoxLayout(result);
  return result;
}

} // namespace

MissionPanel::MissionPanel(MissionData* mission_data,
                           MapData* map_data,
                           QWidget* parent)
    : QWidget(parent)
    , m_mission_data(mission_data)
    , m_map_data(map_data) {
  setup_ui();
  connect(m_mission_data, &MissionData::data_changed, this, &MissionPanel::refresh);
  connect(m_map_data, &MapData::data_changed, this, &MissionPanel::refresh);
  refresh();
}

void MissionPanel::setup_ui() {
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(8, 8, 8, 8);
  root_layout->setSpacing(8);
  auto* title = new QLabel(QStringLiteral("Mission Authoring"), this);
  title->setObjectName("panelTitle");
  root_layout->addWidget(title);
  auto* hint = new QLabel(
      QStringLiteral("All selectors use options understood by the game runtime. "
                     "The linked battlefield remains editable on the canvas."),
      this);
  hint->setWordWrap(true);
  hint->setObjectName("panelIntro");
  root_layout->addWidget(hint);

  QVBoxLayout* identity_layout = nullptr;
  auto* identity_group =
      group(QStringLiteral("Mission & Intro"), identity_layout, this);
  auto* identity_form = new QFormLayout;
  m_id_edit = new QLineEdit(identity_group);
  m_title_edit = new QLineEdit(identity_group);
  m_summary_edit = new QPlainTextEdit(identity_group);
  m_summary_edit->setMaximumHeight(75);
  m_intro_edit = new QPlainTextEdit(identity_group);
  m_intro_edit->setMaximumHeight(75);
  m_map_path_edit = new QLineEdit(identity_group);
  auto* map_row = new QWidget(identity_group);
  auto* map_layout = new QHBoxLayout(map_row);
  map_layout->setContentsMargins(0, 0, 0, 0);
  auto* map_browse = new QPushButton(QStringLiteral("Browse"), map_row);
  map_layout->addWidget(m_map_path_edit, 1);
  map_layout->addWidget(map_browse);
  identity_form->addRow(QStringLiteral("ID"), m_id_edit);
  identity_form->addRow(QStringLiteral("Title"), m_title_edit);
  identity_form->addRow(QStringLiteral("Objective summary"), m_summary_edit);
  identity_form->addRow(QStringLiteral("Intro / narrative"), m_intro_edit);
  identity_form->addRow(QStringLiteral("Battlefield map"), map_row);
  identity_layout->addLayout(identity_form);
  root_layout->addWidget(identity_group);

  connect(map_browse, &QPushButton::clicked, this, [this]() {
    const QString path =
        QFileDialog::getOpenFileName(this,
                                     QStringLiteral("Select Battlefield Map"),
                                     {},
                                     QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty()) {
      m_map_path_edit->setText(path);
      sync_identity();
      emit map_path_changed(path);
    }
  });
  connect(m_id_edit, &QLineEdit::editingFinished, this, &MissionPanel::sync_identity);
  connect(
      m_title_edit, &QLineEdit::editingFinished, this, &MissionPanel::sync_identity);
  connect(
      m_summary_edit, &QPlainTextEdit::textChanged, this, &MissionPanel::sync_identity);
  connect(
      m_intro_edit, &QPlainTextEdit::textChanged, this, &MissionPanel::sync_identity);
  connect(m_map_path_edit, &QLineEdit::editingFinished, this, [this]() {
    sync_identity();
    emit map_path_changed(m_map_path_edit->text().trimmed());
  });

  QVBoxLayout* player_layout = nullptr;
  auto* player_group = group(QStringLiteral("Player Setup"), player_layout, this);
  auto* player_form = new QFormLayout;
  m_player_nation = combo(MissionData::supported_nations(), player_group);
  m_player_faction = combo({"roman", "carthaginian", "iron_sepulcher"}, player_group);
  m_player_color = combo(MissionData::supported_colors(), player_group);
  m_player_commander =
      combo(QStringList{QStringLiteral("(default)")} + MissionData::supported_troops(),
            player_group);
  m_player_gold = int_box(0, 1000000, 500, player_group);
  m_player_food = int_box(0, 1000000, 500, player_group);
  m_ambient_undead =
      new QCheckBox(QStringLiteral("Ambient undead enabled"), player_group);
  player_form->addRow(QStringLiteral("Nation"), m_player_nation);
  player_form->addRow(QStringLiteral("Faction"), m_player_faction);
  player_form->addRow(QStringLiteral("Color"), m_player_color);
  player_form->addRow(QStringLiteral("Commander"), m_player_commander);
  player_form->addRow(QStringLiteral("Gold"), m_player_gold);
  player_form->addRow(QStringLiteral("Food"), m_player_food);
  player_layout->addLayout(player_form);
  player_layout->addWidget(m_ambient_undead);
  root_layout->addWidget(player_group);
  connect(m_player_nation,
          &QComboBox::currentTextChanged,
          this,
          &MissionPanel::sync_player);
  connect(m_player_faction,
          &QComboBox::currentTextChanged,
          this,
          &MissionPanel::sync_player);
  connect(
      m_player_color, &QComboBox::currentTextChanged, this, &MissionPanel::sync_player);
  connect(m_player_commander,
          &QComboBox::currentTextChanged,
          this,
          &MissionPanel::sync_player);
  connect(m_player_gold, &QSpinBox::valueChanged, this, [this](int) { sync_player(); });
  connect(m_player_food, &QSpinBox::valueChanged, this, [this](int) { sync_player(); });
  connect(m_ambient_undead, &QCheckBox::toggled, this, [this](bool checked) {
    if (!m_refreshing) {
      m_mission_data->set_value("include_ambient_undead", checked);
    }
  });

  QVBoxLayout* ai_layout = nullptr;
  auto* ai_group = group(QStringLiteral("AI Strategy & Personality"), ai_layout, this);
  m_ai_table = table({"ID", "Nation", "Strategy", "Difficulty"}, ai_group);
  ai_layout->addWidget(m_ai_table);
  QPushButton* ai_add = nullptr;
  QPushButton* ai_edit = nullptr;
  QPushButton* ai_remove = nullptr;
  ai_layout->addWidget(
      button_row(ai_group, QStringLiteral("Add AI"), ai_add, ai_edit, ai_remove));
  root_layout->addWidget(ai_group);
  connect(ai_add, &QPushButton::clicked, this, [this]() { edit_ai(-1); });
  connect(ai_edit, &QPushButton::clicked, this, [this]() {
    edit_ai(selected_row(m_ai_table));
  });
  connect(ai_remove, &QPushButton::clicked, this, [this]() {
    remove_ai(selected_row(m_ai_table));
  });
  connect(m_ai_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    edit_ai(row);
  });

  QVBoxLayout* forces_layout = nullptr;
  auto* forces_group = group(QStringLiteral("Starting Forces"), forces_layout, this);
  m_forces_table = table({"Owner", "Kind", "Type", "Position"}, forces_group);
  forces_layout->addWidget(m_forces_table);
  QPushButton* force_add = nullptr;
  QPushButton* force_edit = nullptr;
  QPushButton* force_remove = nullptr;
  forces_layout->addWidget(button_row(
      forces_group, QStringLiteral("Add Force"), force_add, force_edit, force_remove));
  root_layout->addWidget(forces_group);
  connect(force_add, &QPushButton::clicked, this, [this]() { edit_force(-1); });
  connect(force_edit, &QPushButton::clicked, this, [this]() {
    edit_force(selected_row(m_forces_table));
  });
  connect(force_remove, &QPushButton::clicked, this, [this]() {
    remove_force(selected_row(m_forces_table));
  });

  const auto add_condition_group = [this, root_layout](const QString& title_text,
                                                       const QString& key,
                                                       QTableWidget*& target) {
    QVBoxLayout* section_layout = nullptr;
    auto* section = group(title_text, section_layout, this);
    target = table({"Type", "Objective text"}, section);
    section_layout->addWidget(target);
    QPushButton* add = nullptr;
    QPushButton* edit = nullptr;
    QPushButton* remove = nullptr;
    section_layout->addWidget(
        button_row(section, QStringLiteral("Add Objective"), add, edit, remove));
    root_layout->addWidget(section);
    connect(add, &QPushButton::clicked, this, [this, key, target]() {
      edit_condition(key, target, -1);
    });
    connect(edit, &QPushButton::clicked, this, [this, key, target]() {
      edit_condition(key, target, selected_row(target));
    });
    connect(remove, &QPushButton::clicked, this, [this, key, target]() {
      remove_condition(key, target, selected_row(target));
    });
  };
  add_condition_group(
      QStringLiteral("Victory Conditions"), "victory_conditions", m_victory_table);
  add_condition_group(
      QStringLiteral("Defeat Conditions"), "defeat_conditions", m_defeat_table);
  add_condition_group(
      QStringLiteral("Optional Objectives"), "optional_objectives", m_optional_table);

  QVBoxLayout* waves_layout = nullptr;
  auto* waves_group = group(QStringLiteral("Reinforcement Waves"), waves_layout, this);
  m_waves_table = table({"AI", "Time", "Composition", "Entry"}, waves_group);
  waves_layout->addWidget(m_waves_table);
  QPushButton* wave_add = nullptr;
  QPushButton* wave_edit = nullptr;
  QPushButton* wave_remove = nullptr;
  waves_layout->addWidget(button_row(
      waves_group, QStringLiteral("Add Wave"), wave_add, wave_edit, wave_remove));
  root_layout->addWidget(waves_group);
  connect(wave_add, &QPushButton::clicked, this, [this]() { edit_wave(-1); });
  connect(wave_edit, &QPushButton::clicked, this, [this]() {
    edit_wave(selected_row(m_waves_table));
  });
  connect(wave_remove, &QPushButton::clicked, this, [this]() {
    remove_wave(selected_row(m_waves_table));
  });

  QVBoxLayout* phases_layout = nullptr;
  auto* phases_group =
      group(QStringLiteral("Battlefield Phases & Announcements"), phases_layout, this);
  m_phases_table = table({"Time", "Message"}, phases_group);
  phases_layout->addWidget(m_phases_table);
  QPushButton* phase_add = nullptr;
  QPushButton* phase_edit = nullptr;
  QPushButton* phase_remove = nullptr;
  phases_layout->addWidget(button_row(
      phases_group, QStringLiteral("Add Phase"), phase_add, phase_edit, phase_remove));
  root_layout->addWidget(phases_group);
  connect(phase_add, &QPushButton::clicked, this, [this]() { edit_phase(-1); });
  connect(phase_edit, &QPushButton::clicked, this, [this]() {
    edit_phase(selected_row(m_phases_table));
  });
  connect(phase_remove, &QPushButton::clicked, this, [this]() {
    remove_phase(selected_row(m_phases_table));
  });

  QVBoxLayout* environment_layout = nullptr;
  auto* environment =
      group(QStringLiteral("Weather & Fog Zones"), environment_layout, this);
  auto* weather_form = new QFormLayout;
  m_weather_enabled = new QCheckBox(QStringLiteral("Enabled"), environment);
  m_weather_type = combo({"rain", "snow"}, environment);
  m_weather_intensity = double_box(0.0, 1.0, 0.5, environment);
  m_weather_cycle = double_box(1.0, 86400.0, 300.0, environment);
  m_weather_active = double_box(0.0, 86400.0, 60.0, environment);
  m_weather_fade = double_box(0.0, 3600.0, 5.0, environment);
  m_weather_wind = double_box(0.0, 10.0, 0.0, environment);
  weather_form->addRow(QStringLiteral("Weather"), m_weather_enabled);
  weather_form->addRow(QStringLiteral("Type"), m_weather_type);
  weather_form->addRow(QStringLiteral("Intensity"), m_weather_intensity);
  weather_form->addRow(QStringLiteral("Cycle (s)"), m_weather_cycle);
  weather_form->addRow(QStringLiteral("Active (s)"), m_weather_active);
  weather_form->addRow(QStringLiteral("Fade (s)"), m_weather_fade);
  weather_form->addRow(QStringLiteral("Wind"), m_weather_wind);
  environment_layout->addLayout(weather_form);
  m_fog_table = table({"Center", "Size", "Density"}, environment);
  environment_layout->addWidget(m_fog_table);
  QPushButton* fog_add = nullptr;
  QPushButton* fog_edit = nullptr;
  QPushButton* fog_remove = nullptr;
  environment_layout->addWidget(button_row(
      environment, QStringLiteral("Add Fog Zone"), fog_add, fog_edit, fog_remove));
  root_layout->addWidget(environment);
  for (QDoubleSpinBox* box : {m_weather_intensity,
                              m_weather_cycle,
                              m_weather_active,
                              m_weather_fade,
                              m_weather_wind}) {
    connect(
        box, &QDoubleSpinBox::valueChanged, this, [this](double) { sync_weather(); });
  }
  connect(
      m_weather_enabled, &QCheckBox::toggled, this, [this](bool) { sync_weather(); });
  connect(m_weather_type, &QComboBox::currentTextChanged, this, [this](const QString&) {
    sync_weather();
  });
  connect(fog_add, &QPushButton::clicked, this, [this]() { edit_fog_zone(-1); });
  connect(fog_edit, &QPushButton::clicked, this, [this]() {
    edit_fog_zone(selected_row(m_fog_table));
  });
  connect(fog_remove, &QPushButton::clicked, this, [this]() {
    remove_fog_zone(selected_row(m_fog_table));
  });

  auto* actions = new QGroupBox(QStringLiteral("Test Mission"), this);
  auto* action_layout = new QVBoxLayout(actions);
  auto* validate = new QPushButton(QStringLiteral("Validate Mission"), actions);
  auto* launch_game =
      new QPushButton(QStringLiteral("Validate & Launch Game"), actions);
  auto* launch_arena =
      new QPushButton(QStringLiteral("Validate & Launch Arena"), actions);
  launch_game->setProperty("primary", true);
  action_layout->addWidget(validate);
  action_layout->addWidget(launch_game);
  action_layout->addWidget(launch_arena);
  root_layout->addWidget(actions);
  connect(validate, &QPushButton::clicked, this, &MissionPanel::validate_requested);
  connect(
      launch_game, &QPushButton::clicked, this, &MissionPanel::launch_game_requested);
  connect(
      launch_arena, &QPushButton::clicked, this, &MissionPanel::launch_arena_requested);
  root_layout->addStretch(1);
}

void MissionPanel::refresh() {
  if (m_refreshing) {
    return;
  }
  m_refreshing = true;
  const QJsonObject root = m_mission_data->root();
  m_id_edit->setText(root.value("id").toString());
  m_title_edit->setText(root.value("title").toString());
  m_summary_edit->setPlainText(root.value("summary").toString());
  m_intro_edit->setPlainText(root.value("narrative_intent").toString());
  m_map_path_edit->setText(root.value("map_path").toString());
  const QJsonObject player = root.value("player_setup").toObject();
  select_value(m_player_nation, player.value("nation").toString());
  select_value(m_player_faction, player.value("faction").toString());
  select_value(m_player_color, player.value("color").toString());
  select_value(m_player_commander,
               player.value("commander_troop").toString("(default)"));
  const QJsonObject resources = player.value("starting_resources").toObject();
  m_player_gold->setValue(resources.value("gold").toInt());
  m_player_food->setValue(resources.value("food").toInt());
  m_ambient_undead->setChecked(root.value("include_ambient_undead").toBool());
  refresh_ai_table();
  refresh_forces_table();
  refresh_condition_tables();
  refresh_waves_table();
  refresh_phases_table();
  const QJsonObject rain = m_map_data->rain();
  m_weather_enabled->setChecked(rain.value("enabled").toBool(false));
  select_value(m_weather_type, rain.value("type").toString("rain"));
  m_weather_intensity->setValue(rain.value("intensity").toDouble(0.5));
  m_weather_cycle->setValue(rain.value("cycle_duration").toDouble(300.0));
  m_weather_active->setValue(rain.value("active_duration").toDouble(60.0));
  m_weather_fade->setValue(rain.value("fade_duration").toDouble(5.0));
  m_weather_wind->setValue(rain.value("wind_strength").toDouble(0.0));
  refresh_fog_table();
  m_refreshing = false;
}

void MissionPanel::sync_identity() {
  if (m_refreshing) {
    return;
  }
  QJsonObject root = m_mission_data->root();
  root["id"] = m_id_edit->text().trimmed();
  root["title"] = m_title_edit->text().trimmed();
  root["summary"] = m_summary_edit->toPlainText().trimmed();
  root["narrative_intent"] = m_intro_edit->toPlainText().trimmed();
  root["map_path"] = m_map_path_edit->text().trimmed();
  m_mission_data->set_root(root);
}

void MissionPanel::sync_player() {
  if (m_refreshing) {
    return;
  }
  QJsonObject player = m_mission_data->value("player_setup").toObject();
  player["nation"] = m_player_nation->currentText();
  player["faction"] = m_player_faction->currentText();
  player["color"] = m_player_color->currentText();
  if (m_player_commander->currentIndex() > 0) {
    player["commander_troop"] = m_player_commander->currentText();
  } else {
    player.remove("commander_troop");
  }
  QJsonObject resources = player.value("starting_resources").toObject();
  resources["gold"] = m_player_gold->value();
  resources["food"] = m_player_food->value();
  player["starting_resources"] = resources;
  if (!player.contains("starting_units")) {
    player["starting_units"] = QJsonArray{};
  }
  if (!player.contains("starting_buildings")) {
    player["starting_buildings"] = QJsonArray{};
  }
  m_mission_data->set_value("player_setup", player);
}

void MissionPanel::sync_weather() {
  if (m_refreshing) {
    return;
  }
  m_map_data->set_rain(QJsonObject{{"enabled", m_weather_enabled->isChecked()},
                                   {"type", m_weather_type->currentText()},
                                   {"cycle_duration", m_weather_cycle->value()},
                                   {"active_duration", m_weather_active->value()},
                                   {"intensity", m_weather_intensity->value()},
                                   {"fade_duration", m_weather_fade->value()},
                                   {"wind_strength", m_weather_wind->value()}});
}

void MissionPanel::refresh_ai_table() {
  m_ai_table->setRowCount(0);
  for (const QJsonValue& value : m_mission_data->array("ai_setups")) {
    const QJsonObject ai = value.toObject();
    set_row(m_ai_table,
            m_ai_table->rowCount(),
            {ai.value("id").toString(),
             ai.value("nation").toString(),
             ai.value("strategy").toString("balanced"),
             ai.value("difficulty").toString("normal")});
  }
}

void MissionPanel::edit_ai(int row) {
  QJsonArray values = m_mission_data->array("ai_setups");
  const QJsonObject initial =
      row >= 0 && row < values.size() ? values[row].toObject() : QJsonObject{};
  const auto edited = edit_ai_dialog(initial, this);
  if (!edited.has_value()) {
    return;
  }
  if (row >= 0 && row < values.size()) {
    values[row] = *edited;
  } else {
    values.append(*edited);
  }
  m_mission_data->set_array("ai_setups", values);
}

void MissionPanel::remove_ai(int row) {
  QJsonArray values = m_mission_data->array("ai_setups");
  if (row >= 0 && row < values.size()) {
    values.removeAt(row);
    m_mission_data->set_array("ai_setups", values);
  }
}

void MissionPanel::refresh_forces_table() {
  m_forces_table->setRowCount(0);
  const QJsonObject player = m_mission_data->value("player_setup").toObject();
  const QJsonArray ai_setups = m_mission_data->array("ai_setups");
  const auto add_owner =
      [this](const QString& owner_name, int owner, const QJsonObject& setup) {
        const auto append_items = [this, &owner_name, owner](const QJsonArray& items,
                                                             bool building) {
          for (qsizetype i = 0; i < items.size(); ++i) {
            const QJsonObject item = items[i].toObject();
            const QJsonObject position = item.value("position").toObject();
            const int row = m_forces_table->rowCount();
            set_row(m_forces_table,
                    row,
                    {owner_name,
                     building ? QStringLiteral("Structure") : QStringLiteral("Troop"),
                     item.value("type").toString(),
                     QStringLiteral("%1, %2")
                         .arg(position.value("x").toDouble(), 0, 'f', 1)
                         .arg(position.value("z").toDouble(), 0, 'f', 1)});
            m_forces_table->item(row, 0)->setData(Qt::UserRole, owner);
            m_forces_table->item(row, 0)->setData(Qt::UserRole + 1, building);
            m_forces_table->item(row, 0)->setData(Qt::UserRole + 2, i);
          }
        };
        append_items(setup.value("starting_units").toArray(), false);
        append_items(setup.value("starting_buildings").toArray(), true);
      };
  add_owner(QStringLiteral("Player"), -1, player);
  for (qsizetype i = 0; i < ai_setups.size(); ++i) {
    const QJsonObject ai = ai_setups[i].toObject();
    add_owner(ai.value("id").toString(), i, ai);
  }
}

void MissionPanel::edit_force(int row) {
  QJsonArray ai_setups = m_mission_data->array("ai_setups");
  QJsonObject player = m_mission_data->value("player_setup").toObject();
  ForceLocation old_location;
  QJsonObject initial;
  if (row >= 0 && row < m_forces_table->rowCount()) {
    QTableWidgetItem* marker = m_forces_table->item(row, 0);
    old_location = {marker->data(Qt::UserRole).toInt(),
                    marker->data(Qt::UserRole + 1).toBool(),
                    marker->data(Qt::UserRole + 2).toInt()};
    const QJsonObject owner =
        old_location.owner < 0 ? player : ai_setups[old_location.owner].toObject();
    const QString key = old_location.building ? "starting_buildings" : "starting_units";
    initial = owner.value(key).toArray()[old_location.index].toObject();
  }

  const auto edited = edit_force_dialog(ai_setups, old_location, initial, this);
  if (!edited.has_value()) {
    return;
  }

  const auto mutate = [&player, &ai_setups](const ForceLocation& location,
                                            const QJsonObject* add_value,
                                            bool remove) {
    QJsonObject owner =
        location.owner < 0 ? player : ai_setups[location.owner].toObject();
    const QString key = location.building ? QStringLiteral("starting_buildings")
                                          : QStringLiteral("starting_units");
    QJsonArray items = owner.value(key).toArray();
    if (remove && location.index >= 0 && location.index < items.size()) {
      items.removeAt(location.index);
    } else if (add_value != nullptr) {
      items.append(*add_value);
    }
    owner[key] = items;
    if (location.owner < 0) {
      player = owner;
    } else {
      ai_setups[location.owner] = owner;
    }
  };

  if (!initial.isEmpty()) {
    mutate(old_location, nullptr, true);
  }
  ForceLocation new_location = edited->first;
  new_location.index = -1;
  mutate(new_location, &edited->second, false);
  m_mission_data->set_value("player_setup", player);
  m_mission_data->set_array("ai_setups", ai_setups);
}

void MissionPanel::remove_force(int row) {
  if (row < 0 || row >= m_forces_table->rowCount()) {
    return;
  }
  QTableWidgetItem* marker = m_forces_table->item(row, 0);
  const int owner_index = marker->data(Qt::UserRole).toInt();
  const bool building = marker->data(Qt::UserRole + 1).toBool();
  const int item_index = marker->data(Qt::UserRole + 2).toInt();
  const QString key = building ? "starting_buildings" : "starting_units";
  if (owner_index < 0) {
    QJsonObject player = m_mission_data->value("player_setup").toObject();
    QJsonArray items = player.value(key).toArray();
    items.removeAt(item_index);
    player[key] = items;
    m_mission_data->set_value("player_setup", player);
  } else {
    QJsonArray ais = m_mission_data->array("ai_setups");
    QJsonObject ai = ais[owner_index].toObject();
    QJsonArray items = ai.value(key).toArray();
    items.removeAt(item_index);
    ai[key] = items;
    ais[owner_index] = ai;
    m_mission_data->set_array("ai_setups", ais);
  }
}

void MissionPanel::refresh_condition_tables() {
  const auto fill = [this](const QString& key, QTableWidget* target) {
    target->setRowCount(0);
    for (const QJsonValue& value : m_mission_data->array(key)) {
      const QJsonObject condition = value.toObject();
      set_row(target,
              target->rowCount(),
              {condition.value("type").toString(),
               condition.value("description").toString()});
    }
  };
  fill("victory_conditions", m_victory_table);
  fill("defeat_conditions", m_defeat_table);
  fill("optional_objectives", m_optional_table);
}

void MissionPanel::edit_condition(const QString& key,
                                  QTableWidget* table_widget,
                                  int row) {
  QJsonArray values = m_mission_data->array(key);
  const QJsonObject initial =
      row >= 0 && row < values.size() ? values[row].toObject() : QJsonObject{};
  const auto edited =
      edit_condition_dialog(key, initial, m_map_data->undead_zones(), this);
  if (!edited.has_value()) {
    return;
  }
  if (row >= 0 && row < values.size()) {
    values[row] = *edited;
  } else {
    values.append(*edited);
  }
  Q_UNUSED(table_widget)
  m_mission_data->set_array(key, values);
}

void MissionPanel::remove_condition(const QString& key,
                                    QTableWidget* table_widget,
                                    int row) {
  Q_UNUSED(table_widget)
  QJsonArray values = m_mission_data->array(key);
  if (row >= 0 && row < values.size()) {
    values.removeAt(row);
    m_mission_data->set_array(key, values);
  }
}

void MissionPanel::refresh_waves_table() {
  m_waves_table->setRowCount(0);
  const QJsonArray ais = m_mission_data->array("ai_setups");
  for (qsizetype ai_index = 0; ai_index < ais.size(); ++ai_index) {
    const QJsonObject ai = ais[ai_index].toObject();
    const QJsonArray waves = ai.value("waves").toArray();
    for (qsizetype wave_index = 0; wave_index < waves.size(); ++wave_index) {
      const QJsonObject wave = waves[wave_index].toObject();
      QStringList composition;
      for (const QJsonValue& entry_value : wave.value("composition").toArray()) {
        const QJsonObject entry = entry_value.toObject();
        composition.append(QStringLiteral("%1×%2")
                               .arg(entry.value("count").toInt())
                               .arg(entry.value("type").toString()));
      }
      const QJsonObject point = wave.value("entry_point").toObject();
      const int row = m_waves_table->rowCount();
      set_row(m_waves_table,
              row,
              {ai.value("id").toString(),
               QString::number(wave.value("timing").toDouble(), 'f', 0) + "s",
               composition.join(", "),
               QStringLiteral("%1, %2")
                   .arg(point.value("x").toDouble(), 0, 'f', 1)
                   .arg(point.value("z").toDouble(), 0, 'f', 1)});
      m_waves_table->item(row, 0)->setData(Qt::UserRole, ai_index);
      m_waves_table->item(row, 0)->setData(Qt::UserRole + 1, wave_index);
    }
  }
}

void MissionPanel::edit_wave(int row) {
  QJsonArray ais = m_mission_data->array("ai_setups");
  WaveLocation old_location;
  QJsonObject initial;
  if (row >= 0 && row < m_waves_table->rowCount()) {
    QTableWidgetItem* marker = m_waves_table->item(row, 0);
    old_location = {marker->data(Qt::UserRole).toInt(),
                    marker->data(Qt::UserRole + 1).toInt()};
    initial = ais[old_location.ai_index]
                  .toObject()
                  .value("waves")
                  .toArray()[old_location.wave_index]
                  .toObject();
  }
  const auto edited = edit_wave_dialog(ais, old_location, initial, this);
  if (!edited.has_value()) {
    return;
  }
  if (!initial.isEmpty()) {
    QJsonObject old_ai = ais[old_location.ai_index].toObject();
    QJsonArray old_waves = old_ai.value("waves").toArray();
    old_waves.removeAt(old_location.wave_index);
    old_ai["waves"] = old_waves;
    ais[old_location.ai_index] = old_ai;
  }
  QJsonObject new_ai = ais[edited->first.ai_index].toObject();
  QJsonArray new_waves = new_ai.value("waves").toArray();
  new_waves.append(edited->second);
  new_ai["waves"] = new_waves;
  ais[edited->first.ai_index] = new_ai;
  m_mission_data->set_array("ai_setups", ais);
}

void MissionPanel::remove_wave(int row) {
  if (row < 0 || row >= m_waves_table->rowCount()) {
    return;
  }
  QTableWidgetItem* marker = m_waves_table->item(row, 0);
  const int ai_index = marker->data(Qt::UserRole).toInt();
  const int wave_index = marker->data(Qt::UserRole + 1).toInt();
  QJsonArray ais = m_mission_data->array("ai_setups");
  QJsonObject ai = ais[ai_index].toObject();
  QJsonArray waves = ai.value("waves").toArray();
  waves.removeAt(wave_index);
  ai["waves"] = waves;
  ais[ai_index] = ai;
  m_mission_data->set_array("ai_setups", ais);
}

void MissionPanel::refresh_phases_table() {
  m_phases_table->setRowCount(0);
  for (const QJsonValue& value : m_mission_data->array("events")) {
    const QJsonObject event = value.toObject();
    const QJsonObject trigger = event.value("trigger").toObject();
    const QJsonArray actions = event.value("actions").toArray();
    const QString message = actions.isEmpty()
                                ? QString()
                                : actions.first().toObject().value("text").toString();
    set_row(m_phases_table,
            m_phases_table->rowCount(),
            {QString::number(trigger.value("time").toDouble(), 'f', 0) + "s", message});
  }
}

void MissionPanel::edit_phase(int row) {
  QJsonArray values = m_mission_data->array("events");
  const QJsonObject initial =
      row >= 0 && row < values.size() ? values[row].toObject() : QJsonObject{};
  const auto edited = edit_phase_dialog(initial, this);
  if (!edited.has_value()) {
    return;
  }
  if (row >= 0 && row < values.size()) {
    values[row] = *edited;
  } else {
    values.append(*edited);
  }
  m_mission_data->set_array("events", values);
}

void MissionPanel::remove_phase(int row) {
  QJsonArray values = m_mission_data->array("events");
  if (row >= 0 && row < values.size()) {
    values.removeAt(row);
    m_mission_data->set_array("events", values);
  }
}

void MissionPanel::refresh_fog_table() {
  m_fog_table->setRowCount(0);
  for (const FogZoneElement& zone : m_map_data->fog_zones()) {
    set_row(m_fog_table,
            m_fog_table->rowCount(),
            {QStringLiteral("%1, %2").arg(zone.x).arg(zone.z),
             QStringLiteral("%1 × %2").arg(zone.width).arg(zone.height),
             QString::number(zone.density, 'f', 2)});
  }
}

void MissionPanel::edit_fog_zone(int row) {
  FogZoneElement initial;
  if (row >= 0 && row < m_map_data->fog_zones().size()) {
    initial = m_map_data->fog_zones()[row];
  } else {
    initial.x = m_map_data->grid().width * 0.5F;
    initial.z = m_map_data->grid().height * 0.5F;
  }
  const auto edited = edit_fog_dialog(initial, this);
  if (!edited.has_value()) {
    return;
  }
  if (row >= 0) {
    m_map_data->update_fog_zone(row, *edited);
  } else {
    m_map_data->add_fog_zone(*edited);
  }
}

void MissionPanel::remove_fog_zone(int row) {
  m_map_data->remove_fog_zone(row);
}

} // namespace MapEditor
