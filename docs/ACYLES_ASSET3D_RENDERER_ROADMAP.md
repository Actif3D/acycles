# ACYCLES Asset3D Renderer Roadmap

## Overview

This document defines the staged migration plan for replacing SparkTrace with `acycles` as the offline renderer used by Asset3D Studio.

The migration should happen incrementally. Do not attempt a full replacement in a single milestone.

---

# Phase 1 — Preview Render Integration

## Goal

Render Asset3D scenes directly through `acycles` and produce EXR preview outputs.

## Required capabilities

- Asset3D scene-folder reader
- camera import
- mesh import
- transform import
- basic material import
- texture loading
- environment/background support
- EXR output
- machine-readable progress reporting
- CPU/GPU device selection

## Non-goals

- lightmap baking
- UV generation
- texture atlasing
- mesh simplification
- full SparkTrace CLI parity

## Success criteria

- Asset3D Studio can invoke `acycles`
- a representative scene renders correctly
- output EXR matches expected render path
- progress can be tracked by server-side jobs

---

# Phase 2 — Material Parity

## Goal

Improve compatibility between Asset3D materials and Cycles shader graphs.

## Required work

- roughness normalization
- glossiness conversion
- normal-map validation
- emissive normalization
- alpha/cutout handling
- double-sided handling
- texture color-space validation

## Success criteria

- major production scenes render acceptably
- no catastrophic emissive instability
- no major shading corruption

---

# Phase 3 — Offline Lightmap Baking

## Goal

Add UV-space lightmap baking support.

## Required work

- bake mode CLI
- UV-space bake pipeline
- raw lightmap output
- lightmap atlas consumption
- bake pass support
- denoising support

## Important constraint

Do not combine UV unwrapping and baking in the first bake milestone.

The renderer should initially consume already-generated lightmap UVs.

## Success criteria

- bake lightmaps from production scenes
- denoise successfully
- output compatible with Asset3D runtime

---

# Phase 4 — Live Lightmap

## Goal

Support progressive bake updates for interactive workflows.

## Required work

- incremental bake updates
- partial output flushing
- cancellation and restart
- deterministic update paths

## Success criteria

- Studio can display progressive lightmap updates
- cancellation is stable

---

# Phase 5 — SparkTrace Utility Extraction

## Goal

Separate non-renderer utilities from the old SparkTrace architecture.

## Candidate standalone tools

```text
a3d-pack
a3d-atlas
a3d-bake-post
a3d-texture-convert
```

## Utilities not inherently renderer responsibilities

- mesh simplification
- texture atlasing
- HDR-to-RGBM conversion
- UV-scale analysis
- geometry packing

---

# Phase 6 — Full Migration

## Goal

Make `acycles` the default Asset3D offline renderer.

## Success criteria

- SparkTrace no longer required for preview rendering
- SparkTrace no longer required for baking
- production bake workflow stable
- renderer documentation finalized
