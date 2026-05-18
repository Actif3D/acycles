# Ubuntu 24.04 Minimal Standalone Build

This note documents the package setup used to build this fork on Ubuntu 24.04
with system libraries, after Blender's precompiled library submodule could not
be fetched from `projects.blender.org`.

The resulting build is CPU-only and disables USD, Hydra, OSL, Alembic, Embree,
OpenVDB/NanoVDB, OpenSubdiv, OpenImageDenoise, and GPU backends.

## Base Toolchain

```bash
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  ninja-build \
  git-lfs \
  pkg-config
```

## Required System Libraries

```bash
sudo apt install \
  zlib1g-dev \
  libopenimageio-dev \
  openimageio-tools \
  libpugixml-dev \
  libopenjp2-7-dev \
  libjpeg-dev \
  libpng-dev \
  libwebp-dev \
  libtiff-dev \
  libzstd-dev \
  libfmt-dev \
  libimath-dev \
  libopenexr-dev \
  libopencolorio-dev \
  libopencv-dev \
  libtbb-dev
```

If enabling OpenSubdiv later, Ubuntu 24.04 uses this package name:

```bash
sudo apt install libosd-dev opensubdiv-tools
```

If enabling OSL later:

```bash
sudo apt install libosl-dev
```

If enabling OpenVDB later:

```bash
sudo apt install libopenvdb-dev
```

## Configure And Build

Use the local helper for the minimal CPU-only build:

```bash
./scripts/configure-local-minimal.sh
cmake --build build -j"$(nproc)" --target install
./install/cycles --help
```

The helper runs CMake with `WITH_LIBS_PRECOMPILED=OFF` and disables the optional
features listed above. This avoids the Python 3.13/USD dependency path and uses
Ubuntu packages instead of Blender's precompiled libraries.

## NVIDIA CUDA Devices

The minimal helper intentionally disables CUDA:

```cmake
WITH_CYCLES_DEVICE_CUDA=OFF
WITH_CYCLES_DEVICE_OPTIX=OFF
```

With those flags, `./install/cycles --list-devices` will only show `CPU`, even
on a machine with an NVIDIA RTX card.

For an RTX 3060, first verify the NVIDIA driver is visible:

```bash
nvidia-smi
```

Then configure with CUDA enabled. This enables runtime CUDA loading without
building CUDA kernel binaries at compile time:

```bash
rm -rf build
cmake -S . -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_LIBS_PRECOMPILED=OFF \
  -DWITH_CYCLES_ALEMBIC=OFF \
  -DWITH_CYCLES_EMBREE=OFF \
  -DWITH_CYCLES_OPENIMAGEDENOISE=OFF \
  -DWITH_CYCLES_OPENSUBDIV=OFF \
  -DWITH_CYCLES_OPENVDB=OFF \
  -DWITH_CYCLES_NANOVDB=OFF \
  -DWITH_CYCLES_OSL=OFF \
  -DWITH_USD=OFF \
  -DWITH_CYCLES_USD=OFF \
  -DWITH_CYCLES_HYDRA_RENDER_DELEGATE=OFF \
  -DWITH_STRICT_BUILD_OPTIONS=ON \
  -DWITH_CYCLES_DEVICE_CUDA=ON \
  -DWITH_CYCLES_DEVICE_OPTIX=ON \
  -DWITH_CYCLES_DEVICE_HIP=OFF \
  -DWITH_CYCLES_DEVICE_ONEAPI=OFF
cmake --build build -j"$(nproc)" --target install
./install/cycles --list-devices
```

To build CUDA binaries for RTX 3060 at compile time, install the CUDA toolkit
with `nvcc` and use:

```cmake
WITH_CYCLES_CUDA_BINARIES=ON
CYCLES_CUDA_BINARIES_ARCH=sm_86
```

OptiX is separate from CUDA and requires the NVIDIA OptiX SDK plus
`WITH_CYCLES_DEVICE_OPTIX=ON`.

## Notes From Setup

- `zlib1g` is only the runtime package. CMake needs `zlib1g-dev`.
- `libimath-dev` replaces/conflicts with the older `libilmbase-dev`. If apt
  reports `libimath-dev : Conflicts: libilmbase-dev`, remove the old package
  first with `sudo apt remove libilmbase-dev`.
- `libopenimageio-dev` references tool targets such as `/usr/bin/iconvert`;
  install `openimageio-tools` with it.
- `libopenimageio-dev` on Ubuntu 24.04 references `/usr/include/opencv4`;
  install `libopencv-dev`.
- Ubuntu 24.04's OpenColorIO CMake config may export a target with an invalid
  `/include` path. This checkout includes a local
  `src/cmake/Modules/FindOpenColorIO.cmake` finder for the system headers and
  library.
- This checkout needed compatibility edits for Ubuntu 24.04's OpenColorIO 2.1
  and OpenImageIO 2.4 APIs.
- Delete `build/` after dependency or option changes. CMake caches failed
  package lookup results.

## Useful Verification Commands

```bash
test -f /usr/include/zlib.h && echo "zlib.h OK"
test -e /usr/lib/x86_64-linux-gnu/libz.so && echo "libz.so OK"
test -f /usr/include/png.h && echo "png.h OK"
test -x /usr/bin/iconvert && echo "iconvert OK"
test -d /usr/include/opencv4 && echo "opencv4 OK"
```
