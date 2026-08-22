#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(Q_OS_MACOS)
#include <mach-o/dyld.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

#include "animation/rig/horse_attachment_frames.h"
#include "render/creature/quadruped/attachment_resolver.h"
#include "render/elephant/elephant_source_asset.h"
#include "render/elephant/elephant_spec.h"
#include "render/elephant/schema/attachment_schema.h"
#include "render/horse/horse_source_asset.h"
#include "render/horse/horse_spec.h"
#include "render/horse/schema/mounted_sockets.h"

namespace {

using namespace Render::Horse;
using namespace Render::Elephant;
using Render::Creature::BoneDef;
using Render::Creature::BoneIndex;
using Render::Creature::Quadruped::CustomMeshNode;
using Render::Creature::Quadruped::MeshNode;
using Render::GL::Vertex;

constexpr std::string_view k_resource_path = ":/assets/creatures/horse/horse.cmesh";
constexpr std::string_view k_relative_path = "assets/creatures/horse/horse.cmesh";

constexpr std::array<std::string_view, k_horse_source_bone_count> k_bone_names{{
    "Body",
    "Back",
    "Torso",
    "Torso2",
    "Torso3",
    "Neck1",
    "Neck2",
    "Neck3",
    "Head",
    "Ear1.L",
    "Ear2.L",
    "Ear3.L",
    "Ear4.L",
    "Ear1.R",
    "Ear2.R",
    "Ear3.R",
    "Ear4.R",
    "FrontShoulder.L",
    "FrontUpperLeg.L",
    "FrontLowerLeg.L",
    "FrontShoulder.R",
    "FrontUpperLeg.R",
    "FrontLowerLeg.R",
    "BackShoulder.L",
    "BackLeg.L",
    "BackUpperLeg.L",
    "BackLowerLeg.L",
    "BackShoulder.R",
    "BackLeg.R",
    "BackUpperLeg.R",
    "BackLowerLeg.R",
    "Tail1",
    "Tail2",
    "Tail3",
    "Tail4",
    "Tail5",
    "Tail6",
    "Tail7",
    "PoleTargetBack.L",
    "PoleTarget.L",
    "PoleTargetBack.R",
    "PoleTarget.R",
    "IKBackLeg.L",
    "FFB.L",
    "IKFrontLeg.L",
    "FF.L",
    "IKBackLeg.R",
    "FFB.R",
    "IKFrontLeg.R",
    "FF.R",
}};

struct MaterialBinding {
  std::string_view exported_name;
  std::string_view node_name;
  std::uint8_t role;
};

constexpr std::array<MaterialBinding, 8> k_horse_materials{{
    {"horse_production_material_0", "horse.production.coat", 1U},
    {"horse_production_material_1", "horse.production.coat_dark", 2U},
    {"horse_production_material_2", "horse.production.coat_light", 3U},
    {"horse_production_material_3", "horse.production.hooves", 4U},
    {"horse_production_material_4", "horse.production.hair", 5U},
    {"horse_production_material_5", "horse.production.muzzle", 7U},
    {"horse_production_material_6", "horse.production.eye_white", 3U},
    {"horse_production_material_7", "horse.production.eye_black", 8U},
}};

constexpr std::array<std::string_view, Render::Elephant::k_elephant_source_bone_count>
    k_elephant_bone_names{{
        "Bone",
        "Bone.001",
        "Bone.002",
        "Bone.003",
        "Bone.012",
        "Bone.013",
        "Bone.014",
        "Bone.004",
        "Bone.005",
        "UpperLegFront.L",
        "LowerLegFront.L",
        "FootFront.L",
        "UpperLegFront.L.001",
        "LowerLegFront.L.001",
        "FootFront.L.001",
        "UpperLegBack.L",
        "LowerLegBack.L",
        "FootBack.L",
        "UpperLegBack.L.001",
        "LowerLegBack.L.001",
        "FootBack.L.001",
        "Tail",
        "KneeBack.L",
        "KneeFront.L",
        "Bone.016",
        "Bone.017",
        "Bone.018",
        "TrunkIK",
        "KneeBack.L.001",
        "KneeFront.L.001",
        "Bone.019",
        "Bone.020",
    }};
constexpr std::array<MaterialBinding, 3> k_elephant_materials{{
    {"elephant_production_material_0",
     "elephant.production.skin",
     Render::Elephant::k_elephant_role_skin},
    {"elephant_production_material_1",
     "elephant.production.tusks",
     Render::Elephant::k_elephant_role_tusk},
    {"elephant_production_material_2",
     "elephant.production.eyes",
     Render::Elephant::k_elephant_role_eye},
}};

struct SourceConfig {
  std::string_view resource_path;
  std::string_view relative_path;
  std::span<const std::string_view> bone_names;
  std::span<const MaterialBinding> materials;
  std::size_t expected_vertices;
  std::size_t expected_triangles;
  std::size_t expected_clips;
  float scale_x;
  float scale_y;
  float scale_z;
  float ground_y;
  std::string_view sha256;
};

constexpr SourceConfig k_horse_config{
    k_resource_path,
    k_relative_path,
    k_bone_names,
    k_horse_materials,
    4400U,
    2182U,
    13U,
    k_horse_mesh_scale_x,
    k_horse_mesh_scale_y,
    k_horse_mesh_scale_z,
    k_horse_mesh_ground_y,
    "f17f71f44344b9b1be1b53a2f26bf6a0d08f7a5ef15218077363abb7a05db993"};
constexpr SourceConfig k_elephant_config{
    ":/assets/creatures/elephant/elephant.cmesh",
    "assets/creatures/elephant/elephant.cmesh",
    k_elephant_bone_names,
    k_elephant_materials,
    1474U,
    772U,
    5U,
    Render::Elephant::k_elephant_mesh_scale_x,
    Render::Elephant::k_elephant_mesh_scale_y,
    Render::Elephant::k_elephant_mesh_scale_z,
    Render::Elephant::k_elephant_mesh_ground_y,
    "34b00dea1c0f3fdafef08231d73c7c28b3310d341b08058432354da002721ccf"};

struct NodePose {
  QVector3D translation;
  QQuaternion rotation;
  QVector3D scale{1.0F, 1.0F, 1.0F};
};

enum class ChannelPath : std::uint8_t {
  Translation,
  Rotation,
  Scale
};

struct AnimationChannel {
  int node{-1};
  ChannelPath path{ChannelPath::Translation};
  std::vector<float> times;
  std::vector<float> values;
  int components{3};
  bool step{false};
};

struct SourceClip {
  std::string name;
  float duration{0.0F};
  std::vector<AnimationChannel> channels;
};

struct AccessorView {
  const char* data{nullptr};
  std::size_t count{0U};
  std::size_t stride{0U};
  int component_type{0};
  int components{0};
};

struct SourceAsset {
  QByteArray buffer;
  QJsonObject document;
  std::vector<NodePose> default_poses;
  std::vector<int> parents;
  std::vector<int> joint_nodes;
  std::vector<MeshNode> mesh_nodes;
  std::vector<QMatrix4x4> bind_palette;
  std::vector<BoneDef> bone_defs;
  std::vector<SourceClip> clips;
  Render::Creature::CompiledCreatureAssetStatus status;
};

auto executable_directory() -> QString {
  if (QCoreApplication::instance() != nullptr) {
    return QCoreApplication::applicationDirPath();
  }

#if defined(Q_OS_LINUX)
  return QFileInfo(QFile::symLinkTarget(QStringLiteral("/proc/self/exe")))
      .absolutePath();
#elif defined(Q_OS_MACOS)
  std::uint32_t required_size = 0U;
  _NSGetExecutablePath(nullptr, &required_size);
  std::vector<char> path(required_size);
  if (_NSGetExecutablePath(path.data(), &required_size) == 0) {
    return QFileInfo(QFile::decodeName(path.data())).absolutePath();
  }
#elif defined(Q_OS_WIN)
  std::array<wchar_t, 32768> path{};
  DWORD const length =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length > 0U && length < path.size()) {
    return QFileInfo(
               QString::fromWCharArray(path.data(), static_cast<qsizetype>(length)))
        .absolutePath();
  }
#endif
  return {};
}

