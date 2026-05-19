# Task: Fix Asset3D preview render failure on meshopt-compressed faces.buf

  ## Context

  Asset3D Studio Server has been integrated with `acycles` for preview rendering on Windows.

  Server command shape:

  ```text
  F:\GitRepo\acycles\install\acycles.exe
    --status-messages
    --a3d-scene F:\GitRepo\ss343\tmp\scenes\a3s-sample
    --device CPU
    --samples 15
    --max-diffuse-bounces 8
    --max-transparent-bounces ...
    --width ...
    --height ...
    --image-path F:\GitRepo\ss343\tmp\scenes\a3s-sample\tmp\render.exr

  The CLI accepts the SparkTrace-compatible params now, and starts rendering setup correctly.

  ## Failure

  Preview render fails in acycles with:

  error: meshopt_decodeIndexBuffer failed for faces.buf with code -2

  Server log excerpt:

  [Info] Device: CPU, 12th Gen Intel Core i5-12400F
  [Info] Diffuse bounces: 8
  [Info] Samples: 15
  [Debug] CyclesObject 0: tris: 494113
  [Info] Rendering
  ERROR: Job execution failed
  spark.local.subprocess.CmdExecutionError: meshopt_decodeIndexBuffer failed for faces.buf with code -2

  The browser later reports Job result not available, but that is secondary. The actual render failure is the acycles subprocess error above.

  ## Test Scene

  Scene folder:

  F:\GitRepo\ss343\tmp\scenes\a3s-sample

  Relevant files:

  faces.buf       116802 bytes
  faces16.buf          0 bytes
  meshes.buf     5401328 bytes
  vertices.buf    601712 bytes
  normals.buf     182512 bytes
  uvs0.buf        263512 bytes
  uvs1.buf        231830 bytes

  There is also a raw folder with uncompressed-looking buffers:

  F:\GitRepo\ss343\tmp\scenes\a3s-sample\raw\faces.buf     5929356 bytes
  F:\GitRepo\ss343\tmp\scenes\a3s-sample\raw\meshes.buf    2583248 bytes
  F:\GitRepo\ss343\tmp\scenes\a3s-sample\raw\vertices.buf  8754000 bytes

  ## Repro Command

  Run from Windows:

  F:\GitRepo\acycles\install\acycles.exe `
    --status-messages `
    --a3d-scene F:\GitRepo\ss343\tmp\scenes\a3s-sample `
    --device CPU `
    --samples 1 `
    --max-diffuse-bounces 1 `
    --max-transparent-bounces 1 `
    --width 32 `
    --height 32 `
    --image-path F:\GitRepo\ss343\tmp\scenes\a3s-sample\tmp\acycles-check.exr

  Actual output:

  progress: Loading scene; total: 100; done: 0;
  error: meshopt_decodeIndexBuffer failed for faces.buf with code -2

  ## Expected

  acycles should successfully load this Asset3D scene and write the EXR preview output.

  If the packed faces.buf is meshopt-compressed, implement the correct decode path. If faces16.buf is empty, do not assume it is a valid fallback. If the
  intended MVP path is to use raw buffers, detect and load the valid raw buffers from raw/ for preview rendering.

  Please keep stdout Studio-compatible in --status-messages mode:

  - info: ...
  - progress: ...
  - error: ...

  Avoid human/debug-only lines on stdout in status mode unless they are prefixed in a way Studio accepts.