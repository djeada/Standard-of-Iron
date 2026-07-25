#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

namespace MapEditor {

class MissionData : public QObject {
  Q_OBJECT

public:
  explicit MissionData(QObject* parent = nullptr);

  void clear();
  bool load_from_json(const QString& file_path, QString* out_error = nullptr);
  bool save_to_json(const QString& file_path, QString* out_error = nullptr) const;

  [[nodiscard]] const QJsonObject& root() const { return m_root; }
  [[nodiscard]] QJsonValue value(const QString& key) const;
  [[nodiscard]] QJsonArray array(const QString& key) const;
  [[nodiscard]] QString to_json_string() const;
  [[nodiscard]] QString id() const;
  [[nodiscard]] QString title() const;
  [[nodiscard]] QString map_path() const;

  void set_value(const QString& key, const QJsonValue& value);
  void set_array(const QString& key, const QJsonArray& value);
  void set_root(const QJsonObject& root);

  [[nodiscard]] QStringList validate() const;
  [[nodiscard]] bool is_modified() const { return m_modified; }
  void set_modified(bool modified);

  [[nodiscard]] static QStringList supported_nations();
  [[nodiscard]] static QStringList supported_strategies();
  [[nodiscard]] static QStringList supported_difficulties();
  [[nodiscard]] static QStringList supported_colors();
  [[nodiscard]] static QStringList supported_troops();
  [[nodiscard]] static QStringList supported_structures();
  [[nodiscard]] static QStringList supported_victory_conditions();
  [[nodiscard]] static QStringList supported_defeat_conditions();
  [[nodiscard]] static QStringList supported_optional_objectives();

signals:
  void data_changed();
  void modified_changed(bool modified);

private:
  QJsonObject m_root;
  bool m_modified = false;
};

} // namespace MapEditor
