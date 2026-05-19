# A3D Offline Renderer CLI Specification

## Purpose

This document defines the command-line contract expected by Asset3D Studio when invoking an offline renderer backend.

The initial implementation should preserve compatibility with the current subprocess-oriented Studio architecture.

---

# Design Goals

- machine-readable progress
- deterministic output paths
- stable exit codes
- local/cloud execution compatibility
- subprocess-friendly behavior
- cancellation support
- minimal coupling to Studio internals

---

# General Rules

## Exit codes

| Exit Code | Meaning |
|---|---|
| 0 | success |
| non-zero | failure |

## Logging

Renderer should emit machine-readable status lines.

The implemented `acycles --status-messages` mode intentionally matches the current Asset3D Studio subprocess parser:

```text
info: Loading textures
progress: Rendering; total: 100; done: 12;
error: Invalid mesh
```

For compatibility, Studio-facing stdout should contain only `info:`, `progress:`, or `error:` lines. Human-only carriage-return progress output should not be the only progress mechanism.

## Cancellation

Renderer should terminate cleanly when receiving SIGINT or SIGTERM.

---

# Command: Preview Render

## Example

```bash
acycles \
  --a3d-scene /scene/root \
  --output /tmp/render.exr \
  --samples 128 \
  --device CUDA
```

## Required arguments

| Argument | Description |
|---|---|
| `--a3d-scene` | Asset3D scene root folder |
| `--output` | output image path |
| `--image-path` | Studio-compatible alias for `--output` |
| positional scene folder | Studio-compatible alias for `--a3d-scene` when the path is a directory containing `scene.json` |

## Optional arguments

| Argument | Description |
|---|---|
| `--samples` | sample count |
| `--device` | rendering device |
| `--width` | override width |
| `--height` | override height |
| `--threads` | CPU thread count |
| `--tile-size` | tile size |
| `--max-diffuse-bounces` | Studio/SparkTrace-compatible diffuse bounce limit |
| `--max-transparent-bounces` | Studio/SparkTrace-compatible transparent bounce limit |
| `--denoise` | enable denoising |
| `--quiet` | suppress verbose logs |

---

# Command: Bake

## Example

```bash
acycles \
  --a3d-scene /scene/root \
  --bake \
  --lightmap-dir /scene/root/lightmaps \
  --samples 512
```

## Required arguments

| Argument | Description |
|---|---|
| `--a3d-scene` | Asset3D scene root |
| `--bake` | enable bake mode |
| `--lightmap-dir` | output lightmap directory |

## Optional arguments

| Argument | Description |
|---|---|
| `--denoise` | enable denoising |
| `--samples` | bake sample count |
| `--device` | rendering device |
| `--bake-pass` | bake pass selection |

---

# Command: List Capabilities

## Example

```bash
acycles --list-capabilities
```

## Implemented output

The current Studio integration expects the legacy SparkTrace text format:

```text
[OIDN support]
false
[Devices]
CUDA_0;CUDA;NVIDIA RTX 4090
CPU;CPU;CPU description
```

---

# Device Naming

Preferred device types:

- CPU
- CUDA
- OPTIX
- HIP
- METAL
- ONEAPI

Device naming should follow Cycles internal device enumeration whenever practical.

---

# Output Rules

## Preview render

Preview rendering should support:

- EXR
- PNG
- HDR

EXR should remain the preferred production output format.

## Bake output

Bake output should support:

- EXR lightmaps
- optional denoised outputs
- deterministic naming

---

# Compatibility Notes

The CLI should preserve compatibility with Asset3D Studio subprocess orchestration.

Do not assume direct in-process embedding.
