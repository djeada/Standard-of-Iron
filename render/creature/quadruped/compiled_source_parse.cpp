#include "render/creature/quadruped/compiled_source_assets_internal.h"

namespace Render::Creature::Quadruped::compiled_source_detail {

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
                   int component) noexcept -> std::uint32_t {
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
    QMatrix4x4 const local_normal_matrix = mesh_from_world.inverted().transposed();
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

      QVector3D const scaled_normal(source_normal.x() / config.scale_x,
                                    source_normal.y() / config.scale_y,
                                    source_normal.z() / config.scale_z);
      QVector3D local_normal = local_normal_matrix.mapVector(scaled_normal);
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

} // namespace Render::Creature::Quadruped::compiled_source_detail
