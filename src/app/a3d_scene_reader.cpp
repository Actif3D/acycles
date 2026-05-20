/* SPDX-FileCopyrightText: 2026 Actif3D
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "app/a3d_scene_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>

#include "scene/attribute.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/geometry.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include "util/colorspace.h"
#include "util/math.h"

#include "util/path.h"
#include "util/transform.h"
#include "util/vector.h"

#include "meshoptimizer.h"

CCL_NAMESPACE_BEGIN

namespace {

struct Json {
  enum Type { Null, Bool, Number, String, Array, Object };

  Type type = Null;
  bool bool_value = false;
  double number_value = 0.0;
  string string_value;
  std::vector<Json> array_value;
  std::map<string, Json> object_value;

  bool is_null() const
  {
    return type == Null;
  }
  bool is_object() const
  {
    return type == Object;
  }
  bool is_array() const
  {
    return type == Array;
  }
  bool is_number() const
  {
    return type == Number;
  }
  bool is_string() const
  {
    return type == String;
  }
  bool is_bool() const
  {
    return type == Bool;
  }

  const Json &get(const string &key) const
  {
    static const Json null_json;
    if (!is_object()) {
      return null_json;
    }
    const auto it = object_value.find(key);
    return (it == object_value.end()) ? null_json : it->second;
  }
};

class JsonParser {
 public:
  explicit JsonParser(const string &text) : text_(text) {}

  Json parse()
  {
    Json value = parse_value();
    skip_ws();
    if (pos_ != text_.size()) {
      fail("unexpected trailing JSON data");
    }
    return value;
  }

 private:
  void skip_ws()
  {
    while (pos_ < text_.size()) {
      const char c = text_[pos_];
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        pos_++;
      }
      else {
        break;
      }
    }
  }

  void fail(const string &message) const
  {
    throw std::runtime_error(string_printf("%s at byte %zu", message.c_str(), pos_));
  }

  bool consume(const char c)
  {
    skip_ws();
    if (pos_ < text_.size() && text_[pos_] == c) {
      pos_++;
      return true;
    }
    return false;
  }

  void expect_literal(const char *literal)
  {
    const size_t len = strlen(literal);
    if (text_.compare(pos_, len, literal) != 0) {
      fail(string("expected ") + literal);
    }
    pos_ += len;
  }

  Json parse_value()
  {
    skip_ws();
    if (pos_ >= text_.size()) {
      fail("unexpected end of JSON");
    }
    const char c = text_[pos_];
    if (c == '{') {
      return parse_object();
    }
    if (c == '[') {
      return parse_array();
    }
    if (c == '"') {
      Json value;
      value.type = Json::String;
      value.string_value = parse_string();
      return value;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parse_number();
    }
    if (text_.compare(pos_, 4, "true") == 0) {
      expect_literal("true");
      Json value;
      value.type = Json::Bool;
      value.bool_value = true;
      return value;
    }
    if (text_.compare(pos_, 5, "false") == 0) {
      expect_literal("false");
      Json value;
      value.type = Json::Bool;
      value.bool_value = false;
      return value;
    }
    if (text_.compare(pos_, 4, "null") == 0) {
      expect_literal("null");
      return Json();
    }
    fail("unexpected JSON token");
    return Json();
  }

  Json parse_object()
  {
    consume('{');
    Json value;
    value.type = Json::Object;
    if (consume('}')) {
      return value;
    }
    while (true) {
      skip_ws();
      if (pos_ >= text_.size() || text_[pos_] != '"') {
        fail("expected object key");
      }
      const string key = parse_string();
      if (!consume(':')) {
        fail("expected ':' after object key");
      }
      value.object_value[key] = parse_value();
      if (consume('}')) {
        return value;
      }
      if (!consume(',')) {
        fail("expected ',' or '}'");
      }
    }
  }

  Json parse_array()
  {
    consume('[');
    Json value;
    value.type = Json::Array;
    if (consume(']')) {
      return value;
    }
    while (true) {
      value.array_value.push_back(parse_value());
      if (consume(']')) {
        return value;
      }
      if (!consume(',')) {
        fail("expected ',' or ']'");
      }
    }
  }

  Json parse_number()
  {
    const size_t start = pos_;
    if (text_[pos_] == '-') {
      pos_++;
    }
    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
      pos_++;
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      pos_++;
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
        pos_++;
      }
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      pos_++;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
        pos_++;
      }
      while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
        pos_++;
      }
    }

    Json value;
    value.type = Json::Number;
    value.number_value = atof(text_.substr(start, pos_ - start).c_str());
    return value;
  }

  string parse_string()
  {
    if (pos_ >= text_.size() || text_[pos_] != '"') {
      fail("expected string");
    }
    pos_++;
    string result;
    while (pos_ < text_.size()) {
      const char c = text_[pos_++];
      if (c == '"') {
        return result;
      }
      if (c == '\\') {
        if (pos_ >= text_.size()) {
          fail("unterminated string escape");
        }
        const char e = text_[pos_++];
        switch (e) {
          case '"':
          case '\\':
          case '/':
            result += e;
            break;
          case 'b':
            result += '\b';
            break;
          case 'f':
            result += '\f';
            break;
          case 'n':
            result += '\n';
            break;
          case 'r':
            result += '\r';
            break;
          case 't':
            result += '\t';
            break;
          case 'u':
            if (pos_ + 4 > text_.size()) {
              fail("short unicode escape");
            }
            result += '?';
            pos_ += 4;
            break;
          default:
            fail("invalid string escape");
        }
      }
      else {
        result += c;
      }
    }
    fail("unterminated string");
    return result;
  }

  const string &text_;
  size_t pos_ = 0;
};

float json_float(const Json &value, const float fallback)
{
  return value.is_number() ? float(value.number_value) : fallback;
}

bool json_bool(const Json &value, const bool fallback)
{
  return value.is_bool() ? value.bool_value : fallback;
}

string json_string(const Json &value, const string &fallback = "")
{
  return value.is_string() ? value.string_value : fallback;
}

float3 json_float3(const Json &value, const float3 fallback)
{
  if (!value.is_array() || value.array_value.size() < 3) {
    return fallback;
  }
  return make_float3(json_float(value.array_value[0], fallback.x),
                     json_float(value.array_value[1], fallback.y),
                     json_float(value.array_value[2], fallback.z));
}

bool read_text(const string &path, string &text, string *error)
{
  if (!path_read_text(path, text)) {
    *error = "Unable to read " + path;
    return false;
  }
  return true;
}

bool read_buffer(const string &root, const string &name, vector<uint8_t> &data, string *error)
{
  const string path = path_join(root, name);
  if (path_read_binary(path, data)) {
    return true;
  }
  if (path_read_compressed_binary(path + ".gz", data)) {
    return true;
  }
  *error = "Unable to read required Asset3D buffer " + path;
  return false;
}

bool read_optional_buffer(const string &root, const string &name, vector<uint8_t> &data)
{
  const string path = path_join(root, name);
  return path_read_binary(path, data) || path_read_compressed_binary(path + ".gz", data);
}

template<typename T> T read_pod(const vector<uint8_t> &data, const size_t offset)
{
  T value;
  memcpy(&value, data.data() + offset, sizeof(T));
  return value;
}

struct MeshProps {
  int node_id = 0;
  int material_id = -1;
  bool use_faces16 = false;
  int face_byte_offset = 0;
  int face_cnt = 0;
  int vertex_byte_offset = 0;
  int vertex_cnt = 0;
  int normal_byte_offset = -1;
  int uv0_byte_offset = -1;
  int transform_byte_offset = -1;
  float quant_vertex_range = 1.0f;
  int quant_vertex_max = 1;
  float quant_uv0_range = 1.0f;
  int quant_uv0_max = 0;
};

bool parse_meshes(const vector<uint8_t> &meshes,
                  std::vector<MeshProps> &props,
                  int *version,
                  string *error)
{
  if (meshes.size() < sizeof(int32_t) * 2) {
    *error = "meshes.buf is too small";
    return false;
  }

  *version = read_pod<int32_t>(meshes, 0);
  const int stride = read_pod<int32_t>(meshes, 4);
  if (stride < 21) {
    *error = "meshes.buf has unsupported stride";
    return false;
  }

  const size_t count = (meshes.size() / sizeof(int32_t) - 2) / stride;
  props.reserve(count);
  for (size_t i = 0; i < count; i++) {
    const size_t base = sizeof(int32_t) * (2 + i * stride);
    MeshProps mp;
    mp.node_id = read_pod<int32_t>(meshes, base + 0 * sizeof(int32_t));
    mp.material_id = read_pod<int32_t>(meshes, base + 1 * sizeof(int32_t));
    mp.use_faces16 = read_pod<int32_t>(meshes, base + 2 * sizeof(int32_t)) == 1;
    mp.face_byte_offset = read_pod<int32_t>(meshes, base + 3 * sizeof(int32_t));
    mp.face_cnt = read_pod<int32_t>(meshes, base + 4 * sizeof(int32_t));
    mp.vertex_byte_offset = read_pod<int32_t>(meshes, base + 5 * sizeof(int32_t));
    mp.vertex_cnt = read_pod<int32_t>(meshes, base + 6 * sizeof(int32_t));
    mp.normal_byte_offset = read_pod<int32_t>(meshes, base + 7 * sizeof(int32_t));
    mp.uv0_byte_offset = read_pod<int32_t>(meshes, base + 8 * sizeof(int32_t));
    mp.transform_byte_offset = read_pod<int32_t>(meshes, base + 15 * sizeof(int32_t));
    mp.quant_vertex_range = read_pod<float>(meshes, base + 17 * sizeof(int32_t));
    mp.quant_vertex_max = read_pod<int32_t>(meshes, base + 18 * sizeof(int32_t));
    mp.quant_uv0_range = read_pod<float>(meshes, base + 19 * sizeof(int32_t));
    mp.quant_uv0_max = read_pod<int32_t>(meshes, base + 20 * sizeof(int32_t));
    props.push_back(mp);
  }
  return true;
}

bool decode_position(const MeshProps &mp,
                     const vector<uint8_t> &vertices,
                     const int index,
                     float3 *position)
{
  const float scale = mp.quant_vertex_range / max(mp.quant_vertex_max, 1);
  const size_t comp_size = (mp.quant_vertex_max <= 127) ? 1 :
                           (mp.quant_vertex_max <= 32767) ? 2 :
                                                             4;
  const size_t offset = size_t(mp.vertex_byte_offset) + size_t(index) * 3 * comp_size;
  if (offset + comp_size * 3 > vertices.size()) {
    return false;
  }

  float values[3];
  for (int c = 0; c < 3; c++) {
    if (comp_size == 1) {
      values[c] = float(read_pod<int8_t>(vertices, offset + c));
    }
    else if (comp_size == 2) {
      values[c] = float(read_pod<int16_t>(vertices, offset + c * 2));
    }
    else {
      values[c] = float(read_pod<int32_t>(vertices, offset + c * 4));
    }
  }
  *position = make_float3(values[0] * scale, values[1] * scale, values[2] * scale);
  return true;
}

bool decode_normal(const MeshProps &mp,
                   const vector<uint8_t> &normals,
                   const int index,
                   float3 *normal)
{
  if (mp.normal_byte_offset < 0) {
    return false;
  }
  const size_t offset = size_t(mp.normal_byte_offset) + size_t(index) * 2;
  if (offset + 2 > normals.size()) {
    return false;
  }
  const float raw0 = float(read_pod<int8_t>(normals, offset));
  const float raw1 = float(read_pod<int8_t>(normals, offset + 1));
  const float theta = (raw0 / 254.0f + 0.5f) * M_PI_F;
  const float phi = raw1 * (M_PI_F / 127.0f);
  const float sin_theta = sinf(theta);
  *normal = normalize(make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cosf(theta)));
  return true;
}

bool decode_uv0(const MeshProps &mp, const vector<uint8_t> &uvs0, const int index, float2 *uv)
{
  if (mp.uv0_byte_offset < 0) {
    return false;
  }
  if (mp.quant_uv0_max == 0) {
    const size_t offset = size_t(mp.uv0_byte_offset) + size_t(index) * sizeof(float) * 2;
    if (offset + sizeof(float) * 2 > uvs0.size()) {
      return false;
    }
    *uv = make_float2(read_pod<float>(uvs0, offset), read_pod<float>(uvs0, offset + sizeof(float)));
    return true;
  }
  const size_t offset = size_t(mp.uv0_byte_offset) + size_t(index) * sizeof(uint16_t) * 2;
  if (offset + sizeof(uint16_t) * 2 > uvs0.size()) {
    return false;
  }
  const float scale = mp.quant_uv0_range / max(mp.quant_uv0_max, 1);
  *uv = make_float2(float(read_pod<uint16_t>(uvs0, offset)) * scale,
                    float(read_pod<uint16_t>(uvs0, offset + sizeof(uint16_t))) * scale);
  return true;
}

struct FaceRange {
  size_t offset = 0;
  int face_cnt = 0;
  bool use_faces16 = false;
  int node_id = 0;
};

bool append_face_range(std::vector<FaceRange> &ranges, const MeshProps &mp)
{
  if (mp.face_cnt <= 0) {
    return true;
  }
  FaceRange range;
  range.offset = size_t(mp.face_byte_offset) / (mp.use_faces16 ? sizeof(uint16_t) : sizeof(uint32_t));
  range.face_cnt = mp.face_cnt;
  range.use_faces16 = mp.use_faces16;
  range.node_id = mp.node_id;
  ranges.push_back(range);
  return true;
}

void size_face_buffers(const std::vector<MeshProps> &mesh_props, size_t *faces16_size, size_t *faces32_size)
{
  *faces16_size = 0;
  *faces32_size = 0;
  for (const MeshProps &mp : mesh_props) {
    if (mp.face_cnt <= 0) {
      continue;
    }
    const size_t index_count = size_t(mp.face_cnt) * 3;
    if (mp.use_faces16) {
      *faces16_size = max(*faces16_size, size_t(mp.face_byte_offset) + index_count * sizeof(uint16_t));
    }
    else {
      *faces32_size = max(*faces32_size, size_t(mp.face_byte_offset) + index_count * sizeof(uint32_t));
    }
  }
}

bool copy_decoded_indices(const FaceRange &range,
                          const std::vector<uint32_t> &decoded,
                          vector<uint8_t> &faces16,
                          vector<uint8_t> &faces32,
                          string *error)
{
  const size_t index_count = size_t(range.face_cnt) * 3;
  if (range.use_faces16) {
    const size_t byte_offset = range.offset * sizeof(uint16_t);
    for (size_t i = 0; i < index_count; i++) {
      const uint32_t value = decoded[i];
      if (value > 0xffffu) {
        *error = string_printf("Decoded faces16 index out of range in mesh node_id=%d", range.node_id);
        return false;
      }
      const uint16_t packed = uint16_t(value);
      memcpy(faces16.data() + byte_offset + i * sizeof(uint16_t), &packed, sizeof(uint16_t));
    }
  }
  else {
    const size_t byte_offset = range.offset * sizeof(uint32_t);
    memcpy(faces32.data() + byte_offset, decoded.data(), index_count * sizeof(uint32_t));
  }
  return true;
}

bool decode_meshopt_segmented_faces(const vector<uint8_t> &encoded_faces,
                                    const std::vector<MeshProps> &mesh_props,
                                    vector<uint8_t> &faces16,
                                    vector<uint8_t> &faces32,
                                    string *error)
{
  std::vector<FaceRange> ranges16, ranges32;
  for (const MeshProps &mp : mesh_props) {
    append_face_range(mp.use_faces16 ? ranges16 : ranges32, mp);
  }

  const auto sort_by_offset = [](const FaceRange &a, const FaceRange &b) { return a.offset < b.offset; };
  std::sort(ranges16.begin(), ranges16.end(), sort_by_offset);
  std::sort(ranges32.begin(), ranges32.end(), sort_by_offset);

  size_t faces16_size = 0, faces32_size = 0;
  size_face_buffers(mesh_props, &faces16_size, &faces32_size);
  faces16.assign(faces16_size, 0);
  faces32.assign(faces32_size, 0);

  size_t cursor = 0;
  bool have_last = false;
  FaceRange last_range;
  std::vector<FaceRange> all_ranges = ranges16;
  all_ranges.insert(all_ranges.end(), ranges32.begin(), ranges32.end());

  for (const FaceRange &range : all_ranges) {
    if (range.face_cnt == 0) {
      continue;
    }
    if (have_last && last_range.offset == range.offset && last_range.use_faces16 == range.use_faces16) {
      continue;
    }
    have_last = true;
    last_range = range;

    if (cursor + sizeof(uint32_t) > encoded_faces.size()) {
      *error = "Segmented meshopt faces.buf ended before segment length";
      return false;
    }
    const uint32_t byte_length = read_pod<uint32_t>(encoded_faces, cursor);
    cursor += sizeof(uint32_t);
    if (cursor + byte_length > encoded_faces.size()) {
      *error = "Segmented meshopt faces.buf segment exceeds buffer size";
      return false;
    }

    const size_t index_count = size_t(range.face_cnt) * 3;
    std::vector<uint32_t> decoded(index_count);
    const int rc = meshopt_decodeIndexBuffer(
        decoded.data(), index_count, sizeof(uint32_t), encoded_faces.data() + cursor, byte_length);
    if (rc != 0) {
      *error = string_printf(
          "meshopt_decodeIndexBuffer failed for faces.buf segment node_id=%d with code %d",
          range.node_id,
          rc);
      return false;
    }
    cursor += byte_length;

    if (!copy_decoded_indices(range, decoded, faces16, faces32, error)) {
      return false;
    }
  }

  return true;
}

bool decode_meshopt_single_stream_faces(const vector<uint8_t> &encoded_faces,
                                        const std::vector<MeshProps> &mesh_props,
                                        vector<uint8_t> &faces16,
                                        vector<uint8_t> &faces32,
                                        string *error)
{
  size_t total_indices = 0;
  size_t faces16_size = 0, faces32_size = 0;
  size_face_buffers(mesh_props, &faces16_size, &faces32_size);
  for (const MeshProps &mp : mesh_props) {
    total_indices += size_t(max(mp.face_cnt, 0)) * 3;
  }

  std::vector<uint32_t> decoded(total_indices);
  const int rc = meshopt_decodeIndexBuffer(
      decoded.data(), total_indices, sizeof(uint32_t), encoded_faces.data(), encoded_faces.size());
  if (rc != 0) {
    *error = string_printf("meshopt_decodeIndexBuffer failed for faces.buf with code %d", rc);
    return false;
  }

  faces16.assign(faces16_size, 0);
  faces32.assign(faces32_size, 0);

  size_t decoded_offset = 0;
  for (const MeshProps &mp : mesh_props) {
    const size_t index_count = size_t(max(mp.face_cnt, 0)) * 3;
    if (index_count == 0) {
      continue;
    }
    FaceRange range;
    range.offset = size_t(mp.face_byte_offset) / (mp.use_faces16 ? sizeof(uint16_t) : sizeof(uint32_t));
    range.face_cnt = mp.face_cnt;
    range.use_faces16 = mp.use_faces16;
    range.node_id = mp.node_id;
    std::vector<uint32_t> segment(decoded.begin() + decoded_offset, decoded.begin() + decoded_offset + index_count);
    if (!copy_decoded_indices(range, segment, faces16, faces32, error)) {
      return false;
    }
    decoded_offset += index_count;
  }

  return true;
}

bool decode_meshopt_faces(const vector<uint8_t> &encoded_faces,
                          const std::vector<MeshProps> &mesh_props,
                          vector<uint8_t> &faces16,
                          vector<uint8_t> &faces32,
                          string *error)
{
  string segmented_error;
  if (decode_meshopt_segmented_faces(encoded_faces, mesh_props, faces16, faces32, &segmented_error)) {
    return true;
  }

  string single_stream_error;
  if (decode_meshopt_single_stream_faces(
          encoded_faces, mesh_props, faces16, faces32, &single_stream_error))
  {
    return true;
  }

  *error = segmented_error + "; " + single_stream_error;
  return false;
}

bool decode_index(const MeshProps &mp,
                  const vector<uint8_t> &faces16,
                  const vector<uint8_t> &faces32,
                  const int corner,
                  int *index)
{
  if (mp.use_faces16) {
    const size_t offset = size_t(mp.face_byte_offset) + size_t(corner) * sizeof(uint16_t);
    if (offset + sizeof(uint16_t) > faces16.size()) {
      return false;
    }
    *index = int(read_pod<uint16_t>(faces16, offset));
    return true;
  }
  const size_t offset = size_t(mp.face_byte_offset) + size_t(corner) * sizeof(uint32_t);
  if (offset + sizeof(uint32_t) > faces32.size()) {
    return false;
  }
  *index = int(read_pod<uint32_t>(faces32, offset));
  return true;
}

Transform mesh_transform(const MeshProps &mp, const vector<uint8_t> &transforms)
{
  if (mp.transform_byte_offset < 0 ||
      size_t(mp.transform_byte_offset) + sizeof(float) * 16 > transforms.size())
  {
    return transform_identity();
  }
  const size_t o = size_t(mp.transform_byte_offset);
  return make_transform(read_pod<float>(transforms, o + 0 * sizeof(float)),
                        read_pod<float>(transforms, o + 1 * sizeof(float)),
                        read_pod<float>(transforms, o + 2 * sizeof(float)),
                        read_pod<float>(transforms, o + 3 * sizeof(float)),
                        read_pod<float>(transforms, o + 4 * sizeof(float)),
                        read_pod<float>(transforms, o + 5 * sizeof(float)),
                        read_pod<float>(transforms, o + 6 * sizeof(float)),
                        read_pod<float>(transforms, o + 7 * sizeof(float)),
                        read_pod<float>(transforms, o + 8 * sizeof(float)),
                        read_pod<float>(transforms, o + 9 * sizeof(float)),
                        read_pod<float>(transforms, o + 10 * sizeof(float)),
                        read_pod<float>(transforms, o + 11 * sizeof(float)));
}

float3 transform_direction(const Transform &tfm, const float3 v)
{
  return normalize(make_float3(tfm.x.x * v.x + tfm.x.y * v.y + tfm.x.z * v.z,
                               tfm.y.x * v.x + tfm.y.y * v.y + tfm.y.z * v.z,
                               tfm.z.x * v.x + tfm.z.y * v.y + tfm.z.z * v.z));
}

string resolve_texture_path(const string &root, const Json &texture)
{
  if (!texture.is_object()) {
    return "";
  }
  const string url = json_string(texture.get("url"));
  if (!url.empty()) {
    return url;
  }
  const string id = json_string(texture.get("id"));
  const string std_ext = json_string(texture.get("stdExt"));
  if (id.empty() || std_ext.empty()) {
    return "";
  }
  const Json &formats = texture.get("webFormats");
  if (formats.is_array()) {
    for (const Json &format : formats.array_value) {
      const string format_name = json_string(format);
      if (format_name == "large/std" || format_name == "small/std") {
        const string path = path_join(path_join(path_join(root, "img"), format_name),
                                      id + "." + std_ext);
        if (path_exists(path)) {
          return path;
        }
      }
    }
  }
  const string large_std = path_join(path_join(path_join(root, "img"), "large/std"), id + "." + std_ext);
  if (path_exists(large_std)) {
    return large_std;
  }
  const string raw_ext = json_string(texture.get("rawExt"));
  if (!raw_ext.empty()) {
    const string raw_path = path_join(root, id + "." + raw_ext);
    if (path_exists(raw_path)) {
      return raw_path;
    }
  }
  return "";
}

Shader *create_material_shader(Scene *scene, const Json &mat, const string &root, const int index)
{
  Shader *shader = scene->create_node<Shader>();
  shader->name = ustring(json_string(mat.get("name"), string_printf("a3d_material_%d", index).c_str()));

  auto graph = make_unique<ShaderGraph>();
  PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
  principled->set_base_color(json_float3(mat.get("baseColor"), make_float3(0.8f, 0.8f, 0.8f)));
  principled->set_roughness(json_float(mat.get("roughness"), 0.5f));
  principled->set_metallic(json_float(mat.get("metallic"), 0.0f));
  principled->set_alpha(json_float(mat.get("opacity"), 1.0f));
  principled->set_emission_color(json_float3(mat.get("emissive"), make_float3(0.0f, 0.0f, 0.0f)));
  principled->set_emission_strength(json_float(mat.get("emissionStrength"), 0.0f));

  const string base_color_path = resolve_texture_path(root, mat.get("baseColorTexture"));
  if (!base_color_path.empty()) {
    ImageTextureNode *image = graph->create_node<ImageTextureNode>();
    image->set_filename(ustring(base_color_path));
    image->set_colorspace(u_colorspace_srgb);
    graph->connect(image->output("Color"), principled->input("Base Color"));
    graph->connect(image->output("Alpha"), principled->input("Alpha"));
  }

  const string roughness_path = resolve_texture_path(root, mat.get("roughnessTexture"));
  if (!roughness_path.empty()) {
    ImageTextureNode *image = graph->create_node<ImageTextureNode>();
    image->set_filename(ustring(roughness_path));
    image->set_colorspace(u_colorspace_data);
    graph->connect(image->output("Color"), principled->input("Roughness"));
  }

  const string metallic_path = resolve_texture_path(root, mat.get("metallicTexture"));
  if (!metallic_path.empty()) {
    ImageTextureNode *image = graph->create_node<ImageTextureNode>();
    image->set_filename(ustring(metallic_path));
    image->set_colorspace(u_colorspace_data);
    graph->connect(image->output("Color"), principled->input("Metallic"));
  }

  const string normal_path = resolve_texture_path(root, mat.get("normalTexture"));
  if (!normal_path.empty()) {
    ImageTextureNode *image = graph->create_node<ImageTextureNode>();
    image->set_filename(ustring(normal_path));
    image->set_colorspace(u_colorspace_data);
    NormalMapNode *normal = graph->create_node<NormalMapNode>();
    normal->set_strength(json_float(mat.get("normalScale"), 1.0f));
    graph->connect(image->output("Color"), normal->input("Color"));
    graph->connect(normal->output("Normal"), principled->input("Normal"));
  }

  graph->connect(principled->output("BSDF"), graph->output()->input("Surface"));
  shader->set_graph(std::move(graph));
  shader->tag_update(scene);
  return shader;
}

void create_default_background(Scene *scene, const A3DSceneReaderOptions &options)
{
  Shader *shader = scene->default_background;
  auto graph = make_unique<ShaderGraph>();
  BackgroundNode *background = graph->create_node<BackgroundNode>();
  background->set_color(options.has_bg_color ? options.bg_color : make_float3(0.05f, 0.05f, 0.05f));
  background->set_strength(options.has_bg_strength ? options.bg_strength : 1.0f);
  graph->connect(background->output("Background"), graph->output()->input("Surface"));
  shader->set_graph(std::move(graph));
  shader->tag_update(scene);
}

Transform look_transform(const float3 position,
                         const float yaw_deg,
                         const float pitch_deg,
                         const float roll_deg)
{
  const float yaw = yaw_deg * M_PI_F / 180.0f;
  const float pitch = pitch_deg * M_PI_F / 180.0f;
  const float roll = roll_deg * M_PI_F / 180.0f;

  float3 forward = make_float3(-sinf(yaw) * cosf(pitch), cosf(yaw) * cosf(pitch), sinf(pitch));
  forward = normalize(forward);
  float3 up = make_float3(0.0f, 0.0f, 1.0f);
  if (fabsf(dot(up, forward)) > 0.999f) {
    up = make_float3(0.0f, 1.0f, 0.0f);
  }
  float3 right = normalize(cross(forward, up));
  up = normalize(cross(right, forward));
  if (roll != 0.0f) {
    right = normalize(right * cosf(roll) + up * sinf(roll));
    up = normalize(cross(right, forward));
  }

  return make_transform(right.x,
                        up.x,
                        forward.x,
                        position.x,
                        right.y,
                        up.y,
                        forward.y,
                        position.y,
                        right.z,
                        up.z,
                        forward.z,
                        position.z);
}

void setup_camera(Scene *scene, const Json &scene_json, const A3DSceneReaderOptions &options)
{
  float3 position = make_float3(0.0f, -5.0f, 2.0f);
  float yaw = 0.0f, pitch = -15.0f, roll = 0.0f, fov = 60.0f;

  const Json &views = scene_json.get("views");
  if (views.is_array() && !views.array_value.empty()) {
    const Json &view = views.array_value[0];
    position = json_float3(view.get("position"), position);
    const Json &rotation = view.get("rotation");
    if (rotation.is_array()) {
      yaw = json_float(rotation.array_value[0], yaw);
      pitch = rotation.array_value.size() > 1 ? json_float(rotation.array_value[1], pitch) : pitch;
      roll = rotation.array_value.size() > 2 ? json_float(rotation.array_value[2], roll) : roll;
    }
    fov = json_float(view.get("fov"), fov);
  }

  const Json &camera = scene_json.get("camera");
  if (camera.is_object()) {
    position = json_float3(camera.get("position"), position);
    const Json &rotation = camera.get("rotation");
    if (rotation.is_array()) {
      yaw = json_float(rotation.array_value[0], yaw);
      pitch = rotation.array_value.size() > 1 ? json_float(rotation.array_value[1], pitch) : pitch;
      roll = rotation.array_value.size() > 2 ? json_float(rotation.array_value[2], roll) : roll;
    }
    fov = json_float(camera.get("fov"), fov);
  }

  if (options.has_camera_position) {
    position = options.camera_position;
  }
  if (options.has_camera_rotation) {
    yaw = options.camera_yaw;
    pitch = options.camera_pitch;
    roll = options.camera_roll;
  }
  if (options.has_camera_fov) {
    fov = options.camera_fov;
  }

  Camera *cam = scene->camera;
  cam->set_camera_type(CAMERA_PERSPECTIVE);
  cam->set_matrix(look_transform(position, yaw, pitch, roll));
  cam->set_fov(fov * M_PI_F / 180.0f);
  cam->set_nearclip(0.01f);
  cam->set_farclip(100000.0f);
  cam->need_flags_update = true;
  cam->need_device_update = true;
}

void add_lights(Scene *scene, const Json &scene_json)
{
  const Json &lights = scene_json.get("lights");
  if (!lights.is_array()) {
    return;
  }

  for (const Json &light_json : lights.array_value) {
    if (!json_bool(light_json.get("enabled"), true) || json_bool(light_json.get("doNotImport"), false)) {
      continue;
    }
    const string type = json_string(light_json.get("type"), "point");
    const float3 color = json_float3(light_json.get("color"), make_float3(1.0f, 1.0f, 1.0f));
    const float strength = json_float(light_json.get("strength"), 1.0f);
    const Json &instances = light_json.get("instances");
    const std::vector<Json> default_instances = {Json()};
    const std::vector<Json> &instance_values = instances.is_array() ? instances.array_value : default_instances;

    for (const Json &instance : instance_values) {
      Light *light = nullptr;
      if (type == "sun") {
        SunLight *sun = scene->create_node<SunLight>();
        sun->set_angle(json_float(light_json.get("angle"), 0.00918f));
        light = sun;
      }
      else if (type == "spot") {
        SpotLight *spot = scene->create_node<SpotLight>();
        spot->set_angle(json_float(light_json.get("angle"), 45.0f) * M_PI_F / 180.0f);
        spot->set_smooth(0.15f);
        spot->set_radius(json_float(light_json.get("size"), 0.0f));
        light = spot;
      }
      else if (type == "area") {
        AreaLight *area = scene->create_node<AreaLight>();
        const float size = json_float(light_json.get("size"), 1.0f);
        area->set_sizeu(json_float(light_json.get("width"), size));
        area->set_sizev(json_float(light_json.get("height"), size));
        light = area;
      }
      else {
        PointLight *point = scene->create_node<PointLight>();
        point->set_radius(json_float(light_json.get("size"), 0.0f));
        light = point;
      }

      light->set_strength(color * strength);

      const float3 position = json_float3(instance.get("position"), make_float3(0.0f, 0.0f, 0.0f));
      float yaw = 0.0f, pitch = -90.0f, roll = 0.0f;
      const Json &rotation = instance.get("rotation");
      if (rotation.is_array()) {
        yaw = json_float(rotation.array_value[0], yaw);
        pitch = rotation.array_value.size() > 1 ? json_float(rotation.array_value[1], pitch) : pitch;
        roll = rotation.array_value.size() > 2 ? json_float(rotation.array_value[2], roll) : roll;
      }

      Object *object = scene->create_node<Object>();
      object->set_geometry(light);
      object->set_tfm(look_transform(position, yaw, pitch, roll));
    }
  }
}

}  // namespace

bool a3d_read_scene(Scene *scene,
                    const string &scene_root,
                    const A3DSceneReaderOptions &options,
                    string *error)
{
  try {
    if (!path_is_directory(scene_root)) {
      *error = "Asset3D scene path is not a directory: " + scene_root;
      return false;
    }

    string scene_text;
    if (!read_text(path_join(scene_root, "scene.json"), scene_text, error)) {
      return false;
    }
    const Json scene_json = JsonParser(scene_text).parse();
    if (!scene_json.is_object()) {
      *error = "scene.json root must be an object";
      return false;
    }

    create_default_background(scene, options);
    setup_camera(scene, scene_json, options);

    std::vector<Shader *> shaders;
    std::map<int, int> material_id_to_shader;
    const Json &materials = scene_json.get("materials");
    if (materials.is_array()) {
      for (size_t i = 0; i < materials.array_value.size(); i++) {
        Shader *shader = create_material_shader(scene, materials.array_value[i], scene_root, int(i));
        shaders.push_back(shader);
        const Json &id = materials.array_value[i].get("id");
        if (id.is_number()) {
          material_id_to_shader[int(id.number_value)] = int(i);
        }
      }
    }

    if (shaders.empty()) {
      Json default_material;
      default_material.type = Json::Object;
      shaders.push_back(create_material_shader(scene, default_material, scene_root, 0));
    }

    vector<uint8_t> meshes_buf, vertices_buf, normals_buf, uvs0_buf, transforms_buf;
    if (!read_buffer(scene_root, "meshes.buf", meshes_buf, error) ||
        !read_buffer(scene_root, "vertices.buf", vertices_buf, error) ||
        !read_buffer(scene_root, "normals.buf", normals_buf, error) ||
        !read_buffer(scene_root, "uvs0.buf", uvs0_buf, error) ||
        !read_buffer(scene_root, "transforms.buf", transforms_buf, error))
    {
      return false;
    }

    int mesh_version = 0;
    std::vector<MeshProps> mesh_props;
    if (!parse_meshes(meshes_buf, mesh_props, &mesh_version, error)) {
      return false;
    }

    vector<uint8_t> faces16_buf, faces32_buf;
    if (mesh_version >= 3) {
      vector<uint8_t> encoded_faces;
      if (!read_buffer(scene_root, "faces.buf", encoded_faces, error)) {
        return false;
      }
      string decode_error;
      if (!decode_meshopt_faces(encoded_faces, mesh_props, faces16_buf, faces32_buf, &decode_error)) {
        const bool has_faces16 = read_optional_buffer(scene_root, "faces16.buf", faces16_buf);
        const bool has_faces32 = read_optional_buffer(scene_root, "faces.buf", faces32_buf);
        if (!(has_faces16 && has_faces32)) {
          *error = decode_error;
          return false;
        }
      }
    }
    else {
      if (!read_buffer(scene_root, "faces16.buf", faces16_buf, error) ||
          !read_buffer(scene_root, "faces.buf", faces32_buf, error))
      {
        return false;
      }
    }

    int created_meshes = 0;
    for (const MeshProps &mp : mesh_props) {
      if (mp.face_cnt <= 0 || mp.vertex_cnt <= 0) {
        continue;
      }

      vector<float3> verts;
      vector<float3> normals;
      vector<float2> uvs;
      verts.reserve(size_t(mp.face_cnt) * 3);
      normals.reserve(size_t(mp.face_cnt) * 3);
      uvs.reserve(size_t(mp.face_cnt) * 3);

      const Transform tfm = mesh_transform(mp, transforms_buf);
      bool has_normals = true;
      bool has_uvs = true;

      for (int corner = 0; corner < mp.face_cnt * 3; corner++) {
        int source_index = 0;
        if (!decode_index(mp, faces16_buf, faces32_buf, corner, &source_index) ||
            source_index < 0 || source_index >= mp.vertex_cnt)
        {
          *error = string_printf("Invalid face index in mesh node_id=%d", mp.node_id);
          return false;
        }

        float3 position;
        if (!decode_position(mp, vertices_buf, source_index, &position)) {
          *error = string_printf("vertices.buf out of range for mesh node_id=%d", mp.node_id);
          return false;
        }
        verts.push_back(transform_point(&tfm, position));

        float3 normal;
        if (decode_normal(mp, normals_buf, source_index, &normal)) {
          normals.push_back(transform_direction(tfm, normal));
        }
        else {
          has_normals = false;
        }

        float2 uv;
        if (decode_uv0(mp, uvs0_buf, source_index, &uv)) {
          uvs.push_back(uv);
        }
        else {
          has_uvs = false;
        }
      }

      Mesh *mesh = scene->create_node<Mesh>();
      array<float3> mesh_verts;
      mesh_verts.resize(verts.size());
      for (size_t i = 0; i < verts.size(); i++) {
        mesh_verts[i] = verts[i];
      }
      mesh->set_verts(mesh_verts);
      mesh->resize_mesh(verts.size(), mp.face_cnt);
      int *triangles = mesh->get_triangles().data();
      for (int tri = 0; tri < mp.face_cnt; tri++) {
        triangles[tri * 3 + 0] = tri * 3 + 0;
        triangles[tri * 3 + 1] = tri * 3 + 1;
        triangles[tri * 3 + 2] = tri * 3 + 2;
      }

      const auto shader_it = material_id_to_shader.find(mp.material_id);
      const int shader_index = (shader_it != material_id_to_shader.end()) ? shader_it->second :
                               (mp.material_id >= 0 && mp.material_id < int(shaders.size())) ?
                                   mp.material_id :
                                   0;
      array<Node *> used_shaders;
      used_shaders.push_back_slow(shaders[shader_index]);
      mesh->set_used_shaders(used_shaders);
      std::ranges::fill(mesh->get_shader(), 0);
      std::ranges::fill(mesh->get_smooth(), has_normals);

      if (has_normals && normals.size() == verts.size()) {
        Attribute *attr = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);
        packed_normal *dst = attr->data_normal_for_write();
        for (const float3 &normal : normals) {
          *dst++ = packed_normal(normal);
        }
      }

      if (has_uvs && uvs.size() == verts.size()) {
        Attribute *attr = mesh->attributes.add(ATTR_STD_UV);
        float2 *dst = attr->data_float2_for_write();
        for (const float2 &uv : uvs) {
          *dst++ = uv;
        }
      }

      mesh->tag_triangles_modified();
      mesh->tag_shader_modified();
      mesh->tag_smooth_modified();

      Object *object = scene->create_node<Object>();
      object->set_geometry(mesh);
      object->set_tfm(transform_identity());
      created_meshes++;
    }

    if (created_meshes == 0) {
      *error = "Asset3D scene contains no renderable mesh data";
      return false;
    }

    add_lights(scene, scene_json);
    return true;
  }
  catch (const std::exception &ex) {
    *error = string("Failed to read Asset3D scene: ") + ex.what();
    return false;
  }
}

CCL_NAMESPACE_END
