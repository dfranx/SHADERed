// based on https://github.com/nem0/LumixEngine/blob/master/external/imgui/imgui_dock.inl
// modified from https://bitbucket.org/duangle/liminal/src/tip/src/liminal/imgui_dock.cpp

#include "imgui.h"
#define IMGUI_DEFINE_PLACEMENT_NEW
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_dock.h"

// for string comparasion(could be replaced)
#include <string>

using namespace ImGui;

#define nullptr NULL

struct DockContext
{
	enum EndAction_
	{
		EndAction_None,
		EndAction_Panel,
		EndAction_End,
		EndAction_EndChild
	};


	enum Status_
	{
		Status_Docked,
		Status_Float,
		Status_Dragged
	};


	struct Dock
	{
		Dock()
			: id(0)
			, next_tab(nullptr)
			, prev_tab(nullptr)
			, parent(nullptr)
			, pos(0, 0)
			, size(-1, -1)
			, active(true)
			, status(Status_Float)
			, label(nullptr)
			, opened(false)
        
		{
			location[0] = 0;
			children[0] = children[1] = nullptr;
		}


		~Dock() { MemFree(label); }


		ImVec2 getMinSize() const
		{
			if (!children[0]) return ImVec2(16, 16 + GetTextLineHeightWithSpacing());

			ImVec2 s0 = children[0]->getMinSize();
			ImVec2 s1 = children[1]->getMinSize();
			return isHorizontal() ? ImVec2(s0.x + s1.x, ImMax(s0.y, s1.y))
								  : ImVec2(ImMax(s0.x, s1.x), s0.y + s1.y);
		}


		bool isHorizontal() const { return children[0]->pos.x < children[1]->pos.x; }


		void setParent(Dock* dock)
		{
			parent = dock;
			for (Dock* tmp = prev_tab; tmp; tmp = tmp->prev_tab) tmp->parent = dock;
			for (Dock* tmp = next_tab; tmp; tmp = tmp->next_tab) tmp->parent = dock;
		}
        
        Dock& getRoot()
        {
            Dock *dock = this;
            while (dock->parent)
                dock = dock->parent;
            return *dock;
        }


		Dock& getSibling()
		{
			IM_ASSERT(parent);
			if (parent->children[0] == &getFirstTab()) return *parent->children[1];
			return *parent->children[0];
		}


		Dock& getFirstTab()
		{
			Dock* tmp = this;
			while (tmp->prev_tab) tmp = tmp->prev_tab;
			return *tmp;
		}


		void setActive()
		{
			active = true;
			for (Dock* tmp = prev_tab; tmp; tmp = tmp->prev_tab) tmp->active = false;
			for (Dock* tmp = next_tab; tmp; tmp = tmp->next_tab) tmp->active = false;
		}


		bool isContainer() const { return children[0] != nullptr; }


		void setChildrenPosSize(const ImVec2& _pos, const ImVec2& _size)
		{
			ImVec2 s = children[0]->size;
			if (isHorizontal())
			{
				s.y = _size.y;
				s.x = (float)int(
					_size.x * children[0]->size.x / (children[0]->size.x + children[1]->size.x));
				if (s.x < children[0]->getMinSize().x)
				{
					s.x = children[0]->getMinSize().x;
				}
				else if (_size.x - s.x < children[1]->getMinSize().x)
				{
					s.x = _size.x - children[1]->getMinSize().x;
				}
				children[0]->setPosSize(_pos, s);

				s.x = _size.x - children[0]->size.x;
				ImVec2 p = _pos;
				p.x += children[0]->size.x;
				children[1]->setPosSize(p, s);
			}
			else
			{
				s.x = _size.x;
				s.y = (float)int(
					_size.y * children[0]->size.y / (children[0]->size.y + children[1]->size.y));
				if (s.y < children[0]->getMinSize().y)
				{
					s.y = children[0]->getMinSize().y;
				}
				else if (_size.y - s.y < children[1]->getMinSize().y)
				{
					s.y = _size.y - children[1]->getMinSize().y;
				}
				children[0]->setPosSize(_pos, s);

				s.y = _size.y - children[0]->size.y;
				ImVec2 p = _pos;
				p.y += children[0]->size.y;
				children[1]->setPosSize(p, s);
			}
		}


		void setPosSize(const ImVec2& _pos, const ImVec2& _size)
		{
			size = _size;
			pos = _pos;
			for (Dock* tmp = prev_tab; tmp; tmp = tmp->prev_tab)
			{
				tmp->size = _size;
				tmp->pos = _pos;
			}
			for (Dock* tmp = next_tab; tmp; tmp = tmp->next_tab)
			{
				tmp->size = _size;
				tmp->pos = _pos;
			}

			if (!isContainer()) return;
			setChildrenPosSize(_pos, _size);
		}


		char* label;
		ImU32 id;
		Dock* next_tab;
		Dock* prev_tab;
		Dock* children[2];
		Dock* parent;
		bool active;
		ImVec2 pos;
		ImVec2 size;
		Status_ status;
		int last_frame;
		int invalid_frames;
		char location[16];
		bool opened;
		bool first;
	};


	ImVector<Dock*> m_docks;
	ImVec2 m_drag_offset;
	Dock* m_current;
    Dock *m_next_parent;
	int m_last_frame;
	EndAction_ m_end_action;
    ImVec2 m_workspace_pos;
    ImVec2 m_workspace_size;
    ImGuiDockSlot m_next_dock_slot;

