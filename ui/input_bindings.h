#ifndef SOI_UI_INPUT_BINDINGS_H
#define SOI_UI_INPUT_BINDINGS_H

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class InputBindings : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList actions READ actions NOTIFY bindings_changed)
  Q_PROPERTY(bool hasConflicts READ has_conflicts NOTIFY bindings_changed)
  Q_PROPERTY(bool isDefault READ is_default NOTIFY bindings_changed)

public:
  static constexpr char kContextGlobal[] = "global";
  static constexpr char kContextRts[] = "rts";
  static constexpr char kContextCommander[] = "commander";

  enum Slot {
    Primary = 0,
    Alternate = 1
  };
  Q_ENUM(Slot)

  static constexpr int kSlotCount = 2;

  struct Chord {
    int key = 0;
    int mouse_button = 0;
    int modifiers = 0;

    [[nodiscard]] auto is_valid() const -> bool {
      return key != 0 || mouse_button != 0;
    }
    auto operator==(const Chord& other) const -> bool = default;
  };

  struct ActionSpec {
    QString id;
    QString context;
    QString category;
    QString name;
    QString description;
    QString default_shortcut;

    bool contextual = false;

    QString default_alternate;
  };

  static auto instance() -> InputBindings*;
  static auto create(QQmlEngine* engine, QJSEngine* scriptEngine) -> InputBindings*;

  [[nodiscard]] static auto catalog() -> const QVector<ActionSpec>&;

  [[nodiscard]] auto actions() const -> QVariantList;
  [[nodiscard]] auto has_conflicts() const -> bool;
  [[nodiscard]] auto is_default() const -> bool;

  Q_INVOKABLE [[nodiscard]] QString shortcut_for(const QString& action_id,
                                                 int slot = Primary) const;

  Q_INVOKABLE [[nodiscard]] QString display_shortcut_for(const QString& action_id,
                                                         int slot = Primary) const;

  Q_INVOKABLE [[nodiscard]] QString default_shortcut_for(const QString& action_id,
                                                         int slot = Primary) const;

  Q_INVOKABLE [[nodiscard]] QStringList conflicts_for(const QString& action_id,
                                                      const QString& shortcut,
                                                      int slot = Primary) const;

  Q_INVOKABLE bool
  assign(const QString& action_id, const QString& shortcut, int slot = Primary);

  Q_INVOKABLE void assign_overriding(const QString& action_id,
                                     const QString& shortcut,
                                     int slot = Primary);

  Q_INVOKABLE void clear_binding(const QString& action_id, int slot = Primary);
  Q_INVOKABLE void reset_action(const QString& action_id);
  Q_INVOKABLE void reset_to_defaults();

  void reload_from_settings();

  Q_INVOKABLE [[nodiscard]] static QString encode_key(int key, int modifiers);
  Q_INVOKABLE [[nodiscard]] static QString encode_mouse(int button, int modifiers);
  Q_INVOKABLE [[nodiscard]] static QString describe(const QString& shortcut);

  Q_INVOKABLE [[nodiscard]] static bool is_modifier_key(int key);

  Q_INVOKABLE [[nodiscard]] QStringList
  actions_for_key(int key, int modifiers, const QString& context) const;
  Q_INVOKABLE [[nodiscard]] QStringList
  actions_for_mouse(int button, int modifiers, const QString& context) const;

  Q_INVOKABLE [[nodiscard]] int canonical_key_for(const QString& action_id) const;

  [[nodiscard]] static auto parse(const QString& shortcut) -> Chord;
  [[nodiscard]] static auto format(const Chord& chord) -> QString;

signals:
  void bindings_changed();

private:
  explicit InputBindings(QObject* parent = nullptr);

  [[nodiscard]] auto spec_for(const QString& action_id) const -> const ActionSpec*;
  [[nodiscard]] auto chord_for(const QString& action_id, int slot) const -> Chord;
  [[nodiscard]] static auto contexts_overlap(const QString& lhs,
                                             const QString& rhs) -> bool;

  [[nodiscard]] static auto storage_key(const QString& action_id, int slot) -> QString;
  [[nodiscard]] static auto default_in(const ActionSpec& spec, int slot) -> QString;
  [[nodiscard]] static auto is_slot(int slot) -> bool;

  void store(const QString& action_id, int slot, const QString& shortcut);
  void load_stored_bindings();

  static InputBindings* m_instance;

  QHash<QString, QString> m_overrides;
};

#endif