auto read_compiled_asset(SourceConfig const& config) -> QByteArray {
  const QString app_dir = executable_directory();
  const QString relative_path = QString::fromUtf8(
      config.relative_path.data(), static_cast<qsizetype>(config.relative_path.size()));
  std::array<QString, 6> const candidates{
      QString::fromUtf8(config.resource_path.data(),
                        static_cast<qsizetype>(config.resource_path.size())),
      app_dir + QLatin1Char('/') + relative_path,
      app_dir + QStringLiteral("/../") + relative_path,
      relative_path,
      QStringLiteral("../") + relative_path,
      QStringLiteral("../../") + relative_path,
  };
  for (QString const& path : candidates) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
      return file.readAll();
    }
  }
  return {};
}

auto component_count(QString const& type) noexcept -> int {
  if (type == QStringLiteral("SCALAR")) {
    return 1;
  }
  if (type == QStringLiteral("VEC2")) {
    return 2;
  }
  if (type == QStringLiteral("VEC3")) {
    return 3;
  }
  if (type == QStringLiteral("VEC4")) {
    return 4;
  }
  if (type == QStringLiteral("MAT4")) {
    return 16;
  }
  return 0;
}

auto component_size(int type) noexcept -> std::size_t {
  switch (type) {
  case 5120:
  case 5121:
    return 1U;
  case 5122:
  case 5123:
    return 2U;
  case 5125:
  case 5126:
    return 4U;
  default:
    return 0U;
  }
}