    DockContext()
        : m_current(nullptr)
        , m_next_parent(nullptr)
        , m_last_frame(0)
        , m_next_dock_slot(ImGuiDockSlot_Tab)
    {
    }


	~DockContext() {}

	Dock& getDock(const char* label, bool opened)
	{
		ImU32 id = ImHash(label, 0);
		for (int i = 0; i < m_docks.size(); ++i)
		{
			if (m_docks[i]->id == id) return *m_docks[i];
		}

		Dock* new_dock = (Dock*)MemAlloc(sizeof(Dock));
		IM_PLACEMENT_NEW(new_dock) Dock();
		m_docks.push_back(new_dock);
		new_dock->label = ImStrdup(label);
		IM_ASSERT(new_dock->label);
		new_dock->id = id;
		new_dock->setActive();
		new_dock->status = (m_docks.size() == 1)?Status_Docked:Status_Float;
		new_dock->pos = ImVec2(0, 0);
		new_dock->size = GetIO().DisplaySize;
		new_dock->opened = opened;
		new_dock->first = true;
		new_dock->last_frame = 0;
		new_dock->invalid_frames = 0;
		new_dock->location[0] = 0;
		return *new_dock;
	}


	void putInBackground()
	{
		ImGuiWindow* win = GetCurrentWindow();
		ImGuiContext& g = *GImGui;
		if (g.Windows[0] == win) return;

		for (int i = 0; i < g.Windows.Size; i++)
		{
			if (g.Windows[i] == win)
			{
				for (int j = i - 1; j >= 0; --j)
				{
					g.Windows[j + 1] = g.Windows[j];
				}
				g.Windows[0] = win;
				break;
			}
		}
	}


	void splits()
	{
		if (GetFrameCount() == m_last_frame) return;
		m_last_frame = GetFrameCount();

		putInBackground();
        
		for (int i = 0; i < m_docks.size(); ++i) {
			Dock& dock = *m_docks[i];
            if (!dock.parent && (dock.status == Status_Docked)) {
                dock.setPosSize(m_workspace_pos, m_workspace_size);
            }
        }

		ImU32 color = GetColorU32(ImGuiCol_Button);
		ImU32 color_hovered = GetColorU32(ImGuiCol_ButtonHovered);
		ImDrawList* draw_list = GetWindowDrawList();
		ImGuiIO& io = GetIO();
		for (int i = 0; i < m_docks.size(); ++i)
		{
			Dock& dock = *m_docks[i];
			if (!dock.isContainer()) continue;

			PushID(i);
			if (!IsMouseDown(0)) dock.status = Status_Docked;
            
            ImVec2 pos0 = dock.children[0]->pos;
            ImVec2 pos1 = dock.children[1]->pos;
            ImVec2 size0 = dock.children[0]->size;
            ImVec2 size1 = dock.children[1]->size;
            
            ImGuiMouseCursor cursor;

			ImVec2 dsize(0, 0);
			ImVec2 min_size0 = dock.children[0]->getMinSize();
			ImVec2 min_size1 = dock.children[1]->getMinSize();
			if (dock.isHorizontal())
			{
                cursor = ImGuiMouseCursor_ResizeEW;
                SetCursorScreenPos(ImVec2(dock.pos.x + size0.x, dock.pos.y));
				InvisibleButton("split", ImVec2(3, dock.size.y));
				if (dock.status == Status_Dragged) dsize.x = io.MouseDelta.x;
				dsize.x = -ImMin(-dsize.x, dock.children[0]->size.x - min_size0.x);
				dsize.x = ImMin(dsize.x, dock.children[1]->size.x - min_size1.x);
                size0 += dsize;
                size1 -= dsize;
                pos0 = dock.pos;
                pos1.x = pos0.x + size0.x;
                pos1.y = dock.pos.y;
                size0.y = size1.y = dock.size.y;
                size1.x = ImMax(min_size1.x, dock.size.x - size0.x);
                size0.x = ImMax(min_size0.x, dock.size.x - size1.x);
			}
			else
			{
                cursor = ImGuiMouseCursor_ResizeNS;
                SetCursorScreenPos(ImVec2(dock.pos.x, dock.pos.y + size0.y - 3));
				InvisibleButton("split", ImVec2(dock.size.x, 3));
				if (dock.status == Status_Dragged) dsize.y = io.MouseDelta.y;
				dsize.y = -ImMin(-dsize.y, dock.children[0]->size.y - min_size0.y);
				dsize.y = ImMin(dsize.y, dock.children[1]->size.y - min_size1.y);
                size0 += dsize;
                size1 -= dsize;
                pos0 = dock.pos;
                pos1.x = dock.pos.x;
                pos1.y = pos0.y + size0.y;
                size0.x = size1.x = dock.size.x;
                size1.y = ImMax(min_size1.y, dock.size.y - size0.y);
                size0.y = ImMax(min_size0.y, dock.size.y - size1.y);
			}
			dock.children[0]->setPosSize(pos0, size0);
			dock.children[1]->setPosSize(pos1, size1);

            if (IsItemHovered()) {
                SetMouseCursor(cursor);
            }
            
			if (IsItemHovered() && IsMouseClicked(0))
			{
				dock.status = Status_Dragged;
			}

			draw_list->AddRectFilled(
				GetItemRectMin(), GetItemRectMax(), IsItemHovered() ? color_hovered : color);
			PopID();
		}
	}


