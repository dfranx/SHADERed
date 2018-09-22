// based on https://github.com/nem0/LumixEngine/blob/master/external/imgui/imgui_dock.h
// modified from https://bitbucket.org/duangle/liminal/src/tip/src/liminal/imgui_dock.h

#pragma once

// Forward declarations
typedef int ImGuiWindowFlags;
    
typedef enum ImGuiDockSlot {
    ImGuiDockSlot_Left,
    ImGuiDockSlot_Right,
    ImGuiDockSlot_Top,
    ImGuiDockSlot_Bottom,
    ImGuiDockSlot_Tab,

    ImGuiDockSlot_Float,
    ImGuiDockSlot_None
} ImGuiDockSlot;

namespace ImGui{

IMGUI_API bool BeginDockspace();
IMGUI_API void EndDockspace();
IMGUI_API void ShutdownDock();
IMGUI_API void SetNextDock( const char* dock_panel , ImGuiDockSlot slot );
IMGUI_API bool BeginDock(const char* label, bool* opened = NULL, ImGuiWindowFlags extra_flags = 0);
IMGUI_API void EndDock();
IMGUI_API void DockDebugWindow(const char* dock_panel);
IMGUI_API void InitDock();

};