auto accessor_view(SourceAsset const& asset, int accessor_index) -> AccessorView {
  QJsonArray const accessors =
      asset.document.value(QStringLiteral("accessors")).toArray();
  QJsonArray const buffer_views =
      asset.document.value(QStringLiteral("bufferViews")).toArray();
  if (accessor_index < 0 || accessor_index >= accessors.size()) {
    return {};
  }
  QJsonObject const accessor = accessors[accessor_index].toObject();
  int const view_index = accessor.value(QStringLiteral("bufferView")).toInt(-1);
  if (view_index < 0 || view_index >= buffer_views.size()) {
    return {};
  }
  QJsonObject const view = buffer_views[view_index].toObject();
  int const component_type = accessor.value(QStringLiteral("componentType")).toInt();
  int const components =
      component_count(accessor.value(QStringLiteral("type")).toString());
  std::size_t const element_size =
      component_size(component_type) * static_cast<std::size_t>(components);
  std::size_t const view_offset =
      static_cast<std::size_t>(view.value(QStringLiteral("byteOffset")).toInt(0));
  std::size_t const accessor_offset =
      static_cast<std::size_t>(accessor.value(QStringLiteral("byteOffset")).toInt(0));
  std::size_t const stride = static_cast<std::size_t>(
      view.value(QStringLiteral("byteStride")).toInt(static_cast<int>(element_size)));
  std::size_t const count =
      static_cast<std::size_t>(accessor.value(QStringLiteral("count")).toInt());
  std::size_t const byte_offset = view_offset + accessor_offset;
  if (element_size == 0U ||
      byte_offset + (count == 0U ? 0U : (count - 1U) * stride) + element_size >
          static_cast<std::size_t>(asset.buffer.size())) {
    return {};
  }
  return {asset.buffer.constData() + byte_offset,
          count,
          stride,
          component_type,
          components};
}

template <typename T>
auto read_component(const char* data) noexcept -> T {
  T result{};
  std::memcpy(&result, data, sizeof(T));
  return result;
}

auto accessor_float(AccessorView const& view,
                    std::size_t index,
                    int component) noexcept -> float {
  if (view.data == nullptr || index >= view.count || component < 0 ||
      component >= view.components) {
    return 0.0F;
  }
  const char* data =
      view.data + index * view.stride +
      static_cast<std::size_t>(component) * component_size(view.component_type);
  switch (view.component_type) {
  case 5126:
    return read_component<float>(data);
  case 5121:
    return static_cast<float>(read_component<std::uint8_t>(data));
  case 5123:
    return static_cast<float>(read_component<std::uint16_t>(data));
  case 5125:
    return static_cast<float>(read_component<std::uint32_t>(data));
  default:
    return 0.0F;
  }
}

auto accessor_uint(AccessorView const& view,
                   std::size_t index,
                   int component = 0) noexcept -> std::uint32_t {
  if (view.data == nullptr || index >= view.count || component < 0 ||
      component >= view.components) {
    return 0U;
  }
  const char* data =
      view.data + index * view.stride +
      static_cast<std::size_t>(component) * component_size(view.component_type);
  switch (view.component_type) {
  case 5121:
    return read_component<std::uint8_t>(data);
  case 5123:
    return read_component<std::uint16_t>(data);
  case 5125:
    return read_component<std::uint32_t>(data);
  default:
    return 0U;
  }
}

auto json_vec3(QJsonValue const& value, QVector3D fallback) -> QVector3D {
  QJsonArray const array = value.toArray();
  if (array.size() != 3) {
    return fallback;
  }
  return {static_cast<float>(array[0].toDouble()),
          static_cast<float>(array[1].toDouble()),
          static_cast<float>(array[2].toDouble())};
}

auto json_quaternion(QJsonValue const& value) -> QQuaternion {
  QJsonArray const array = value.toArray();
  if (array.size() != 4) {
    return {};
  }
  return {static_cast<float>(array[3].toDouble()),
          static_cast<float>(array[0].toDouble()),
          static_cast<float>(array[1].toDouble()),
          static_cast<float>(array[2].toDouble())};
}

auto local_matrix(NodePose const& pose) -> QMatrix4x4 {
  QMatrix4x4 result;
  result.translate(pose.translation);
  result.rotate(pose.rotation);
  result.scale(pose.scale);
  return result;
}

void evaluate_world_matrices(std::span<const NodePose> poses,
                             std::span<const int> parents,
                             std::vector<QMatrix4x4>& out) {
  out.assign(poses.size(), QMatrix4x4{});

  thread_local std::vector<bool> evaluated;
  evaluated.assign(poses.size(), false);
  auto evaluate = [&](auto&& self, std::size_t node) -> void {
    if (evaluated[node]) {
      return;
    }
    int const parent = parents[node];
    if (parent >= 0) {
      self(self, static_cast<std::size_t>(parent));
      out[node] = out[static_cast<std::size_t>(parent)] * local_matrix(poses[node]);
    } else {
      out[node] = local_matrix(poses[node]);
    }
    evaluated[node] = true;
  };
  for (std::size_t node = 0U; node < poses.size(); ++node) {
    evaluate(evaluate, node);
  }
}