	void checkNonexistent()
	{
		int frame_limit = ImMax(0, ImGui::GetFrameCount() - 2);
		for (int i = 0; i < m_docks.size(); ++i)
		{
			Dock *dock = m_docks[i];
			if (dock->isContainer()) continue;
			if (dock->status == Status_Float) continue;
			if (dock->last_frame < frame_limit)
			{
				++dock->invalid_frames;
				if (dock->invalid_frames > 2)
				{
					doUndock(*dock);
					dock->status = Status_Float;
				}
				return;
			}
			dock->invalid_frames = 0;
		}
	}


	Dock* getDockAt() const
	{
		for (int i = 0; i < m_docks.size(); ++i)
		{
			Dock& dock = *m_docks[i];
			if (dock.isContainer()) continue;
			if (dock.status != Status_Docked) continue;
			if (IsMouseHoveringRect(dock.pos, dock.pos + dock.size, false))
			{
				return &dock;
			}
		}

		return nullptr;
	}


	static ImRect getDockedRect(const ImRect& rect, ImGuiDockSlot dock_slot)
	{
		ImVec2 half_size = rect.GetSize() * 0.5f;
		switch (dock_slot)
		{
			default: return rect;
			case ImGuiDockSlot_Top: return ImRect(rect.Min, ImVec2(rect.Max.x, rect.Min.y + half_size.y));
			case ImGuiDockSlot_Right: return ImRect(rect.Min + ImVec2(half_size.x, 0), rect.Max);
			case ImGuiDockSlot_Bottom: return ImRect(rect.Min + ImVec2(0, half_size.y), rect.Max);
			case ImGuiDockSlot_Left: return ImRect(rect.Min, ImVec2(rect.Min.x + half_size.x, rect.Max.y));
		}
	}


	static ImRect getSlotRect(ImRect parent_rect, ImGuiDockSlot dock_slot)
	{
		ImVec2 size = parent_rect.Max - parent_rect.Min;
		ImVec2 center = parent_rect.Min + size * 0.5f;
		switch (dock_slot)
		{
			default: return ImRect(center - ImVec2(20, 20), center + ImVec2(20, 20));
			case ImGuiDockSlot_Top: return ImRect(center + ImVec2(-20, -50), center + ImVec2(20, -30));
			case ImGuiDockSlot_Right: return ImRect(center + ImVec2(30, -20), center + ImVec2(50, 20));
			case ImGuiDockSlot_Bottom: return ImRect(center + ImVec2(-20, +30), center + ImVec2(20, 50));
			case ImGuiDockSlot_Left: return ImRect(center + ImVec2(-50, -20), center + ImVec2(-30, 20));
		}
	}


	static ImRect getSlotRectOnBorder(ImRect parent_rect, ImGuiDockSlot dock_slot)
	{
		ImVec2 size = parent_rect.Max - parent_rect.Min;
		ImVec2 center = parent_rect.Min + size * 0.5f;
		switch (dock_slot)
		{
			case ImGuiDockSlot_Top:
				return ImRect(ImVec2(center.x - 20, parent_rect.Min.y + 10),
					ImVec2(center.x + 20, parent_rect.Min.y + 30));
			case ImGuiDockSlot_Left:
				return ImRect(ImVec2(parent_rect.Min.x + 10, center.y - 20),
					ImVec2(parent_rect.Min.x + 30, center.y + 20));
			case ImGuiDockSlot_Bottom:
				return ImRect(ImVec2(center.x - 20, parent_rect.Max.y - 30),
					ImVec2(center.x + 20, parent_rect.Max.y - 10));
			case ImGuiDockSlot_Right:
				return ImRect(ImVec2(parent_rect.Max.x - 30, center.y - 20),
					ImVec2(parent_rect.Max.x - 10, center.y + 20));
			default: IM_ASSERT(false);
		}
		IM_ASSERT(false);
		return ImRect();
	}


	Dock* getRootDock()
	{
		for (int i = 0; i < m_docks.size(); ++i)
		{
			if (!m_docks[i]->parent &&
				(m_docks[i]->status == Status_Docked || m_docks[i]->children[0]))
			{
				return m_docks[i];
			}
		}
		return nullptr;
	}


	bool dockSlots(Dock& dock, Dock* dest_dock, const ImRect& rect, bool on_border)
	{
		ImDrawList* canvas = GetWindowDrawList();
		ImU32 color = GetColorU32(ImGuiCol_Button);
		ImU32 color_hovered = GetColorU32(ImGuiCol_ButtonHovered);
		ImVec2 mouse_pos = GetIO().MousePos;
		for (int i = 0; i < (on_border ? 4 : 5); ++i)
		{
			ImRect r =
				on_border ? getSlotRectOnBorder(rect, (ImGuiDockSlot)i) : getSlotRect(rect, (ImGuiDockSlot)i);
			bool hovered = r.Contains(mouse_pos);
			canvas->AddRectFilled(r.Min, r.Max, hovered ? color_hovered : color);
			if (!hovered) continue;

			if (!IsMouseDown(0))
			{
				doDock(dock, dest_dock ? dest_dock : getRootDock(), (ImGuiDockSlot)i);
				return true;
			}
			ImRect docked_rect = getDockedRect(rect, (ImGuiDockSlot)i);
			canvas->AddRectFilled(docked_rect.Min, docked_rect.Max, GetColorU32(ImGuiCol_Button));
		}
		return false;
	}


