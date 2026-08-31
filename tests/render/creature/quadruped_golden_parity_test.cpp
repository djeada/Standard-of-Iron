

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "animation/rig/horse_gait.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/render_request.h"
#include "render/draw_commands.h"
#include "render/elephant/elephant_motion.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/elephant/prepare.h"
#include "render/elephant/runtime/motion_sample.h"
#include "render/elephant/schema/attachment_schema.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_renderer_base.h"
#include "render/horse/prepare.h"
#include "render/rigged_mesh.h"
#include "render/submitter.h"
#include "render/world_view.h"

namespace {

constexpr const char* k_golden_path =
    "tests/render/creature/golden/quadruped_pipeline_golden.txt";

constexpr float k_matrix_tolerance = 1.0e-4F;
constexpr float k_scalar_tolerance = 1.0e-4F;

constexpr std::uint64_t k_fnv_offset = 0xcbf29ce484222325ULL;

auto fnv1a(std::uint64_t hash, const void* data, std::size_t bytes) -> std::uint64_t {
  const auto* bytes_ptr = static_cast<const std::uint8_t*>(data);
  for (std::size_t i = 0; i < bytes; ++i) {
    hash ^= bytes_ptr[i];
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

auto mesh_topology_hash(const Render::GL::RiggedMesh* mesh) -> std::uint64_t {
  if (mesh == nullptr) {
    return 0U;
  }
  std::uint64_t hash = k_fnv_offset;
  const auto vertex_count = static_cast<std::uint64_t>(mesh->vertex_count());
  const auto index_count = static_cast<std::uint64_t>(mesh->index_count());
  hash = fnv1a(hash, &vertex_count, sizeof(vertex_count));
  hash = fnv1a(hash, &index_count, sizeof(index_count));
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

struct RiggedRecord {
  std::uint64_t topology{0};
  std::uint64_t binding{0};
  std::uint32_t bone_count{0};
  std::uint32_t role_color_count{0};
  QVector3D color{};
  QVector4D wear{};
  QMatrix4x4 world{};
  std::vector<QMatrix4x4> palette;
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

  void rigged(const Render::GL::RiggedCreatureCmd& cmd) override {
    RiggedRecord record;
    record.topology = mesh_topology_hash(cmd.mesh);
    record.binding = mesh_binding_hash(cmd.mesh);
    record.bone_count = cmd.bone_count;
    record.role_color_count = cmd.role_color_count;
    record.color = cmd.color;
    record.wear = cmd.wear_params;
    record.world = cmd.world;
    if (cmd.bone_palette != nullptr) {
      record.palette.assign(cmd.bone_palette, cmd.bone_palette + cmd.bone_count);
    }
    records.push_back(std::move(record));
  }

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

auto archetype_debug_name(Render::Creature::ArchetypeId id) -> std::string {
  const auto* desc = Render::Creature::ArchetypeRegistry::instance().get(id);
  if (desc == nullptr || desc->debug_name.empty()) {
    return "<none>";
  }
  return std::string(desc->debug_name);
}

struct HorseCase {
  const char* name;
  Render::GL::GaitType gait;
  bool fighting;
  bool dying;
  Render::Creature::CreatureLOD lod;
};

const std::array<HorseCase, 8> k_horse_cases{{
    {"horse/idle",
     Render::GL::GaitType::IDLE,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"horse/walk",
     Render::GL::GaitType::WALK,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"horse/trot",
     Render::GL::GaitType::TROT,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"horse/canter",
     Render::GL::GaitType::CANTER,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"horse/gallop",
     Render::GL::GaitType::GALLOP,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"horse/fight",
     Render::GL::GaitType::IDLE,
     true,
     false,
     Render::Creature::CreatureLOD::Full},
    {"horse/death",
     Render::GL::GaitType::IDLE,
     false,
     true,
     Render::Creature::CreatureLOD::Full},
    {"horse/minimal_lod",
     Render::GL::GaitType::WALK,
     false,
     false,
     Render::Creature::CreatureLOD::Minimal},
}};

auto speed_for_gait(Render::GL::GaitType gait) -> float {
  switch (gait) {
  case Render::GL::GaitType::IDLE:
    return 0.0F;
  case Render::GL::GaitType::WALK:
    return 1.4F;
  case Render::GL::GaitType::TROT:
    return 3.2F;
  case Render::GL::GaitType::CANTER:
    return 5.6F;
  case Render::GL::GaitType::GALLOP:
    return 8.4F;
  }
  return 0.0F;
}

void capture_horse(const HorseCase& horse_case,
                   RecordingSubmitter& sink,
                   Render::Creature::Pipeline::CreaturePreparationResult& prep,
                   Render::GL::HorseMotionSample& out_motion) {
  Render::GL::HorseRendererBase const owner;

  Engine::Core::StandaloneEntity entity_scratch(4001);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* state = entity.add_component<Render::Creature::HorseAnimationStateComponent>();

  state->current_gait = horse_case.gait;
  state->target_gait = horse_case.gait;
  state->gait_transition_progress = 1.0F;
  state->transition_start_time = 0.0F;
  state->locomotion_phase = 0.25F;
  state->locomotion_phase_time = 2.0F;
  state->locomotion_phase_valid = true;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of(Game::Session::SessionContext::active());
  ctx.entity = &entity;
  ctx.allow_template_cache = false;
  ctx.model.translate(0.0F, 4.0F, 0.0F);

  Render::GL::AnimationInputs anim{};
  anim.time = 2.0F;
  anim.is_dying = horse_case.dying;
  anim.death_progress = horse_case.dying ? 0.55F : 0.0F;
  if (horse_case.gait != Render::GL::GaitType::IDLE) {
    anim.movement_state = horse_case.gait == Render::GL::GaitType::WALK
                              ? Render::Creature::MovementAnimationState::Walk
                              : Render::Creature::MovementAnimationState::Run;
  }

  Render::GL::HumanoidAnimationContext rider_ctx{};
  rider_ctx.gait.speed = speed_for_gait(horse_case.gait);
  rider_ctx.gait.normalized_speed = speed_for_gait(horse_case.gait) / 8.0F;
  rider_ctx.inputs.is_attacking = horse_case.fighting;
  rider_ctx.inputs.is_melee = horse_case.fighting;

  Render::GL::HorseProfile profile = Render::GL::make_horse_profile(
      17U, QVector3D(0.4F, 0.3F, 0.2F), QVector3D(0.6F, 0.1F, 0.1F));

  auto motion = Render::GL::evaluate_horse_motion(profile, anim, rider_ctx, state);
  motion.is_fighting = horse_case.fighting;

  Render::Horse::prepare_horse_render(
      owner, ctx, anim, rider_ctx, profile, &motion, horse_case.lod, prep, 0x51eedU);
  Render::Creature::Pipeline::submit_preparation(prep, sink);
  out_motion = motion;
}

struct ElephantCase {
  const char* name;
  Render::Creature::MovementAnimationState movement;
  bool fighting;
  bool dying;
  bool howdah_occupied;
  Render::Creature::CreatureLOD lod;
};

const std::array<ElephantCase, 6> k_elephant_cases{{
    {"elephant/idle",
     Render::Creature::MovementAnimationState::Idle,
     false,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"elephant/walk",
     Render::Creature::MovementAnimationState::Walk,
     false,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"elephant/fast_walk",
     Render::Creature::MovementAnimationState::Run,
     false,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"elephant/fight",
     Render::Creature::MovementAnimationState::Idle,
     true,
     false,
     false,
     Render::Creature::CreatureLOD::Full},
    {"elephant/death",
     Render::Creature::MovementAnimationState::Idle,
     false,
     true,
     false,
     Render::Creature::CreatureLOD::Full},
    {"elephant/howdah_occupied",
     Render::Creature::MovementAnimationState::Walk,
     false,
     false,
     true,
     Render::Creature::CreatureLOD::Full},
}};

void capture_elephant(const ElephantCase& elephant_case,
                      RecordingSubmitter& sink,
                      Render::Creature::Pipeline::CreaturePreparationResult& prep,
                      Render::GL::HowdahAttachmentFrame& out_howdah,
                      Render::GL::ElephantMotionSample& out_motion) {
  Render::GL::ElephantRendererBase const owner;

  Engine::Core::StandaloneEntity entity_scratch(4002);
  Engine::Core::Entity& entity = entity_scratch.entity();
  entity.add_component<Render::Creature::ElephantAnimationStateComponent>();

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of(Game::Session::SessionContext::active());
  ctx.entity = &entity;
  ctx.allow_template_cache = false;
  ctx.model.translate(0.0F, 4.0F, 0.0F);

  Render::GL::AnimationInputs anim{};
  anim.time = 2.0F;
  anim.movement_state = elephant_case.movement;
  anim.is_attacking = elephant_case.fighting;
  anim.is_melee = elephant_case.fighting;
  anim.is_dying = elephant_case.dying;
  anim.death_progress = elephant_case.dying ? 0.55F : 0.0F;

  Render::GL::ElephantProfile profile = Render::GL::make_elephant_profile(
      23U, QVector3D(0.45F, 0.42F, 0.40F), QVector3D(0.30F, 0.10F, 0.10F));

  auto motion = Render::GL::evaluate_elephant_motion(
      profile,
      anim,
      entity.get_component<Render::Creature::ElephantAnimationStateComponent>());
  motion.is_fighting = elephant_case.fighting;
  out_howdah = motion.howdah;

  Render::Elephant::prepare_elephant_render(
      owner,
      ctx,
      anim,
      profile,
      elephant_case.howdah_occupied ? &motion.howdah : nullptr,
      &motion,
      elephant_case.lod,
      prep);
  Render::Creature::Pipeline::submit_preparation(prep, sink);
  out_motion = motion;
}

struct MountedCase {
  const char* name;
  const char* renderer_id;
  Render::Creature::MovementAnimationState movement;
  float speed_hint;
};

const std::array<MountedCase, 2> k_mounted_cases{{
    {"mounted/walk",
     "troops/roman/horse_swordsman",
     Render::Creature::MovementAnimationState::Walk,
     1.4F},
    {"mounted/gallop",
     "troops/roman/horse_swordsman",
     Render::Creature::MovementAnimationState::Run,
     8.4F},
}};

auto capture_mounted(const Render::GL::EntityRendererRegistry& registry,
                     const MountedCase& mounted_case,
                     RecordingSubmitter& sink,
                     Render::Creature::Pipeline::CreaturePreparationResult& prep)
    -> bool {
  const auto handle = registry.get_handle(mounted_case.renderer_id);
  if (handle == Render::GL::k_invalid_renderer_handle) {
    return false;
  }
  const auto* preparer = registry.get_preparer(handle);
  const auto renderer = registry.get(mounted_case.renderer_id);
  if (preparer == nullptr || !renderer) {
    return false;
  }

  Engine::Core::StandaloneEntity entity_scratch(4003);
  Engine::Core::Entity& entity = entity_scratch.entity();
  auto* unit = entity.add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::MountedKnight;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->render_individuals_per_unit_override = 1;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  renderable->renderer_id = mounted_case.renderer_id;
  renderable->visible = true;

  Render::GL::AnimationInputs anim{};
  anim.time = 2.0F;
  anim.is_mounted = true;
  anim.movement_state = mounted_case.movement;
  anim.visual_movement.is_authoritative = true;
  anim.visual_movement.movement_state = mounted_case.movement;
  anim.visual_movement.has_velocity = true;
  anim.visual_movement.speed_hint = mounted_case.speed_hint;

  Render::GL::DrawContext ctx{};
  ctx.world_view = Render::WorldView::of(Game::Session::SessionContext::active());
  ctx.entity = &entity;
  ctx.animation_time = anim.time;
  ctx.animation_override = &anim;
  ctx.allow_template_cache = false;
  ctx.suppress_animation_state_persistence = true;
  ctx.has_seed_override = true;
  ctx.seed_override = 0x51eedU;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = Render::Creature::CreatureLOD::Full;
  ctx.force_quadruped_lod = true;
  ctx.forced_quadruped_lod = Render::Creature::CreatureLOD::Full;

  preparer->prepare(ctx, prep);
  renderer(ctx, sink);
  return true;
}

void write_matrix(std::ostream& out, const std::string& prefix, const QMatrix4x4& m) {
  out << prefix;
  for (int col = 0; col < 4; ++col) {
    const QVector4D column = m.column(col);
    out << ' ' << column.x() << ' ' << column.y() << ' ' << column.z() << ' '
        << column.w();
  }
  out << '\n';
}

void write_case(std::ostream& out,
                const std::string& name,
                const Render::Creature::Pipeline::CreaturePreparationResult& prep,
                const RecordingSubmitter& sink) {
  out << "case " << name << " requests " << prep.bodies.requests().size() << " draws "
      << sink.records.size() << " meshes " << sink.mesh_calls << '\n';

  std::size_t index = 0;
  for (const auto& request : prep.bodies.requests()) {
    std::ostringstream key;
    key << name << '@' << index++;
    out << "  request " << key.str() << " archetype "
        << archetype_debug_name(request.archetype) << " asset "
        << request.creature_asset_id << " variant " << request.variant << " lod "
        << static_cast<int>(request.lod) << " pass " << static_cast<int>(request.pass)
        << " state " << static_cast<int>(request.state) << " clip " << request.clip_id
        << " clipvariant " << static_cast<int>(request.clip_variant) << " roles "
        << static_cast<int>(request.role_color_count) << " seed " << request.seed
        << " grounded " << (request.world_already_grounded ? 1 : 0) << '\n';
    out << "  params " << key.str() << " phase " << request.phase << " color "
        << request.base_color.x() << ' ' << request.base_color.y() << ' '
        << request.base_color.z() << " wear " << request.wear_params.x() << ' '
        << request.wear_params.y() << ' ' << request.wear_params.z() << ' '
        << request.wear_params.w() << '\n';
    write_matrix(out, "  requestworld " + key.str(), request.world);
  }

  for (std::size_t i = 0; i < sink.records.size(); ++i) {
    const auto& record = sink.records[i];
    std::ostringstream key;
    key << name << '#' << i;
    out << "  draw " << key.str() << " topology " << record.topology << " binding "
        << record.binding << " bones " << record.bone_count << " roles "
        << record.role_color_count << '\n';
    out << "  color " << key.str() << ' ' << record.color.x() << ' ' << record.color.y()
        << ' ' << record.color.z() << " wear " << record.wear.x() << ' '
        << record.wear.y() << ' ' << record.wear.z() << ' ' << record.wear.w() << '\n';
    write_matrix(out, "  world " + key.str(), record.world);
    for (std::size_t b = 0; b < record.palette.size(); ++b) {
      write_matrix(
          out, "  bone " + key.str() + '.' + std::to_string(b), record.palette[b]);
    }
  }
}

void write_golden(std::ostream& out) {
  out.setf(std::ios::fixed);
  out.precision(6);

  for (const auto& horse_case : k_horse_cases) {
    RecordingSubmitter sink;
    Render::Creature::Pipeline::CreaturePreparationResult prep;
    Render::GL::HorseMotionSample motion{};
    capture_horse(horse_case, sink, prep, motion);
    write_case(out, horse_case.name, prep, sink);

    out << "  motion " << horse_case.name << " gait "
        << static_cast<int>(motion.gait_type) << " playback "
        << static_cast<int>(motion.playback_gait_type) << " phase " << motion.phase
        << " cycle " << motion.gait.cycle_time << " swing " << motion.gait.stride_swing
        << " lift " << motion.gait.stride_lift << " moving "
        << (motion.is_moving ? 1 : 0) << " fighting " << (motion.is_fighting ? 1 : 0)
        << " bob " << motion.bob << " sway " << motion.body_sway << " pitch "
        << motion.body_pitch << '\n';
  }

  Render::GL::EntityRendererRegistry registry;
  Render::GL::register_built_in_entity_renderers(registry);
  for (const auto& mounted_case : k_mounted_cases) {
    RecordingSubmitter sink;
    Render::Creature::Pipeline::CreaturePreparationResult prep;
    const bool available = capture_mounted(registry, mounted_case, sink, prep);
    out << "available " << mounted_case.name << ' ' << (available ? 1 : 0) << '\n';
    write_case(out, mounted_case.name, prep, sink);
  }

  for (const auto& elephant_case : k_elephant_cases) {
    RecordingSubmitter sink;
    Render::Creature::Pipeline::CreaturePreparationResult prep;
    Render::GL::HowdahAttachmentFrame howdah{};
    Render::GL::ElephantMotionSample motion_out{};
    capture_elephant(elephant_case, sink, prep, howdah, motion_out);
    write_case(out, elephant_case.name, prep, sink);

    out << "  motion " << elephant_case.name << " phase " << motion_out.phase
        << " movement " << static_cast<int>(motion_out.movement_state) << " moving "
        << (motion_out.is_moving ? 1 : 0) << " fighting "
        << (motion_out.is_fighting ? 1 : 0) << " bob " << motion_out.bob << " sway "
        << motion_out.body_sway << " trunk " << motion_out.trunk_swing << " ear "
        << motion_out.ear_flap << '\n';

    auto write_point = [&out](const QVector3D& point) {
      out << ' ' << point.x() << ' ' << point.y() << ' ' << point.z();
    };
    out << "  howdah " << elephant_case.name << " center";
    write_point(howdah.howdah_center);
    out << " seat";
    write_point(howdah.seat_position);
    out << " forward";
    write_point(howdah.seat_forward);
    out << " right";
    write_point(howdah.seat_right);
    out << " up";
    write_point(howdah.seat_up);
    out << " ground";
    write_point(howdah.ground_offset);
    out << '\n';
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
  if (kind == "world" || kind == "bone" || kind == "requestworld" || kind == "howdah" ||
      kind == "motion") {
    return k_matrix_tolerance;
  }
  return k_scalar_tolerance;
}

} // namespace

TEST(QuadrupedGoldenParity, PipelineMatchesRecordedGolden) {
  ScopedFlatTerrain const terrain;

  std::ostringstream produced;
  write_golden(produced);

  if (std::getenv("SOI_WRITE_QUADRUPED_GOLDEN") != nullptr) {
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
         "SOI_WRITE_QUADRUPED_GOLDEN=1";
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

      double want_value = 0.0;
      double got_value = 0.0;
      const auto want_parse =
          std::from_chars(want.data(), want.data() + want.size(), want_value);
      const auto got_parse =
          std::from_chars(got.data(), got.data() + got.size(), got_value);
      const bool numeric =
          want_parse.ec == std::errc{} && want_parse.ptr == want.data() + want.size() &&
          got_parse.ec == std::errc{} && got_parse.ptr == got.data() + got.size();
      ASSERT_TRUE(numeric) << "line " << i << " token " << t << ": expected '" << want
                           << "' got '" << got << "'";
      EXPECT_NEAR(want_value, got_value, tolerance)
          << "line " << i << " token " << t << " of '" << expected[i] << "'";
    }
  }
}