auto normalize_world_matrix(QMatrix4x4 matrix,
                            SourceConfig const& config) noexcept -> QMatrix4x4 {
  QVector3D translation = matrix.column(3).toVector3D();
  translation = {translation.x() * config.scale_x,
                 (translation.y() - config.ground_y) * config.scale_y,
                 translation.z() * config.scale_z};
  matrix.setColumn(3, QVector4D(translation, 1.0F));
  return matrix;
}

auto parse_source(SourceConfig const& config) -> SourceAsset {
  SourceAsset result;
  QByteArray const package = read_compiled_asset(config);
  if (package.isEmpty()) {
    result.status.error = "compiled creature package could not be opened";
    return result;
  }
  QByteArray const digest =
      QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex();
  if (std::string_view(digest.constData(), static_cast<std::size_t>(digest.size())) !=
      config.sha256) {
    result.status.error = "compiled creature package failed its integrity check";
    return result;
  }
  if (!package.startsWith("SOICM001")) {
    result.status.error = "compiled creature package has an invalid header";
    return result;
  }
  QByteArray const source = qUncompress(package.mid(8));
  if (source.isEmpty()) {
    result.status.error = "compiled creature package could not be decompressed";
    return result;
  }
  QJsonParseError parse_error{};
  QJsonDocument const parsed = QJsonDocument::fromJson(source, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !parsed.isObject()) {
    result.status.error = "compiled creature package contains invalid data";
    return result;
  }
  result.document = parsed.object();

  QJsonArray const buffers = result.document.value(QStringLiteral("buffers")).toArray();
  if (buffers.size() != 1) {
    result.status.error = "compiled creature package must contain one embedded buffer";
    return result;
  }
  QString const uri = buffers[0].toObject().value(QStringLiteral("uri")).toString();
  qsizetype const comma = uri.indexOf(QLatin1Char(','));
  if (comma < 0 || !uri.left(comma).contains(QStringLiteral("base64"))) {
    result.status.error = "compiled creature package buffer is not embedded base64";
    return result;
  }
  result.buffer = QByteArray::fromBase64(uri.mid(comma + 1).toLatin1());
  if (result.buffer.isEmpty()) {
    result.status.error = "compiled creature package embedded buffer is empty";
    return result;
  }

  QJsonArray const nodes = result.document.value(QStringLiteral("nodes")).toArray();
  result.default_poses.resize(static_cast<std::size_t>(nodes.size()));
  result.parents.assign(static_cast<std::size_t>(nodes.size()), -1);
  for (int i = 0; i < nodes.size(); ++i) {
    QJsonObject const node = nodes[i].toObject();
    NodePose& pose = result.default_poses[static_cast<std::size_t>(i)];
    pose.translation =
        json_vec3(node.value(QStringLiteral("translation")), QVector3D{});
    pose.rotation = json_quaternion(node.value(QStringLiteral("rotation")));
    pose.scale =
        json_vec3(node.value(QStringLiteral("scale")), QVector3D(1.0F, 1.0F, 1.0F));
    for (QJsonValue const child_value :
         node.value(QStringLiteral("children")).toArray()) {
      int const child = child_value.toInt(-1);
      if (child >= 0 && child < nodes.size()) {
        result.parents[static_cast<std::size_t>(child)] = i;
      }
    }
  }

  QJsonArray const skins = result.document.value(QStringLiteral("skins")).toArray();
  if (skins.isEmpty()) {
    result.status.error = "compiled creature package has no skin";
    return result;
  }
  QJsonArray const joints =
      skins[0].toObject().value(QStringLiteral("joints")).toArray();
  if (joints.size() != static_cast<int>(config.bone_names.size())) {
    result.status.error = "compiled creature package joint count changed";
    return result;
  }
  result.joint_nodes.reserve(config.bone_names.size());
  result.bind_palette.resize(config.bone_names.size());
  result.bone_defs.resize(config.bone_names.size());
  std::vector<int> node_to_joint(static_cast<std::size_t>(nodes.size()), -1);
  for (int slot = 0; slot < joints.size(); ++slot) {
    int const node = joints[slot].toInt(-1);
    if (node < 0 || node >= nodes.size()) {
      result.status.error = "compiled creature package contains an invalid joint";
      return result;
    }
    result.joint_nodes.push_back(node);
    node_to_joint[static_cast<std::size_t>(node)] = slot;
  }

  std::vector<QMatrix4x4> bind_world;
  evaluate_world_matrices(result.default_poses, result.parents, bind_world);
  for (std::size_t slot = 0U; slot < config.bone_names.size(); ++slot) {
    int const node = result.joint_nodes[slot];
    result.bind_palette[slot] =
        normalize_world_matrix(bind_world[static_cast<std::size_t>(node)], config);
    int const parent_node = result.parents[static_cast<std::size_t>(node)];
    BoneIndex parent = Render::Creature::k_invalid_bone;
    if (parent_node >= 0) {
      int const parent_slot = node_to_joint[static_cast<std::size_t>(parent_node)];
      if (parent_slot >= 0) {
        parent = static_cast<BoneIndex>(parent_slot);
      }
    }

    if (slot != 0U && parent == Render::Creature::k_invalid_bone) {
      parent = 0U;
    }
    result.bone_defs[slot] = {config.bone_names[slot], parent};
  }

  QMatrix4x4 const mesh_from_world = result.bind_palette[0].inverted();
  QJsonArray const meshes = result.document.value(QStringLiteral("meshes")).toArray();
  if (meshes.size() != 1) {
    result.status.error = "compiled creature package mesh count changed";
    return result;
  }
  QJsonArray const primitives =
      meshes[0].toObject().value(QStringLiteral("primitives")).toArray();
  if (primitives.size() != static_cast<int>(config.materials.size())) {
    result.status.error = "compiled creature package material primitive count changed";
    return result;
  }
  QJsonArray const materials =
      result.document.value(QStringLiteral("materials")).toArray();
  result.mesh_nodes.reserve(config.materials.size());
  for (int primitive_index = 0; primitive_index < primitives.size();
       ++primitive_index) {
    QJsonObject const primitive = primitives[primitive_index].toObject();
    QJsonObject const attributes =
        primitive.value(QStringLiteral("attributes")).toObject();
    AccessorView const positions =
        accessor_view(result, attributes.value(QStringLiteral("POSITION")).toInt(-1));
    AccessorView const normals =
        accessor_view(result, attributes.value(QStringLiteral("NORMAL")).toInt(-1));
    AccessorView const joints_view =
        accessor_view(result, attributes.value(QStringLiteral("JOINTS_0")).toInt(-1));
    AccessorView const weights =
        accessor_view(result, attributes.value(QStringLiteral("WEIGHTS_0")).toInt(-1));
    AccessorView const indices =
        accessor_view(result, primitive.value(QStringLiteral("indices")).toInt(-1));
    if (positions.data == nullptr || positions.components != 3 ||
        normals.count != positions.count || joints_view.count != positions.count ||
        weights.count != positions.count || joints_view.components != 4 ||
        weights.components != 4 || indices.data == nullptr) {
      result.status.error = "compiled creature package mesh accessors changed";
      return result;
    }

    CustomMeshNode mesh;
    mesh.vertices.reserve(positions.count);
    mesh.indices.reserve(indices.count);
    for (std::size_t vertex_index = 0U; vertex_index < positions.count;
         ++vertex_index) {
      QVector3D const source_position(accessor_float(positions, vertex_index, 0),
                                      accessor_float(positions, vertex_index, 1),
                                      accessor_float(positions, vertex_index, 2));
      QVector3D const normalized_position(source_position.x() * config.scale_x,
                                          (source_position.y() - config.ground_y) *
                                              config.scale_y,
                                          source_position.z() * config.scale_z);
      QVector3D const local_position = mesh_from_world.map(normalized_position);
      QVector3D const source_normal(accessor_float(normals, vertex_index, 0),
                                    accessor_float(normals, vertex_index, 1),
                                    accessor_float(normals, vertex_index, 2));
      QVector3D local_normal = mesh_from_world.mapVector(source_normal);
      local_normal.normalize();

      Vertex vertex{};
      vertex.position = {local_position.x(), local_position.y(), local_position.z()};
      vertex.normal = {local_normal.x(), local_normal.y(), local_normal.z()};
      float weight_sum = 0.0F;
      for (int influence = 0; influence < 4; ++influence) {
        std::uint32_t const joint = accessor_uint(joints_view, vertex_index, influence);
        if (joint >= config.bone_names.size()) {
          result.status.error = "compiled creature package skin index is out of range";
          return result;
        }
        vertex.bone_indices[static_cast<std::size_t>(influence)] =
            static_cast<std::uint8_t>(joint);
        float const weight = accessor_float(weights, vertex_index, influence);
        vertex.bone_weights[static_cast<std::size_t>(influence)] = weight;
        weight_sum += weight;
      }
      if (weight_sum <= 1.0e-6F) {
        vertex.bone_indices = {0U, 0U, 0U, 0U};
        vertex.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
      } else {
        for (float& weight : vertex.bone_weights) {
          weight /= weight_sum;
        }
      }
      mesh.vertices.push_back(vertex);
    }
    for (std::size_t index = 0U; index < indices.count; ++index) {
      std::uint32_t const value = accessor_uint(indices, index);
      if (value >= positions.count) {
        result.status.error = "compiled creature package mesh index is out of range";
        return result;
      }
      mesh.indices.push_back(value);
    }
    result.status.vertex_count += mesh.vertices.size();
    result.status.triangle_count += mesh.indices.size() / 3U;
    int const material_index = primitive.value(QStringLiteral("material")).toInt(-1);
    if (material_index < 0 || material_index >= materials.size()) {
      result.status.error = "compiled creature package material index is out of range";
      return result;
    }
    QString const material_name =
        materials[material_index].toObject().value(QStringLiteral("name")).toString();
    auto const binding = std::find_if(
        config.materials.begin(),
        config.materials.end(),
        [&](MaterialBinding const& candidate) {
          return material_name ==
                 QLatin1String(candidate.exported_name.data(),
                               static_cast<qsizetype>(candidate.exported_name.size()));
        });
    if (binding == config.materials.end()) {
      result.status.error = "compiled creature package uses an unknown material: " +
                            material_name.toStdString();
      return result;
    }
    result.mesh_nodes.push_back({binding->node_name,
                                 0U,
                                 binding->role,
                                 Render::Creature::k_lod_all,
                                 6,
                                 std::move(mesh)});
  }

  QJsonArray const animations =
      result.document.value(QStringLiteral("animations")).toArray();
  result.clips.reserve(static_cast<std::size_t>(animations.size()));
  for (QJsonValue const animation_value : animations) {
    QJsonObject const animation = animation_value.toObject();
    SourceClip clip;
    clip.name = animation.value(QStringLiteral("name")).toString().toStdString();
    QJsonArray const samplers = animation.value(QStringLiteral("samplers")).toArray();
    for (QJsonValue const channel_value :
         animation.value(QStringLiteral("channels")).toArray()) {
      QJsonObject const channel_object = channel_value.toObject();
      int const sampler_index =
          channel_object.value(QStringLiteral("sampler")).toInt(-1);
      if (sampler_index < 0 || sampler_index >= samplers.size()) {
        result.status.error = "compiled creature package animation sampler is invalid";
        return result;
      }
      QJsonObject const sampler = samplers[sampler_index].toObject();
      QString const interpolation = sampler.value(QStringLiteral("interpolation"))
                                        .toString(QStringLiteral("LINEAR"));
      if (interpolation != QStringLiteral("LINEAR") &&
          interpolation != QStringLiteral("STEP")) {
        result.status.error =
            "compiled creature package uses unsupported interpolation";
        return result;
      }
      AccessorView const input =
          accessor_view(result, sampler.value(QStringLiteral("input")).toInt(-1));
      AccessorView const output =
          accessor_view(result, sampler.value(QStringLiteral("output")).toInt(-1));
      QJsonObject const target =
          channel_object.value(QStringLiteral("target")).toObject();
      AnimationChannel channel;
      channel.step = interpolation == QStringLiteral("STEP");
      channel.node = target.value(QStringLiteral("node")).toInt(-1);
      QString const path = target.value(QStringLiteral("path")).toString();
      if (path == QStringLiteral("translation")) {
        channel.path = ChannelPath::Translation;
        channel.components = 3;
      } else if (path == QStringLiteral("rotation")) {
        channel.path = ChannelPath::Rotation;
        channel.components = 4;
      } else if (path == QStringLiteral("scale")) {
        channel.path = ChannelPath::Scale;
        channel.components = 3;
      } else {
        result.status.error = "compiled creature package animation path is unsupported";
        return result;
      }
      if (channel.node < 0 || channel.node >= nodes.size() || input.components != 1 ||
          output.components != channel.components || input.count != output.count ||
          input.count == 0U) {
        result.status.error = "compiled creature package animation accessor changed";
        return result;
      }
      channel.times.reserve(input.count);
      channel.values.reserve(input.count *
                             static_cast<std::size_t>(channel.components));
      for (std::size_t key = 0U; key < input.count; ++key) {
        float const time = accessor_float(input, key, 0);
        channel.times.push_back(time);
        clip.duration = std::max(clip.duration, time);
        for (int component = 0; component < channel.components; ++component) {
          channel.values.push_back(accessor_float(output, key, component));
        }
      }
      clip.channels.push_back(std::move(channel));
    }
    result.clips.push_back(std::move(clip));
  }

  result.status.clip_count = result.clips.size();
  result.status.loaded = result.status.vertex_count == config.expected_vertices &&
                         result.status.triangle_count == config.expected_triangles &&
                         result.status.clip_count == config.expected_clips;
  if (!result.status.loaded) {
    result.status.error = "compiled creature package topology or clip counts changed";
  }
  return result;
}

