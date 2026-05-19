# A3D Studio Integration Plan

## Purpose

This document defines how `acycles` should integrate with Asset3D Studio.

---

# Current Studio Architecture

Current architecture:

```text
Asset3D Studio UI
→ a3d-studio-server
→ subprocess renderer
→ SparkTrace
```

The migration should preserve the subprocess-oriented renderer model initially.

---

# Recommended Integration Strategy

Do not deeply embed Cycles into:

- Electron
- Python server
- Studio UI

Instead:

```text
Asset3D Studio
→ subprocess
→ acycles
```

This preserves:

- cancellation
- local/cloud compatibility
- job orchestration
- process isolation
- independent renderer updates

---

# Renderer Backend Abstraction

Studio should gradually evolve toward:

```python
RENDERER_BACKEND = "sparktrace" | "acycles"
```

rather than hardcoding SparkTrace assumptions.

---

# Initial Renderer Operations

The first migration slice should support:

- preview rendering
- device enumeration
- progress reporting
- cancellation

Bake support comes later.

---

# Progress Reporting

Renderer output should be machine-readable.

Recommended format:

```text
[progress] 10.5
[status] Rendering
```

---

# Output Expectations

Initial preview render target:

```text
render.exr
```

Bake targets:

```text
lightmaps/*.exr
```

---

# Suggested Long-Term Tool Separation

Recommended long-term architecture:

```text
acycles            renderer

a3d-pack           geometry packaging

a3d-atlas          atlas generation

a3d-bake-post      bake post-processing

a3d-texture-convert
```

Do not overload the renderer executable with unrelated utilities long-term.

---

# Migration Recommendation

Recommended order:

1. Preview rendering
2. Material parity
3. Offline baking
4. Denoising
5. Live lightmap
6. SparkTrace utility extraction
7. SparkTrace retirement
