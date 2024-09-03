# Pixel Physics

A simple 2D physics sandbox game where you can place blocks of different materials and watch them interact with each other.

## Downloading

Head to the [actions tab](https://github.com/someretical/pixel-physics/actions) and access the latest build. Download the artifact that matches your operating system. Linux users will be able to choose between GNU and clang builds. Extract the contents of the archive and run the executable.

## Controls

- Left click on mouse to place a pixel of material
- Right click on mouse to erase all pixels in the brush area
- Scroll up and down to increase and decrease the size of the brush respectively
- Middle click to set the brush material to the material of the pixel directly under the cursor
- Press 1 to select regular sand
- Press 2 to select water (less dense than regular sand)
- Press 3 to select red sand (less dense than regular sand but more dense than water)
- More features to come...

## Building
```
$ git clone --recurse-submodules https://github.com/someretical/pixel-physics.git
$ cd pixel-physics
$ cmake -DCMAKE_BUILD_TYPE=Debug   -S . -B cmake-build-debug-[your compiler]
$ cmake -DCMAKE_BUILD_TYPE=Release -S . -B cmake-build-release-[your compiler]
$ cmake --build cmake-build-debug-[your compiler]   --config=Debug   --target pixels -j [no. of threads]
$ cmake --build cmake-build-release-[your compiler] --config=Release --target pixels -j [no. of threads]
```

Binaries will be in `cmake-build-debug-[your compiler]` and `cmake-build-release-[your compiler]` respectively.