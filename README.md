# Introduction

This is an implementation of the grid-based Dual Contouring algorithm entirely on the GPU. It lacks
many important features, like materials, some actual shading, mesh simplification -
to name a few.

**It's no longer under development, and likely would never be again.**

# What can you do with it

At the moment it allows to add / subtract two predefined surfaces (sphere and a cube). Result of
the operations are saved on the fly under `scene/` subdirectory automatically. So, when restarted,
the scene created previously will get loaded.

It implements a QEF solver, allowing to reproduce sharp features relatively well.

# Pictures

![Some CSG ops](https://github.com/sznaider/volume-modeler/blob/master/solid.png)

![Wireframe view](https://github.com/sznaider/volume-modeler/blob/master/wireframe.png)

# Keyboard & mouse control

- `W`, `S`, `A`, `D` moves the camera around the scene,
- `1`, `2` switches between first (sphere) / second (cube) brush,
- `Left ALT` causes the brush to rotate with camera,
- `F1`, `F2` switches between wireframe and solid rendering,
- `ESC` causes the mouse cursor to not be grabbed by the application anymore.

- `Mouse Left` adds the brush at the position indicated by the rendered bounding box,
- `Mouse Right` subtracts the brush at the position indicated by the rendered bounding box.

# Compilation and configuration

There are a number of CMake configuration options that can be tweaked:
- `VM_CHUNK_SIZE` - size of the single voxel chunk (64x64x64 by default),
- `VM_VOXEL_SIZE` - distance in world-space unit between two voxels (0.02 by default),
- `WITH_FEATURES` - allows to enable reproduction of sharp features (off by default),
- `WITH_TEST` - enables compilation of unit tests (on by default).

## Compilation
- `git submodule update --init`
- `mkdir build && cd build`
- `cmake .. -DWITH_FEATURES=ON`
- `make -j$(nproc)`

# Dependencies
- OpenCL 1.2
- GLFW3
- glm
- glew
- zlib
- Boost filesystem, regex, iostreams
- pthreads
