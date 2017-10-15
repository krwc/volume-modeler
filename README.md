This is an implementation of the grid-based Dual Contouring algorithm entirely on the GPU. It lacks
many important features I'd want to have, like materials, some actual shading, mesh simplification -
to name a few.

It currently allows to just carve. As a bonus, the operations are saved on the fly to the `scene/`
subdirectory - so, when restarted the scene created previously will get loaded.

It implements rather stable QEF solver, allowing to reproduce sharp features quite well.

![Some CSG ops](https://github.com/sznaider/volume-modeler/blob/master/solid.png)

![Wireframe view](https://github.com/sznaider/volume-modeler/blob/master/wireframe.png)

Compilation:
* git submodule update --init
* mkdir build && cd build
* cmake ..
* make -j4

Dependencies:
* OpenCL 1.2
* GLFW3
* glm
* glew
* zlib
* Boost filesystem, regex, iostreams
* pthreads
