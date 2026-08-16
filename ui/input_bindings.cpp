#include "input_bindings.h"

#include <QJSEngine>
#include <QKeySequence>
#include <QQmlEngine>
#include <QVariantMap>
#include <Qt>

#include <array>

#include "app/core/user_settings.h"

namespace UserSettings = App::Core::UserSettings;

namespace {

constexpr char kUnboundToken[] = "None";

constexpr int kModifierMask =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

struct NamedModifier {
  Qt::KeyboardModifier modifier;
  const char* name;
};

constexpr std::array<NamedModifier, 4> k_modifier_names{{{Qt::ControlModifier, "Ctrl"},
                                                         {Qt::AltModifier, "Alt"},
                                                         {Qt::ShiftModifier, "Shift"},
                                                         {Qt::MetaModifier, "Meta"}}};

struct NamedButton {
  Qt::MouseButton button;
  const char* name;
};

constexpr std::array<NamedButton, 5> k_button_names{
    {{Qt::LeftButton, QT_TRANSLATE_NOOP("InputBindings", "Mouse Left")},
     {Qt::RightButton, QT_TRANSLATE_NOOP("InputBindings", "Mouse Right")},
     {Qt::MiddleButton, QT_TRANSLATE_NOOP("InputBindings", "Mouse Middle")},
     {Qt::BackButton, QT_TRANSLATE_NOOP("InputBindings", "Mouse Back")},
     {Qt::ForwardButton, QT_TRANSLATE_NOOP("InputBindings", "Mouse Forward")}}};

struct NamedBareKey {
  Qt::Key key;
  const char* name;
};

constexpr std::array<NamedBareKey, 5> k_bare_modifier_keys{{{Qt::Key_Shift, "Shift"},
                                                            {Qt::Key_Control, "Ctrl"},
                                                            {Qt::Key_Alt, "Alt"},
                                                            {Qt::Key_AltGr, "AltGr"},
                                                            {Qt::Key_Meta, "Meta"}}};

auto modifiers_to_string(int modifiers) -> QString {
  QString text;
  for (const auto& entry : k_modifier_names) {
    if ((modifiers & entry.modifier) != 0) {
      text += QString::fromLatin1(entry.name) + QLatin1Char('+');
    }
  }
  return text;
}

auto modifiers_from_string(const QString& text) -> int {
  int modifiers = 0;
  const auto parts = text.split(QLatin1Char('+'), Qt::SkipEmptyParts);
  for (const QString& part : parts) {
    for (const auto& entry : k_modifier_names) {
      if (part.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0) {
        modifiers |= entry.modifier;
      }
    }
  }
  return modifiers;
}

auto key_combination_to_int(const QKeySequence& sequence) -> int {
  if (sequence.count() != 1) {
    return 0;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return sequence[0].toCombined();
#else
  return sequence[0];
#endif
}

auto sequence_from_combined(int combined) -> QKeySequence {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QKeySequence(QKeyCombination::fromCombined(combined));
#else
  return QKeySequence(combined);
#endif
}

auto make_catalog() -> QVector<InputBindings::ActionSpec> {
  using Spec = InputBindings::ActionSpec;
  const QString global = QString::fromLatin1(InputBindings::kContextGlobal);
  const QString rts = QString::fromLatin1(InputBindings::kContextRts);
  const QString commander = QString::fromLatin1(InputBindings::kContextCommander);

  const QString system = QObject::tr("System");
  const QString camera = QObject::tr("Camera");
  const QString selection = QObject::tr("Selection");
  const QString orders = QObject::tr("Orders");
  const QString movement = QObject::tr("Commander movement");
  const QString combat = QObject::tr("Commander combat");

  QVector<Spec> catalog;
  catalog.reserve(45);

  catalog.append({QStringLiteral("global.menu"),
                  global,
                  system,
                  QObject::tr("Open menu"),
                  QObject::tr("Also cancels a placement or rally in progress"),
                  QStringLiteral("Esc"),
                  false});
  catalog.append({QStringLiteral("global.toggle_control_mode"),
                  global,
                  system,
                  QObject::tr("Switch between army and commander"),
                  {},
                  QStringLiteral("Return"),
                  false});
  catalog.append({QStringLiteral("global.quicksave"),
                  global,
                  system,
                  QObject::tr("Quick save"),
                  {},
                  QStringLiteral("F5"),
                  false});
  catalog.append({QStringLiteral("global.quickload"),
                  global,
                  system,
                  QObject::tr("Quick load"),
                  {},
                  QStringLiteral("F9"),
                  false});

  catalog.append({QStringLiteral("rts.pause"),
                  rts,
                  system,
                  QObject::tr("Pause"),
                  {},
                  QStringLiteral("Space"),
                  false});
  catalog.append({QStringLiteral("rts.speed_up"),
                  rts,
                  system,
                  QObject::tr("Speed up the battle"),
                  QObject::tr("Steps through the speeds shown on the top bar"),
                  QStringLiteral("+"),
                  false});
  catalog.append({QStringLiteral("rts.speed_down"),
                  rts,
                  system,
                  QObject::tr("Slow down the battle"),
                  QObject::tr("Steps through the speeds shown on the top bar"),
                  QStringLiteral("-"),
                  false});
  catalog.append({QStringLiteral("rts.camera_pan_up"),
                  rts,
                  camera,
                  QObject::tr("Pan camera up"),
                  QObject::tr("Hold Shift while panning to move faster"),
                  QStringLiteral("Up"),
                  false});
  catalog.append({QStringLiteral("rts.camera_pan_down"),
                  rts,
                  camera,
                  QObject::tr("Pan camera down"),
                  {},
                  QStringLiteral("Down"),
                  false});
  catalog.append({QStringLiteral("rts.camera_pan_left"),
                  rts,
                  camera,
                  QObject::tr("Pan camera left"),
                  {},
                  QStringLiteral("Left"),
                  false});
  catalog.append({QStringLiteral("rts.camera_pan_right"),
                  rts,
                  camera,
                  QObject::tr("Pan camera right"),
                  {},
                  QStringLiteral("Right"),
                  false});
  catalog.append({QStringLiteral("rts.camera_yaw_left"),
                  rts,
                  camera,
                  QObject::tr("Rotate camera left"),
                  {},
                  QStringLiteral("Q"),
                  false});
  catalog.append({QStringLiteral("rts.camera_yaw_right"),
                  rts,
                  camera,
                  QObject::tr("Rotate camera right"),
                  {},
                  QStringLiteral("E"),
                  false});
  catalog.append({QStringLiteral("rts.camera_orbit_left"),
                  rts,
                  camera,
                  QObject::tr("Orbit camera left"),
                  {},
                  QStringLiteral("R"),
                  false});
  catalog.append({QStringLiteral("rts.camera_orbit_right"),
                  rts,
                  camera,
                  QObject::tr("Orbit camera right"),
                  {},
                  QStringLiteral("T"),
                  false});
  catalog.append({QStringLiteral("rts.select"),
                  rts,
                  selection,
                  QObject::tr("Select unit or drag a selection box"),
                  {},
                  QStringLiteral("Mouse Left"),
                  false});
  catalog.append({QStringLiteral("rts.select_all_troops"),
                  rts,
                  selection,
                  QObject::tr("Select all troops"),
                  {},
                  QStringLiteral("X"),
                  false});
  catalog.append({QStringLiteral("rts.command"),
                  rts,
                  orders,
                  QObject::tr("Move or attack-move to the cursor"),
                  {},
                  QStringLiteral("Mouse Right"),
                  false});
  catalog.append({QStringLiteral("rts.order_stop"),
                  rts,
                  orders,
                  QObject::tr("Stop"),
                  {},
                  QStringLiteral("S"),
                  false});
  catalog.append({QStringLiteral("rts.order_attack"),
                  rts,
                  orders,
                  QObject::tr("Attack"),
                  {},
                  QStringLiteral("A"),
                  false});
  catalog.append({QStringLiteral("rts.order_move"),
                  rts,
                  orders,
                  QObject::tr("Move"),
                  {},
                  QStringLiteral("M"),
                  false});
  catalog.append({QStringLiteral("rts.order_patrol"),
                  rts,
                  orders,
                  QObject::tr("Patrol"),
                  {},
                  QStringLiteral("P"),
                  false});
  catalog.append({QStringLiteral("rts.order_guard"),
                  rts,
                  orders,
                  QObject::tr("Guard"),
                  {},
                  QStringLiteral("G"),
                  false});
  catalog.append({QStringLiteral("rts.order_hold"),
                  rts,
                  orders,
                  QObject::tr("Hold position"),
                  {},
                  QStringLiteral("H"),
                  false});
  catalog.append({QStringLiteral("rts.order_formation"),
                  rts,
                  orders,
                  QObject::tr("Deploy the selection in a formation"),
                  QObject::tr("Opens the formation planner for the selected troops"),
                  QStringLiteral("F"),
                  false});
  catalog.append({QStringLiteral("rts.commander_rally"),
                  rts,
                  orders,
                  QObject::tr("Place commander rally flag"),
                  QObject::tr("Shares its key with orbit camera left by default and "
                              "takes priority only while a rally can be placed"),
                  QStringLiteral("R"),
                  true});

  catalog.append({QStringLiteral("commander.move_forward"),
                  commander,
                  movement,
                  QObject::tr("Move forward"),
                  {},
                  QStringLiteral("W"),
                  false});
  catalog.append({QStringLiteral("commander.move_back"),
                  commander,
                  movement,
                  QObject::tr("Move back"),
                  {},
                  QStringLiteral("S"),
                  false});
  catalog.append({QStringLiteral("commander.strafe_left"),
                  commander,
                  movement,
                  QObject::tr("Strafe left"),
                  {},
                  QStringLiteral("A"),
                  false});
  catalog.append({QStringLiteral("commander.strafe_right"),
                  commander,
                  movement,
                  QObject::tr("Strafe right"),
                  {},
                  QStringLiteral("D"),
                  false});
  catalog.append({QStringLiteral("commander.turn_left"),
                  commander,
                  movement,
                  QObject::tr("Turn left"),
                  {},
                  QStringLiteral("Q"),
                  false});
  catalog.append({QStringLiteral("commander.turn_right"),
                  commander,
                  movement,
                  QObject::tr("Turn right"),
                  {},
                  QStringLiteral("E"),
                  false});
  catalog.append({QStringLiteral("commander.sprint"),
                  commander,
                  movement,
                  QObject::tr("Sprint"),
                  {},
                  QStringLiteral("Shift"),
                  false});
  catalog.append({QStringLiteral("commander.dodge"),
                  commander,
                  movement,
                  QObject::tr("Dodge"),
                  {},
                  QStringLiteral("Space"),
                  false});
  catalog.append({QStringLiteral("commander.jump"),
                  commander,
                  movement,
                  QObject::tr("Jump"),
                  {},
                  QStringLiteral("Alt"),
                  false});
  catalog.append({QStringLiteral("commander.primary_action"),
                  commander,
                  combat,
                  QObject::tr("Attack"),
                  {},
                  QStringLiteral("Mouse Left"),
                  false});
  catalog.append({QStringLiteral("commander.secondary_action"),
                  commander,
                  combat,
                  QObject::tr("Block"),
                  {},
                  QStringLiteral("Mouse Right"),
                  false});
  catalog.append({QStringLiteral("commander.cycle_lock_on"),
                  commander,
                  combat,
                  QObject::tr("Cycle locked target"),
                  {},
                  QStringLiteral("Tab"),
                  false});
  catalog.append({QStringLiteral("commander.special_action"),
                  commander,
                  combat,
                  QObject::tr("Special action"),
                  {},
                  QStringLiteral("F"),
                  false});
  catalog.append({QStringLiteral("commander.ability_vanguard_rush"),
                  commander,
                  combat,
                  QObject::tr("Vanguard rush"),
                  {},
                  QStringLiteral("1"),
                  false});
  catalog.append({QStringLiteral("commander.ability_second_wind"),
                  commander,
                  combat,
                  QObject::tr("Second wind"),
                  {},
                  QStringLiteral("2"),
                  false});
  catalog.append({QStringLiteral("commander.ability_aura"),
                  commander,
                  combat,
                  QObject::tr("Commanding aura"),
                  {},
                  QStringLiteral("3"),
                  false});
  catalog.append({QStringLiteral("commander.rally"),
                  commander,
                  combat,
                  QObject::tr("Rally nearby troops"),
                  {},
                  QStringLiteral("R"),
                  false});
  catalog.append({QStringLiteral("commander.toggle_weapon"),
                  commander,
                  combat,
                  QObject::tr("Switch between melee weapon and bow"),
                  {},
                  QStringLiteral("X"),
                  false});
  catalog.append({QStringLiteral("commander.toggle_camera_mode"),
                  commander,
                  camera,
                  QObject::tr("Toggle first and third person"),
                  {},
                  QStringLiteral("C"),
                  false});

  return catalog;
}

} // namespace

InputBindings* InputBindings::m_instance = nullptr;

InputBindings::InputBindings(QObject* parent)
    : QObject(parent) {
  for (const ActionSpec& spec : catalog()) {
    const QString stored = UserSettings::load_input_binding(spec.id);
    if (stored.isEmpty()) {
      continue;
    }
    if (stored == QLatin1String(kUnboundToken)) {
      m_overrides.insert(spec.id, QString());
      continue;
    }
    const Chord chord = parse(stored);
    if (!chord.is_valid()) {
      qWarning() << "Ignoring unreadable saved binding for" << spec.id << stored;
      continue;
    }
    m_overrides.insert(spec.id, format(chord));
  }
}

auto InputBindings::instance() -> InputBindings* {
  if (m_instance == nullptr) {
    m_instance = new InputBindings();
  }
  return m_instance;
}

auto InputBindings::create(QQmlEngine* engine,
                           QJSEngine* scriptEngine) -> InputBindings* {
  Q_UNUSED(engine)
  Q_UNUSED(scriptEngine)
  auto* bindings = instance();
  QQmlEngine::setObjectOwnership(bindings, QQmlEngine::CppOwnership);
  return bindings;
}

auto InputBindings::catalog() -> const QVector<ActionSpec>& {
  static const QVector<ActionSpec> k_catalog = make_catalog();
  return k_catalog;
}

auto InputBindings::spec_for(const QString& action_id) const -> const ActionSpec* {
  for (const ActionSpec& spec : catalog()) {
    if (spec.id == action_id) {
      return &spec;
    }
  }
  return nullptr;
}

auto InputBindings::shortcut_for(const QString& action_id) const -> QString {
  const auto override_it = m_overrides.constFind(action_id);
  if (override_it != m_overrides.constEnd()) {
    return override_it.value();
  }
  const ActionSpec* spec = spec_for(action_id);
  return spec != nullptr ? spec->default_shortcut : QString();
}

auto InputBindings::display_shortcut_for(const QString& action_id) const -> QString {
  return describe(shortcut_for(action_id));
}

auto InputBindings::default_shortcut_for(const QString& action_id) const -> QString {
  const ActionSpec* spec = spec_for(action_id);
  return spec != nullptr ? spec->default_shortcut : QString();
}

auto InputBindings::chord_for(const QString& action_id) const -> Chord {
  return parse(shortcut_for(action_id));
}

auto InputBindings::contexts_overlap(const QString& lhs, const QString& rhs) -> bool {
  return lhs == rhs || lhs == QLatin1String(kContextGlobal) ||
         rhs == QLatin1String(kContextGlobal);
}

auto InputBindings::conflicts_for(const QString& action_id,
                                  const QString& shortcut) const -> QStringList {
  QStringList conflicts;
  const ActionSpec* subject = spec_for(action_id);
  const Chord chord = parse(shortcut);
  if (subject == nullptr || !chord.is_valid()) {
    return conflicts;
  }

  for (const ActionSpec& other : catalog()) {
    if (other.id == action_id) {
      continue;
    }
    if (!contexts_overlap(subject->context, other.context)) {
      continue;
    }

    if (subject->contextual || other.contextual) {
      continue;
    }
    if (chord_for(other.id) == chord) {
      conflicts.append(other.id);
    }
  }
  return conflicts;
}

auto InputBindings::assign(const QString& action_id, const QString& shortcut) -> bool {
  if (!conflicts_for(action_id, shortcut).isEmpty()) {
    return false;
  }
  const Chord chord = parse(shortcut);
  if (spec_for(action_id) == nullptr || !chord.is_valid()) {
    return false;
  }
  store(action_id, format(chord));
  emit bindings_changed();
  return true;
}

void InputBindings::assign_overriding(const QString& action_id,
                                      const QString& shortcut) {
  const Chord chord = parse(shortcut);
  if (spec_for(action_id) == nullptr || !chord.is_valid()) {
    return;
  }

  for (const QString& conflicting : conflicts_for(action_id, shortcut)) {
    store(conflicting, QString());
  }
  store(action_id, format(chord));
  emit bindings_changed();
}

void InputBindings::clear_binding(const QString& action_id) {
  if (spec_for(action_id) == nullptr) {
    return;
  }
  store(action_id, QString());
  emit bindings_changed();
}

void InputBindings::reset_action(const QString& action_id) {
  if (spec_for(action_id) == nullptr || m_overrides.remove(action_id) == 0) {
    return;
  }
  UserSettings::save_input_binding(action_id, QString());
  emit bindings_changed();
}

void InputBindings::reset_to_defaults() {
  if (m_overrides.isEmpty()) {
    return;
  }
  m_overrides.clear();
  UserSettings::clear_input_bindings();
  emit bindings_changed();
}

void InputBindings::store(const QString& action_id, const QString& shortcut) {
  const ActionSpec* spec = spec_for(action_id);
  if (spec == nullptr) {
    return;
  }

  if (shortcut == spec->default_shortcut) {
    m_overrides.remove(action_id);
    UserSettings::save_input_binding(action_id, QString());
    return;
  }

  m_overrides.insert(action_id, shortcut);
  UserSettings::save_input_binding(
      action_id, shortcut.isEmpty() ? QString::fromLatin1(kUnboundToken) : shortcut);
}

auto InputBindings::actions() const -> QVariantList {
  QVariantList list;
  list.reserve(catalog().size());
  for (const ActionSpec& spec : catalog()) {
    const QString shortcut = shortcut_for(spec.id);
    QVariantMap entry;
    entry[QStringLiteral("id")] = spec.id;
    entry[QStringLiteral("context")] = spec.context;
    entry[QStringLiteral("category")] = spec.category;
    entry[QStringLiteral("name")] = spec.name;
    entry[QStringLiteral("description")] = spec.description;
    entry[QStringLiteral("shortcut")] = shortcut;
    entry[QStringLiteral("displayShortcut")] = describe(shortcut);
    entry[QStringLiteral("defaultShortcut")] = spec.default_shortcut;
    entry[QStringLiteral("isDefault")] = shortcut == spec.default_shortcut;
    entry[QStringLiteral("unbound")] = shortcut.isEmpty();
    entry[QStringLiteral("conflicts")] = conflicts_for(spec.id, shortcut);
    list.append(entry);
  }
  return list;
}

auto InputBindings::has_conflicts() const -> bool {
  for (const ActionSpec& spec : catalog()) {
    if (!conflicts_for(spec.id, shortcut_for(spec.id)).isEmpty()) {
      return true;
    }
  }
  return false;
}

auto InputBindings::is_default() const -> bool {
  return m_overrides.isEmpty();
}

auto InputBindings::actions_for_key(int key,
                                    int modifiers,
                                    const QString& context) const -> QStringList {
  QStringList exact;
  QStringList unmodified;
  const int masked = modifiers & kModifierMask;

  for (const ActionSpec& spec : catalog()) {
    if (!contexts_overlap(spec.context, context)) {
      continue;
    }
    const Chord chord = chord_for(spec.id);
    if (chord.key == 0 || chord.key != key) {
      continue;
    }
    QStringList& bucket = chord.modifiers == masked ? exact : unmodified;
    if (chord.modifiers != masked && chord.modifiers != 0) {
      continue;
    }
    if (spec.contextual) {
      bucket.prepend(spec.id);
    } else {
      bucket.append(spec.id);
    }
  }

  if (!exact.isEmpty()) {
    return exact;
  }
  return unmodified;
}

auto InputBindings::actions_for_mouse(int button,
                                      int modifiers,
                                      const QString& context) const -> QStringList {
  QStringList exact;
  QStringList unmodified;
  const int masked = modifiers & kModifierMask;

  for (const ActionSpec& spec : catalog()) {
    if (!contexts_overlap(spec.context, context)) {
      continue;
    }
    const Chord chord = chord_for(spec.id);
    if (chord.mouse_button == 0 || chord.mouse_button != button) {
      continue;
    }
    QStringList& bucket = chord.modifiers == masked ? exact : unmodified;
    if (chord.modifiers != masked && chord.modifiers != 0) {
      continue;
    }
    if (spec.contextual) {
      bucket.prepend(spec.id);
    } else {
      bucket.append(spec.id);
    }
  }

  if (!exact.isEmpty()) {
    return exact;
  }
  return unmodified;
}

auto InputBindings::canonical_key_for(const QString& action_id) const -> int {
  const ActionSpec* spec = spec_for(action_id);
  if (spec == nullptr) {
    return 0;
  }
  return parse(spec->default_shortcut).key;
}

auto InputBindings::is_modifier_key(int key) -> bool {
  for (const auto& entry : k_bare_modifier_keys) {
    if (key == entry.key) {
      return true;
    }
  }
  return false;
}

auto InputBindings::encode_key(int key, int modifiers) -> QString {
  if (key == 0) {
    return {};
  }
  Chord chord;
  chord.key = key;

  chord.modifiers = is_modifier_key(key) ? 0 : (modifiers & kModifierMask);
  return format(chord);
}

auto InputBindings::encode_mouse(int button, int modifiers) -> QString {
  if (button == 0) {
    return {};
  }
  Chord chord;
  chord.mouse_button = button;
  chord.modifiers = modifiers & kModifierMask;
  return format(chord);
}

auto InputBindings::format(const Chord& chord) -> QString {
  if (!chord.is_valid()) {
    return {};
  }

  if (chord.mouse_button != 0) {
    for (const auto& entry : k_button_names) {
      if (chord.mouse_button == static_cast<int>(entry.button)) {
        return modifiers_to_string(chord.modifiers) + QString::fromLatin1(entry.name);
      }
    }
    return {};
  }

  for (const auto& entry : k_bare_modifier_keys) {
    if (chord.key == entry.key) {
      return QString::fromLatin1(entry.name);
    }
  }

  const QString text = sequence_from_combined(chord.key | chord.modifiers)
                           .toString(QKeySequence::PortableText);
  return text;
}

auto InputBindings::parse(const QString& shortcut) -> Chord {
  Chord chord;
  const QString text = shortcut.trimmed();
  if (text.isEmpty() ||
      text.compare(QLatin1String(kUnboundToken), Qt::CaseInsensitive) == 0) {
    return chord;
  }

  const int mouse_index = text.indexOf(QLatin1String("Mouse"), 0, Qt::CaseInsensitive);
  if (mouse_index >= 0) {
    const QString button_text = text.mid(mouse_index).trimmed();
    for (const auto& entry : k_button_names) {
      if (button_text.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0) {
        chord.mouse_button = entry.button;
        chord.modifiers = modifiers_from_string(text.left(mouse_index));
        return chord;
      }
    }
    return {};
  }

  for (const auto& entry : k_bare_modifier_keys) {
    if (text.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0) {
      chord.key = entry.key;
      return chord;
    }
  }

  const int combined = key_combination_to_int(
      QKeySequence::fromString(text, QKeySequence::PortableText));
  if (combined == 0) {
    return {};
  }
  chord.key = combined & ~kModifierMask;
  chord.modifiers = combined & kModifierMask;
  if (chord.key == 0 || chord.key == Qt::Key_unknown) {
    return {};
  }
  return chord;
}

auto InputBindings::describe(const QString& shortcut) -> QString {
  const Chord chord = parse(shortcut);
  if (!chord.is_valid()) {
    return tr("Unbound");
  }
  if (chord.mouse_button != 0) {
    for (const auto& entry : k_button_names) {
      if (chord.mouse_button == static_cast<int>(entry.button)) {
        return modifiers_to_string(chord.modifiers) + tr(entry.name);
      }
    }
    return shortcut;
  }
  for (const auto& entry : k_bare_modifier_keys) {
    if (chord.key == entry.key) {
      return shortcut;
    }
  }
  return sequence_from_combined(chord.key | chord.modifiers)
      .toString(QKeySequence::NativeText);
}
