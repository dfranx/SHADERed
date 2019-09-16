# TODO
List of features I want to implement:
 - multiple cameras
   - this feature would allow you to render same scene from multiple angles
 - node shader editor that can export to GLSL and HLSL
 - shader & theme "store"
   - a place where you can upload your shaders or themes and see other peoples creations
 - "Export as DirectX/OpenGL application" option (.cpp file)
 - import from ShaderToy
 - render your shader to a video file
 - pixel inspector
 - audio shaders
 - debugger
 - add compute shader GLSL example with instancing and HLSL example with "rendering" to texture

Except those features, I am also looking to do some small QoL changes and fixes:
 - write geometry type in properties
 - fix TextEditor
 - change TextEditor light theme error msg color
 - fix for: SetColumnWidth doesn't allow user to resize the columns
 - fix "Show the error list window when a build finishes with errors" option
 - fix ShowHorizontalScrollbar option
 - crashes when opening cubemap example only on Linux and only in Release mode!
 - prevent crashes (loading wrong 3d models, compute shaders not supported, etc...)
 - color compute shader labels light/dark green
 - remove everything FXAA related
 - disable path textboxs
 - drag and drop files (textures, audio files, etc...)
 - bring all Preview tools to ObjectPreview window
 - CodeEditorUI loses focus when pressing ALT keys on linux
 - name already taken when creating item through CreateItemUI? then color the "Name:" label red or write a message to user
 - add Options -> Project -> Disable clear
 - feature such as "Change variables" for items but for changing textures -> render textured 3D models easily (have an option to set up everything automatically)
 - move 3D models to ObjectManager
 - create UI/Tools directory (for CubemapPreview, Magnifier, PixelInspect, etc...)
 - recompile the shader after we change shader path in a shader pass
 - when adding a shader pass, decompile the shader and detect input variables
 - when loading a texture it might not be loaded if not bound to any shader pass
 - ability to change warning, message and error text colors
 - add a */ImGuiWindowFlags_UnsavedDocument to Pipeline (and add '\*' to window titlebar) after changing anything in the project (+ popup window on exit)
 - rename AddPass to AddShaderPass
 - bound UBOs should be GLuint's and not std::string -> performance improvement
 - lower down std::map and std::unordered_map usage
 - why isn't first arg in CodeEditorUI::m_open a pointer?
 - PipelineUI::GetSelectedShaderPass() -> add Geometry and other options under "Create" menu item in GUIManager
 - ability to edit the buttons that show up in the toolbar
 - shift+drag with right click to move ArcBallCamera focus point
 - add an option to hide and/or lock certain shader pass
 - error marker should "stick" on the line when new lines are added
 - find and replace in code editor
 - right click in code editor
 - improve the overall code (enum for shader type, etc...)
 - show tips on startup & 'whats new' on first startup
 - option "Show menu bar on mouse move" or "Hide menu bar in performance mode" or "Hide menu bar"
 - fix memory leaks
 - undo/redo
 - languages (english, croatian, etc...)
 - switch from sdl to sfml (?)
 - dragging items from one pass to another, duplicating them, etc...

 # PLAN
 1.2    -> compute shaders

 1.2.*  -> polishing & fixing

 1.3    -> shader debugger

 1.3.*  -> improve debugger