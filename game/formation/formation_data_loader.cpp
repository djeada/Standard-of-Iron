#include "formation_data_loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>

#include <algorithm>

#include "../units/troop_catalog.h"

namespace Game::Formation {

namespace {

Q_LOGGING_CATEGORY(formation_data_logger, "soi.formation.data")

auto resolve_data_path(const QString& relative) -> QString {
  const QString direct = QDir::current().filePath(relative);
  if (QFile::exists(direct)) {
    return direct;
  }

  const QString app_dir = QCoreApplication::applicationDirPath();
  if (!app_dir.isEmpty()) {
    const QString from_app = QDir(app_dir).filePath(relative);
    if (QFile::exists(from_app)) {
      return from_app;
    }
    const QString parent = QDir(app_dir).filePath("../" + relative);
    if (QFile::exists(parent)) {
      return QDir(parent).canonicalPath();
    }
  }

  const QString resource_path = QStringLiteral(":/") + relative;
  if (QFile::exists(resource_path)) {
    return resource_path;
  }

  return {};
}

auto read_float(const QJsonObject& obj, const char* key, float fallback) -> float {
  auto const value = obj.value(QLatin1String(key));
  return value.isDouble() ? static_cast<float>(value.toDouble()) : fallback;
}

auto read_int(const QJsonObject& obj, const char* key, int fallback) -> int {
  auto const value = obj.value(QLatin1String(key));
  return value.isDouble() ? value.toInt() : fallback;
}

auto read_bool(const QJsonObject& obj, const char* key, bool fallback) -> bool {
  auto const value = obj.value(QLatin1String(key));
  return value.isBool() ? value.toBool() : fallback;
}

auto read_role_mask(const QJsonObject& obj,
                    const char* key,
                    FormationContentReport& report,
                    const QString& source) -> RoleTagSet {
  RoleTagSet mask = 0U;
  auto const array = obj.value(QLatin1String(key)).toArray();
  for (const auto& entry : array) {
    auto const text = entry.toString();
    if (auto parsed = try_parse_role_tag(text)) {
      mask |= to_mask(*parsed);
    } else {
      report.issues.push_back({source,
                               QStringLiteral("Unknown role tag '%1' in '%2'")
                                   .arg(text, QLatin1String(key)),
                               true});
    }
  }
  return mask;
}

auto parse_placement(const QString& value) -> std::optional<LinePlacement> {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("centre_block") ||
      lowered == QStringLiteral("center_block")) {
    return LinePlacement::CentreBlock;
  }
  if (lowered == QStringLiteral("split_flanks")) {
    return LinePlacement::SplitFlanks;
  }
  if (lowered == QStringLiteral("screen")) {
    return LinePlacement::Screen;
  }
  if (lowered == QStringLiteral("trailing")) {
    return LinePlacement::Trailing;
  }
  return std::nullopt;
}

auto parse_line_rule(const QJsonObject& obj,
                     FormationContentReport& report,
                     const QString& source) -> DoctrineLineRule {
  DoctrineLineRule rule;

  auto const role_text = obj.value(QStringLiteral("role")).toString();
  if (!role_text.isEmpty()) {
    if (auto parsed = try_parse_army_role(role_text)) {
      rule.role = *parsed;
    } else {
      report.issues.push_back(
          {source, QStringLiteral("Unknown army role '%1'").arg(role_text), true});
    }
  }

  auto const placement_text = obj.value(QStringLiteral("placement")).toString();
  if (!placement_text.isEmpty()) {
    if (auto parsed = parse_placement(placement_text)) {
      rule.placement = *parsed;
    } else {
      report.issues.push_back(
          {source,
           QStringLiteral("Unknown line placement '%1'").arg(placement_text),
           true});
    }
  }

  rule.match_any = read_role_mask(obj, "match_any", report, source);
  rule.match_all = read_role_mask(obj, "match_all", report, source);
  rule.exclude = read_role_mask(obj, "exclude", report, source);

  rule.max_per_row = read_int(obj, "max_per_row", rule.max_per_row);
  rule.min_per_row = read_int(obj, "min_per_row", rule.min_per_row);
  rule.lateral_spacing_scale =
      read_float(obj, "lateral_spacing_scale", rule.lateral_spacing_scale);
  rule.depth_spacing_scale =
      read_float(obj, "depth_spacing_scale", rule.depth_spacing_scale);
  rule.line_gap_scale = read_float(obj, "line_gap_scale", rule.line_gap_scale);
  rule.row_echelon_scale = read_float(obj, "row_echelon_scale", rule.row_echelon_scale);
  rule.row_stagger_scale = read_float(obj, "row_stagger_scale", rule.row_stagger_scale);
  rule.lateral_jitter_scale =
      read_float(obj, "lateral_jitter_scale", rule.lateral_jitter_scale);
  rule.depth_jitter_scale =
      read_float(obj, "depth_jitter_scale", rule.depth_jitter_scale);
  rule.front_offset_scale =
      read_float(obj, "front_offset_scale", rule.front_offset_scale);
  rule.flank_gap_scale = read_float(obj, "flank_gap_scale", rule.flank_gap_scale);
  rule.right_side_weight = read_float(obj, "right_side_weight", rule.right_side_weight);
  rule.flank_forward_step_scale =
      read_float(obj, "flank_forward_step_scale", rule.flank_forward_step_scale);
  rule.consumes_depth = read_bool(obj, "consumes_depth", rule.consumes_depth);
  rule.optional = read_bool(obj, "optional", rule.optional);

  if (rule.max_per_row < 1) {
    report.issues.push_back(
        {source, QStringLiteral("max_per_row must be at least 1"), true});
    rule.max_per_row = 1;
  }
  if (rule.min_per_row < 1) {
    rule.min_per_row = 1;
  }
  return rule;
}

auto parse_intent_template(ArmyFormationIntent intent,
                           const QJsonObject& obj,
                           FormationContentReport& report,
                           const QString& source) -> DoctrineIntentTemplate {
  DoctrineIntentTemplate tmpl;
  tmpl.intent = intent;
  tmpl.frontage_scale = read_float(obj, "frontage_scale", tmpl.frontage_scale);
  tmpl.depth_scale = read_float(obj, "depth_scale", tmpl.depth_scale);
  tmpl.spacing_scale = read_float(obj, "spacing_scale", tmpl.spacing_scale);
  tmpl.reserve_rows = read_int(obj, "reserve_rows", tmpl.reserve_rows);

  if (auto parsed = try_parse_flank_preference(
          obj.value(QStringLiteral("default_flank")).toString())) {
    tmpl.default_flank = *parsed;
  }
  if (auto parsed = try_parse_ranged_placement(
          obj.value(QStringLiteral("default_ranged")).toString())) {
    tmpl.default_ranged = *parsed;
  }
  if (auto parsed = try_parse_movement_policy(
          obj.value(QStringLiteral("default_movement")).toString())) {
    tmpl.default_movement = *parsed;
  }

  tmpl.required_roles = read_role_mask(obj, "requires_roles", report, source);
  tmpl.requirement_hint =
      obj.value(QStringLiteral("requirement_hint")).toString().toStdString();

  auto const lines = obj.value(QStringLiteral("lines")).toArray();
  tmpl.lines.reserve(static_cast<std::size_t>(lines.size()));
  for (const auto& entry : lines) {
    tmpl.lines.push_back(parse_line_rule(entry.toObject(), report, source));
  }

  if (tmpl.lines.empty()) {
    report.issues.push_back({source,
                             QStringLiteral("Intent '%1' declares no lines")
                                 .arg(QLatin1String(intent_to_string(intent))),
                             true});
  }
  return tmpl;
}

} // namespace