auto source_asset() -> const SourceAsset& {
  static const SourceAsset result = parse_source(k_horse_config);
  return result;
}

auto elephant_asset() -> const SourceAsset& {
  static const SourceAsset result = parse_source(k_elephant_config);
  return result;
}

void sample_channel(AnimationChannel const& channel, float time, NodePose& pose) {
  auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
  std::size_t right = static_cast<std::size_t>(upper - channel.times.begin());
  if (right == 0U) {
    right = 1U;
  }
  if (right >= channel.times.size()) {
    right = channel.times.size() - 1U;
  }
  std::size_t const left = right - 1U;
  float const span = channel.times[right] - channel.times[left];
  float const alpha = channel.step || span <= 1.0e-7F
                          ? 0.0F
                          : std::clamp((time - channel.times[left]) / span, 0.0F, 1.0F);
  auto value = [&](std::size_t key, int component) {
    return channel.values[key * static_cast<std::size_t>(channel.components) +
                          static_cast<std::size_t>(component)];
  };
  if (channel.path == ChannelPath::Rotation) {
    QQuaternion const a(value(left, 3), value(left, 0), value(left, 1), value(left, 2));
    QQuaternion const b(
        value(right, 3), value(right, 0), value(right, 1), value(right, 2));
    pose.rotation = QQuaternion::slerp(a, b, alpha).normalized();
    return;
  }
  QVector3D const a(value(left, 0), value(left, 1), value(left, 2));
  QVector3D const b(value(right, 0), value(right, 1), value(right, 2));
  QVector3D const sampled = a + (b - a) * alpha;
  if (channel.path == ChannelPath::Translation) {
    pose.translation = sampled;
  } else {
    pose.scale = sampled;
  }
}

