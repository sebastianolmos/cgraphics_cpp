# cgraphics_cpp
OpenGL example code in C++ language 

Based on [LearnOpenGL](https://learnopengl.com/), [dantros/grafica_cpp](https://github.com/dantros/grafica_cpp) and [OGLdev](https://ogldev.org/) resources

## Compilation and execution

1. Download submodules:

        git submodule update --init --recursive

2. Make the build:

        mkdir build
        cd build
        cmake ..

3. Compile from the repo directory:

        cmake --build build -A x64 --config Release