	void handleDrag(Dock& dock)
	{
		Dock* dest_dock = getDockAt();

		Begin("##Overlay",
			NULL,
			ImVec2(0, 0),
			0.f,
			ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_AlwaysAutoResize);
		ImDrawList* canvas = GetWindowDrawList();

		canvas->PushClipRectFullScreen();

		ImU32 docked_color = GetColorU32(ImGuiCol_FrameBg);
		docked_color = (docked_color & 0x00ffFFFF) | 0x80000000;
		dock.pos = GetIO().MousePos - m_drag_offset;
		if (dest_dock)
		{
			if (dockSlots(dock,
					dest_dock,
					ImRect(dest_dock->pos, dest_dock->pos + dest_dock->size),
					false))
			{
				canvas->PopClipRect();
				End();
				return;
			}
		}
		if (dockSlots(dock, nullptr, ImRect(m_workspace_pos, m_workspace_pos + m_workspace_size), true))
		{
			canvas->PopClipRect();
			End();
			return;
		}
		canvas->AddRectFilled(dock.pos, dock.pos + dock.size, docked_color);
		canvas->PopClipRect();

		if (!IsMouseDown(0))
		{
			dock.status = Status_Float;
			dock.location[0] = 0;
			dock.setActive();
		}

		End();
	}


	void fillLocation(Dock& dock)
	{
		if (dock.status == Status_Float) return;
		char* c = dock.location;
		Dock* tmp = &dock;
		while (tmp->parent)
		{
			*c = getLocationCode(tmp);
			tmp = tmp->parent;
			++c;
		}
		*c = 0;
	}


	void doUndock(Dock& dock)
	{
		if (dock.prev_tab)
			dock.prev_tab->setActive();
		else if (dock.next_tab)
			dock.next_tab->setActive();
		else
			dock.active = false;
		Dock* container = dock.parent;

		if (container)
		{
			Dock& sibling = dock.getSibling();
			if (container->children[0] == &dock)
			{
				container->children[0] = dock.next_tab;
			}
			else if (container->children[1] == &dock)
			{
				container->children[1] = dock.next_tab;
			}

			bool remove_container = !container->children[0] || !container->children[1];
			if (remove_container)
			{
				if (container->parent)
				{
					Dock*& child = container->parent->children[0] == container
									   ? container->parent->children[0]
									   : container->parent->children[1];
					child = &sibling;
					child->setPosSize(container->pos, container->size);
					child->setParent(container->parent);
				}
				else
				{
					if (container->children[0])
					{
						container->children[0]->setParent(nullptr);
						container->children[0]->setPosSize(container->pos, container->size);
					}
					if (container->children[1])
					{
						container->children[1]->setParent(nullptr);
						container->children[1]->setPosSize(container->pos, container->size);
					}
				}
				for (int i = 0; i < m_docks.size(); ++i)
				{
					if (m_docks[i] == container)
					{
						m_docks.erase(m_docks.begin() + i);
						break;
					}
				}
                if (container == m_next_parent)
                    m_next_parent = nullptr;
				container->~Dock();
				MemFree(container);
			}
		}
		if (dock.prev_tab) dock.prev_tab->next_tab = dock.next_tab;
		if (dock.next_tab) dock.next_tab->prev_tab = dock.prev_tab;
		dock.parent = nullptr;
		dock.prev_tab = dock.next_tab = nullptr;
	}


	void drawTabbarListButton(Dock& dock)
	{
		if (!dock.next_tab) return;

		ImDrawList* draw_list = GetWindowDrawList();
		if (InvisibleButton("list", ImVec2(16, 16)))
		{
			OpenPopup("tab_list_popup");
		}
		if (BeginPopup("tab_list_popup"))
		{
			Dock* tmp = &dock;
			while (tmp)
			{
				bool dummy = false;
				if (Selectable(tmp->label, &dummy))
				{
					tmp->setActive();
                    m_next_parent = tmp;
				}
				tmp = tmp->next_tab;
			}
			EndPopup();
		}

		bool hovered = IsItemHovered();
		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();
		ImVec2 center = (min + max) * 0.5f;
		ImU32 text_color = GetColorU32(ImGuiCol_Text);
		ImU32 color_active = GetColorU32(ImGuiCol_FrameBgActive);
		draw_list->AddRectFilled(ImVec2(center.x - 4, min.y + 3),
			ImVec2(center.x + 4, min.y + 5),
			hovered ? color_active : text_color);
		draw_list->AddTriangleFilled(ImVec2(center.x - 4, min.y + 7),
			ImVec2(center.x + 4, min.y + 7),
			ImVec2(center.x, min.y + 12),
			hovered ? color_active : text_color);
	}


