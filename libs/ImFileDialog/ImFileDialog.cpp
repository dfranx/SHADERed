#include "ImFileDialog.h"

#include <fstream>
#include <algorithm>
#include <filesystem>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <stb/stb_image.h>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#include <Lmcons.h>
#pragma comment(lib, "Shell32.lib")
#else
#include <unistd.h>
#include <pwd.h>
#endif

#define ICON_SIZE ImGui::GetFont()->FontSize + 3
#define GUI_ELEMENT_SIZE GImGui->FontSize + 12
#define DEFAULT_ICON_SIZE 32
#define PI 3.141592f

namespace ifd {
	/* UI CONTROLS */
	bool FolderNode(const char* label, ImTextureID icon, bool& clicked)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		clicked = false;

		ImU32 id = window->GetID(label);
		int opened = window->StateStorage.GetInt(id, 0);
		ImVec2 pos = window->DC.CursorPos;
		const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= pos.x && g.IO.MousePos.x < pos.x + g.FontSize);
		if (ImGui::InvisibleButton(label, ImVec2(-1, g.FontSize + g.Style.FramePadding.y * 2)))
		{
			if (is_mouse_x_over_arrow) {
				int* p_opened = window->StateStorage.GetIntRef(id, 0);
				opened = *p_opened = !*p_opened;
			} else {
				clicked = true;
			}
		}
		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();
		bool doubleClick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		if (doubleClick && hovered) {
			int* p_opened = window->StateStorage.GetIntRef(id, 0);
			opened = *p_opened = !*p_opened;
			clicked = false;
		}
		if (hovered || active)
			window->DrawList->AddRectFilled(window->DC.LastItemRect.Min, window->DC.LastItemRect.Max, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]));
		
		// Icon, text
		float icon_posX = pos.x + g.FontSize + g.Style.FramePadding.y;
		float text_posX = icon_posX + g.Style.FramePadding.y + ICON_SIZE;
		ImGui::RenderArrow(window->DrawList, ImVec2(pos.x, pos.y+g.Style.FramePadding.y), ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[((hovered && is_mouse_x_over_arrow) || opened) ? ImGuiCol_Text : ImGuiCol_TextDisabled]), opened ? ImGuiDir_Down : ImGuiDir_Right);
		window->DrawList->AddImage(icon, ImVec2(icon_posX, pos.y), ImVec2(icon_posX + ICON_SIZE, pos.y + ICON_SIZE));
		ImGui::RenderText(ImVec2(text_posX, pos.y + g.Style.FramePadding.y), label);
		if (opened)
			ImGui::TreePush(label);
		return opened != 0;
	}
	bool FileNode(const char* label, ImTextureID icon) {
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		ImU32 id = window->GetID(label);
		ImVec2 pos = window->DC.CursorPos;
		bool ret = ImGui::InvisibleButton(label, ImVec2(-1, g.FontSize + g.Style.FramePadding.y * 2));

		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();
		if (hovered || active)
			window->DrawList->AddRectFilled(window->DC.LastItemRect.Min, window->DC.LastItemRect.Max, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]));

		// Icon, text
		window->DrawList->AddImage(icon, ImVec2(pos.x, pos.y), ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE));
		ImGui::RenderText(ImVec2(pos.x + g.Style.FramePadding.y + ICON_SIZE, pos.y + g.Style.FramePadding.y), label);
		
		return ret;
	}
	bool PathBox(const char* label, std::wstring& path, char* pathBuffer, ImVec2 size_arg) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		bool ret = false;
		const ImGuiID id = window->GetID(label);
		int* state = window->StateStorage.GetIntRef(id, 0);
		
		ImGui::SameLine();

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		ImVec2 pos = window->DC.CursorPos;
		ImVec2 uiPos = ImGui::GetCursorPos();
		ImVec2 size = ImGui::CalcItemSize(size_arg, 200, GUI_ELEMENT_SIZE);
		const ImRect bb(pos, pos + size);
		
		// buttons
		if (!(*state & 0b001)) {
			ImGui::PushClipRect(bb.Min, bb.Max, false);

			// background
			bool hovered = g.IO.MousePos.x >= bb.Min.x && g.IO.MousePos.x <= bb.Max.x &&
				g.IO.MousePos.y >= bb.Min.y && g.IO.MousePos.y <= bb.Max.y;
			bool clicked = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
			bool anyOtherHC = false; // are any other items hovered or clicked?
			window->DrawList->AddRectFilled(pos, pos + size, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[(*state & 0b10) ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg]));

			// fetch the buttons (so that we can throw some away if needed)
			std::vector<std::string> btnList;
			std::vector<int> indices;
			float totalWidth = 0.0f;
			int lastSplitLoc = 0;
			for (int i = 0; i < path.size(); i++) {
				if (path[i] == '\\' || path[i] == '/') {
					std::wstring section(path.substr(lastSplitLoc, i - lastSplitLoc));
					std::string sectionStr(section.begin(), section.end());
					
					totalWidth += ImGui::CalcTextSize(sectionStr.c_str()).x + style.FramePadding.x * 2.0f + GUI_ELEMENT_SIZE;
					btnList.push_back(sectionStr);
					indices.push_back(i);

					lastSplitLoc = i + 1;
				}
			}
			if (!ret && path[path.size() - 1] != '\\' && path[path.size() - 1] != '/') {
				std::wstring section = path.substr(lastSplitLoc);
				std::string sectionStr(section.begin(), section.end());

				totalWidth += ImGui::CalcTextSize(sectionStr.c_str()).x + style.FramePadding.x * 2.0f;
				btnList.push_back(sectionStr);
				indices.push_back(path.size());
			}

			// UI buttons
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
			bool isFirstElement = true;
			for (int i = 0; i < btnList.size(); i++) {
				if (totalWidth > size.x - 30 && i != btnList.size() - 1) { // trim some buttons if there's not enough space
					float elSize = ImGui::CalcTextSize(btnList[i].c_str()).x + style.FramePadding.x * 2.0f + GUI_ELEMENT_SIZE;
					totalWidth -= elSize;
					continue;
				}

				ImGui::PushID(i);
				if (!isFirstElement) {
					ImGui::ArrowButtonEx("##dir_dropdown", ImGuiDir_Right, ImVec2(GUI_ELEMENT_SIZE, GUI_ELEMENT_SIZE));
					anyOtherHC |= ImGui::IsItemHovered() | ImGui::IsItemClicked();
					ImGui::SameLine();
				}
				if (ImGui::Button(btnList[i].c_str(), ImVec2(0, GUI_ELEMENT_SIZE))) {
					path = std::filesystem::path(path.substr(0, indices[i])).wstring();
					ret = true;
				}
				anyOtherHC |= ImGui::IsItemHovered() | ImGui::IsItemClicked();
				ImGui::SameLine();
				ImGui::PopID();

				isFirstElement = false;
			}
			ImGui::PopStyleVar();


			// click state
			if (!anyOtherHC && clicked) {
				strcpy(pathBuffer, std::string(path.begin(), path.end()).c_str());
				*state |= 0b001;
				*state &= 0b011; // remove SetKeyboardFocus flag
			}
			else
				*state &= 0b110;

			// hover state
			if (!anyOtherHC && hovered && !clicked) 
				*state |= 0b010;
			else 
				*state &= 0b101;

			ImGui::PopClipRect();

			// allocate space
			ImGui::SetCursorPos(uiPos);
			ImGui::ItemSize(size);
		}
		// input box
		else {
			bool skipActiveCheck = false;
			if (!(*state & 0b100)) {
				skipActiveCheck = true;
				ImGui::SetKeyboardFocusHere();
				if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					*state |= 0b100;
			}
			if (ImGui::InputTextEx("##pathbox_input", "", pathBuffer, 1024, size_arg, ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::string tempStr(pathBuffer);
				if (std::filesystem::exists(tempStr))
					path = std::filesystem::path(tempStr).wstring(); 
				ret = true;
			}
			if (!skipActiveCheck && !ImGui::IsItemActive())
				*state &= 0b010;
		}

		return ret;
	}
	bool FavoriteButton(const char* label, bool isFavorite)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		ImVec2 pos = window->DC.CursorPos;
		bool ret = ImGui::InvisibleButton(label, ImVec2(GUI_ELEMENT_SIZE, GUI_ELEMENT_SIZE));
		
		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();

		float size = window->DC.LastItemRect.Max.x - window->DC.LastItemRect.Min.x;

		int numPoints = 5;
		float innerRadius = size / 4;
		float outerRadius = size / 2;
		float angle = PI / numPoints;
		ImVec2 center = ImVec2(pos.x + size / 2, pos.y + size / 2);

		// fill
		if (isFavorite || hovered || active) {
			ImU32 fillColor = 0xff00ffff;// ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);
			if (hovered || active)
				fillColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]);

			// since there is no PathFillConcave, fill first the inner part, then the triangles
			// inner
			window->DrawList->PathClear();
			for (int i = 1; i < numPoints * 2; i += 2)
				window->DrawList->PathLineTo(ImVec2(center.x + innerRadius * sin(i * angle), center.y - innerRadius * cos(i * angle)));
			window->DrawList->PathFillConvex(fillColor);

			// triangles
			for (int i = 0; i < numPoints; i++) {
				window->DrawList->PathClear();

				int pIndex = i * 2;
				window->DrawList->PathLineTo(ImVec2(center.x + outerRadius * sin(pIndex * angle), center.y - outerRadius * cos(pIndex * angle)));
				window->DrawList->PathLineTo(ImVec2(center.x + innerRadius * sin((pIndex + 1) * angle), center.y - innerRadius * cos((pIndex + 1) * angle)));
				window->DrawList->PathLineTo(ImVec2(center.x + innerRadius * sin((pIndex - 1) * angle), center.y - innerRadius * cos((pIndex - 1) * angle)));

				window->DrawList->PathFillConvex(fillColor);
			}
		}

		// outline
		window->DrawList->PathClear();
		for (int i = 0; i < numPoints * 2; i++) {
			int radius = i & 1 ? innerRadius : outerRadius;
			window->DrawList->PathLineTo(ImVec2(center.x + radius * sin(i * angle), center.y - radius * cos(i * angle)));
		}
		window->DrawList->PathStroke(ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), true, 2.0f);

		return ret;
	}
	bool FileIcon(const char* label, ImTextureID icon, ImVec2 size, bool hasPreview, int previewWidth, int previewHeight)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		float windowSpace = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		ImVec2 pos = window->DC.CursorPos;
		bool ret = false;

		if (ImGui::InvisibleButton(label, size))
			ret = true;

		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();
		bool doubleClick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		if (doubleClick && hovered)
			ret = true;


		float iconSize = size.y - g.FontSize * 2;
		float iconPosX = pos.x + (size.x - iconSize) / 2.0f;
		ImVec2 textSize = ImGui::CalcTextSize(label, 0, true, size.x);

		if (hovered || active)
			window->DrawList->AddRectFilled(window->DC.LastItemRect.Min, window->DC.LastItemRect.Max, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]));
		
		if (hasPreview) {
			ImVec2 availSize = ImVec2(size.x, iconSize);

			float scale = std::min<float>(availSize.x / previewWidth, availSize.y / previewHeight);
			availSize.x = previewWidth * scale;
			availSize.y = previewHeight * scale;

			float previewPosX = pos.x + (size.x - availSize.x) / 2.0f;
			float previewPosY = pos.y + (iconSize - availSize.y) / 2.0f;

			window->DrawList->AddImage(icon, ImVec2(previewPosX, previewPosY), ImVec2(previewPosX + availSize.x, previewPosY + availSize.y));
		} else
			window->DrawList->AddImage(icon, ImVec2(iconPosX, pos.y), ImVec2(iconPosX + iconSize, pos.y + iconSize));
		
		window->DrawList->AddText(g.Font, g.FontSize, ImVec2(pos.x + (size.x-textSize.x) / 2.0f, pos.y + iconSize), ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), label, 0, size.x);


		float lastButtomPos = ImGui::GetItemRectMax().x;
		float thisButtonPos = lastButtomPos + style.ItemSpacing.x + size.x; // Expected position if next button was on same line
		if (thisButtonPos < windowSpace)
			ImGui::SameLine();

		return ret;
	}

	FileDialog::FileData::FileData(const std::wstring& path) {
		std::error_code ec;
		Path = path;
		DateModified = std::chrono::time_point_cast<std::chrono::seconds>(std::filesystem::last_write_time(path, ec)).time_since_epoch().count();
		IsDirectory = std::filesystem::is_directory(path, ec);
		Size = std::filesystem::file_size(path, ec);

		HasIconPreview = false;
		IconPreview = nullptr;
		IconPreviewData = nullptr;
		IconPreviewHeight = 0;
		IconPreviewWidth = 0;
	}

	FileDialog::FileDialog() {
		m_isOpen = false;
		m_type = 0;
		m_calledOpenPopup = false;
		m_sortColumn = 0;
		m_sortDirection = ImGuiSortDirection_Ascending;
		m_filterSelection = 0;
		m_inputTextbox[0] = 0;
		m_pathBuffer[0] = 0;
		m_searchBuffer[0] = 0;
		m_newEntryBuffer[0] = 0;
		m_selectedFileItem = -1;
		m_zoom = 1.0f;

		m_previewLoader = nullptr;
		m_previewLoaderRunning = false;

		m_setDirectory(std::filesystem::current_path().wstring(), false);

		// favorites are available on every OS
		FileTreeNode* quickAccess = new FileTreeNode(L"Quick Access");
		quickAccess->Read = true;
		m_treeCache.push_back(quickAccess);

#ifdef _WIN32
		wchar_t username[UNLEN + 1] = { 0 };
		DWORD username_len = UNLEN + 1;
		GetUserNameW(username, &username_len);

		std::wstring userPath = L"C:\\Users\\" + std::wstring(username) + L"\\";

		// Quick Access / Bookmarks
		quickAccess->Children.push_back(new FileTreeNode(userPath + L"Desktop"));
		quickAccess->Children.push_back(new FileTreeNode(userPath + L"Documents"));
		quickAccess->Children.push_back(new FileTreeNode(userPath + L"Downloads"));
		quickAccess->Children.push_back(new FileTreeNode(userPath + L"Pictures"));

		// OneDrive
		FileTreeNode* oneDrive = new FileTreeNode(userPath + L"OneDrive");
		m_treeCache.push_back(oneDrive);

		// This PC
		FileTreeNode* thisPC = new FileTreeNode(L"This PC");
		thisPC->Read = true;
		if (std::filesystem::exists(userPath + L"3D Objects"))
			thisPC->Children.push_back(new FileTreeNode(userPath + L"3D Objects"));
		thisPC->Children.push_back(new FileTreeNode(userPath + L"Desktop"));
		thisPC->Children.push_back(new FileTreeNode(userPath + L"Documents"));
		thisPC->Children.push_back(new FileTreeNode(userPath + L"Downloads"));
		thisPC->Children.push_back(new FileTreeNode(userPath + L"Music"));
		thisPC->Children.push_back(new FileTreeNode(userPath + L"Pictures"));
		thisPC->Children.push_back(new FileTreeNode(userPath + L"Videos"));
		DWORD d = GetLogicalDrives();
		for (int i = 0; i < 26; i++)
			if (d & (1 << i))
				thisPC->Children.push_back(new FileTreeNode(std::wstring(1, 'A' + i) + L":"));
		m_treeCache.push_back(thisPC);
#else
		std::error_code ec;

		// Quick Access
		struct passwd *pw;
		uid_t uid;
		uid = geteuid();
		pw = getpwuid(uid);
		if (pw) {
			std::string username(pw->pw_name);
			std::wstring homePath = L"/home/" + std::wstring(username.begin(), username.end());
			
			if (std::filesystem::exists(homePath, ec))
				quickAccess->Children.push_back(new FileTreeNode(homePath));
			if (std::filesystem::exists(homePath + L"/Desktop", ec))
				quickAccess->Children.push_back(new FileTreeNode(homePath + L"/Desktop"));
			if (std::filesystem::exists(homePath + L"/Documents", ec))
				quickAccess->Children.push_back(new FileTreeNode(homePath + L"/Documents"));
			if (std::filesystem::exists(homePath + L"/Downloads", ec))
				quickAccess->Children.push_back(new FileTreeNode(homePath + L"/Downloads"));
			if (std::filesystem::exists(homePath + L"/Pictures", ec))
				quickAccess->Children.push_back(new FileTreeNode(homePath + L"/Pictures"));
		}

		// This PC
		FileTreeNode* thisPC = new FileTreeNode(L"This PC");
		thisPC->Read = true;
		for (const auto& entry : std::filesystem::directory_iterator("/", ec)) {
			if (std::filesystem::is_directory(entry, ec))
				thisPC->Children.push_back(new FileTreeNode(entry.path().wstring()));
		}
		m_treeCache.push_back(thisPC);
#endif
	}
	FileDialog::~FileDialog() {
		m_clearIconPreview();
		m_clearIcons();

		for (auto fn : m_treeCache)
			m_clearTree(fn);
		m_treeCache.clear();
	}
	bool FileDialog::Save(const std::string& key, const std::string& title, const std::string& filter, const std::string& startingDir)
	{
		if (!m_currentKey.empty())
			return false;

		m_currentKey = key;
		m_currentTitle = title + "###" + key;
		m_isOpen = true;
		m_calledOpenPopup = false;
		m_result = L"";
		m_hasResult = false;
		m_inputTextbox[0] = 0;
		m_selectedFileItem = -1;

		m_type = IFD_DIALOG_SAVE;

		m_parseFilter(filter);
		if (!startingDir.empty())
			m_setDirectory(std::wstring(startingDir.begin(), startingDir.end()), false);
		else
			m_setDirectory(m_currentDirectory, false); // refresh contents

		return true;
	}
	bool FileDialog::Open(const std::string& key, const std::string& title, const std::string& filter, const std::string& startingDir)
	{
		if (!m_currentKey.empty())
			return false;

		m_currentKey = key;
		m_currentTitle = title + "###" + key;
		m_isOpen = true;
		m_calledOpenPopup = false;
		m_result = L"";
		m_hasResult = false;
		m_inputTextbox[0] = 0;
		m_selectedFileItem = -1;
		m_type = filter.empty() ? IFD_DIALOG_DIRECTORY : IFD_DIALOG_FILE;

		m_parseFilter(filter);
		if (!startingDir.empty())
			m_setDirectory(std::wstring(startingDir.begin(), startingDir.end()), false);
		else
			m_setDirectory(m_currentDirectory, false); // refresh contents

		return true;
	}
	bool FileDialog::IsDone(const std::string& key)
	{
		bool isMe = m_currentKey == key;

		if (isMe && m_isOpen) {
			if (!m_calledOpenPopup) {
				ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
				ImGui::OpenPopup(m_currentTitle.c_str());
				m_calledOpenPopup = true;
			}

			if (ImGui::BeginPopupModal(m_currentTitle.c_str(), &m_isOpen, ImGuiWindowFlags_NoScrollbar)) {
				m_renderFileDialog();
				ImGui::EndPopup();
			}
			else m_isOpen = false;
		}

		return isMe && !m_isOpen;
	}
	void FileDialog::Close()
	{
		m_currentKey.clear();
		m_backHistory = std::stack<std::wstring>();
		m_forwardHistory = std::stack<std::wstring>();

		// clear the tree
		for (auto fn : m_treeCache) {
			for (auto item : fn->Children) {
				for (auto ch : item->Children)
					m_clearTree(ch);
				item->Children.clear();
				item->Read = false;
			}
		}

		// free icon textures
		m_clearIconPreview();
		m_clearIcons();
	}

	void FileDialog::RemoveFavorite(const std::wstring& path)
	{
		auto itr = std::find(m_favorites.begin(), m_favorites.end(), m_currentDirectory);

		if (itr != m_favorites.end())
			m_favorites.erase(itr);

		// remove from sidebar
		for (auto& p : m_treeCache)
			if (p->Path == L"Quick Access") {
				for (int i = 0; i < p->Children.size(); i++)
					if (p->Children[i]->Path == path) {
						p->Children.erase(p->Children.begin() + i);
						break;
					}
				break;
			}
	}

	void FileDialog::AddFavorite(const std::wstring& path)
	{
		if (std::count(m_favorites.begin(), m_favorites.end(), path) > 0)
			return;

		if (!std::filesystem::exists(path))
			return;

		m_favorites.push_back(path);
		
		// add to sidebar
		for (auto& p : m_treeCache)
			if (p->Path == L"Quick Access") {
				p->Children.push_back(new FileTreeNode(path));
				break;
			}
	}
	
	bool FileDialog::m_finalize(const std::wstring& filename)
	{
		bool hasResult = (!filename.empty() && m_type != IFD_DIALOG_DIRECTORY) || m_type == IFD_DIALOG_DIRECTORY;
		
		if (hasResult) {
			std::filesystem::path path(filename);
			if (path.is_absolute())
				m_result = filename;
			else
				m_result = (std::filesystem::path(m_currentDirectory) / path).wstring();


			if (m_type == IFD_DIALOG_DIRECTORY || m_type == IFD_DIALOG_FILE) {
				if (!std::filesystem::exists(m_result)) {
					m_result = L"";
					return false;
				}
			}
			else if (m_type == IFD_DIALOG_SAVE) {
				std::filesystem::path resultPath(m_result);
				
				// add the extension
				if (m_filterSelection < m_filterExtensions.size() && m_filterExtensions[m_filterSelection].size() > 0) {
					if (!resultPath.has_extension()) {
						std::string extAdd = m_filterExtensions[m_filterSelection][0];
						m_result += std::wstring(extAdd.begin(), extAdd.end());
					}
				}
			}
		}

		m_hasResult = hasResult;
		m_isOpen = false;

		return true;
	}

	void FileDialog::m_parseFilter(const std::string& filter)
	{
		m_filter = "";
		m_filterExtensions.clear();
		m_filterSelection = 0;

		if (filter.empty())
			return;

		std::vector<std::string> exts;

		int lastSplit = 0, lastExt = 0;
		bool inExtList = false;
		for (int i = 0; i < filter.size(); i++) {
			if (filter[i] == ',') {
				if (!inExtList)
					lastSplit = i + 1;
				else {
					exts.push_back(filter.substr(lastExt, i - lastExt));
					lastExt = i + 1;
				}
			}
			else if (filter[i] == '{') {
				std::string filterName = filter.substr(lastSplit, i - lastSplit);
				if (filterName == ".*") {
					m_filter += std::string(std::string("All Files (*.*)\0").c_str(), 16);
					m_filterExtensions.push_back(std::vector<std::string>());
				}
				else
					m_filter += std::string((filterName + "\0").c_str(), filterName.size() + 1);
				inExtList = true;
				lastExt = i + 1;
			}
			else if (filter[i] == '}') {
				exts.push_back(filter.substr(lastExt, i - lastExt));
				m_filterExtensions.push_back(exts);
				exts.clear();

				inExtList = false;
			}
		}
		if (lastSplit != 0) {
			std::string filterName = filter.substr(lastSplit);
			if (filterName == ".*") {
				m_filter += std::string(std::string("All Files (*.*)\0").c_str(), 16);
				m_filterExtensions.push_back(std::vector<std::string>());
			}
			else
				m_filter += std::string((filterName + "\0").c_str(), filterName.size() + 1);
		}
	}

	void* FileDialog::m_getIcon(const std::wstring& path)
	{
#ifdef _WIN32
		if (m_icons.count(path) > 0)
			return m_icons[path];

		std::error_code ec;
		m_icons[path] = nullptr;

		DWORD attrs = 0;
		UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
		if (!std::filesystem::exists(path, ec)) {
			flags |= SHGFI_USEFILEATTRIBUTES;
			attrs = FILE_ATTRIBUTE_DIRECTORY;
		}

		SHFILEINFOW fileInfo = { 0 };
		std::wstring modPath = path;
		for (int i = 0; i < modPath.size(); i++)
			if (modPath[i] == '/')
				modPath[i] = '\\';
		SHGetFileInfoW(modPath.c_str(), attrs, &fileInfo, sizeof(SHFILEINFOW), flags);

		if (fileInfo.hIcon == nullptr)
			return nullptr;

		// check if icon is already loaded
		auto itr = std::find(m_iconIndices.begin(), m_iconIndices.end(), fileInfo.iIcon);
		if (itr != m_iconIndices.end()) {
			const std::wstring& existingIconFilepath = m_iconFilepaths[itr - m_iconIndices.begin()];
			m_icons[path] = m_icons[existingIconFilepath];
			return m_icons[path];
		}

		m_iconIndices.push_back(fileInfo.iIcon);
		m_iconFilepaths.push_back(path);

		ICONINFO iconInfo = { 0 };
		GetIconInfo(fileInfo.hIcon, &iconInfo);
		
		if (iconInfo.hbmColor == nullptr)
			return nullptr;

		DIBSECTION ds;
		GetObject(iconInfo.hbmColor, sizeof(ds), &ds);
		int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

		if (byteSize == 0)
			return nullptr;

		uint8_t* data = (uint8_t*)malloc(byteSize);
		GetBitmapBits(iconInfo.hbmColor, byteSize, data);

		m_icons[path] = this->CreateTexture(data, ds.dsBm.bmWidth, ds.dsBm.bmHeight, 0);

		free(data);

		return m_icons[path];
#else
		if (m_icons.count(path) > 0)
			return m_icons[path];

		m_icons[path] = nullptr;

		std::error_code ec;
		int iconID = 1;
		if (std::filesystem::is_directory(path, ec))
			iconID = 0;

		// check if icon is already loaded
		auto itr = std::find(m_iconIndices.begin(), m_iconIndices.end(), iconID);
		if (itr != m_iconIndices.end()) {
			const std::wstring& existingIconFilepath = m_iconFilepaths[itr - m_iconIndices.begin()];
			m_icons[path] = m_icons[existingIconFilepath];
			return m_icons[path];
		}

		m_iconIndices.push_back(iconID);
		m_iconFilepaths.push_back(path);

		ImVec4 wndBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);

		// light theme - load default icons
		if ((wndBg.x + wndBg.y + wndBg.z) / 3.0f > 0.5f) {
			uint8_t* data = (uint8_t*)ifd::GetDefaultFileIcon();
			if (iconID == 0)
				data = (uint8_t*)ifd::GetDefaultFolderIcon();
			m_icons[path] = this->CreateTexture(data, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, 0);
		}
		// dark theme - invert the colors
		else {
			uint8_t* data = (uint8_t*)ifd::GetDefaultFileIcon();
			if (iconID == 0)
				data = (uint8_t*)ifd::GetDefaultFolderIcon();

			uint8_t* invData = (uint8_t*)malloc(DEFAULT_ICON_SIZE * DEFAULT_ICON_SIZE * 4);
			for (int y = 0; y < 32; y++) {
				for (int x = 0; x < 32; x++) {
					int index = (y* DEFAULT_ICON_SIZE + x) * 4;
					invData[index + 0] = 255 - data[index + 0];
					invData[index + 1] = 255 - data[index + 1];
					invData[index + 2] = 255 - data[index + 2];
					invData[index + 3] = data[index + 3];
				}
			}
			m_icons[path] = this->CreateTexture(invData, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, 0);

			free(invData);
		}

		return m_icons[path];