auto sample_source_clip(SourceAsset const& asset,
                        SourceConfig const& config,
                        std::string_view source_clip,
                        float normalized_phase,
                        std::span<QMatrix4x4> out) noexcept -> bool {
  if (!asset.status.loaded || out.size() < asset.bind_palette.size()) {
    return false;
  }
  auto const clip_it =
      std::find_if(asset.clips.begin(), asset.clips.end(), [&](SourceClip const& clip) {
        return clip.name == source_clip;
      });
  if (clip_it == asset.clips.end()) {
    return false;
  }
  SourceClip const& clip = *clip_it;
  float const phase = std::clamp(normalized_phase, 0.0F, 1.0F);
  float const time = phase * clip.duration;

  thread_local std::vector<NodePose> poses;
  poses.assign(asset.default_poses.begin(), asset.default_poses.end());
  for (AnimationChannel const& channel : clip.channels) {
    sample_channel(channel, time, poses[static_cast<std::size_t>(channel.node)]);
  }
  thread_local std::vector<QMatrix4x4> world;
  evaluate_world_matrices(poses, asset.parents, world);
  for (std::size_t slot = 0U; slot < asset.bind_palette.size(); ++slot) {
    out[slot] = normalize_world_matrix(
        world[static_cast<std::size_t>(asset.joint_nodes[slot])], config);
  }
  return true;
}

