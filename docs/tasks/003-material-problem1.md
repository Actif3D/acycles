The black material cases in src/app/a3d_scene_reader.cpp.

  Root causes:

  Water_Pool_Light and Translucent_Glass_Gold
  Their materials have opacity < 1, but I was always connecting the texture Alpha output into Principled Alpha. For JPG textures that alpha is 1, so it overwrote the intended
  opacity and made the surfaces opaque/dark. Now texture alpha is connected only when baseColorTexture.alpha == true.

  Nagarjuna_sagar_vik_001
  Its texture exists at:

  tmp/a3s-sample/raw/textures/Nagarjuna_sagar_vik_001.png

  but the resolver only checked img/... and scene root. I added raw/textures/<id>.<rawExt> fallback.