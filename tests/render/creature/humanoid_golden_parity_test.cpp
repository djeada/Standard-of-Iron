

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include "animation/pose_manifest.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"
#include "render/draw_commands.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/runtime/frame_control.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/rigged_mesh.h"
#include "render/submitter.h"
#include "render/world_view.h"

namespace {

constexpr const char* k_golden_path =
    "tests/render/creature/golden/humanoid_pipeline_golden.txt";

constexpr float k_matrix_tolerance = 1.0e-4F;
constexpr float k_scalar_tolerance = 1.0e-4F;

auto fnv1a(std::uint64_t hash, const void* data, std::size_t bytes) -> std::uint64_t {
  const auto* bytes_ptr = static_cast<const std::uint8_t*>(data);
  for (std::size_t i = 0; i < bytes; ++i) {
    hash ^= bytes_ptr[i];
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

constexpr std::uint64_t k_fnv_offset = 0xcbf29ce484222325ULL;

auto mesh_topology_hash(const Render::GL::RiggedMesh* mesh) -> std::uint64_t {
  if (mesh == nullptr) {
    return 0U;
  }
  std::uint64_t hash = k_fnv_offset;
  const auto vertex_count = static_cast<std::uint64_t>(mesh->vertex_count());
  const auto index_count = static_cast<std::uint64_t>(mesh->index_count());
  const auto rigid_count = static_cast<std::uint64_t>(mesh->rigid_index_count());
  hash = fnv1a(hash, &vertex_count, sizeof(vertex_count));
  hash = fnv1a(hash, &index_count, sizeof(index_count));
  hash = fnv1a(hash, &rigid_count, sizeof(rigid_count));
  hash = fnv1a(hash, mesh->get_indices().data(), mesh->get_indices().size() * 4U);
  return hash;
}

auto mesh_binding_hash(const Render::GL::RiggedMesh* mesh) -> std::uint64_t {
  if (mesh == nullptr) {
    return 0U;
  }
  std::uint64_t hash = k_fnv_offset;
  for (const auto& vertex : mesh->get_vertices()) {
    hash = fnv1a(hash, vertex.bone_indices.data(), vertex.bone_indices.size());
    hash = fnv1a(hash, &vertex.color_role, sizeof(vertex.color_role));
    for (float weight : vertex.bone_weights) {
      const auto quantized = static_cast<std::int32_t>(std::lround(weight * 4096.0F));
      hash = fnv1a(hash, &quantized, sizeof(quantized));
    }
  }
  return hash;
}

auto archetype_debug_name(Render::Creature::ArchetypeId id) -> std::string {
  const auto* desc = Render::Creature::ArchetypeRegistry::instance().get(id);
  if (desc == nullptr || desc->debug_name.empty()) {
    return "<none>";
  }
  return std::string(desc->debug_name);
}

struct RiggedRecord {
  std::uint64_t topology{0};
  std::uint64_t binding{0};
  std::uint32_t bone_count{0};
  std::uint32_t role_color_count{0};
  QVector3D color{};
  QVector4D wear{};
  QVector3D variation_scale{};
  QMatrix4x4 world{};
  std::vector<QMatrix4x4> palette;
  std::array<QMatrix4x4, Render::Humanoid::k_socket_count> sockets{};
};

class RecordingSubmitter : public Render::GL::ISubmitter {
public:
  std::vector<RiggedRecord> records;
  int mesh_calls{0};

  void mesh(Render::GL::Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Render::GL::Texture*,
            float,
            int) override {
    ++mesh_calls;
  }

  void record_rigged(const Render::GL::RiggedCreatureCmd& cmd) {
    RiggedRecord record;
    record.topology = mesh_topology_hash(cmd.mesh);
    record.binding = mesh_binding_hash(cmd.mesh);
    record.bone_count = cmd.bone_count;
    record.role_color_count = cmd.role_color_count;
    record.color = cmd.color;
    record.wear = cmd.wear_params;
    record.variation_scale = cmd.variation_scale;
    record.world = cmd.world;
    if (cmd.bone_palette != nullptr) {
      record.palette.assign(cmd.bone_palette, cmd.bone_palette + cmd.bone_count);
    }
    if (cmd.bone_palette != nullptr &&
        cmd.bone_count >= Render::Humanoid::k_bone_count) {
      Render::Humanoid::BonePalette palette{};
      for (std::size_t i = 0; i < Render::Humanoid::k_bone_count; ++i) {
        palette[i] = cmd.bone_palette[i];
      }
      for (std::size_t i = 0; i < Render::Humanoid::k_socket_count; ++i) {
        record.sockets[i] = Render::Humanoid::socket_transform(
            palette, static_cast<Render::Humanoid::HumanoidSocket>(i));
      }
    }
    records.push_back(std::move(record));
  }

  void rigged(const Render::GL::RiggedCreatureCmd& cmd) override { record_rigged(cmd); }

  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const Render::GL::GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

struct ScopedFlatTerrain {
  ScopedFlatTerrain() {
    auto& terrain = Game::Map::TerrainService::instance();
    std::vector<float> const heights(9, 0.0F);
    std::vector<Game::Map::TerrainType> const types(9, Game::Map::TerrainType::Flat);
    terrain.restore_from_serialized(
        3, 3, 1.0F, heights, types, {}, {}, {}, Game::Map::BiomeSettings{});
  }
  ~ScopedFlatTerrain() { Game::Map::TerrainService::instance().clear(); }
  ScopedFlatTerrain(const ScopedFlatTerrain&) = delete;
  auto operator=(const ScopedFlatTerrain&) -> ScopedFlatTerrain& = delete;
};

using ConfigureFn = void (*)(Render::GL::AnimationInputs&);

struct GoldenCase {
  const char* name;
  const char* renderer_id;
  Game::Units::SpawnType spawn_type;
  Game::Systems::NationID nation;
  Render::Creature::CreatureLOD lod;
  ConfigureFn configure;
};

void state_bind(Render::GL::AnimationInputs&) {
}

void state_idle(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.idle_duration = 3.25F;
}

void state_walk(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.movement_state = Animation::MovementState::Walk;
  anim.visual_movement.is_authoritative = true;
  anim.visual_movement.movement_state = Animation::MovementState::Walk;
  anim.visual_movement.has_velocity = true;
  anim.visual_movement.speed_hint = 1.4F;
  anim.visual_movement.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
}

void state_run(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.movement_state = Animation::MovementState::Run;
  anim.visual_movement.is_authoritative = true;
  anim.visual_movement.movement_state = Animation::MovementState::Run;
  anim.visual_movement.has_velocity = true;
  anim.visual_movement.speed_hint = 3.6F;
  anim.visual_movement.locomotion_direction = QVector3D(0.0F, 0.0F, 1.0F);
}

void state_attack(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.is_attacking = true;
  anim.is_melee = true;
  anim.attack_family = Engine::Core::CombatAttackFamily::Sword;
  anim.combat_phase = Render::GL::CombatAnimPhase::Strike;
  anim.combat_phase_progress = 0.4F;
}

void state_guard(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.is_guarding = true;
  anim.guard_pose_progress = 1.0F;
  anim.is_defensive_layout_locked = true;
}

void state_hit(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.is_hit_reacting = true;
  anim.hit_reaction_intensity = 1.0F;
  anim.hit_reaction_progress = 0.35F;
  anim.hit_recoil_x = 0.4F;
  anim.hit_recoil_z = -0.2F;
}

void state_death(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.is_dying = true;
  anim.death_progress = 0.55F;
}

void state_construction(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.is_constructing = true;
  anim.construction_progress = 0.4F;
  anim.construction_job = 1U;
}

void state_mounted(Render::GL::AnimationInputs& anim) {
  anim.time = 3.25F;
  anim.is_mounted = true;
  anim.movement_state = Animation::MovementState::Walk;
  anim.visual_movement.is_authoritative = true;
  anim.visual_movement.movement_state = Animation::MovementState::Walk;
  anim.visual_movement.has_velocity = true;
  anim.visual_movement.speed_hint = 2.2F;
}

const std::array<GoldenCase, 26> k_cases{{
    {"roman_sword/bind",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_bind},
    {"roman_sword/idle",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_idle},
    {"roman_sword/walk",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_walk},
    {"roman_sword/run",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_run},
    {"roman_sword/attack",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_attack},
    {"roman_sword/guard",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_guard},
    {"roman_sword/hit",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_hit},
    {"roman_sword/death",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_death},
    {"roman_sword/minimal_lod",
     "troops/roman/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Minimal,
     state_idle},

    {"carthage_sword/idle",
     "troops/carthage/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::Carthage,
     Render::Creature::CreatureLOD::Full,
     state_idle},
    {"carthage_sword/guard",
     "troops/carthage/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::Carthage,
     Render::Creature::CreatureLOD::Full,
     state_guard},
    {"carthage_sword/attack",
     "troops/carthage/swordsman",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::Carthage,
     Render::Creature::CreatureLOD::Full,
     state_attack},

    {"roman_spear/idle",
     "troops/roman/spearman",
     Game::Units::SpawnType::Spearman,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_idle},
    {"roman_spear/walk",
     "troops/roman/spearman",
     Game::Units::SpawnType::Spearman,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_walk},
    {"roman_spear/attack",
     "troops/roman/spearman",
     Game::Units::SpawnType::Spearman,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_attack},

    {"roman_archer/idle",
     "troops/roman/archer",
     Game::Units::SpawnType::Archer,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_idle},
    {"roman_archer/run",
     "troops/roman/archer",
     Game::Units::SpawnType::Archer,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_run},
    {"roman_archer/attack",
     "troops/roman/archer",
     Game::Units::SpawnType::Archer,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_attack},

    {"roman_builder/idle",
     "troops/roman/builder",
     Game::Units::SpawnType::Builder,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_idle},
    {"roman_builder/construction",
     "troops/roman/builder",
     Game::Units::SpawnType::Builder,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_construction},
    {"carthage_builder/construction",
     "troops/carthage/builder",
     Game::Units::SpawnType::Builder,
     Game::Systems::NationID::Carthage,
     Render::Creature::CreatureLOD::Full,
     state_construction},

    {"roman_commander/idle",
     "troops/roman/commanders/scipio_africanus",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_idle},
    {"roman_commander/attack",
     "troops/roman/commanders/scipio_africanus",
     Game::Units::SpawnType::Knight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_attack},

    {"roman_mounted/idle",
     "troops/roman/horse_swordsman",
     Game::Units::SpawnType::MountedKnight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_mounted},
    {"roman_mounted/attack",
     "troops/roman/horse_swordsman",
     Game::Units::SpawnType::MountedKnight,
     Game::Systems::NationID::RomanRepublic,
     Render::Creature::CreatureLOD::Full,
     state_attack},
    {"carthage_mounted/idle",
     "troops/carthage/horse_swordsman",
     Game::Units::SpawnType::MountedKnight,
     Game::Systems::NationID::Carthage,
     Render::Creature::CreatureLOD::Full,
     state_mounted},
}};

auto capture_case(const Render::GL::EntityRendererRegistry& registry,
                  const GoldenCase& gcase,
                  RecordingSubmitter& sink,
                  Render::Creature::Pipeline::CreaturePreparationResult& out_prep)
    -> bool {
  const auto renderer = registry.get(gcase.renderer_id);
  if (!renderer) {
    return false;
  }

  Render::GL::reset_humanoid_runtime_context();

  Engine::Core::StandaloneEntity entity_scratch(1);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  unit->owner_id = 1;
  unit->spawn_type = gcase.spawn_type;
  unit->nation_id = gcase.nation;
  unit->render_individuals_per_unit_override = 3;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  renderable->renderer_id = gcase.renderer_id;
  renderable->visible = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 0.0F;
  gcase.configure(anim);

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of_active_session();
  ctx.entity = &entity;
  ctx.animation_time = anim.time;
  ctx.animation_override = &anim;
  ctx.allow_template_cache = false;
  ctx.suppress_animation_state_persistence = true;
  ctx.has_seed_override = true;
  ctx.seed_override = 0x5eed1234U;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = gcase.lod;
  ctx.force_quadruped_lod = true;
  ctx.forced_quadruped_lod = gcase.lod;

  renderer(ctx, sink);

  const auto handle = registry.get_handle(gcase.renderer_id);
  const auto* preparer = registry.get_preparer(handle);
  if (preparer != nullptr) {
    Render::GL::reset_humanoid_runtime_context();
    preparer->prepare(ctx, out_prep);
  }
  return true;
}

void write_record(std::ostream& out, const std::string& prefix, const QMatrix4x4& m) {
  out << prefix;
  for (int col = 0; col < 4; ++col) {
    const QVector4D column = m.column(col);
    out << ' ' << column.x() << ' ' << column.y() << ' ' << column.z() << ' '
        << column.w();
  }
  out << '\n';
}

void write_golden(std::ostream& out) {
  out.setf(std::ios::fixed);
  out.precision(6);
  out << "schema " << Render::Humanoid::humanoid_skeleton_schema_hash() << '\n';

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);

  for (const auto& gcase : k_cases) {
    RecordingSubmitter sink;
    Render::Creature::Pipeline::CreaturePreparationResult prep;
    const bool available = capture_case(registry, gcase, sink, prep);
    out << "case " << gcase.name << " available " << (available ? 1 : 0) << " draws "
        << sink.records.size() << " meshes " << sink.mesh_calls << " requests "
        << prep.bodies.requests().size() << '\n';
    {
      std::size_t request_index = 0;
      for (const auto& request : prep.bodies.requests()) {
        std::ostringstream key;
        key << gcase.name << '@' << request_index++;
        out << "  request " << key.str() << " archetype "
            << archetype_debug_name(request.archetype) << " asset "
            << request.creature_asset_id << " variant " << request.variant << " lod "
            << static_cast<int>(request.lod) << " pass "
            << static_cast<int>(request.pass) << " state "
            << static_cast<int>(request.state) << " clip " << request.clip_id
            << " clipvariant " << static_cast<int>(request.clip_variant) << " roles "
            << static_cast<int>(request.role_color_count) << " seed " << request.seed
            << " grounded " << (request.world_already_grounded ? 1 : 0) << '\n';
        out << "  params " << key.str() << " phase " << request.phase << " color "
            << request.base_color.x() << ' ' << request.base_color.y() << ' '
            << request.base_color.z() << " wear " << request.wear_params.x() << ' '
            << request.wear_params.y() << ' ' << request.wear_params.z() << ' '
            << request.wear_params.w() << '\n';
        write_record(out, "  requestworld " + key.str(), request.world);
      }
    }
    for (std::size_t i = 0; i < sink.records.size(); ++i) {
      const auto& record = sink.records[i];
      std::ostringstream key;
      key << gcase.name << '#' << i;
      out << "  draw " << key.str() << " topology " << record.topology << " binding "
          << record.binding << " bones " << record.bone_count << " roles "
          << record.role_color_count << '\n';
      out << "  color " << key.str() << ' ' << record.color.x() << ' '
          << record.color.y() << ' ' << record.color.z() << " wear " << record.wear.x()
          << ' ' << record.wear.y() << ' ' << record.wear.z() << ' ' << record.wear.w()
          << " scale " << record.variation_scale.x() << ' '
          << record.variation_scale.y() << ' ' << record.variation_scale.z() << '\n';
      write_record(out, "  world " + key.str(), record.world);
      for (std::size_t b = 0; b < record.palette.size(); ++b) {
        write_record(
            out, "  bone " + key.str() + '.' + std::to_string(b), record.palette[b]);
      }
      for (std::size_t s = 0; s < record.sockets.size(); ++s) {
        write_record(
            out, "  socket " + key.str() + '.' + std::to_string(s), record.sockets[s]);
      }
    }
  }
}

auto tokenize(const std::string& line) -> std::vector<std::string> {
  std::vector<std::string> tokens;
  std::istringstream stream(line);
  std::string token;
  while (stream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

auto read_lines(const std::string& text) -> std::vector<std::string> {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

auto tolerance_for(const std::string& kind) -> float {
  if (kind == "world" || kind == "bone" || kind == "socket" || kind == "requestworld") {
    return k_matrix_tolerance;
  }
  return k_scalar_tolerance;
}

} // namespace

TEST(HumanoidGoldenParity, PipelineMatchesRecordedGolden) {
  ScopedFlatTerrain const terrain;

  std::ostringstream produced;
  write_golden(produced);

  if (std::getenv("SOI_WRITE_HUMANOID_GOLDEN") != nullptr) {
    std::ofstream file(k_golden_path, std::ios::trunc);
    ASSERT_TRUE(file.is_open())
        << "cannot write " << k_golden_path << " (run from the repo root)";
    file << produced.str();
    file.close();
    GTEST_SKIP() << "regenerated " << k_golden_path;
  }

  std::ifstream file(k_golden_path);
  ASSERT_TRUE(file.is_open())
      << "missing " << k_golden_path
      << " -- run the suite from the repo root, or regenerate with "
         "SOI_WRITE_HUMANOID_GOLDEN=1";
  std::stringstream expected_stream;
  expected_stream << file.rdbuf();

  const auto expected = read_lines(expected_stream.str());
  const auto actual = read_lines(produced.str());

  ASSERT_EQ(expected.size(), actual.size())
      << "golden line count changed: the pipeline emits a different number of draws "
         "or bones";

  for (std::size_t i = 0; i < expected.size(); ++i) {
    const auto expected_tokens = tokenize(expected[i]);
    const auto actual_tokens = tokenize(actual[i]);
    ASSERT_EQ(expected_tokens.size(), actual_tokens.size())
        << "line " << i << ": '" << expected[i] << "' vs '" << actual[i] << "'";
    if (expected_tokens.empty()) {
      continue;
    }
    const float tolerance = tolerance_for(expected_tokens[0]);
    for (std::size_t t = 0; t < expected_tokens.size(); ++t) {
      const std::string& want = expected_tokens[t];
      const std::string& got = actual_tokens[t];
      if (want == got) {
        continue;
      }
      char* want_end = nullptr;
      char* got_end = nullptr;
      const double want_value = std::strtod(want.c_str(), &want_end);
      const double got_value = std::strtod(got.c_str(), &got_end);
      const bool numeric = want_end != want.c_str() && *want_end == '\0' &&
                           got_end != got.c_str() && *got_end == '\0';
      ASSERT_TRUE(numeric) << "line " << i << " token " << t << ": expected '" << want
                           << "' got '" << got << "'";
      EXPECT_NEAR(want_value, got_value, tolerance)
          << "line " << i << " token " << t << " of '" << expected[i] << "'";
    }
  }
}

TEST(HumanoidGoldenParity, SkeletonSchemaIsStable) {
  std::ifstream file(k_golden_path);
  ASSERT_TRUE(file.is_open()) << "missing " << k_golden_path;
  std::string kind;
  std::uint64_t recorded = 0;
  file >> kind >> recorded;
  ASSERT_EQ(kind, "schema");
  EXPECT_EQ(recorded, Render::Humanoid::humanoid_skeleton_schema_hash())
      << "the humanoid bone/socket schema changed; baked assets keyed on the old "
         "schema are no longer valid";
}
