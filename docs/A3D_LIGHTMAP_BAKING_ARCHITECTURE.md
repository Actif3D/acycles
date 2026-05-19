# A3D Lightmap Baking Architecture

## Purpose

This document defines the intended architecture for offline lightmap baking using `acycles`.

---

# Architectural Principle

Realtime rendering representation and offline bake representation are not equivalent.

Offline GI requires:

- physically stable materials
- bake-safe topology
- deterministic UV-space evaluation
- stable tangent basis
- stable sampling

A dedicated bake pipeline is required.

---

# Bake Pipeline

```text
Asset3D Scene
→ Deferred Object Resolution
→ Bake Preprocess
→ UV-space Bake
→ Raw Lightmaps
→ Denoising
→ Post-process
→ Runtime Lightmaps
```

---

# Initial Scope

The first bake milestone should:

- consume existing lightmap UVs
- consume existing atlas layout
- produce raw EXR lightmaps
- support denoising

Do not combine UV unwrapping and baking in the first milestone.

---

# Bake Representation

Bake preprocessing should include:

- transform baking
- topology validation
- tangent reconstruction
- normal reconstruction
- material normalization
- degenerate cleanup

---

# UV-space Rendering

Lightmap baking is not normal camera rendering.

The renderer must evaluate lighting in UV space.

Conceptually:

```text
for each lightmap texel:
  find corresponding surface point
  evaluate lighting
  write lightmap texel
```

---

# Sample Count

Interior HDR scenes may require substantially higher sample counts.

Recommended ranges:

| Quality | Suggested SPP |
|---|---|
| Preview | 32–64 |
| Medium | 128–256 |
| High | 512+ |

---

# Denoising

AI denoising is strongly recommended.

Preferred implementations:

- Intel OpenImageDenoise
- OptiX Denoiser

---

# High-Variance Interior Scenes

The following scene types are inherently difficult:

- HDR window lighting
- glossy interiors
- polished marble
- emissive interiors
- reflective surfaces

These scenes may require:

- higher spp
- adaptive sampling
- denoising
- stable roughness normalization

---

# Deferred Object Materialization

Deferred objects must be fully materialized before baking.

Bake representation should avoid runtime-only deferred renderer assumptions.

---

# Future Improvements

Potential future improvements:

- adaptive sampling
- live lightmap streaming
- bake-only mesh refinement
- portal/window sampling
- ReSTIR GI
- reservoir sampling