auto FormationContentReport::has_errors() const -> bool {
  return std::any_of(
      issues.begin(), issues.end(), [](const auto& issue) { return issue.fatal; });
}

auto FormationContentReport::summary() const -> QString {
  return QStringLiteral("%1 doctrines, %2 unit layouts, %3 troop profiles, %4 issues")
      .arg(doctrines_loaded)
      .arg(layouts_loaded)
      .arg(troop_profiles_loaded)
      .arg(issues.size());
}

void FormationDataLoader::reset_to_builtin_defaults() {
  DoctrineRegistry::instance().reset_to_defaults();
  UnitLayoutLibrary::instance().reset_to_defaults();
  TroopRoleRegistry::instance().reset_to_defaults();
}

auto FormationDataLoader::load_doctrine(const QJsonObject& root,
                                        FormationContentReport& report,
                                        const QString& source) -> bool {
  auto const id = root.value(QStringLiteral("id")).toString().trimmed().toLower();
  if (id.isEmpty()) {
    report.issues.push_back({source, QStringLiteral("Doctrine is missing 'id'"), true});
    return false;
  }

  FormationDoctrine doctrine =
      DoctrineRegistry::instance().get_or_neutral(id.toStdString());
  doctrine.id = id.toStdString();

  auto const display = root.value(QStringLiteral("display_name")).toString();
  if (!display.isEmpty()) {
    doctrine.display_name = display.toStdString();
  }

  auto const default_intent = root.value(QStringLiteral("default_intent")).toString();
  if (!default_intent.isEmpty()) {
    if (auto parsed = try_parse_intent(default_intent)) {
      doctrine.default_intent = *parsed;
    } else {
      report.issues.push_back(
          {source,
           QStringLiteral("Unknown default_intent '%1'").arg(default_intent),
           true});
    }
  }

  auto const intents = root.value(QStringLiteral("intents")).toObject();
  for (auto it = intents.begin(); it != intents.end(); ++it) {
    auto parsed_intent = try_parse_intent(it.key());
    if (!parsed_intent) {
      report.issues.push_back(
          {source, QStringLiteral("Unknown intent '%1'").arg(it.key()), true});
      continue;
    }
    doctrine.intents[static_cast<int>(*parsed_intent)] =
        parse_intent_template(*parsed_intent, it.value().toObject(), report, source);
  }

  DoctrineRegistry::instance().register_doctrine(std::move(doctrine));
  ++report.doctrines_loaded;
  return true;
}