	bool tabbar(Dock& dock, bool close_button)
	{
		float tabbar_height = 2 * GetTextLineHeightWithSpacing();
		ImVec2 size(dock.size.x, tabbar_height);
		bool tab_closed = false;

		SetCursorScreenPos(dock.pos);
		char tmp[20];
		ImFormatString(tmp, IM_ARRAYSIZE(tmp), "tabs%d", (int)dock.id);
		if (BeginChild(tmp, size, true))
		{
			Dock* dock_tab = &dock;

			ImDrawList* draw_list = GetWindowDrawList();
			ImU32 color = GetColorU32(ImGuiCol_FrameBg);
			ImU32 color_active = GetColorU32(ImGuiCol_FrameBgActive);
			ImU32 color_hovered = GetColorU32(ImGuiCol_FrameBgHovered);
			ImU32 text_color = GetColorU32(ImGuiCol_Text);
			float line_height = GetTextLineHeightWithSpacing();
			float tab_base;

			drawTabbarListButton(dock);

			while (dock_tab)
			{
				SameLine(0, 15);

				const char* text_end = FindRenderedTextEnd(dock_tab->label);
				ImVec2 size(CalcTextSize(dock_tab->label, text_end).x, line_height);
				if (InvisibleButton(dock_tab->label, size))
				{
					dock_tab->setActive();
                    m_next_parent = dock_tab;
				}

				if (IsItemActive() && IsMouseDragging())
				{
					m_drag_offset = GetMousePos() - dock_tab->pos;
					doUndock(*dock_tab);
					dock_tab->status = Status_Dragged;
				}

				bool hovered = IsItemHovered();
				ImVec2 pos = GetItemRectMin();
				if (dock_tab->active && close_button)
				{
					size.x += 16 + GetStyle().ItemSpacing.x;
					SameLine();
					tab_closed = InvisibleButton("close", ImVec2(16, 16));
					ImVec2 center = (GetItemRectMin() + GetItemRectMax()) * 0.5f;
					draw_list->AddLine(
						center + ImVec2(-3.5f, -3.5f), center + ImVec2(3.5f, 3.5f), text_color);
					draw_list->AddLine(
						center + ImVec2(3.5f, -3.5f), center + ImVec2(-3.5f, 3.5f), text_color);
				}
				tab_base = pos.y;
				draw_list->PathClear();
				draw_list->PathLineTo(pos + ImVec2(-15, size.y));
				draw_list->PathBezierCurveTo(
					pos + ImVec2(-10, size.y), pos + ImVec2(-5, 0), pos + ImVec2(0, 0), 10);
				draw_list->PathLineTo(pos + ImVec2(size.x, 0));
				draw_list->PathBezierCurveTo(pos + ImVec2(size.x + 5, 0),
					pos + ImVec2(size.x + 10, size.y),
					pos + ImVec2(size.x + 15, size.y),
					10);
				draw_list->PathFillConvex(
					hovered ? color_hovered : (dock_tab->active ? color_active : color));
				draw_list->AddText(pos + ImVec2(0, 1), text_color, dock_tab->label, text_end);

				dock_tab = dock_tab->next_tab;
			}
			ImVec2 cp(dock.pos.x, tab_base + line_height);
			draw_list->AddLine(cp, cp + ImVec2(dock.size.x, 0), color);
		}
		EndChild();
		return tab_closed;
	}


	static void setDockPosSize(Dock& dest, Dock& dock, ImGuiDockSlot dock_slot, Dock& container)
	{
		IM_ASSERT(!dock.prev_tab && !dock.next_tab && !dock.children[0] && !dock.children[1]);

		dest.pos = container.pos;
		dest.size = container.size;
		dock.pos = container.pos;
		dock.size = container.size;

		switch (dock_slot)
		{
			case ImGuiDockSlot_Bottom:
				dest.size.y *= 0.5f;
				dock.size.y *= 0.5f;
				dock.pos.y += dest.size.y;
				break;
			case ImGuiDockSlot_Right:
				dest.size.x *= 0.5f;
				dock.size.x *= 0.5f;
				dock.pos.x += dest.size.x;
				break;
			case ImGuiDockSlot_Left:
				dest.size.x *= 0.5f;
				dock.size.x *= 0.5f;
				dest.pos.x += dock.size.x;
				break;
			case ImGuiDockSlot_Top:
				dest.size.y *= 0.5f;
				dock.size.y *= 0.5f;
				dest.pos.y += dock.size.y;
				break;
			default: IM_ASSERT(false); break;
		}
		dest.setPosSize(dest.pos, dest.size);

		if (container.children[1]->pos.x < container.children[0]->pos.x ||
			container.children[1]->pos.y < container.children[0]->pos.y)
		{
			Dock* tmp = container.children[0];
			container.children[0] = container.children[1];
			container.children[1] = tmp;
		}
	}


	void doDock(Dock& dock, Dock* dest, ImGuiDockSlot dock_slot)
	{
		IM_ASSERT(!dock.parent);
		if (!dest)
		{
			dock.status = Status_Docked;
			dock.setPosSize(m_workspace_pos, m_workspace_size);
		}
		else if (dock_slot == ImGuiDockSlot_Tab)
		{
			Dock* tmp = dest;
			while( tmp->next_tab )	tmp = tmp->next_tab;

			auto inLinkList = []( const Dock* linkList , const Dock* checkNode )
			{
				bool isLinkNode = ( linkList == checkNode );

				const Dock* temp = linkList;
				while( !isLinkNode  && temp->prev_tab )
				{
					temp = temp->prev_tab;

					isLinkNode = ( temp == checkNode );
				}

				temp = linkList;

				while( !isLinkNode && temp->next_tab )
				{
					temp = temp->next_tab;

					isLinkNode = ( temp == checkNode );
				}

				return isLinkNode;
			};

			if( !inLinkList( dest , &dock ) )
			{
				tmp->next_tab = &dock;
				dock.prev_tab = tmp;
				dock.size = tmp->size;
				dock.pos = tmp->pos;
				dock.parent = dest->parent;
				dock.status = Status_Docked;
			}
		}
		else if (dock_slot == ImGuiDockSlot_None)
		{
			dock.status = Status_Float;
		}
		else
		{
			Dock* container = (Dock*)MemAlloc(sizeof(Dock));
			IM_PLACEMENT_NEW(container) Dock();
			m_docks.push_back(container);
			container->children[0] = &dest->getFirstTab();
			container->children[1] = &dock;
			container->next_tab = nullptr;
			container->prev_tab = nullptr;
			container->parent = dest->parent;
			container->size = dest->size;
			container->pos = dest->pos;
			container->status = Status_Docked;
			container->label = ImStrdup("");

			if (!dest->parent)
			{
			}
			else if (&dest->getFirstTab() == dest->parent->children[0])
			{
				dest->parent->children[0] = container;
			}
			else
			{
				dest->parent->children[1] = container;
			}

			dest->setParent(container);
			dock.parent = container;
			dock.status = Status_Docked;

			setDockPosSize(*dest, dock, dock_slot, *container);
		}
		dock.setActive();
	}


