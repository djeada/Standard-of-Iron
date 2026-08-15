#pragma once

#include <QJsonObject>
#include <QString>

#include <string>
#include <vector>

#include "formation_doctrine.h"
#include "troop_role_registry.h"
#include "unit_layout.h"

namespace Game::Formation {

struct FormationContentIssue {
  QString file;
  QString message;
  bool fatal{false};
};

struct FormationContentReport {
  int doctrines_loaded{0};
  int layouts_loaded{0};
  int troop_profiles_loaded{0};
  std::vector<FormationContentIssue> issues;

  [[nodiscard]] auto has_errors() const -> bool;
  [[nodiscard]] auto summary() const -> QString;
};

class FormationDataLoader {
public:
  static constexpr const char* k_default_root = ":/assets/data/formations";

  static auto load_all(const QString& root_path = QString()) -> FormationContentReport;

  static auto load_doctrine(const QJsonObject& root,
                            FormationContentReport& report,
                            const QString& source) -> bool;

  static auto load_layout(const QJsonObject& root,
                          FormationContentReport& report,
                          const QString& source) -> bool;

  static auto validate(FormationContentReport& report) -> bool;

  static void merge_troop_profiles_from_catalog();

  static void reset_to_builtin_defaults();
};

} // namespace Game::Formation
