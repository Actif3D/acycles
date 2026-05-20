Fixed in src/app/cycles_standalone.cpp.

  Root cause from server.log: Studio passes device IDs like:

  CUDA_NVIDIA GeForce RTX 3060_0000:01:00
  CUDA_NVIDIA GeForce RTX 3060_0000:01:00_OptiX

  but acycles only accepted plain Cycles names: CUDA, OPTIX, CPU. So it failed with Unknown device.

  I added normalization so:

  CUDA_NVIDIA...            -> CUDA
  CUDA_NVIDIA..._OptiX      -> OPTIX
  CPU_...                   -> CPU

  Validation done:

  cmake --build build --target install -j2
  ./install/acycles --list-capabilities
  ./install/acycles --a3d-scene tmp/a3s-sample --device "CUDA_NVIDIA GeForce RTX 3060_0000:01:00" ...

  CUDA now renders successfully with the exact device ID from Studio. Status-message mode also completes and returns the binary preview payload.

  One caveat: on this WSL machine OptiX still reports:

  WARNING: OptiX initialization failed with error code 7804

  so I cannot prove OptiX end-to-end here. The parser side is fixed; on a Windows build where --list-capabilities exposes an OptiX-capable device, the Studio
  OptiX ID should now route into Cycles’ OPTIX backend instead of being rejected as an unknown device.