	void rootDock(const ImVec2& pos, const ImVec2& size)
	{
		Dock* root = getRootDock();
		if (!root) return;

		ImVec2 min_size = root->getMinSize();
		ImVec2 requested_size = size;
		root->setPosSize(pos, ImMax(min_size, requested_size));
	}

	static ImGuiDockSlot getSlotFromLocationCode(char code)
	{
		switch (code)
		{
			case '1': return ImGuiDockSlot_Left;
			case '2': return ImGuiDockSlot_Top;
			case '3': return ImGuiDockSlot_Bottom;
			default: return ImGuiDockSlot_Right;
		}
	}


	static char getLocationCode(Dock* dock)
	{
		if (!dock) return '0';

		if (dock->parent->isHorizontal())
		{
			if (dock->pos.x < dock->parent->children[0]->pos.x) return '1';
			if (dock->pos.x < dock->parent->children[1]->pos.x) return '1';
			return '0';
		}
		else
		{
			if (dock->pos.y < dock->parent->children[0]->pos.y) return '2';
			if (dock->pos.y < dock->parent->children[1]->pos.y) return '2';
			return '3';
		}
	}


	void tryDockToStoredLocation(Dock& dock)
	{
		if (dock.status == Status_Docked) return;
		if (dock.location[0] == 0) return;
		
		Dock* tmp = getRootDock();
		if (!tmp) return;

		Dock* prev = nullptr;
		char* c = dock.location + strlen(dock.location) - 1;
		while (c >= dock.location && tmp)
		{
			prev = tmp;
			tmp = *c == getLocationCode(tmp->children[0]) ? tmp->children[0] : tmp->children[1];
			if(tmp) --c;
		}
		doDock(dock, tmp ? tmp : prev, tmp ? ImGuiDockSlot_Tab : getSlotFromLocationCode(*c));
	}


	bool begin(const char* label, bool* opened, ImGuiWindowFlags extra_flags)
	{
        ImGuiDockSlot next_slot = m_next_dock_slot;
        m_next_dock_slot = ImGuiDockSlot_Tab;
		Dock& dock = getDock(label, !opened || *opened);
		if (!dock.opened && (!opened || *opened)) tryDockToStoredLocation(dock);
		dock.last_frame = ImGui::GetFrameCount();
		if (strcmp(dock.label, label) != 0)
		{
			MemFree(dock.label);
			dock.label = ImStrdup(label);
		}

		m_end_action = EndAction_None;

        bool prev_opened = dock.opened;
        bool first = dock.first;
		if (dock.first && opened) *opened = dock.opened;
		dock.first = false;
		if (opened && !*opened)
		{
			if (dock.status != Status_Float)
			{
				fillLocation(dock);
				doUndock(dock);
				dock.status = Status_Float;
			}
			dock.opened = false;
			return false;
		}
		dock.opened = true;

		checkNonexistent();
        
        if (first || (prev_opened != dock.opened)) {
            Dock* root = m_next_parent ? m_next_parent : getRootDock();
            if (root && (&dock != root) && !dock.parent) {
                doDock(dock, root, next_slot);
            }
            m_next_parent = &dock;
        }
        
		m_current = &dock;
		if (dock.status == Status_Dragged) handleDrag(dock);

		bool is_float = dock.status == Status_Float;

		if (is_float)
		{
			SetNextWindowPos(dock.pos);
			SetNextWindowSize(dock.size);
			bool ret = Begin(label,
				opened,
				dock.size,
				-1.0f,
				ImGuiWindowFlags_NoCollapse /*| ImGuiWindowFlags_ShowBorders*/ | extra_flags); // ImGuiWindowFlags_ShowBorders not used in new version of ImGui
			m_end_action = EndAction_End;
			dock.pos = GetWindowPos();
			dock.size = GetWindowSize();

			ImGuiContext& g = *GImGui;

			if (g.ActiveId == GetCurrentWindow()->GetID("#MOVE") && g.IO.MouseDown[0])
			{
				m_drag_offset = GetMousePos() - dock.pos;
				doUndock(dock);
				dock.status = Status_Dragged;
			}
			return ret;
		}

		if (!dock.active && dock.status != Status_Dragged) return false;

        //beginPanel();

		m_end_action = EndAction_EndChild;
        
        splits();

		PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		float tabbar_height = GetTextLineHeightWithSpacing();
		if (tabbar(dock.getFirstTab(), opened != nullptr))
		{
			fillLocation(dock);
			*opened = false;
		}
		ImVec2 pos = dock.pos;
		ImVec2 size = dock.size;
		pos.y += tabbar_height + GetStyle().WindowPadding.y;
		size.y -= tabbar_height + GetStyle().WindowPadding.y;

		SetCursorScreenPos(pos);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
								 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
								 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
								 extra_flags;
		bool ret = BeginChild(label, size, true, flags);
		PopStyleColor();
        
		return ret;
	}