auto FormationDataLoader::load_layout(const QJsonObject& root,
                                      FormationContentReport& report,
                                      const QString& source) -> bool {
  auto const id = root.value(QStringLiteral("id")).toString().trimmed();
  if (id.isEmpty()) {
    report.issues.push_back({source, QStringLiteral("Layout is missing 'id'"), true});
    return false;
  }

  auto& library = UnitLayoutLibrary::instance();
  UnitLayoutStyle style = library.style(library.find(id.toStdString()));
  style.id = id.toStdString();

  auto const shape = root.value(QStringLiteral("shape")).toString();
  if (!shape.isEmpty()) {
    if (auto parsed = try_parse_layout_shape(shape)) {
      style.shape = *parsed;
    } else {
      report.issues.push_back(
          {source, QStringLiteral("Unknown layout shape '%1'").arg(shape), true});
    }
  }

  style.lateral_spacing_scale =
      read_float(root, "lateral_spacing_scale", style.lateral_spacing_scale);
  style.depth_spacing_scale =
      read_float(root, "depth_spacing_scale", style.depth_spacing_scale);
  style.rank_stagger = read_float(root, "rank_stagger", style.rank_stagger);
  style.rank_echelon = read_float(root, "rank_echelon", style.rank_echelon);
  style.rank_arc = read_float(root, "rank_arc", style.rank_arc);
  style.front_rank_tightening =
      read_float(root, "front_rank_tightening", style.front_rank_tightening);
  style.rear_rank_loosening =
      read_float(root, "rear_rank_loosening", style.rear_rank_loosening);
  style.rear_depth_bias = read_float(root, "rear_depth_bias", style.rear_depth_bias);
  style.lateral_jitter = read_float(root, "lateral_jitter", style.lateral_jitter);
  style.depth_jitter = read_float(root, "depth_jitter", style.depth_jitter);
  style.rear_jitter_gain = read_float(root, "rear_jitter_gain", style.rear_jitter_gain);
  style.facing_jitter_degrees =
      read_float(root, "facing_jitter_degrees", style.facing_jitter_degrees);
  style.wedge_slope = read_float(root, "wedge_slope", style.wedge_slope);
  style.wedge_growth = read_float(root, "wedge_growth", style.wedge_growth);
  style.file_grouping = read_float(root, "file_grouping", style.file_grouping);
  style.group_gap = read_float(root, "group_gap", style.group_gap);
  style.group_depth_stagger =
      read_float(root, "group_depth_stagger", style.group_depth_stagger);
  style.cluster_pull = read_float(root, "cluster_pull", style.cluster_pull);
  style.cluster_size = read_float(root, "cluster_size", style.cluster_size);
  style.radius_scale = read_float(root, "radius_scale", style.radius_scale);
  style.weapon_clearance = read_float(root, "weapon_clearance", style.weapon_clearance);
  style.min_separation_scale =
      read_float(root, "min_separation_scale", style.min_separation_scale);
  style.column_files = read_float(root, "column_files", style.column_files);

  if (style.lateral_spacing_scale <= 0.0F || style.depth_spacing_scale <= 0.0F) {
    report.issues.push_back(
        {source,
         QStringLiteral("Layout '%1' has a non-positive spacing scale").arg(id),
         true});
    return false;
  }
  if (style.file_grouping < 0.0F || style.group_gap < 0.0F ||
      style.group_depth_stagger < 0.0F) {
    report.issues.push_back(
        {source,
         QStringLiteral("Layout '%1' has a negative file grouping value").arg(id),
         true});
    return false;
  }
  if (style.wedge_growth <= 0.0F) {
    report.issues.push_back(
        {source,
         QStringLiteral("Layout '%1' wedge_growth must be positive").arg(id),
         true});
    return false;
  }
  if (style.min_separation_scale < 0.0F || style.min_separation_scale >= 1.0F) {
    report.issues.push_back(
        {source,
         QStringLiteral("Layout '%1' min_separation_scale must be in [0, 1)").arg(id),
         true});
    style.min_separation_scale = std::clamp(style.min_separation_scale, 0.0F, 0.95F);
  }

  library.register_style(std::move(style));
  ++report.layouts_loaded;
  return true;
}

