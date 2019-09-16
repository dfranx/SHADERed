# SHADERed

SHADERed is a lightweight tool for creating and testing **HLSL and GLSL shaders**. It is easy to use,
open source, cross-platform (runs on Windows & Linux - HLSL shaders work on both OS) and frequently updated with new features.

<img src="./Screenshots/IMG2.png"/>

## Sponsors
No one :(

Contact: **dfranx00 at gmail dot com**

## Support
Support the development of this project on Patreon: [<img width="120" src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png">](https://www.patreon.com/dfranx)

You can support the development of this project via **PayPal**: [<img src="https://www.paypalobjects.com/webstatic/en_US/i/buttons/pp-acceptance-medium.png" alt="Buy now with PayPal" />](https://paypal.me/dfranx) 

My e-mail address for businesses (or if you just want to contact me):
**dfranx00 at gmail dot com**

## Features

### See changes instantly
You can press F5 (or whatever shortcut you have set in the options) to see you changes:
<img src="./Screenshots/instantresult.gif">

Don't want to press F5 constantly? Using an external editor and not the built-in one? Turn on
the `"Recompile shader on file change"` flag in the options and once you make changes to the
file, SHADERed will instantly detect that and recompile your shaders:
<img src="./Screenshots/detectfilechange.gif">

Want to see how a certain object affects your scene? Grab the object and scale, rotate and move it
around the scene:
<img src="./Screenshots/gizmo.gif">

### Geometry shaders
You are not limited to vertex and pixel shaders. SHADERed also has support for geometry
shaders. Just enable GS in your shader pass and set the path to your shader. Create advanced
animations and effects using geometry shaders.
<p align="center">
    <img width="200" src="./Screenshots/geometryshader.gif">
</p>

### Render states
You can modify rasterizer, blend and depth-stencil state. Using these states you can: turn on wireframe mode,
disable depth test, use stencil buffer, disable culling, custom blending, etc... All those things help you achieve even more advanced effects.

Heres an example of rasterizer state properties:
<p align="center">
    <img width="300" src="./Screenshots/rasterizerstate.jpg">
</p>

### Audio files
Load a song and create amazing music visualizers!
<p align="center">
    <img width="200" src="./Screenshots/music.gif">
</p>

### Loading 3D models
You can easily add your custom 3D models to the scene. You can also add built-in geometry objects such as cubes, spheres, planes,
full screen quads, etc...

### Textures
Load textures from files and bind them to your shader. SHADERed also supports cubemaps.
<p align="center">
    <img width="260" src="./Screenshots/texture.jpg">
</p>

### Render textures
You can render your results to a window or a custom render texture. You can bind multiple render textures
to one shader pass. It helps you create things such as G-Buffer more easily and in only one shader pass. 
<p align="center">
    <img width="220" src="./Screenshots/multiplert.jpg">
</p>

### Shader input variables
You can create your own variables and edit their values in real time. SHADERed also comes with lots of built-in 'system' shader variables (elapsed time, window size, various
view matrices, etc...).
<img src="./Screenshots/varmanager.jpg">

You can also pin those variables. All pinned variables (from different shader passes) will be visible on
a single window and easily accessible. You can edit them there quickly and see your results instantly.
<p align="center">
    <img width="230" src="./Screenshots/pinned.jpg">
</p>

You can change a shader variable value only for a specific 3D model/geometry item. No programming is required.
You can change multiple variables for each item in the scene before rendering it.
<p align="center">
    <img width="400" src="./Screenshots/itemvarvalue.gif">
</p>

### Workspace
You can modify SHADERed workspace however you want to thanks to Omar Cornut's great work on
[Dear ImGUI's docking branch](https://github.com/ocornut/imgui/tree/docking), which implements window docking.
<p align="center">
    <img width="400" src="./Screenshots/workspace.gif">
</p>

### Code editor
SHADERed has a built-in code editor. The code editor features a very basic version of code predictions.
It is called Smart predictions and it will be updated and improved over time. It also has very basic version
of autocompletion (inserts/removes brackets) and automatic indenting. All of these features can be turned on/off
in options. Theres also an option to convert your tabs to spaces. The code editor will be improved over time.

**You are not forced to use the built-in code editor (and you probably shouldn't use it right now to avoid some
annoying bugs)**. You can seperately run your favourite code editor and SHADERed. 
**Modify your shaders in the code editor of your choice and just save the file - SHADERed will automatically
recompile the shaders for you**.

### Error markers
Error markers help you locate and identify your shader errors more easily. Hover over a line with an error
to see the message or just check it in the "Output" window.
<p align="center">
    <img width="370" src="./Screenshots/error.jpg">
</p>

### Custom themes
You can create your own theme SHADERed theme. Modify ImGuiStyle members in an *.ini file. SHADERed themes allow you to
customize everything including text editor color palette. SHADERed comes with a few built-in themes. I am bad at designing 
so please submit your own themes!

Want to create your own theme but don't know how? Visit [TUTORIAL.md](./TUTORIAL.md).

### Custom templates
You can create your own custom templates. SHADERed comes with a GLSL, HLSL and HLSL deferred rendering template.
Templates help you start developing shaders quickly. To create your own project template, paste your project
directory in /templates directory and name your project file `template.sprj`. You have to reopen SHADERed if it was
already running to see the template under `File -> New`. A menu item for creating a template will
have the same text as the parent directory of the template.

### And many more
Instancing, variable pointers, shader macros, etc...
Check out the list of features that I want to implement in near future: [TODO.md](./TODO.md)

## Binaries
You can download precompiled binaries here: [Releases](https://github.com/dfranx/SHADERed/releases)

## Building
First clone the project & submodules:
```
git clone https://github.com/dfranx/SHADERed.git
git submodule init
git submodule update
```

### Linux
Install all the libraries that are needed:
```
sudo apt install libsdl2-dev libsfml-dev libglew-dev libglm-dev libassimp-dev libgtk-3-dev
```

Build:
```
cmake .
make
```

**NOTE:** If you dont have SFML 2.5 installed on your machine, run these commands:
```
cmake -DUSE_FINDSFML=ON .
make
```

Run:
```
./bin/SHADERed
```

### Windows
1. Install SDL2, SFML, GLEW, GLM, ASSIMP through your favourite package manager (I recommend vcpkg)
2. Run cmake-gui and set CMAKE_TOOLCHAIN_FILE variable
3. Press Configure and then Generate if no errors occured
4. Open the .sln and build the project!

## Tutorial
Don't know how or where to start? Want to create your own shader or custom SHADERed theme? Visit [TUTORIAL.md](TUTORIAL.md) to see
detailed steps on how to do so.

## Screenshots
![](./Screenshots/screen1.jpg)
![](https://user-images.githubusercontent.com/3957610/64042734-e1f9f880-cb62-11e9-8751-90bea93a55a7.png)

Send your own screenshots [here](https://github.com/dfranx/SHADERed/issues/8)!

## Dependencies
This project uses:
 - [ocornut/imgui](https://github.com/ocornut/imgui/tree/docking) (docking branch)
 - [BalazsJako/ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit)
 - [zeux/pugixml](https://github.com/zeux/pugixml)
 - [benhoyt/inih](https://github.com/benhoyt/inih)
 - [KhronosGroup/glslangValidator](https://github.com/KhronosGroup/glslang)
 - [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)
 - [gulrak/filesystem](https://github.com/gulrak/filesystem)
 - [nothings/stb](https://github.com/nothings/stb)
 - [mlabbe/nativefiledialog](https://github.com/mlabbe/nativefiledialog)

Some of the examples in the `examples` directory were taken from AMD's Render Monkey, so credits to AMD.

## LICENSE
SHADERed is licensed under MIT license. See [LICENSE](./LICENSE) for more details.
