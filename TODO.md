# PLAN
 1.2.*  -> polishing & fixing

 1.3    -> shader debugger

 1.3.*  -> improve debugger
 
 **BUGS:**
 - CodeEditorUI loses focus when pressing ALT keys on linux
 - Home/End is buggy

# TODO
List of features I want to implement:
 - debugger & pixel inspect tool
 - godot shaders
 - ability to render your shader to a video file
 - node shader editor that can export to GLSL and HLSL
 - shader & theme "store"
   - a place where you can upload your shaders or themes and see other peoples creations
 - "Export as DirectX/OpenGL application" option (.cpp file)
 - "import from ShaderToy" option

Except those features, I am also looking to do some small QoL changes and fixes:
 - dragging items from one pass to another, duplicating them, etc...
 - improve the overall code (enum for shader type, etc...)
 - fix memory leaks
 - shift+drag with right click to move ArcBallCamera focus point
 - move 3D models to ObjectManager
 - languages (english, croatian, etc...)
 - undo/redo
 - show tips on startup & 'whats new' on first startup
 - add an option to hide and/or lock certain shader pass