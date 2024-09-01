# Pixel Physics

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