# PLAN
 1.2.*  -> polishing & fixing

 1.3    -> shader debugger

 1.3.*  -> improve debugger
 
# TODO
List of features I want to implement:
 - multiple cameras
   - this feature would allow you to render same scene from multiple angles
   - maybe add it as "Camera snapshot" and have a variable value function that takes in cam snapshot name
 - node shader editor that can export to GLSL and HLSL
 - shader & theme "store"
   - a place where you can upload your shaders or themes and see other peoples creations
 - "Export as DirectX/OpenGL application" option (.cpp file)
 - "import from ShaderToy" option
 - render your shader to a video file
 - pixel inspector
 - audio shaders
 - debugger

Except those features, I am also looking to do some small QoL changes and fixes:
 - crashes when opening cubemap example only on Linux and only in Release mode!
 - openning Edge->Sobel->Edge or Sobel->Edge->Sobel breaks SHADERed
 - CodeEditorUI loses focus when pressing ALT keys on linux
 - multi camera system
 - audio shader pass
 - update all 3rd party libs
---
 - dragging items from one pass to another, duplicating them, etc...
 - improve the overall code (enum for shader type, etc...)
 - fix memory leaks (if there are any)
 - shift+drag with right click to move ArcBallCamera focus point
 - move 3D models to ObjectManager
 - languages (english, croatian, etc...)
 - undo/redo
 - show tips on startup & 'whats new' on first startup
 - add an option to hide and/or lock certain shader pass