#endif
	}
	void FileDialog::m_clearIcons()
	{
		std::vector<unsigned int> deletedIcons;

		// delete textures
		for (auto& icon : m_icons) {
			unsigned int ptr = (unsigned int)((uintptr_t)icon.second);
			if (std::count(deletedIcons.begin(), deletedIcons.end(), ptr)) // skip duplicates
				continue;

			deletedIcons.push_back(ptr);
			DeleteTexture(icon.second);
		}
		m_iconFilepaths.clear();
		m_iconIndices.clear();
		m_icons.clear();
	}
	void FileDialog::m_refreshIconPreview()
	{
		if (m_zoom >= 5.0f) {
			if (m_previewLoader == nullptr) {
				m_previewLoaderRunning = true;
				m_previewLoader = new std::thread(&FileDialog::m_loadPreview, this);
			}
		} else
			m_clearIconPreview();
	}
	void FileDialog::m_clearIconPreview()
	{
		m_stopPreviewLoader();

		for (auto& data : m_content) {
			if (!data.HasIconPreview)
				continue;

			data.HasIconPreview = false;
			this->DeleteTexture(data.IconPreview);

			if (data.IconPreviewData != nullptr) {
				stbi_image_free(data.IconPreviewData);
				data.IconPreviewData = nullptr;
			}
		}
	}
	void FileDialog::m_stopPreviewLoader()
	{
		if (m_previewLoader != nullptr) {
			m_previewLoaderRunning = false;

			if (m_previewLoader && m_previewLoader->joinable())
				m_previewLoader->join();

			delete m_previewLoader;
			m_previewLoader = nullptr;
		}
	}
	void FileDialog::m_loadPreview()
	{
		for (int i = 0; m_previewLoaderRunning && i < m_content.size(); i++) {
			auto& data = m_content[i];

			if (data.HasIconPreview)
				continue;

			std::filesystem::path path(data.Path);
			if (path.has_extension()) {
				std::string ext = path.extension().string();
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
					int width, height, nrChannels;
					unsigned char* image = stbi_load(std::string(data.Path.begin(), data.Path.end()).c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);

					if (image == nullptr || width == 0 || height == 0)
						continue;

					data.HasIconPreview = true;
					data.IconPreviewData = image;
					data.IconPreviewWidth = width;
					data.IconPreviewHeight = height;
				}
			}
		}
		m_previewLoaderRunning = false;
	}
	void FileDialog::m_clearTree(FileTreeNode* node)
	{
		if (node == nullptr)
			return;

		for (auto n : node->Children)
			m_clearTree(n);

		delete node;
		node = nullptr;
	}
	void FileDialog::m_setDirectory(const std::wstring& p, bool addHistory)
	{
		bool isSameDir = m_currentDirectory == p;

		if (addHistory && !isSameDir)
			m_backHistory.push(m_currentDirectory);

		m_currentDirectory = p;
		m_clearIconPreview();
		m_content.clear(); // p == "" after this line, due to reference
		m_selectedFileItem = -1;
		
		if (m_type == IFD_DIALOG_DIRECTORY)
			m_inputTextbox[0] = 0;

		if (!isSameDir) {
			m_searchBuffer[0] = 0;
			m_clearIcons();
		}

		if (p == L"Quick Access") {
			for (auto& node : m_treeCache) {
				if (node->Path == p)
					for (auto& c : node->Children)
						m_content.push_back(FileData(c->Path));
			}
		} 
		else if (p == L"This PC") {
			for (auto& node : m_treeCache) {
				if (node->Path == p)
					for (auto& c : node->Children)
						m_content.push_back(FileData(c->Path));
			}
		}
		else {
			bool isDrive = m_currentDirectory.size() == 2 && m_currentDirectory[1] == ':';
			std::error_code ec;
			if (std::filesystem::exists(m_currentDirectory, ec))
				for (const auto& entry : std::filesystem::directory_iterator(isDrive ? (m_currentDirectory + L"\\") : m_currentDirectory, ec)) {
					FileData info(entry.path().wstring());
					std::filesystem::path infoPath(info.Path);

					// skip files when IFD_DIALOG_DIRECTORY
					if (!info.IsDirectory && m_type == IFD_DIALOG_DIRECTORY)
						continue;

					// check if filename matches search query
					if (m_searchBuffer[0]) {
						std::wstring filename = infoPath.filename().wstring();
						std::string filenameStr(filename.begin(), filename.end());

						std::string filenameSearch = filenameStr;
						std::string query(m_searchBuffer);
						std::transform(filenameSearch.begin(), filenameSearch.end(), filenameSearch.begin(), ::tolower);
						std::transform(query.begin(), query.end(), query.begin(), ::tolower);

						if (filenameSearch.find(query, 0) == std::string::npos)
							continue;
					}

					// check if extension matches
					if (!info.IsDirectory && m_type != IFD_DIALOG_DIRECTORY) {
						if (m_filterSelection < m_filterExtensions.size()) {
							const auto& exts = m_filterExtensions[m_filterSelection];
							if (exts.size() > 0) {
								std::wstring extension = infoPath.extension().wstring();
								std::string extensionStr(extension.begin(), extension.end());

								// extension not found? skip
								if (std::count(exts.begin(), exts.end(), extensionStr) == 0)
									continue;
							}
						}
					}

					m_content.push_back(info);
				}
		}

		m_sortContent(m_sortColumn, m_sortDirection);
		m_refreshIconPreview();
	}
	void FileDialog::m_sortContent(unsigned int column, unsigned int sortDirection)
	{
		// 0 -> name, 1 -> date, 2 -> size
		m_sortColumn = column;
		m_sortDirection = sortDirection;

		// split into directories and files
		std::partition(m_content.begin(), m_content.end(), [](const FileData& data) {
			return data.IsDirectory;
		});

		if (m_content.size() > 0) {
			// find where the file list starts
			int fileIndex = 0;
			for (; fileIndex < m_content.size(); fileIndex++)
				if (!m_content[fileIndex].IsDirectory)
					break;

			// compare function
			auto compareFn = [column, sortDirection](const FileData& left, const FileData& right) -> bool {
				// name
				if (column == 0) {
					std::wstring lName = std::filesystem::path(left.Path).wstring();
					std::wstring rName = std::filesystem::path(right.Path).wstring();

					int comp = lName.compare(rName);

					if (sortDirection == ImGuiSortDirection_Ascending)
						return comp < 0;
					return comp > 0;
				}
				// date
				else if (column == 1) {
					if (sortDirection == ImGuiSortDirection_Ascending)
						return left.DateModified < right.DateModified;
					else
						return left.DateModified > right.DateModified;
				}
				// size
				else if (column == 2) {
					if (sortDirection == ImGuiSortDirection_Ascending)
						return left.Size < right.Size;
					else
						return left.Size > right.Size;
				}

				return false;
			};

			// sort the directories
			std::sort(m_content.begin(), m_content.begin() + fileIndex, compareFn);

			// sort the files
			std::sort(m_content.begin() + fileIndex, m_content.end(), compareFn);
		}
	}

	void FileDialog::m_renderTree(FileTreeNode* node)
	{
		// directory
		std::error_code ec;
		ImGui::PushID(node);
		bool isClicked = false;
		bool isDrive = node->Path.size() == 2 && node->Path[1] == ':';
		std::wstring wpath = isDrive ? node->Path.c_str() : std::filesystem::path(node->Path).stem().wstring().c_str();
		if (FolderNode(std::string(wpath.begin(), wpath.end()).c_str(), (ImTextureID)m_getIcon(node->Path), isClicked)) {
			if (!node->Read) {
				// cache children if it's not already cached
				if (std::filesystem::exists(node->Path, ec))
					for (const auto& entry : std::filesystem::directory_iterator(isDrive ? (node->Path + L"\\") : node->Path, ec)) {
						if (std::filesystem::is_directory(entry, ec))
							node->Children.push_back(new FileTreeNode(entry.path().wstring()));
					}
				node->Read = true;
			}

			// display children
			for (auto c : node->Children)
				m_renderTree(c);

			ImGui::TreePop();
		}
		if (isClicked)
			m_setDirectory(node->Path);
		ImGui::PopID();
	}
	void FileDialog::m_renderContent()
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			m_selectedFileItem = -1;

		// table view
		if (m_zoom == 1.0f) {
			if (ImGui::BeginTable("##contentTable", 3, ImGuiTableFlags_ScrollFreezeTopRow | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable, ImVec2(0, -1))) {
				// header
				ImGui::TableSetupColumn("Name##filename", ImGuiTableColumnFlags_WidthStretch, -1, 0);
				ImGui::TableSetupColumn("Date modified##filedate", ImGuiTableColumnFlags_WidthAlwaysAutoResize, -1, 1);
				ImGui::TableSetupColumn("Size##filesize", ImGuiTableColumnFlags_WidthAlwaysAutoResize, -1, 2);
				ImGui::TableAutoHeaders();

				// sort
				if (const ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
					if (sortSpecs->SpecsChanged)
						m_sortContent(sortSpecs->Specs->ColumnUserID, sortSpecs->Specs->SortDirection);
				}

				// content
				int fileId = 0;
				for (auto& entry : m_content) {
					std::wstring filename = std::filesystem::path(entry.Path).filename().wstring();
					if (filename.size() == 0)
						filename = entry.Path; // drive
					std::string filenameStr(filename.begin(), filename.end());
					
					ImGui::TableNextRow();

					// file name
					ImGui::TableSetColumnIndex(0);
					ImGui::Image((ImTextureID)m_getIcon(entry.Path), ImVec2(ICON_SIZE, ICON_SIZE));
					ImGui::SameLine();
					if (ImGui::Selectable(filenameStr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
						std::error_code ec;
						bool isDir = std::filesystem::is_directory(entry.Path, ec);

						if (ImGui::IsMouseDoubleClicked(0)) {
							if (isDir) {
								m_setDirectory(entry.Path);
								break;
							} else
								m_finalize(filename);
						} else {
							if ((isDir && m_type == IFD_DIALOG_DIRECTORY) || !isDir)
								strcpy(m_inputTextbox, filenameStr.c_str());
						}
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						m_selectedFileItem = fileId;
					fileId++;

					// date
					ImGui::TableSetColumnIndex(1);
					auto tm = std::localtime(&entry.DateModified);
					if (tm != nullptr)
						ImGui::Text("%d/%d/%d %02d:%02d", tm->tm_mon + 1, tm->tm_mday, 1900 - 369 + tm->tm_year, tm->tm_hour, tm->tm_min); // idk why the year is wrong (have to -369)
					else ImGui::Text("---");

					// size
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%.3f KiB", entry.Size/1024.0f);
				}

				ImGui::EndTable();
			}
		}
		// "icon" view
		else {
			// content
			int fileId = 0;
			for (auto& entry : m_content) {
				if (entry.HasIconPreview && entry.IconPreviewData != nullptr) {
					entry.IconPreview = this->CreateTexture(entry.IconPreviewData, entry.IconPreviewWidth, entry.IconPreviewHeight, 1);
					stbi_image_free(entry.IconPreviewData);
					entry.IconPreviewData = nullptr;
				}

				std::wstring filename = std::filesystem::path(entry.Path).filename().wstring();
				if (filename.size() == 0)
					filename = entry.Path; // drive
				std::string filenameStr(filename.begin(), filename.end());

				if (FileIcon(filenameStr.c_str(), entry.HasIconPreview ? entry.IconPreview : (ImTextureID)m_getIcon(entry.Path), ImVec2(32 + 16 * m_zoom, 32 + 16 * m_zoom), entry.HasIconPreview, entry.IconPreviewWidth, entry.IconPreviewHeight)) {
					std::error_code ec;
					bool isDir = std::filesystem::is_directory(entry.Path, ec);

					if (ImGui::IsMouseDoubleClicked(0)) {
						if (isDir) {
							m_setDirectory(entry.Path);
							break;
						}
						else
							m_finalize(filename);
					}
					else {
						if ((isDir && m_type == IFD_DIALOG_DIRECTORY) || !isDir)
							strcpy(m_inputTextbox, filenameStr.c_str());
					}
				}
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
					m_selectedFileItem = fileId;
				fileId++;
			}
		}
	}
	void FileDialog::m_renderPopups()
	{
		bool openAreYouSureDlg = false, openNewFileDlg = false, openNewDirectoryDlg = false;
		if (ImGui::BeginPopupContextItem("##dir_context")) {
			if (ImGui::Selectable("New file"))
				openNewFileDlg = true;
			if (ImGui::Selectable("New directory"))
				openNewDirectoryDlg = true;
			if (m_selectedFileItem != -1 && ImGui::Selectable("Delete"))
				openAreYouSureDlg = true;
			ImGui::EndPopup();
		}
		if (openAreYouSureDlg)
			ImGui::OpenPopup("Are you sure?##delete");
		if (openNewFileDlg)
			ImGui::OpenPopup("Enter file name##newfile");
		if (openNewDirectoryDlg)
			ImGui::OpenPopup("Enter directory name##newdir");
		if (ImGui::BeginPopupModal("Are you sure?##delete")) {
			if (m_selectedFileItem >= m_content.size() || m_content.size() == 0)
				ImGui::CloseCurrentPopup();
			else {
				const FileData& data = m_content[m_selectedFileItem];
				ImGui::TextWrapped("Are you sure you want to delete %s?", std::string(data.Path.begin(), data.Path.end()).c_str());
				if (ImGui::Button("Yes")) {
					std::error_code ec;
					std::filesystem::remove_all(std::filesystem::path(data.Path), ec);
					m_setDirectory(m_currentDirectory, false); // refresh
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("No"))
					ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Enter file name##newfile")) {
			ImGui::PushItemWidth(250.0f);
			ImGui::InputText("##newfilename", m_newEntryBuffer, 1024); // TODO: remove hardcoded literals
			ImGui::PopItemWidth();

			if (ImGui::Button("OK")) {
				std::ofstream out((std::filesystem::path(m_currentDirectory) / std::string(m_newEntryBuffer)).string());
				out << "";
				out.close();

				m_setDirectory(m_currentDirectory, false); // refresh
				m_newEntryBuffer[0] = 0;

				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				m_newEntryBuffer[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal("Enter directory name##newdir")) {
			ImGui::PushItemWidth(250.0f);
			ImGui::InputText("##newfilename", m_newEntryBuffer, 1024); // TODO: remove hardcoded literals
			ImGui::PopItemWidth();

			if (ImGui::Button("OK")) {
				std::error_code ec;
				std::filesystem::create_directory(std::filesystem::path(m_currentDirectory) / std::string(m_newEntryBuffer), ec);
				m_setDirectory(m_currentDirectory, false); // refresh
				m_newEntryBuffer[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
				m_newEntryBuffer[0] = 0;
			}
			ImGui::EndPopup();
		}
	}
	void FileDialog::m_renderFileDialog()
	{
		/***** TOP BAR *****/
		bool noBackHistory = m_backHistory.empty(), noForwardHistory = m_forwardHistory.empty();
		
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		if (noBackHistory) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		if (ImGui::ArrowButtonEx("##back", ImGuiDir_Left, ImVec2(GUI_ELEMENT_SIZE, GUI_ELEMENT_SIZE), m_backHistory.empty() * ImGuiButtonFlags_Disabled)) {
			std::wstring newPath = m_backHistory.top();
			m_backHistory.pop();
			m_forwardHistory.push(m_currentDirectory);

			m_setDirectory(newPath, false);
		}
		if (noBackHistory) ImGui::PopStyleVar();
		ImGui::SameLine();
		
		if (noForwardHistory) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		if (ImGui::ArrowButtonEx("##forward", ImGuiDir_Right, ImVec2(GUI_ELEMENT_SIZE, GUI_ELEMENT_SIZE), m_forwardHistory.empty() * ImGuiButtonFlags_Disabled)) {
			std::wstring newPath = m_forwardHistory.top();
			m_forwardHistory.pop();
			m_backHistory.push(m_currentDirectory);

			m_setDirectory(newPath, false);
		}
		if (noForwardHistory) ImGui::PopStyleVar();
		ImGui::SameLine();
		
		if (ImGui::ArrowButtonEx("##up", ImGuiDir_Up, ImVec2(GUI_ELEMENT_SIZE, GUI_ELEMENT_SIZE))) {
			if (std::filesystem::path(m_currentDirectory).has_parent_path())
				m_setDirectory(std::filesystem::path(m_currentDirectory).parent_path().wstring());
		}
		
		std::wstring curDirCopy = m_currentDirectory;
		if (PathBox("##pathbox", curDirCopy, m_pathBuffer, ImVec2(-250, GUI_ELEMENT_SIZE)))
			m_setDirectory(curDirCopy);
		ImGui::SameLine();
		
		if (FavoriteButton("##dirfav", std::count(m_favorites.begin(), m_favorites.end(), m_currentDirectory))) {
			if (std::count(m_favorites.begin(), m_favorites.end(), m_currentDirectory))
				RemoveFavorite(m_currentDirectory);
			else 
				AddFavorite(m_currentDirectory);
		}
		ImGui::SameLine();
		ImGui::PopStyleColor();

		if (ImGui::InputTextEx("##searchTB", "Search", m_searchBuffer, 128, ImVec2(-1, GUI_ELEMENT_SIZE), 0)) // TODO: no hardcoded literals
			m_setDirectory(m_currentDirectory, false); // refresh



		/***** CONTENT *****/
		float bottomBarHeight = (GImGui->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f) * 2;
		if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_Resizable, ImVec2(0, -bottomBarHeight))) {
			ImGui::TableSetupColumn("##tree", ImGuiTableColumnFlags_WidthFixed, 160.0f);
			ImGui::TableSetupColumn("##content", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			// the tree on the left side
			ImGui::TableSetColumnIndex(0);
			ImGui::BeginChild("##treeContainer", ImVec2(0, -bottomBarHeight));
			for (auto node : m_treeCache)
				m_renderTree(node);
			ImGui::EndChild();
			
			// content on the right side
			ImGui::TableSetColumnIndex(1);
			ImGui::BeginChild("##contentContainer", ImVec2(0, -bottomBarHeight));
				m_renderContent();
			ImGui::EndChild();
			if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
				m_zoom = std::min<float>(25.0f, std::max<float>(1.0f, m_zoom + ImGui::GetIO().MouseWheel));
				m_refreshIconPreview();
			}

			// New file, New directory and Delete popups
			m_renderPopups();

			ImGui::EndTable();
		}


		
		/***** BOTTOM BAR *****/
		ImGui::Text("File name:");
		ImGui::SameLine();
		if (ImGui::InputTextEx("##file_input", "Filename", m_inputTextbox, 1024, ImVec2((m_type != IFD_DIALOG_DIRECTORY) ? -250 : -1, 0), ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string filename(m_inputTextbox);
			bool success = m_finalize(std::wstring(filename.begin(), filename.end()));
#ifdef _WIN32
			if (!success)
				MessageBeep(MB_ICONERROR);
#endif
		}
		if (m_type != IFD_DIALOG_DIRECTORY) {
			ImGui::SameLine();
			ImGui::PushItemWidth(-1);
			if (ImGui::Combo("##ext_combo", &m_filterSelection, m_filter.c_str()))
				m_setDirectory(m_currentDirectory, false); // refresh
			ImGui::PopItemWidth();
		}

		// buttons
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 250);
		if (ImGui::Button(m_type == IFD_DIALOG_SAVE ? "Save" : "Open", ImVec2(250 / 2 - ImGui::GetStyle().ItemSpacing.x, 0.0f))) {
			std::string filename(m_inputTextbox);
			bool success = false;
			if (!filename.empty() || m_type == IFD_DIALOG_DIRECTORY)
				success = m_finalize(std::wstring(filename.begin(), filename.end()));
#ifdef _WIN32
			if (!success)
				MessageBeep(MB_ICONERROR);
#endif
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(-1, 0.0f)))
			m_finalize();
	}
}


static const unsigned int file_icon[] = {
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x4c000000, 0xf5000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xdd000000, 0x2d000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0xd1000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6a000000, 0xa1000000, 0xff000000, 0xff000000, 0x2e000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x54000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x46000000, 0xf5000000, 0xe0000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6e000000, 0xf8000000, 0x01000000, 0xc3000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00000000, 0x00000000, 0xd2000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x13000000, 0x00000000, 0x00000000, 0xd2000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x73000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xbe000000, 0xff000000, 0x30000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x65000000, 0xff000000, 0x34000000, 0x10000000, 0x10000000, 0x03000000, 0x0a000000, 0xdb000000, 0xff000000, 0x2f000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0f000000, 0xd9000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xed000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x06000000, 0x5e000000, 0x6c000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x60000000, 0x9e000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x52000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x54000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x54000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0xd2000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0xd2000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x4c000000, 0xf5000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xf5000000, 0x4b000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
};
static const unsigned int folder_icon[] = {
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00000000, 0x00000000, 0x45000000, 0x8a000000, 0x99000000, 0x97000000, 0x97000000, 0x97000000, 0x97000000, 0x97000000, 0x98000000, 0x81000000, 0x35000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x9e000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x76000000, 0xff000000, 0xff000000, 0xf6000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xff000000, 0xff000000, 0xff000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0xe7000000, 0xff000000, 0xbe000000, 0x11000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1e000000, 0xd1000000, 0xff000000, 0xff000000, 0x75000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0xfa000000, 0xff000000, 0x5a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x06000000, 0xe0000000, 0xff000000, 0xff000000, 0x68000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0xf4000000, 0xff000000, 0x67000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0xe4000000, 0xff000000, 0xff000000, 0xad000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x96000000, 0x8b000000, 0x4f000000, 0x00000000, 0x00000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x17000000, 0xe8000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xaf000000, 0x00000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0e000000, 0x88000000, 0xc3000000, 0xcd000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcb000000, 0xcc000000, 0xe2000000, 0xff000000, 0xff000000, 0x81000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xb6000000, 0xff000000, 0xec000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5b000000, 0xff000000, 0xf9000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x68000000, 0xff000000, 0xf4000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
 0xf4000000, 0xff000000, 0x68000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x68000000, 0xff000000, 0xf4000000,
 0xfa000000, 0xff000000, 0x5a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5a000000, 0xff000000, 0xf9000000,
 0xea000000, 0xff000000, 0xb5000000, 0x05000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x05000000, 0xb5000000, 0xff000000, 0xea000000,
 0x7e000000, 0xff000000, 0xff000000, 0xeb000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xeb000000, 0xff000000, 0xff000000, 0x7f000000,
 0x00000000, 0xac000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xac000000, 0x00000000,
 0x00000000, 0x00000000, 0x53000000, 0x8f000000, 0x9a000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x9a000000, 0x8f000000, 0x53000000, 0x00000000, 0x00000000,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
};
const char* ifd::GetDefaultFolderIcon()
{
	return (const char*)&folder_icon[0];
}
const char* ifd::GetDefaultFileIcon()
{
	return (const char*)&file_icon[0];
}