auto FormationDataLoader::validate(FormationContentReport& report) -> bool {
  const auto& library = UnitLayoutLibrary::instance();
  auto& roles = TroopRoleRegistry::instance();

  constexpr int k_last_troop = static_cast<int>(Game::Units::TroopType::Builder);
  for (int i = 0; i <= k_last_troop; ++i) {
    auto const troop = static_cast<Game::Units::TroopType>(i);
    const auto& profile = roles.profile(troop);
    auto const troop_name = Game::Units::troop_typeToQString(troop);

    auto check = [&](const std::string& layout_name, const char* field) {
      if (layout_name.empty()) {
        return;
      }
      if (!library.contains(layout_name)) {
        report.issues.push_back(
            {QStringLiteral("troop:%1").arg(troop_name),
             QStringLiteral("%1 references unknown unit layout '%2'")
                 .arg(QLatin1String(field), QString::fromStdString(layout_name)),
             true});
      }
    };
    check(profile.unit_layout, "unit_layout");
    check(profile.defensive_layout, "defensive_layout");
    check(profile.marching_layout, "marching_layout");

    if (profile.roles == 0U) {
      report.issues.push_back({QStringLiteral("troop:%1").arg(troop_name),
                               QStringLiteral("Troop declares no tactical roles"),
                               true});
    }
  }

  report.troop_profiles_loaded = 0;
  for (int i = 0; i <= k_last_troop; ++i) {
    if (!roles.profile(static_cast<Game::Units::TroopType>(i)).unit_layout.empty()) {
      ++report.troop_profiles_loaded;
    }
  }

  for (const auto& doctrine_id : DoctrineRegistry::instance().ids()) {
    const auto& doctrine = DoctrineRegistry::instance().get_or_neutral(doctrine_id);
    auto const source =
        QStringLiteral("doctrine:%1").arg(QString::fromStdString(doctrine_id));
    if (doctrine.resolve_template(ArmyFormationIntent::FactionDefault) == nullptr) {
      report.issues.push_back(
          {source, QStringLiteral("Doctrine has no faction_default template"), true});
    }
    for (const auto& entry : doctrine.intents) {
      bool has_catch_all = false;
      for (const auto& rule : entry.second.lines) {
        if (rule.match_any == 0U && rule.match_all == 0U) {
          has_catch_all = true;
        }
      }
      if (!has_catch_all) {
        report.issues.push_back(
            {source,
             QStringLiteral("Intent '%1' has no catch-all line; some troops could "
                            "go unplaced")
                 .arg(QLatin1String(
                     intent_to_string(static_cast<ArmyFormationIntent>(entry.first)))),
             false});
      }
    }
  }

  return !report.has_errors();
}

