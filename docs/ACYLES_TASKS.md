# ACYCLES Tasks

## Milestone 1 — Preview Render MVP

### CLI

- [x] Add `--a3d-scene`
- [x] Add machine-readable progress output
- [x] Add stable exit codes
- [x] Add output-path validation

### Scene Reader

- [x] Create Asset3D scene reader module
- [x] Load scene package
- [x] Load geometry buffers
- [ ] Decode meshopt-compressed `faces.buf` packages
- [x] Load transforms
- [x] Load camera
- [x] Load lights
- [x] Load textures

### Geometry

- [x] Construct Cycles meshes
- [x] Construct Cycles objects
- [x] Validate indices
- [x] Validate UVs
- [x] Validate normals

### Materials

- [x] Base color
- [x] Roughness
- [x] Metallic
- [x] Normal maps
- [x] Emissive
- [x] Opacity/cutout

### Rendering

- [x] EXR output
- [x] CPU rendering
- [ ] CUDA rendering
- [x] Device enumeration

### Integration

- [ ] Test with Asset3D Studio preview pipeline
- [x] Verify render path compatibility
- [x] Verify subprocess progress parsing

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