	void end()
	{
		m_current = nullptr;
        if (m_end_action != EndAction_None) {
            if (m_end_action == EndAction_End)
            {
                End();
            }
            else if (m_end_action == EndAction_EndChild)
            {
                PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                EndChild();
                PopStyleColor();
            }
            //endPanel();
        }
	}


    void debugWindow() {
        //SetNextWindowSize(ImVec2(300, 300));
        if (Begin("Dock Debug Info")) {
            for (int i = 0; i < m_docks.size(); ++i) {
                if (TreeNode((void*)i, "Dock %d (%p)", i, m_docks[i])) {
                    Dock &dock = *m_docks[i];
                    Text("pos=(%.1f %.1f) size=(%.1f %.1f)", 
                        dock.pos.x, dock.pos.y,
                        dock.size.x, dock.size.y);
                    Text("parent = %p\n",
                        dock.parent);
                    Text("isContainer() == %s\n",
                        dock.isContainer()?"true":"false");
                    Text("status = %s\n",
                        (dock.status == Status_Docked)?"Docked":
                            ((dock.status == Status_Dragged)?"Dragged": 
                                ((dock.status == Status_Float)?"Float": "?")));
                    TreePop();
                }            
            }
            
        }
        End();
    }
};

// --------------------------------------Wrap Function------------------------------------------
#include <map>
#include <string>

static std::map<std::string , DockContext> g_docklist;
static const char* cur_dock_panel = nullptr;

int getDockIndex( const DockContext& context , DockContext::Dock* dock )
{
    if(!dock) return -1;

    for(int i = 0; i < context.m_docks.size(); ++i)
    {
        if(dock == context.m_docks[i]) 
            return i;
    }

    IM_ASSERT(false);
    return -1;
}

DockContext::Dock* getDockByIndex( const DockContext& context , int idx )
{ 
	if( idx >= 0 && idx < context.m_docks.size() )
	{
		return context.m_docks[idx];
	}

	return nullptr;
}

struct readHelper
{
	DockContext* context = nullptr;
	DockContext::Dock* dock = nullptr;
};
static readHelper rhelper;

static void* readOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name)
{
	static std::string context_panel = "";

	rhelper.context = nullptr;
	rhelper.dock	= nullptr;

    std::string tag(name);

	if( tag.substr( 0 , 6 ) == "panel:" )
	{
		context_panel = tag.substr( 6 );
	}
    // specific size of docks
    else if(tag.substr(0, 5) == "Size:")
    {
		DockContext& context = g_docklist[context_panel.c_str()];

		std::string size = tag.substr( 5 );
		int dockSize = atoi( size.c_str() );

		for( int i = 0; i < dockSize; i++ )
		{
			DockContext::Dock* new_dock = ( DockContext::Dock* ) MemAlloc( sizeof( DockContext::Dock ) );
			context.m_docks.push_back( IM_PLACEMENT_NEW( new_dock ) DockContext::Dock() );
		}

        return (void*)NULL;
    }
    // specific index of dock
    else if(tag.substr(0, 5) == "Dock:")
    {
		if( g_docklist.find( context_panel.c_str() ) != g_docklist.end() )
		{
			DockContext& context = g_docklist[context_panel.c_str()];

			std::string indexStr = tag.substr( 5 );
			int index = atoi( indexStr.c_str() );
			if( index >= 0 && index < ( int )context.m_docks.size() )
			{
				rhelper.dock = context.m_docks[index];
				rhelper.context = &context;
			}
		}
    }

    return (void*) &rhelper;
}

static void readLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line_start)
{
	readHelper* userdata = ( readHelper* )entry;

    if( userdata )
    {
        int active, opened, status;
        int x, y, size_x, size_y;
        int prev, next, child0, child1, parent;
        char label[64], location[64];

        if(sscanf(line_start, "label=%[^\n^\r]", label) == 1)
        {
            userdata->dock->label = ImStrdup(label);
			userdata->dock->id = ImHash( userdata->dock->label, 0);
        }
        else if(sscanf(line_start, "x=%d", &x) == 1)
        {
			userdata->dock->pos.x = ( float )x;
        }
        else if(sscanf(line_start, "y=%d", &y) == 1)
        {
			userdata->dock->pos.y = ( float )y;
        }
        else if(sscanf(line_start, "size_x=%d", &size_x) == 1)
        {
			userdata->dock->size.x = ( float )size_x;
        }
        else if(sscanf(line_start, "size_y=%d", &size_y) == 1)
        {
			userdata->dock->size.y = ( float )size_y;
        }
        else if(sscanf(line_start, "active=%d", &active) == 1)
        {
			userdata->dock->active = (bool) active;
        }
        else if(sscanf(line_start, "opened=%d", &opened) == 1)
        {
			userdata->dock->opened = (bool) opened;
        }
        else if(sscanf(line_start, "location=%[^\n^\r]", location) == 1)
        {
            strcpy( userdata->dock->location, location);
        }
        else if(sscanf(line_start, "status=%d", &status) == 1)
        {
			userdata->dock->status = (DockContext::Status_) status;
        }
        else if(sscanf(line_start, "prev=%d", &prev) == 1)
        {
			userdata->dock->prev_tab = getDockByIndex( *( userdata->context ) , prev );
        }
        else if(sscanf(line_start, "next=%d", &next) == 1)
        {
			userdata->dock->next_tab = getDockByIndex( *( userdata->context ) , next );
        }
        else if(sscanf(line_start, "child0=%d", &child0) == 1)
        {
			userdata->dock->children[0] = getDockByIndex( *( userdata->context ) , child0 );
        }
        else if(sscanf(line_start, "child1=%d", &child1) == 1)
        {
			userdata->dock->children[1] = getDockByIndex( *( userdata->context ) , child1 );
        }
        else if(sscanf(line_start, "parent=%d", &parent) == 1)
        {
			userdata->dock->parent = getDockByIndex( *( userdata->context ) , parent );
        }
    }
}

