# ACYCLES Tasks

## Milestone 1 — Preview Render MVP

### CLI

- [ ] Add `--a3d-scene`
- [ ] Add machine-readable progress output
- [ ] Add stable exit codes
- [ ] Add output-path validation

### Scene Reader

- [ ] Create Asset3D scene reader module
- [ ] Load scene package
- [ ] Load geometry buffers
- [ ] Load transforms
- [ ] Load camera
- [ ] Load lights
- [ ] Load textures

### Geometry

- [ ] Construct Cycles meshes
- [ ] Construct Cycles objects
- [ ] Validate indices
- [ ] Validate UVs
- [ ] Validate normals

### Materials

- [ ] Base color
- [ ] Roughness
- [ ] Metallic
- [ ] Normal maps
- [ ] Emissive
- [ ] Opacity/cutout

### Rendering

- [ ] EXR output
- [ ] CPU rendering
- [ ] CUDA rendering
- [ ] Device enumeration

### Integration

- [ ] Test with Asset3D Studio preview pipeline
- [ ] Verify render path compatibility
- [ ] Verify subprocess progress parsing

---

# Milestone 2 — Material Stability

- [ ] Roughness normalization
- [ ] Glossiness conversion
- [ ] Emissive normalization
- [ ] Normal-map validation
- [ ] Texture color-space validation
- [ ] Double-sided handling

---

# Milestone 3 — Bake MVP

- [ ] Add `--bake`
- [ ] Implement UV-space bake pipeline
- [ ] Generate raw EXR lightmaps
- [ ] Integrate denoising
- [ ] Support existing lightmap UV layouts

---

# Milestone 4 — Live Lightmap

- [ ] Progressive bake updates
- [ ] Partial output flushing
- [ ] Restart handling
- [ ] Cancellation handling

---

# Milestone 5 — Utility Separation

- [ ] Extract geometry packing
- [ ] Extract atlas generation
- [ ] Extract bake post-process
- [ ] Extract texture conversion

---

# Validation Scenes

Recommended regression scenes:

- [ ] simple room
- [ ] glossy interior
- [ ] HDR window lighting
- [ ] emissive scene
- [ ] marble surfaces
- [ ] heavy normal mapping
- [ ] large production scene