void rotate_elephant_subtree(std::span<QMatrix4x4> palette,
                             std::span<const BoneDef> bones,
                             std::size_t root,
                             float degrees) noexcept {
  QVector3D const origin = palette[root].column(3).toVector3D();
  QMatrix4x4 delta;
  delta.translate(origin);
  delta.rotate(degrees, 1.0F, 0.0F, 0.0F);
  delta.translate(-origin);
  for (std::size_t bone = 0U; bone < bones.size(); ++bone) {
    std::size_t cursor = bone;
    while (cursor != root && bones[cursor].parent != Render::Creature::k_invalid_bone) {
      cursor = static_cast<std::size_t>(bones[cursor].parent);
    }
    if (cursor == root) {
      palette[bone] = delta * palette[bone];
    }
  }
}

constexpr float k_elephant_shin_counter_rotation = 0.55F;

auto synthesise_elephant_locomotion(SourceAsset const& asset,
                                    float phase,
                                    bool fast,
                                    std::span<QMatrix4x4> out) noexcept -> bool {
  if (!asset.status.loaded || out.size() < asset.bind_palette.size()) {
    return false;
  }
  std::copy(asset.bind_palette.begin(), asset.bind_palette.end(), out.begin());
  constexpr std::array<std::size_t, 4> upper{{9U, 12U, 15U, 18U}};
  constexpr std::array<std::size_t, 4> lower{{10U, 13U, 16U, 19U}};

  constexpr std::array<float, 4> offset{{0.25F, 0.75F, 0.0F, 0.50F}};
  float const stride_degrees = fast ? 13.0F : 10.5F;
  float const knee_degrees = fast ? 10.0F : 8.0F;
  float const two_pi = 6.2831853071795864769F;
  for (std::size_t leg = 0U; leg < upper.size(); ++leg) {
    float const wave = std::sin((phase + offset[leg]) * two_pi);
    float const thigh = wave * stride_degrees;

    float const shin = (-thigh * k_elephant_shin_counter_rotation) +
                       (std::max(wave, 0.0F) * knee_degrees);
    rotate_elephant_subtree(out, asset.bone_defs, upper[leg], thigh);
    rotate_elephant_subtree(out, asset.bone_defs, lower[leg], shin);
  }
  float const bob = std::sin(phase * two_pi * 2.0F) * (fast ? 0.0225F : 0.0125F);
  float const sway = std::sin(phase * two_pi) * (fast ? 1.8F : 1.0F);
  QMatrix4x4 body_delta;
  body_delta.translate(0.0F, bob, 0.0F);
  QVector3D const root_origin = out[0].column(3).toVector3D();
  body_delta.translate(root_origin);
  body_delta.rotate(sway, 0.0F, 0.0F, 1.0F);
  body_delta.translate(-root_origin);
  for (QMatrix4x4& bone : out.first(asset.bind_palette.size())) {
    bone = body_delta * bone;
  }
  rotate_elephant_subtree(
      out, asset.bone_defs, 4U, std::sin((phase + 0.18F) * two_pi) * 7.0F);
  return true;
}