static void writeAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
	int totalDockNum = 0;
	for( const auto& iter : g_docklist )
	{
		const DockContext& context = iter.second;
		totalDockNum += context.m_docks.size();
	}

    // Write a buffer
	buf->reserve( buf->size() + totalDockNum * sizeof( DockContext::Dock ) + 32 * ( totalDockNum + ( int )g_docklist.size() * 2 ) );

    // output size
	for( const auto& iter : g_docklist )
	{
		const DockContext& context = iter.second;

		buf->appendf( "[%s][panel:%s]\n" , handler->TypeName , iter.first.c_str() );
		buf->appendf( "[%s][Size:%d]\n" , handler->TypeName , ( int )context.m_docks.size() );

		for( int i = 0 , docksize = context.m_docks.size(); i < docksize; i++ )
		{
			const DockContext::Dock* d = context.m_docks[i];

			// some docks invisible but do exist
			buf->appendf( "[%s][Dock:%d]\n" , handler->TypeName , i );
			buf->appendf( "label=%s\n" , d->label );
			buf->appendf( "x=%d\n" , ( int )d->pos.x );
			buf->appendf( "y=%d\n" , ( int )d->pos.y );
			buf->appendf( "size_x=%d\n" , ( int )d->size.x );
			buf->appendf( "size_y=%d\n" , ( int )d->size.y );
			buf->appendf( "active=%d\n" , ( int )d->active );
			buf->appendf( "opened=%d\n" , ( int )d->opened );
			buf->appendf( "location=%s\n" , d->location );
			buf->appendf( "status=%d\n" , ( int )d->status );
			buf->appendf( "prev=%d\n" , ( int )getDockIndex( context , d->prev_tab ) );
			buf->appendf( "next=%d\n" , ( int )getDockIndex( context , d->next_tab ) );
			buf->appendf( "child0=%d\n" , ( int )getDockIndex( context , d->children[0] ) );
			buf->appendf( "child1=%d\n" , ( int )getDockIndex( context , d->children[1] ) );
			buf->appendf( "parent=%d\n" , ( int )getDockIndex( context , d->parent ) );
		}
	}
}


// ----------------------------------------------API-------------------------------------------------
void ImGui::ShutdownDock()
{
	for( auto& iter : g_docklist )
	{
		DockContext& context = iter.second;

		for( int k = 0 , dock_count = ( int )context.m_docks.size(); k < dock_count; k++ )
		{
			context.m_docks[k]->~Dock();
			MemFree( context.m_docks[k] );
			context.m_docks[k] = nullptr;
		}
	}
	g_docklist.clear();
}

void ImGui::SetNextDock(const char* panel , ImGuiDockSlot slot) {
	if( panel && g_docklist.find( panel ) != g_docklist.end() )
	{
		g_docklist[panel].m_next_dock_slot = slot;
	}
}

bool ImGui::BeginDockspace()
{
    ImGuiContext& g = *GImGui;
	cur_dock_panel = g.CurrentWindow->Name;

	IM_ASSERT( cur_dock_panel );

	if( !cur_dock_panel )	return false;
	
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
	char child_name[1024];
	sprintf( child_name , "##%s" , cur_dock_panel );
	bool result = BeginChild( child_name , ImVec2( 0 , 0 ) , false , flags );

	DockContext& dock = g_docklist[cur_dock_panel];
	dock.m_workspace_pos = GetWindowPos();
	dock.m_workspace_size = GetWindowSize();

	return result;
}

IMGUI_API void ImGui::EndDockspace()
{
	EndChild();

	cur_dock_panel = nullptr;
}

bool ImGui::BeginDock(const char* label, bool* opened, ImGuiWindowFlags extra_flags)
{
	IM_ASSERT( cur_dock_panel );

	if( !cur_dock_panel )	return false;

	if( g_docklist.find( cur_dock_panel ) != g_docklist.end() )
	{
		DockContext& context = g_docklist[cur_dock_panel];

		char new_label[128];
		sprintf_s( new_label , "%s##%s" , label , cur_dock_panel );

		return context.begin( new_label , opened , extra_flags );
	}
	
	return false;
}


void ImGui::EndDock()
{
	IM_ASSERT( cur_dock_panel );

	if( !cur_dock_panel )	return;

	if( g_docklist.find( cur_dock_panel ) != g_docklist.end() )
	{
		DockContext& context = g_docklist[cur_dock_panel];
		context.end();
	}
}

void ImGui::DockDebugWindow(const char* dock_panel)
{
	if( dock_panel && g_docklist.find( dock_panel ) != g_docklist.end() )
	{
		DockContext& context = g_docklist[dock_panel];
		context.debugWindow();
	}
}

void ImGui::InitDock()
{
    ImGuiContext& g = *GImGui;
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "Dock";
    ini_handler.TypeHash = ImHash("Dock", 0, 0);
    ini_handler.ReadOpenFn = readOpen;
    ini_handler.ReadLineFn = readLine;
    ini_handler.WriteAllFn = writeAll;
    g.SettingsHandlers.push_front(ini_handler);
}