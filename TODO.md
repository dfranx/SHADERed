# TODO
List of features I want to implement:
 - multiple cameras
   - this feature would allow you to render same scene from multiple angles
 - node shader editor that can export to GLSL and HLSL
 - shader & theme "store"
   - a place where you can upload your shaders or themes and see other peoples creations
 - "Export as DirectX/OpenGL application" option (.cpp file)
 - compute shaders
 - render your shader to a video file
 - buffers read from file or built using in-app buffer editor
 - magnifier tool
 - pixel inspector
 - debugger


Except those features, I am also looking to do some small QoL changes and fixes:
 - fix for: SetColumnWidth doesn't allow user to resize the columns
 - fix "Show the error list window when a build finishes with errors" option
 - add "Hide the error list window when a build doesn't hve any errors" option
 - remove everything FXAA related
 - CodeEditorUI loses focus when pressing ALT keys on linux
 - name already taken when creating item thru CreateItemUI? then color the "Name:" label red
 - add Options -> Project -> Disable clear
 - feature such as "Change variables" for items but for changing textures -> render textured 3D models easily (have an option to set up everything automatically)
 - add "Advanced" menu that allows user to modify (at least some) system variables when taking screenshot/video
 - cubemap preview
 - click on cubemap, audio, 3d model, texture and render texture preview should open a preview window
 - recompile the shader after we change shader path in a shader pass
 - when adding a shader pass, decompile the shader and detect the input layout + input variables
 - vec4 MouseState (x, y, lBtn, rBtn)
 - when loading a texture it might not be loaded if not bound to any shader pass
 - indent controls after fps in the status bar - bad UX and UI
 - ability to change warning, message and error text colors
 - add a */ImGuiWindowFlags_UnsavedDocument to Pipeline (and add '\*' to window titlebar) after changing anything in the project (+ popup window on exit)
 - PipelineUI::GetSelectedShaderPass() -> add Geometry and other options under "Create" menu item in GUIManager
 - ability to edit the buttons that show up in the toolbar
 - shift+drag with right click to move ArcBallCamera focus point
 - add an option to hide/lock certain shader pass
 - error marker should "stick" on the line when new lines are added
 - find and replace in code editor
 - right click in code editor
 - improve the overall code (enum for shader type, etc...)
 - show tips on startup
 - option "Show menu bar on mouse move" or "Hide menu bar in performance mode" or "Hide menu bar"
 - fix memory leaks
 - undo/redo adding shader items
 - languages (english, croatian, etc...)
 - switch from sdl to sfml (?)
 - dragging items from one pass to another, duplicating them, etc...