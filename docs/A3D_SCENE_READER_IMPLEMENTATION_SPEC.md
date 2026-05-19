# A3D Scene Reader Implementation Specification

## Purpose

This document defines how Asset3D scenes should be loaded into Cycles.

The initial implementation target is preview rendering, not full baking parity.

---

# Design Principles

- load Asset3D scene folders directly
- avoid intermediate XML generation long-term
- preserve separation of concerns
- keep reader modular
- avoid scene-specific hacks

---

# Scene Package Assumption

Do not treat `scene.json` as the entire scene.

Asset3D scenes are package-oriented and may contain:

```text
scene.json
render.json
cover.json
meshes.buf
vertices.buf
normals.buf
uvs0.buf
uvs1.buf
faces.buf
web/
raw/
textures/
lightmaps/
```

The scene reader should be designed around the package.

---

# Recommended File Organization

Suggested modules:

```text
src/app/a3d_scene_reader.cpp
src/app/a3d_scene_reader.h

src/app/a3d_geometry_reader.cpp
src/app/a3d_geometry_reader.h

src/app/a3d_material_reader.cpp
src/app/a3d_material_reader.h

src/app/a3d_texture_reader.cpp
src/app/a3d_texture_reader.h

src/app/a3d_light_reader.cpp
src/app/a3d_light_reader.h

src/app/a3d_camera_reader.cpp
src/app/a3d_camera_reader.h
```

The exact naming may evolve, but scene loading should remain modular.

---

# Geometry Import

## Requirements

Reader must support:

- indexed triangle meshes
- object transforms
- multiple UV sets
- normals
- tangents when available
- multiple materials

## Validation

Validate:

- NaN vertices
- invalid indices
- degenerate triangles
- invalid UVs
- invalid normals

---

# Transform Handling

Transforms should preserve:

- translation
- rotation
- scale

Negative determinant transforms should be detected for winding correction during bake-oriented preprocessing.

---

# Material Import

Initial material scope:

- baseColor
- roughness
- metallic
- normal texture
- opacity/cutout
- emissive

The material reader should construct Cycles shader graphs directly.

Do not hardcode material behavior into geometry import logic.

---

# Texture Handling

## Requirements

Support:

- relative paths
- package-relative paths
- image caching
- color-space awareness

## Important

Normal maps must use linear color space.

---

# Camera Import

Reader should support:

- perspective camera
- FOV
- transform
- render resolution overrides
- near/far clipping where relevant

---

# Environment Import

Reader should support:

- HDR environment maps
- background color fallback
- environment rotation when available

---

# Output Scene Construction

The reader should construct:

- `Scene`
- `Camera`
- `Mesh`
- `Object`
- `Shader`
- `ShaderGraph`
- `Light`

using standard Cycles APIs.

---

# Relationship to Existing XML Reader

The existing XML reader demonstrates:

- mesh construction
- shader graph construction
- transform application
- camera setup
- light creation

The Asset3D reader should reuse those concepts but should not force Asset3D scenes through an XML conversion pipeline long-term.

---

# Future Extensions

Future scene-reader extensions may include:

- lightmaps
- bake metadata
- deferred object materialization
- instances
- procedural geometry
- motion blur
- subdivision
- volume support
