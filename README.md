# SHADERed

SHADERed is a lightweight tool for creating and testing **HLSL and GLSL shaders**. It is easy to use,
open source and frequently updated with new features. It has many **features** that the competition
is lacking.

<img src="./Screenshots/IMG2.png"/>

**NOTE**: SHADERed has a built-in text editor but I would suggest using some external text editor
as currently the one that is built-in is slow, buggy and doesn't have some basic features.

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

### Loading .obj models
You can easily add your custom 3D models to the scene. Only Wavefront .obj models are currently supported, but
you can expect more formats to be supported in near future. You can also add built-in geometry objects (cubes, spheres, planes,
full screen quads, etc...).

### Textures
Load textures from files and bind them to your shader. SHADERed supports JPG, PNG,
DDS and BMP file format. Currently there is only a built-in sampler state. I plan to add customizable
sampler states in future. The sampler state is always bound to s0 register (in a shader pass which is
using a texture). SHADERed also supports cubemaps.
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
You can create your own variables and edit their values. Those variables will be sent to a shader using a
constant buffer. There are also a lot of built-in 'system' shader variables (elapsed time, window size, various
view matrices, etc...).
<img src="./Screenshots/varmanager.jpg">

You can also pin those variables. All pinned variables (from different shader passes) will be visible on
one window. You can edit them there easily and see your results in real time.
<p align="center">
    <img width="230" src="./Screenshots/pinned.jpg">
</p>

You can change a shader variable value only for a specific 3D model/geometry item. No programming is required.
Imagine passing a variable objColor with value (1,1,1,1) to a shader in a constant buffer. You can change that
variable's value before rendering a specific item:
<p align="center">
    <img width="400" src="./Screenshots/itemvarvalue.gif">
</p>


### Shader stats
Don't know if your new shader has less instructions and is more optimized? Just check the stats page for your shader
in SHADERed. It shows you total number of instructions and number of specific instructions. This way you can see if your shader
really became more optimized:
<p align="center">
    <img width="250" src="./Screenshots/stats.jpg">
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
Templates help you start with an already built base for your new project. To create your own project template, paste your project
directory in /templates directory and name your project file `template.sprj`. You have to reopen SHADERed if it was
already running to see the template under `File -> New`. A menu item for creating a template will
have the same text as the parent directory of the template.

## Support
Support the development of this project on Patreon: [<img width="120" src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png">](https://www.patreon.com/dfranx)

You can support the development of this project via **PayPal**: [PayPal link](https://paypal.me/dfranx) 

This is my e-mail address for businesses or if you just want to contact me:
**dfranx00 at gmail dot com**

Also feel free to contact me and suggest some missing feature you would like me to implement.

## TODO
There are also some features that I am looking to implement:
 - custom sampler states
 - more 3D model file formats
 - multiple cameras
   - this feature would allow you to render same scene from multiple angles
 - shader variable pointers
   - currently if you want to have same value in two shader passes you will have to enter the values manually
 - shader flags
   - custom flags when compiling the shader
 - audio files
   - audio files would allow you to create music visualization
 - node shader editor that can export to GLSL and HLSL
 - shader & theme "store"
   - a place where you can upload your shaders or themes and see other peoples creations
 - "Export as DirectX/OpenGL application" option (.cpp file)
 - research tesselation and compute shaders and how to implement them
 - render your shader to a video file
 - support #include and macros
 - inspect render target output pixels (show RGB values on hover over pixel)
 - buffers read from file or built using in-app buffer editor


Except those large features, I am also planning to do some small QoL changes:
 - find and replace in code editor
 - right click in code editor
 - when adding a shader pass, decompile the shader and detect the input layout + input variables
 - PipelineUI::GetSelectedShaderPass() -> add Geometry and other options under "Create" menu item in GUIManager
 - recompile the shader after we change shader path in a shader pass
 - right click on an empty objects panel should open a context menu
 - ctrl + click -> select multiple items
 - bounding box around selected item + possible resize points on the bounding box
 - polished gizmo
 - add a */ImGuiWindowFlags_UnsavedDocument to Pipeline after changing anything in the project
 - cubemap preview
 - click on cubemap, texture and render texture preview should open a preview window
 - fix selecting items when user has a skybox in scene
 - use pointers to RTs in pipe::ShaderPass instead of names
 - remember collapsed items in a project
 - remember focused window in workspace.dat (worked before updating my imgui/docking clone)

## Binaries
To get started you can visit [Release](https://github.com/dfranx/SHADERed/releases) page and download
latest stable binary release.

If you want to compile the program yourself, install [vcpkg](https://github.com/Microsoft/vcpkg)
or any package manager. Then run following command: ```vcpkg install directxtex```. If you don't have/want to use
a C++ package manager, download and compile [DirectXTex](https://github.com/Microsoft/directxtex) library manually. 
You also have to download, compile and link [MoonLight](https://github.com/dfranx/MoonLight) - a Direct3D 11 wrapper.
Tutorial on building MoonLight is written in the [README.md](https://github.com/dfranx/MoonLight/README.md) file of the MoonLight repo.

## Tutorial
Don't know how or where to start? Want to create your own shader or custom SHADERed theme? Visit [TUTORIAL.md](TUTORIAL.md) to see
detailed steps on how to do so.

## Dependencies
This project uses:
 - DirectX 11
 - DirectXMath
 - [imgui](https://github.com/ocornut/imgui/tree/docking) (docking branch)
 - [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit)
 - [pugixml](https://github.com/zeux/pugixml)
 - [MoonLight](https://github.com/dfranx/MoonLight)
 - [inih](https://github.com/benhoyt/inih)
 - [DirectXTex](https://github.com/microsoft/DirectXTex)
 - [KhronosGroup/glslangValidator](https://github.com/KhronosGroup/glslang)
 - [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)

Some of the examples in the `examples` directory were taken from AMD's Render Monkey, so credits to AMD.

## Screenshots
![](./Screenshots/screen1.jpg)
![](./Screenshots/screen2.jpg)

Send your own screenshots!

## Credits
Huge thanks to Omar Cornut, go follow him on [twitter](https://twitter.com/ocornut) or support him on [patreon](https://www.patreon.com/imgui).
I dont think SHADERed would exist without his awesome library [Dear ImGUI](https://github.com/ocornut/).
I came up with the shader editor idea when I stubmled upon a [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit).
So big thanks to BalazsJako for his amazing creation. Also, thanks to Raph Levien for the [Inconsolata](https://fonts.google.com/specimen/Inconsolata) font.

Thanks to Khronos for creating [glslangValidator](https://github.com/KhronosGroup/glslang) and [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) tools.
Those programs allowed me to add support for GLSL. Thanks to AMD and their RenderMonkey examples. Credits to creators of the
themes that SHADERed comes with. Those themes and their creators can be found [here](https://github.com/ocornut/imgui/issues/707).

## LICENSE
SHADERed is licensed under MIT license. See [LICENSE](./LICENSE) for more details.