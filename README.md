# TRIGLAV

Rendering engine based on Vulkan. It uses deferred shading.
It supports a PBR material system and screen space ambient occlusion.

## Supported Platforms

- Linux (X11, Wayland)
- Windows

## Building From Source

- Install Conan as described here <https://docs.conan.io/2/installation.html>
- Install Meson as descibed here <https://mesonbuild.com/Quick-guide.html>.
- Clone the source code.
- Run `conan install . --build=missing -of buildDir`.
- Run `conan build . -of buildDir`.

## Running the demo

Once the demo is built you can start it with the following command:

```
./buildDir/game/demo/demo -buildDir=buildDir -contentDir=game/demo/content
```

## Movement

- Move around - WSAD.
- Jump - Space.
- Hold middle mouse button to look around.

## Features

- **F3** - Toggle Bounding Boxes.
- **F4** - Toggle Ambient Occlusion.
- **F5** - Toggle Anti-Aliasing.
- **F6** - Toggle Hide UI.
- **F7** - Toggle Bloom.

## Command Line Options

- `-threadCount=<COUNT>` - Number of worker threads in a thread pool.
- `-width=<SIZE>` - Width of the viewport window.
- `-height=<SIZE>` - Height of the viewport window.
- `-buildDir=<PATH>` - Path to the game build directory.
- `-contentDir=<PATH>` - Path to the game content directory.
- `-preferIntegratedGpu` - Choose an integrated GPU over a dedicated one.
- `-presentMode=<MODE>` - Presentation mode. Must be one of allowed values: fifo, immediate, or mailbox.
