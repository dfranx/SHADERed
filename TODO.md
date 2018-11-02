- [ ] load files with path relative to project file (add a [static] class { bool IsProjectLoaded; string ProjectPath; etc... })
- [ ] output window (outputs errors, warnings and other messages) + error & warning stack (check if project has any errors) -> dont render the preview if there are errors
- [ ] save per-window (window and widget sizes, positions, etc...) and per-project (opened files, property viewer item, etc..) settings
- [ ] add Passes aka groups - merge shaders and all states into the Pass
- [ ] creating pipeline items
- [ ] various states -> Blend States, Rasterizer State, Depth-Stencil State, etc...
- [ ] textures
- [ ] movable geometry (left click)
- [ ] options (code editor font, theme, etc...)
- [ ] keyboard shortcuts
- [ ] help -> settings | manual | about
- [ ] fix various mem leaks in PipelineManager::Item Data objects
- [ ] do all TODOs

QoL:
- [ ] shader variable pointers & copying
- [ ] automatically detect buffer index when adding variables on triple click on buffer index (using D3DReflect)