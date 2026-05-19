# AGENT.md

## Purpose

This repository is a fork of standalone Blender Cycles. The current objective is to evolve it into the offline ray-tracing backend for Asset3D Studio, eventually replacing SparkTrace for preview rendering and lightmap baking.

This document gives Codex and other coding agents the project context, constraints, implementation priorities, and guardrails required to start work safely.

## Current repository state

The current fork still largely follows standalone Cycles structure:

- `src/app/cycles_standalone.cpp` owns the CLI entry point, session construction, XML/USD scene loading, render start/wait, progress output, and basic device selection.
- `src/app/cycles_xml.cpp` reads the standalone XML scene format and constructs Cycles cameras, shaders, meshes, lights, transforms, and objects.
- `src/app/oiio_output_driver.cpp` writes the final full-frame render output through OpenImageIO.
- `src/app/CMakeLists.txt` builds the standalone executable and currently names its output `acycles`.

Do not assume an Asset3D-specific renderer already exists. The work begins by introducing one deliberately.

## Product goal

Create an Asset3D-oriented offline renderer built on Cycles that can be invoked from Asset3D Studio through a CLI process. The initial goal is not to replace every SparkTrace utility at once. The first vertical slice is:

```text
Asset3D scene folder
→ acycles CLI
→ Cycles scene construction
→ EXR preview render
→ Asset3D Studio can consume the output
```

Lightmap baking comes after the preview-render vertical slice is stable.

## Non-goals for the first implementation slice

Do not attempt to implement all SparkTrace commands in the first pass.

Specifically, do not make the first milestone responsible for:

- lightmap UV unwrapping
- atlas generation
- mesh simplification
- HDR-to-RGBM conversion
- texture packing
- live lightmap streaming
- full SparkTrace command parity
- perfect material equivalence with the realtime renderer

Those are later milestones and are documented in the roadmap files.

## Architectural direction

Keep the integration process-oriented:

```text
Asset3D Studio UI
→ a3d-studio-server job system
→ external renderer executable
→ acycles
```

Do not embed Cycles directly into the Python server or Electron app during the initial migration.

Create a renderer-specific CLI contract and preserve the existing subprocess/job architecture used by Asset3D Studio.

## Naming

Use `acycles` for the repository and current executable naming unless a document explicitly proposes a future renderer command name. Avoid inventing additional product names casually.

## First implementation milestone

Implement the first render vertical slice behind a new Asset3D scene reader path.

### Required behavior

1. Add an Asset3D scene-folder input mode to the standalone CLI.
2. Read the Asset3D scene root instead of XML when that input mode is selected.
3. Construct a Cycles scene containing:
   - camera
   - meshes
   - object transforms
   - basic physically-based materials
   - textures
   - scene lights
   - environment/background when available
4. Render an image and write an EXR output compatible with Asset3D Studio render preview workflows.
5. Emit machine-parseable progress/status messages suitable for server-side job tracking.
6. Preserve existing XML rendering behavior.

## Recommended file organization

Prefer adding Asset3D-specific code beside the standalone app code rather than mixing it into the XML reader.

Suggested new files:

```text
src/app/a3d_scene_reader.h
src/app/a3d_scene_reader.cpp
src/app/a3d_geometry_reader.h
src/app/a3d_geometry_reader.cpp
src/app/a3d_material_reader.h
src/app/a3d_material_reader.cpp
src/app/a3d_texture_reader.h
src/app/a3d_texture_reader.cpp
src/app/a3d_light_reader.h
src/app/a3d_light_reader.cpp
src/app/a3d_camera_reader.h
src/app/a3d_camera_reader.cpp
```

These exact file names are suggestions, not a strict mandate. The important rule is separation of concerns.

## Asset3D scene handling principle

Do not treat `scene.json` as the entire scene. Asset3D scenes are folder-based packages that may include:

- `scene.json`
- `render.json`
- `cover.json`
- geometry buffers
- texture resources
- lightmap buffers/textures later

The renderer must be designed around the scene package, not a single JSON file.

## Material mapping scope for the first slice

Support a conservative subset first:

- base color
- base-color texture
- roughness
- metallic
- normal texture when available
- opacity/cutout where straightforward
- emissive color/intensity where already well-defined

Do not implement every legacy House3D/A3D material branch in the first pass. Use the dedicated material mapping specification as the source of truth.

## Device and build constraints

The current fork inherits standalone Cycles build assumptions. Build-aware changes should respect:

- CPU rendering
- CUDA/OptiX builds where enabled
- existing standalone build flow
- OpenImageIO output path
- optional OpenImageDenoise support if built

Do not add hard dependencies that break the existing standalone build unexpectedly.

## Progress and cancellation

The existing standalone renderer already has progress callbacks. For Asset3D Studio integration, prefer explicit, parseable status lines rather than human-only carriage-return progress.

Progress messages should be documented before implementation and should remain stable once Studio consumes them.

## Coding guardrails

- Keep changes small and milestone-driven.
- Preserve current standalone XML behavior.
- Prefer modular readers/builders over monolithic additions to `cycles_standalone.cpp`.
- Do not silently change existing CLI behavior without updating docs.
- Avoid one-off scene-specific heuristics.
- Add targeted tests or sample fixtures when practical.
- Write docs first when introducing external contracts.

## Documents Codex should read before coding

1. `docs/ACYLES_ASSET3D_RENDERER_ROADMAP.md`
2. `docs/A3D_OFFLINE_RENDERER_CLI_SPEC.md`
3. `docs/A3D_SCENE_READER_IMPLEMENTATION_SPEC.md`
4. `docs/A3D_MATERIAL_TO_CYCLES_MAPPING.md`
5. `docs/A3D_LIGHTMAP_BAKING_ARCHITECTURE.md`
6. `docs/A3D_STUDIO_INTEGRATION_PLAN.md`
7. `docs/ACYLES_TASKS.md`
8. `docs/CODING_GUIDELINES.md`

## Definition of done for the first slice

The first slice is complete when:

- `acycles` accepts an Asset3D scene-folder render command.
- At least one representative Asset3D scene can be rendered end-to-end.
- Output is written to the requested EXR path.
- Progress output is machine-readable.
- XML scene rendering still works.
- The new reader code is modular and documented.
- The relevant docs remain accurate.
