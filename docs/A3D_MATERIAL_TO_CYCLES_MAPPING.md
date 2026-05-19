# A3D Material to Cycles Mapping

## Purpose

This document defines how Asset3D and legacy House3D materials should map into Cycles shader graphs.

The goal is not one-to-one renderer equivalence.

The goal is:

- physically stable offline rendering
- visually acceptable parity
- deterministic bake behavior
- stable GI convergence

---

# Design Principles

- prefer physically plausible ranges
- normalize unsafe legacy values
- avoid renderer-specific hacks
- preserve material intent where practical
- prioritize stable offline GI

---

# Base PBR Mapping

| Asset3D | Cycles |
|---|---|
| baseColor | Principled BSDF Base Color |
| roughness | Principled Roughness |
| metallic | Principled Metallic |
| emissive | Emission Shader |
| opacity | Alpha / transparent mix |
| normalTexture | Normal Map Node |

---

# Roughness Rules

Very low roughness values may cause unstable variance.

Recommended normalization:

```ts
roughness = max(roughness, 0.04)
```

---

# Glossiness Conversion

Legacy glossiness workflows should convert using:

```ts
roughness = 1.0 - glossiness
```

Additional normalization may still be required.

---

# Albedo Normalization

Diffuse albedo should remain within physically plausible ranges.

Recommended clamp:

```ts
baseColor = clamp(baseColor, 0.0, 0.8)
```

Especially important for:

- walls
- ceilings
- marble
- floors

---

# Emissive Handling

Legacy emissive materials may require normalization.

Recommended:

```ts
emissionStrength = min(emissionStrength, safeLimit)
```

Offline GI should prioritize stability over exact realtime energy equivalence.

---

# Normal Maps

Normal maps must:

- use linear color space
- avoid sRGB sampling
- use valid tangent basis
- contain normalized vectors

---

# Transparency

Initial implementation should prioritize:

- cutout opacity
- alpha masking

before attempting full physically-correct transmissive materials.

---

# Double-Sided Materials

Double-sided behavior should be explicit.

Do not assume realtime rasterizer behavior automatically maps to offline GI behavior.

---

# Legacy Material Classes

The following legacy classes may require custom normalization logic:

- VRayLightMtl
- glossiness-based materials
- legacy phong/specular workflows
- custom House3D material branches

---

# Validation Strategy

All material mapping changes should be validated against:

- glossy interiors
- emissive scenes
- HDR window lighting
- heavy normal mapping
- marble surfaces
- production bake scenes
