/* SPDX-FileCopyrightText: 2026 Actif3D
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class Scene;

struct A3DSceneReaderOptions {
  bool has_camera_position = false;
  float3 camera_position = make_float3(0.0f, 0.0f, -5.0f);

  bool has_camera_rotation = false;
  float camera_yaw = 0.0f;
  float camera_pitch = 0.0f;
  float camera_roll = 0.0f;

  bool has_camera_fov = false;
  float camera_fov = 60.0f;

  bool has_bg_strength = false;
  float bg_strength = 1.0f;

  bool has_bg_color = false;
  float3 bg_color = make_float3(0.05f, 0.05f, 0.05f);
};

struct A3DRenderSettings {
  A3DSceneReaderOptions scene_options;

  bool has_flood_dark_limit = false;
  float flood_dark_limit = 0.0f;

  bool has_use_oidn_denoiser = false;
  bool use_oidn_denoiser = false;

  bool has_use_post_process_filters = false;
  bool use_post_process_filters = true;
};

bool a3d_read_render_settings(const string &scene_root, A3DRenderSettings *settings, string *error);

bool a3d_read_scene(Scene *scene,
                    const string &scene_root,
                    const A3DSceneReaderOptions &options,
                    string *error);

CCL_NAMESPACE_END