auto sample_elephant_locomotion(SourceAsset const& asset,
                                float phase,
                                bool fast,
                                std::span<QMatrix4x4> out) noexcept -> bool {
  const std::string_view authored = fast ? "Run" : "Walk";
  if (sample_source_clip(asset, k_elephant_config, authored, phase, out)) {
    return true;
  }
  return synthesise_elephant_locomotion(asset, phase, fast, out);
}

} // namespace

namespace Render::Horse {

auto horse_source_mesh_nodes() noexcept -> std::span<const MeshNode> {
  return source_asset().mesh_nodes;
}

auto horse_source_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  return source_asset().bind_palette;
}

auto horse_source_bone_defs() noexcept -> std::span<const BoneDef> {
  return source_asset().bone_defs;
}

auto horse_source_sample_clip(std::string_view source_clip,
                              float normalized_phase,
                              std::span<QMatrix4x4> out) noexcept -> bool {
  return sample_source_clip(
      source_asset(), k_horse_config, source_clip, normalized_phase, out);
}

auto horse_source_pose_mount_frame(std::string_view source_clip,
                                   float normalized_phase,
                                   Render::GL::MountedAttachmentFrame& frame) noexcept
    -> bool {
  thread_local std::array<QMatrix4x4, k_horse_source_bone_count> pose{};
  if (!horse_source_sample_clip(source_clip, normalized_phase, pose)) {
    return false;
  }

  namespace RCQ = Render::Creature::Quadruped;
  auto const bind = source_asset().bind_palette;
  auto const sockets = Render::Horse::mounted_socket_set();
  auto const back =
      RCQ::bone_delta(pose, bind, static_cast<std::size_t>(sockets.saddle));
  auto const head =
      RCQ::bone_delta(pose, bind, static_cast<std::size_t>(sockets.bridle));

  frame.saddle_center = back.point(frame.saddle_center);
  frame.seat_position = back.point(frame.seat_position);
  frame.stirrup_attach_left = back.point(frame.stirrup_attach_left);
  frame.stirrup_attach_right = back.point(frame.stirrup_attach_right);
  frame.stirrup_bottom_left = back.point(frame.stirrup_bottom_left);
  frame.stirrup_bottom_right = back.point(frame.stirrup_bottom_right);
  frame.seat_forward = back.axis(frame.seat_forward);
  frame.seat_right = back.axis(frame.seat_right);
  frame.seat_up = back.axis(frame.seat_up);

  frame.rein_bit_left = head.point(frame.rein_bit_left);
  frame.rein_bit_right = head.point(frame.rein_bit_right);
  frame.bridle_base = head.point(frame.bridle_base);
  return true;
}

auto horse_source_asset_status() noexcept
    -> const Render::Creature::CompiledCreatureAssetStatus& {
  return source_asset().status;
}

} // namespace Render::Horse

namespace Render::Elephant {

auto elephant_source_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode> {
  return elephant_asset().mesh_nodes;
}

auto elephant_source_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  return elephant_asset().bind_palette;
}

auto elephant_source_bone_defs() noexcept
    -> std::span<const Render::Creature::BoneDef> {
  return elephant_asset().bone_defs;
}

auto elephant_source_sample_clip(std::string_view source_clip,
                                 float normalized_phase,
                                 std::span<QMatrix4x4> out) noexcept -> bool {
  auto const& asset = elephant_asset();
  if (source_clip == "Walk" || source_clip == "Run") {
    return sample_elephant_locomotion(
        asset, normalized_phase, source_clip == "Run", out);
  }
  return sample_source_clip(
      asset, k_elephant_config, source_clip, normalized_phase, out);
}

auto elephant_source_pose_howdah(std::string_view source_clip,
                                 float normalized_phase,
                                 Render::GL::HowdahAttachmentFrame& frame) noexcept
    -> bool {
  std::array<QMatrix4x4, k_elephant_source_bone_count> pose{};
  if (!elephant_source_sample_clip(source_clip, normalized_phase, pose)) {
    return false;
  }
  namespace RCQ = Render::Creature::Quadruped;
  auto const back = RCQ::bone_delta(pose,
                                    elephant_source_bind_palette(),
                                    static_cast<std::size_t>(ElephantBone::MountBack));
  frame.howdah_center = back.point(frame.howdah_center);
  frame.seat_position = back.point(frame.seat_position);
  frame.seat_forward = back.axis(frame.seat_forward);
  frame.seat_right = back.axis(frame.seat_right);
  frame.seat_up = back.axis(frame.seat_up);
  return true;
}

auto elephant_source_asset_status() noexcept
    -> const Render::Creature::CompiledCreatureAssetStatus& {
  static const Render::Creature::CompiledCreatureAssetStatus status = [] {
    auto const& source = elephant_asset().status;
    return Render::Creature::CompiledCreatureAssetStatus{
        .loaded = source.loaded,
        .vertex_count = source.vertex_count,
        .triangle_count = source.triangle_count,
        .joint_count = source.joint_count,
        .clip_count = source.clip_count,
        .error = source.error};
  }();
  return status;
}

} // namespace Render::Elephant