void FormationDataLoader::merge_troop_profiles_from_catalog() {
  auto& roles = TroopRoleRegistry::instance();
  for (const auto& [troop, troop_class] :
       Game::Units::TroopCatalog::instance().get_all_classes()) {
    if (troop_class.formation_profile.has_value()) {
      roles.merge_profile(troop, *troop_class.formation_profile);
    }
  }
}

auto FormationDataLoader::load_all(const QString& root_path) -> FormationContentReport {
  FormationContentReport report;

  merge_troop_profiles_from_catalog();

  QString root = root_path;
  if (root.isEmpty()) {
    root = resolve_data_path(QStringLiteral("assets/data/formations"));
  }
  if (root.isEmpty()) {
    qCInfo(formation_data_logger())
        << "No formation data directory found; using built-in defaults";
    validate(report);
    return report;
  }

  auto load_directory = [&](const QString& sub_dir, bool is_doctrine) {
    QDir const dir(QDir(root).filePath(sub_dir));
    if (!dir.exists()) {
      return;
    }
    QDirIterator it(dir.absolutePath(),
                    QStringList{QStringLiteral("*.json")},
                    QDir::Files | QDir::Readable);
    while (it.hasNext()) {
      const QString path = it.next();
      QFile file(path);
      if (!file.open(QIODevice::ReadOnly)) {
        report.issues.push_back({path, QStringLiteral("Could not open file"), true});
        continue;
      }
      QJsonParseError error{};
      auto const document = QJsonDocument::fromJson(file.readAll(), &error);
      if (error.error != QJsonParseError::NoError) {
        report.issues.push_back(
            {path,
             QStringLiteral("JSON parse error: %1").arg(error.errorString()),
             true});
        continue;
      }
      if (!document.isObject()) {
        report.issues.push_back(
            {path, QStringLiteral("Root value must be an object"), true});
        continue;
      }
      if (is_doctrine) {
        load_doctrine(document.object(), report, path);
      } else {
        load_layout(document.object(), report, path);
      }
    }
  };

  load_directory(QStringLiteral("unit_layouts"), false);
  load_directory(QStringLiteral("army"), true);

  validate(report);

  for (const auto& issue : report.issues) {
    if (issue.fatal) {
      qCWarning(formation_data_logger()) << issue.file << issue.message;
    } else {
      qCInfo(formation_data_logger()) << issue.file << issue.message;
    }
  }
  qCInfo(formation_data_logger()) << "Formation content:" << report.summary();

  return report;
}

} // namespace Game::Formation
