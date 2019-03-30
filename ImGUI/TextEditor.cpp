#include <algorithm>
#include <functional>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>

#include "TextEditor.h"

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML
// - handle non-monospace fonts
// - handle unicode/utf
// - testing

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1,
	InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
	for (; first1 != last1 && first2 != last2; ++first1, ++first2) 
	{
		if (!p(*first1, *first2))
			return false;
	}
	return first1 == last1 && first2 == last2;
}

TextEditor::TextEditor()
	: mLineSpacing(0.0f)
	, mUndoIndex(0)
	, mInsertSpaces(false)
	, mTabSize(4)
	, mAutocomplete(true)
	, mACOpened(false)
	, mHighlightLine(true)
	, mHorizontalScroll(true)
	, mCompleteBraces(true)
	, mShowLineNumbers(true)
	, mSmartIndent(true)
	, mOverwrite(false)
	, mReadOnly(false)
	, mWithinRender(false)
	, mScrollToCursor(false)
	, mTextChanged(false)
	, mColorRangeMin(0)
	, mColorRangeMax(0)
	, mSelectionMode(SelectionMode::Normal)
	, mCheckMultilineComments(true)
{
	SetPalette(GetDarkPalette());
	SetLanguageDefinition(LanguageDefinition::HLSL());
	mLines.push_back(Line());
}


TextEditor::~TextEditor()
{
}

void TextEditor::SetLanguageDefinition(const LanguageDefinition & aLanguageDef)
{
	mLanguageDefinition = aLanguageDef;
	mRegexList.clear();

	for (auto& r : mLanguageDefinition.mTokenRegexStrings)
		mRegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));
}

void TextEditor::SetPalette(const Palette & aValue)
{
	mPalette = aValue;
}

int TextEditor::AppendBuffer(std::string& aBuffer, char chr, int aIndex)
{
	if (chr != '\t')
	{
		aBuffer.push_back(chr);
		return aIndex + 1;
	}
	else
	{
		auto num = mTabSize - aIndex % mTabSize;
		for (int j = num; j > 0; --j)
			aBuffer.push_back(' ');
		return aIndex + num;
	}
}

std::string TextEditor::GetText(const Coordinates & aStart, const Coordinates & aEnd) const
{
	std::string result;

	int prevLineNo = aStart.mLine;
	for (auto it = aStart; it <= aEnd; Advance(it))
	{
		if (prevLineNo != it.mLine && it.mLine < (int) mLines.size())
			result.push_back('\n');

		if (it == aEnd)
			break;

		prevLineNo = it.mLine;
		const auto& line = mLines[it.mLine];
		if (!line.empty() && it.mColumn < (int)line.size())
			result.push_back(line[it.mColumn].mChar);
	}

	return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates() const
{
	return SanitizeCoordinates(mState.mCursorPosition);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates & aValue) const
{
	auto line = aValue.mLine;
	auto column = aValue.mColumn;

	if (line >= (int)mLines.size())
	{
		line = (int)mLines.size() - 1;
		column = mLines.empty() ? 0 : (int)mLines[line].size();
	}
	else
	{
		column = mLines.empty() ? 0 : std::min<int>((int)mLines[line].size(), aValue.mColumn);
	}

	return Coordinates(line, column);
}

void TextEditor::Advance(Coordinates & aCoordinates) const
{
	if (aCoordinates.mLine < (int)mLines.size())
	{
		auto& line = mLines[aCoordinates.mLine];

		if (aCoordinates.mColumn + 1 < (int)line.size())
			++aCoordinates.mColumn;
		else
		{
			++aCoordinates.mLine;
			aCoordinates.mColumn = 0;
		}
	}
}

void TextEditor::DeleteRange(const Coordinates & aStart, const Coordinates & aEnd)
{
	assert(aEnd >= aStart);
	assert(!mReadOnly);

	if (aEnd == aStart)
		return;

	if (aStart.mLine == aEnd.mLine)
	{
		auto& line = mLines[aStart.mLine];
		if (aEnd.mColumn >= (int)line.size())
			line.erase(line.begin() + aStart.mColumn, line.end());
		else
			line.erase(line.begin() + aStart.mColumn, line.begin() + aEnd.mColumn);
	}
	else
	{
		auto& firstLine = mLines[aStart.mLine];
		auto& lastLine = mLines[aEnd.mLine];

		firstLine.erase(firstLine.begin() + aStart.mColumn, firstLine.end());
		lastLine.erase(lastLine.begin(), lastLine.begin() + aEnd.mColumn);

		if (aStart.mLine < aEnd.mLine)
			firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

		if (aStart.mLine < aEnd.mLine)
			RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
	}

	mTextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates& /* inout */ aWhere, const char * aValue)
{
	assert(!mReadOnly);

	int totalLines = 0;
	auto chr = *aValue;
	while (chr != '\0')
	{
		if (mLines.empty())
			mLines.push_back(Line());

		if (chr == '\r')
		{
			// skip
		}
		else if (chr == '\n')
		{
			if (aWhere.mColumn < (int)mLines[aWhere.mLine].size())
			{
				auto& newLine = InsertLine(aWhere.mLine + 1);
				auto& line = mLines[aWhere.mLine];
				newLine.insert(newLine.begin(), line.begin() + aWhere.mColumn, line.end());
				line.erase(line.begin() + aWhere.mColumn, line.end());
			}
			else
			{
				InsertLine(aWhere.mLine + 1);
			}
			++aWhere.mLine;
			aWhere.mColumn = 0;
			++totalLines;
		}
		else
		{
			auto& line = mLines[aWhere.mLine];
			line.insert(line.begin() + aWhere.mColumn, Glyph(chr, PaletteIndex::Default));
			++aWhere.mColumn;
		}
		chr = *(++aValue);

		mTextChanged = true;
	}

	return totalLines;
}

void TextEditor::AddUndo(UndoRecord& aValue)
{
	assert(!mReadOnly);

	mUndoBuffer.resize(mUndoIndex + 1);
	mUndoBuffer.back() = aValue;
	++mUndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2& aPosition) const
{
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);
	
	int lineNo = std::max<int>(0, (int)floor(local.y / mCharAdvance.y));
	int columnCoord = std::max<int>(0, (int)floor(local.x / mCharAdvance.x) - GetTextStart());

	int column = 0;
	if (lineNo >= 0 && lineNo < (int)mLines.size())
	{
		auto& line = mLines[lineNo];
		auto distance = 0;
		while (distance < columnCoord && column < (int)line.size())
		{
			if (line[column].mChar == '\t')
				distance = (distance / mTabSize) * mTabSize + mTabSize;
			else
				++distance;
			++column;
		}
	}
	return Coordinates(lineNo, column);
}

ImVec2 TextEditor::CoordinatesToScreenPos(const TextEditor::Coordinates& aPosition) const
{
	ImVec2 origin = ImGui::GetCursorScreenPos();
	int dist = 0;

	auto& line = mLines[aPosition.mLine];
	for (int i = 0; i < std::min(line.size(), (size_t)aPosition.mColumn); i++) {
		if (line[i].mChar == '\t')
			dist += mTabSize;
		else dist++;
	}


	int retY = origin.y + aPosition.mLine * mCharAdvance.y;
	int retX = origin.x + GetTextStart()*mCharAdvance.x + dist * mCharAdvance.x - ImGui::GetScrollX();

	return ImVec2(retX, retY);
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];

	if (at.mColumn >= (int)line.size())
		return at;

	auto cstart = (PaletteIndex)line[at.mColumn].mColorIndex;
	while (at.mColumn > 0)
	{
		if (cstart != (PaletteIndex)line[at.mColumn - 1].mColorIndex)
			break;
		--at.mColumn;
	}
	return at;
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates & aFrom) const
{
	Coordinates at = aFrom;
	if (at.mLine >= (int)mLines.size())
		return at;

	auto& line = mLines[at.mLine];

	if (at.mColumn >= (int)line.size())
		return at;

	auto cstart = (PaletteIndex)line[at.mColumn].mColorIndex;
	while (at.mColumn < (int)line.size())
	{
		if (cstart != (PaletteIndex)line[at.mColumn].mColorIndex)
			break;
		++at.mColumn;
	}
	return at;
}

bool TextEditor::IsOnWordBoundary(const Coordinates & aAt) const
{
	if (aAt.mLine >= (int)mLines.size() || aAt.mColumn == 0)
		return true;

	auto& line = mLines[aAt.mLine];
	if (aAt.mColumn >= (int)line.size())
		return true;

	return line[aAt.mColumn].mColorIndex != line[aAt.mColumn - 1].mColorIndex;
}

void TextEditor::RemoveLine(int aStart, int aEnd)
{
	assert(!mReadOnly);

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
		if (e.first >= aStart && e.first <= aEnd)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i >= aStart && i <= aEnd)
			continue;
		btmp.insert(i >= aStart ? i - 1 : i);
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);

	mTextChanged = true;
}

void TextEditor::RemoveLine(int aIndex)
{
	assert(!mReadOnly);

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
	{
		ErrorMarkers::value_type e(i.first >= aIndex ? i.first - 1 : i.first, i.second);
		if (e.first == aIndex)
			continue;
		etmp.insert(e);
	}
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
	{
		if (i == aIndex)
			continue;
		btmp.insert(i >= aIndex ? i - 1 : i);
	}
	mBreakpoints = std::move(btmp);

	mLines.erase(mLines.begin() + aIndex);

	mTextChanged = true;
}

TextEditor::Line& TextEditor::InsertLine(int aIndex)
{
	assert(!mReadOnly);

	auto& result = *mLines.insert(mLines.begin() + aIndex, Line());

	ErrorMarkers etmp;
	for (auto& i : mErrorMarkers)
		etmp.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
	mErrorMarkers = std::move(etmp);

	Breakpoints btmp;
	for (auto i : mBreakpoints)
		btmp.insert(i >= aIndex ? i + 1 : i);
	mBreakpoints = std::move(btmp);

	return result;
}

std::string TextEditor::GetWordUnderCursor() const
{
	auto c = GetCursorPosition();
	c.mColumn = std::max(c.mColumn-1, 0);
	return GetWordAt(c);
}

std::string TextEditor::GetWordAt(const Coordinates & aCoords) const
{
	auto start = FindWordStart(aCoords);
	auto end = FindWordEnd(aCoords);

	std::string r;

	for (auto it = start; it < end; Advance(it))
		r.push_back(mLines[it.mLine][it.mColumn].mChar);

	return r;
}

void TextEditor::Render(const char* aTitle, const ImVec2& aSize, bool aBorder)
{
	mWithinRender = true;
	mTextChanged = false;

	ImGuiIO& io = ImGui::GetIO();
	auto xadv = (ImGui::GetFont()->IndexAdvanceX['X']);
	mCharAdvance = ImVec2(io.FontGlobalScale * xadv, io.FontGlobalScale * ImGui::GetFontSize() + mLineSpacing);
	
	ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int)PaletteIndex::Background]));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::BeginChild(aTitle, aSize, aBorder, (ImGuiWindowFlags_HorizontalScrollbar * mHorizontalScroll) | ImGuiWindowFlags_NoMove);

	ImGui::PushAllowKeyboardFocus(true);

	auto shift = io.KeyShift;

	auto ctrl = io.KeyCtrl;
	auto alt = io.KeyAlt;
	mFocused = ImGui::IsWindowFocused();

	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		//ImGui::CaptureKeyboardFromApp(true);

		io.WantCaptureKeyboard = true;
		io.WantTextInput = true;

		if (mACOpened) {
			int keyCount = 0;

			for (size_t i = 0; i < sizeof(io.InputCharacters) / sizeof(io.InputCharacters[0]); i++)
				if (io.InputCharacters[i] != 0)
					keyCount++;
			for (size_t i = 0; i < ImGuiKey_COUNT; i++)
				keyCount+=ImGui::IsKeyPressed(ImGui::GetKeyIndex(i));

			if (keyCount != 0) {
				if (!ctrl && !alt && !shift && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
					mACIndex = std::max(mACIndex - 1, 0), mACSwitched = true;
				else if (!ctrl && !alt && !shift && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
					mACIndex = std::min(mACIndex + 1, (int)mACSuggestions.size()), mACSwitched = true;
				else if (!ctrl && !alt && !shift && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) {}
				else if (mACSwitched && !ctrl && !alt && !shift && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {}
				else mACOpened = false;
			}
		}

		if (!IsReadOnly() && ImGui::IsKeyPressed('Z'))
			if (ctrl && !shift && !alt)
				Undo();
		if (!IsReadOnly() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			if (!ctrl && !shift && alt)
				Undo();
		if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed('Y'))
			Redo();
		
		if (!mACOpened && !ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
			MoveUp(1, shift);
		else if (!mACOpened && !ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
			MoveDown(1, shift);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
			MoveLeft(1, shift, ctrl);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
			MoveRight(1, shift, ctrl);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
			MoveUp(GetPageSize() - 4, shift);
		else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
			MoveDown(GetPageSize() - 4, shift);
		else if (!alt && ctrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
			MoveTop(shift);
		else if (ctrl && !alt &&  ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
			MoveBottom(shift);
		else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
			MoveHome(shift);
		else if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
			MoveEnd(shift);
		else if (!IsReadOnly() && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
			if (ctrl)
				MoveRight(1, true, true);
			Delete();
		}
		else if (!IsReadOnly() && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace))) {
			if (ctrl)
				MoveLeft(1, true, true);
			BackSpace();
		}
		else if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(45))
			mOverwrite ^= true;
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(45))
			Copy();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed('C'))
			Copy();
		else if (!IsReadOnly() && !ctrl && shift && !alt && ImGui::IsKeyPressed(45))
			Paste();
		else if (!IsReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed('V'))
			Paste();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed('X'))
			Cut();
		else if (!ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
			Cut();
		else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
			SelectAll();
		else if (mAutocomplete && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))) {
			mACWord = GetWordUnderCursor();

			bool isValid = false;
			for (int i = 0; i < mACWord.size(); i++)
				if ((mACWord[i] >= 'a' && mACWord[i] <= 'z') || (mACWord[i] >= 'A' && mACWord[i] <= 'Z'))
				{
					isValid = true;
					break;
				}

			if (isValid) {
				mACSuggestions.clear();
				mACIndex = 0;
				mACSwitched = false;

				// starting with the written word
				for (auto& str : mLanguageDefinition.mKeywords)
					if (str.find(mACWord) == 0)
						mACSuggestions.push_back(str);

				for (auto& str : mLanguageDefinition.mIdentifiers)
					if (str.first.find(mACWord) == 0)
						mACSuggestions.push_back(str.first);

				// containing the word
				for (auto& str : mLanguageDefinition.mKeywords) {
					size_t ind = str.find(mACWord);
					if (ind > 0 && ind != std::string::npos)
						mACSuggestions.push_back(str);
				}

				for (auto& str : mLanguageDefinition.mIdentifiers) {
					size_t ind = str.first.find(mACWord);
					if (ind > 0 && ind != std::string::npos)
						mACSuggestions.push_back(str.first);
				}

				if (mACSuggestions.size() > 0) {
					mACOpened = true;
					mACPosition = GetCursorPosition();
				}
			}
		}
		else if (mACOpened && !ctrl && !alt && !shift && (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))
			|| (mACSwitched && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))))) {

			auto curCoord = GetCursorPosition();
			curCoord.mColumn = std::max(curCoord.mColumn - 1, 0);

			auto acStart = FindWordStart(curCoord);
			auto acEnd = FindWordEnd(curCoord);

			SetSelection(acStart, acEnd);
			BackSpace();
			InsertText(mACSuggestions[mACIndex]);

			mACOpened = false;
		}
		else if (!IsReadOnly())
		{
			for (size_t i = 0; i < sizeof(io.InputCharacters) / sizeof(io.InputCharacters[0]); i++)
			{
				auto c = (unsigned char)io.InputCharacters[i];
				
				if (c != 0)
				{
					if (isprint(c) || isspace(c))
					{
						if (c == '\r')
							c = '\n';
						EnterCharacter((char)c);
					}
				}
			}
		}

	}
	GetActualCursorCoordinates();

	if (ImGui::IsMouseClicked(0))
		mACOpened = false;

	if (ImGui::IsWindowHovered())
	{
		static float lastClick = -1.0f;
		if (!alt)
		{
			auto click = ImGui::IsMouseClicked(0);
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick && t - lastClick < io.MouseDoubleClickTime;
			
			if (tripleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos()));
					mSelectionMode = SelectionMode::Line;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				lastClick = -1.0f;
			}
			else if (doubleClick)
			{
				if (!ctrl)
				{
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos()));
					if (mSelectionMode == SelectionMode::Line)
						mSelectionMode = SelectionMode::Normal;
					else
						mSelectionMode = SelectionMode::Word;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				lastClick = ImGui::GetTime();
			}
			else if (click)
			{
				auto clickCoords = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos()));
				if (shift) {
					SetSelection(clickCoords, mInteractiveEnd);
					mState.mCursorPosition = mInteractiveStart = clickCoords;
				} else {
					mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = clickCoords;
					if (ctrl)
						mSelectionMode = SelectionMode::Word;
					else
						mSelectionMode = SelectionMode::Normal;
					SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
				}

				lastClick = ImGui::GetTime();
			}
			else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0))
			{
				io.WantCaptureMouse = true;
				mState.mCursorPosition = mInteractiveEnd = SanitizeCoordinates(ScreenPosToCoordinates(ImGui::GetMousePos()));
				SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
			}
			else
			{
			}
		}

		//if (!ImGui::IsMouseDown(0))
		//	mWordSelectionMode = false;
	}

	static std::string buffer;
	auto contentSize = ImGui::GetWindowContentRegionMax();
	auto drawList = ImGui::GetWindowDrawList();
	int appendIndex = 0;
	int longest = GetTextStart();

	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	auto scrollX = ImGui::GetScrollX();
	auto scrollY = ImGui::GetScrollY();

	auto lineNo = (int)floor(scrollY / mCharAdvance.y);
	auto lineMax = std::max<int>(0, std::min<int>((int)mLines.size() - 1, lineNo + (int)floor((scrollY + contentSize.y) / mCharAdvance.y)));
	if (!mLines.empty())
	{
		while (lineNo <= lineMax)
		{
			ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * mCharAdvance.y);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mCharAdvance.x * GetTextStart(), lineStartScreenPos.y);

			auto& line = mLines[lineNo];
			longest = std::max<int>(GetTextStart() + TextDistanceToLineStart(Coordinates(lineNo, (int) line.size())), longest);
			auto columnNo = 0;
			Coordinates lineStartCoord(lineNo, 0);
			Coordinates lineEndCoord(lineNo, (int)line.size());

			int sstart = -1;
			int ssend = -1;

			assert(mState.mSelectionStart <= mState.mSelectionEnd);
			if (mState.mSelectionStart <= lineEndCoord)
				sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart) : 0;
			if (mState.mSelectionEnd > lineStartCoord)
				ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd : lineEndCoord);

			if (mState.mSelectionEnd.mLine > lineNo)
				++ssend;

			if (sstart != -1 && ssend != -1 && sstart < ssend)
			{
				ImVec2 vstart(lineStartScreenPos.x + (mCharAdvance.x) * (sstart + GetTextStart()), lineStartScreenPos.y);
				ImVec2 vend(lineStartScreenPos.x + (mCharAdvance.x) * (ssend + GetTextStart()), lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(vstart, vend, mPalette[(int)PaletteIndex::Selection]);
			}

			static char buf[16];
			auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

			if (mBreakpoints.find(lineNo + 1) != mBreakpoints.end())
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::Breakpoint]);
			}

			auto errorIt = mErrorMarkers.find(lineNo + 1);
			if (errorIt != mErrorMarkers.end())
			{
				auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(start, end, mPalette[(int)PaletteIndex::ErrorMarker]);

				if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end))
				{
					ImGui::BeginTooltip();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
					ImGui::Text("Error at line %d:", errorIt->first);
					ImGui::PopStyleColor();
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
					ImGui::Text("%s", errorIt->second.c_str());
					ImGui::PopStyleColor();
					ImGui::EndTooltip();
				}
			}

			if (mShowLineNumbers) {
				auto chars = snprintf(buf, 16, "%6d", lineNo + 1);
				assert(chars >= 0 && chars < 16);
				drawList->AddText(ImVec2(lineStartScreenPos.x /*+ mCharAdvance.x * 1*/, lineStartScreenPos.y), mPalette[(int)PaletteIndex::LineNumber], buf);
			}

			if (mState.mCursorPosition.mLine == lineNo)
			{
				auto focused = ImGui::IsWindowFocused();

				if (!HasSelection() && mHighlightLine)
				{
					auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y);
					drawList->AddRectFilled(start, end, mPalette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
					drawList->AddRect(start, end, mPalette[(int)PaletteIndex::CurrentLineEdge], 1.0f);
				}

				int cx = TextDistanceToLineStart(mState.mCursorPosition);

				if (focused)
				{
					static auto timeStart = std::chrono::system_clock::now();
					auto timeEnd = std::chrono::system_clock::now();
					auto diff = timeEnd - timeStart;
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
					if (elapsed > 400)
					{
						ImVec2 cstart(lineStartScreenPos.x + mCharAdvance.x * (cx + GetTextStart()), lineStartScreenPos.y);
						ImVec2 cend(lineStartScreenPos.x + mCharAdvance.x * (cx + GetTextStart()) + (mOverwrite ? mCharAdvance.x : 1.0f), lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(cstart, cend, mPalette[(int)PaletteIndex::Cursor]);
						if (elapsed > 800)
							timeStart = timeEnd;
					}
				}
			}

			appendIndex = 0;
			auto prevColor = line.empty() ? PaletteIndex::Default : (line[0].mMultiLineComment ? PaletteIndex::MultiLineComment : line[0].mColorIndex);

			for (auto& glyph : line)
			{
				auto color = glyph.mMultiLineComment ? PaletteIndex::MultiLineComment : glyph.mColorIndex;

				if (color != prevColor && !buffer.empty())
				{
					drawList->AddText(textScreenPos, mPalette[(uint8_t)prevColor], buffer.c_str());
					textScreenPos.x += mCharAdvance.x * buffer.length();
					buffer.clear();
					prevColor = color;
				}
				appendIndex = AppendBuffer(buffer, glyph.mChar, appendIndex);
				++columnNo;
			}

			if (!buffer.empty())
			{
				drawList->AddText(textScreenPos, mPalette[(uint8_t)prevColor], buffer.c_str());
				buffer.clear();
			}
			appendIndex = 0;
			lineStartScreenPos.y += mCharAdvance.y;
			textScreenPos.x = lineStartScreenPos.x + mCharAdvance.x * GetTextStart();
			textScreenPos.y = lineStartScreenPos.y;
			++lineNo;
		}

		auto id = GetWordAt(ScreenPosToCoordinates(ImGui::GetMousePos()));
		if (!id.empty())
		{
			auto it = mLanguageDefinition.mIdentifiers.find(id);
			if (it != mLanguageDefinition.mIdentifiers.end())
			{
				ImGui::SetNextWindowSize(ImVec2(350, 0));
				ImGui::BeginTooltip();
				ImGui::TextWrapped(it->second.mDeclaration.c_str());
				ImGui::EndTooltip();
			}
			else
			{
				auto pi = mLanguageDefinition.mPreprocIdentifiers.find(id);
				if (pi != mLanguageDefinition.mPreprocIdentifiers.end())
				{
					ImGui::SetNextWindowSize(ImVec2(2350, 0));
					ImGui::BeginTooltip();
					ImGui::TextWrapped(pi->second.mDeclaration.c_str());
					ImGui::EndTooltip();
				}
			}
		}
	}

	ColorizeInternal();

	// suggestions window
	if (mACOpened) {
		auto acCoord = mACPosition;
		acCoord.mColumn++;
		
		ImFont* font = ImGui::GetFont();
		ImGui::PopFont();

		ImGui::SetNextWindowPos(CoordinatesToScreenPos(acCoord), ImGuiCond_Always);
		ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
		ImGui::BeginChild("##texteditor_autocompl", ImVec2(150, 100), true);

		for (int i = 0; i < mACSuggestions.size(); i++) {
			ImGui::Selectable(mACSuggestions[i].c_str(), i == mACIndex);
			if (i == mACIndex)
				ImGui::SetScrollHereY();
		}
		
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::PushFont(font);

		ImGui::SetWindowFocus();
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
			mACOpened = false;
	}

	ImGui::Dummy(ImVec2((longest + 2) * mCharAdvance.x, mLines.size() * mCharAdvance.y));

	if (mScrollToCursor)
	{
		EnsureCursorVisible();
		ImGui::SetWindowFocus();
		mScrollToCursor = false;
	}

	ImGui::PopAllowKeyboardFocus();
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	mWithinRender = false;
}

void TextEditor::SetText(const std::string & aText)
{
	mLines.clear();
	for (auto chr : aText)
	{
		if (mLines.empty())
			mLines.push_back(Line());
		if (chr == '\n')
			mLines.push_back(Line());
		else
		{
			mLines.back().push_back(Glyph(chr, PaletteIndex::Default));
		}

		mTextChanged = true;
	}

	mUndoBuffer.clear();

	Colorize();
}

void TextEditor::EnterCharacter(Char aChar)
{
	assert(!mReadOnly);

	// smart indent
	int indentBraces = 0, indent = 0;
	if (mSmartIndent && (aChar == '\n' || aChar == '}' || aChar == '{')) {
		std::string txt = GetText();
		Coordinates cursor = GetCursorPosition();

		int cline = 0, lnstart = 0, i = 0;
		for (; i < txt.size(); i++) {
			if (cline == cursor.mLine && i - lnstart == cursor.mColumn)
				break;

			if (txt[i] == '{') indentBraces++;
			if (txt[i] == '}') indentBraces--;
			if (txt[i] == '\t') indent++;

			if (txt[i] == '\n') {
				cline++;
				lnstart = i + 1;
				indent = 0;
			}
		}

		if (aChar == '}' && indentBraces != 0) {
			if (i != 0 && txt[i - 1] == '\t')
				BackSpace();
		}
		else if (aChar == '{' && indent != indentBraces)
			InsertText(std::string(indentBraces - indent, '\t'));
	}

	// normal part of entercharacter()
	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;
		DeleteSelection();
	}

	auto coord = GetActualCursorCoordinates();
	u.mAddedStart = coord;

	if (mLines.empty())
		mLines.push_back(Line());

	if (aChar == '\n')
	{
		InsertLine(coord.mLine + 1);
		auto& line = mLines[coord.mLine];
		auto& newLine = mLines[coord.mLine + 1];
		newLine.insert(newLine.begin(), line.begin() + coord.mColumn, line.end());
		line.erase(line.begin() + coord.mColumn, line.begin() + line.size());
		mState.mCursorPosition = Coordinates(coord.mLine + 1, 0);
	}
	else if (aChar == '\t' && mInsertSpaces)
	{
		auto& line = mLines[coord.mLine];
		for (int i = 0; i < mTabSize; i++) {
			line.insert(line.begin() + coord.mColumn, Glyph(' ', PaletteIndex::Default));
			coord.mColumn++;
			mState.mCursorPosition = coord;
		}
	}
	else
	{
		auto& line = mLines[coord.mLine];
		if (mOverwrite && (int)line.size() > coord.mColumn)
			line[coord.mColumn] = Glyph(aChar, PaletteIndex::Default);
		else
			line.insert(line.begin() + coord.mColumn, Glyph(aChar, PaletteIndex::Default));
		mState.mCursorPosition = coord;
		++mState.mCursorPosition.mColumn;
	}

	mTextChanged = true;

	u.mAdded = aChar;
	u.mAddedEnd = GetActualCursorCoordinates();
	u.mAfter = mState;

	AddUndo(u);

	Colorize(coord.mLine - 1, 3);
	EnsureCursorVisible();

	// smart indent pt2 - new line
	if (mSmartIndent && aChar == '\n')
		InsertText(std::string(indentBraces, '\t'));

	// auto brace completion
	if (mCompleteBraces) {
		if (aChar == '{') {
			EnterCharacter('\n');
			EnterCharacter('}');
		} else if (aChar == '(')
			EnterCharacter(')');
		else if (aChar == '[')
			EnterCharacter(']');

		if (aChar == '{' || aChar == '(' || aChar == '[')
			mState.mCursorPosition.mColumn--;
	}
}

void TextEditor::SetReadOnly(bool aValue)
{
	mReadOnly = aValue;
}

void TextEditor::SetCursorPosition(const Coordinates & aPosition)
{
	if (mState.mCursorPosition != aPosition)
	{
		mState.mCursorPosition = aPosition;
		EnsureCursorVisible();
	}
}

void TextEditor::SetSelectionStart(const Coordinates & aPosition)
{
	mState.mSelectionStart = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates & aPosition)
{
	mState.mSelectionEnd = SanitizeCoordinates(aPosition);
	if (mState.mSelectionStart > mState.mSelectionEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelection(const Coordinates & aStart, const Coordinates & aEnd, SelectionMode aMode)
{
	mState.mSelectionStart = SanitizeCoordinates(aStart);
	mState.mSelectionEnd = SanitizeCoordinates(aEnd);
	if (aStart > aEnd)
		std::swap(mState.mSelectionStart, mState.mSelectionEnd);

	switch (aMode)
	{
	case TextEditor::SelectionMode::Normal:
		break;
	case TextEditor::SelectionMode::Word:
	{
		mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
		if (!IsOnWordBoundary(mState.mSelectionEnd))
			mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
		break;
	}
	case TextEditor::SelectionMode::Line:
	{
		const auto lineNo = mState.mSelectionEnd.mLine;
		const auto lineSize = lineNo < mLines.size() ? mLines[lineNo].size() : 0;
		mState.mSelectionStart = Coordinates(mState.mSelectionStart.mLine, 0);
		mState.mSelectionEnd = Coordinates(lineNo, (int) lineSize);
		break;
	}
	default:
		break;
	}
}

void TextEditor::InsertText(const std::string & aValue)
{
	InsertText(aValue.c_str());
}

void TextEditor::InsertText(const char * aValue)
{
	if (aValue == nullptr)
		return;

	auto pos = GetActualCursorCoordinates();
	auto start = std::min<Coordinates>(pos, mState.mSelectionStart);
	int totalLines = pos.mLine - start.mLine;

	totalLines += InsertTextAt(pos, aValue);

	SetSelection(pos, pos);
	SetCursorPosition(pos);
	Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection()
{
	assert(mState.mSelectionEnd >= mState.mSelectionStart);

	if (mState.mSelectionEnd == mState.mSelectionStart)
		return;

	DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

	SetSelection(mState.mSelectionStart, mState.mSelectionStart);
	SetCursorPosition(mState.mSelectionStart);
	Colorize(mState.mSelectionStart.mLine, 1);
}

void TextEditor::MoveUp(int aAmount, bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = std::max<int>(0, mState.mCursorPosition.mLine - aAmount);
	if (oldPos != mState.mCursorPosition)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

void TextEditor::MoveDown(int aAmount, bool aSelect)
{
	assert(mState.mCursorPosition.mColumn >= 0);
	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition.mLine = std::max<int>(0, std::min<int>((int)mLines.size() - 1, mState.mCursorPosition.mLine + aAmount));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);

		EnsureCursorVisible();
	}
}

void TextEditor::MoveLeft(int aAmount, bool aSelect, bool aWordMode)
{
	if (mLines.empty())
		return;

	auto oldPos = mState.mCursorPosition;
	mState.mCursorPosition = GetActualCursorCoordinates();

	while (aAmount-- > 0)
	{
		if (mState.mCursorPosition.mColumn == 0)
		{
			if (mState.mCursorPosition.mLine > 0)
			{
				--mState.mCursorPosition.mLine;
				mState.mCursorPosition.mColumn = (int)mLines[mState.mCursorPosition.mLine].size();
			}
		}
		else
		{
			mState.mCursorPosition.mColumn = std::max(0, mState.mCursorPosition.mColumn - 1);
			if (aWordMode)
				mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
		}
	}

	assert(mState.mCursorPosition.mColumn >= 0);
	if (aSelect)
	{
		if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else if (oldPos == mInteractiveEnd)
			mInteractiveEnd = mState.mCursorPosition;
		else
		{
			mInteractiveStart = mState.mCursorPosition;
			mInteractiveEnd = oldPos;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveRight(int aAmount, bool aSelect, bool aWordMode)
{
	auto oldPos = mState.mCursorPosition;

	if (mLines.empty())
		return;

	while (aAmount-- > 0)
	{
		auto& line = mLines[mState.mCursorPosition.mLine];
		if (mState.mCursorPosition.mColumn >= (int)line.size())
		{
			if (mState.mCursorPosition.mLine < (int)mLines.size() - 1)
			{
				mState.mCursorPosition.mLine = std::max<int>(0, std::min<int>((int)mLines.size() - 1, mState.mCursorPosition.mLine + 1));
				mState.mCursorPosition.mColumn = 0;
			}
		}
		else
		{
			mState.mCursorPosition.mColumn = std::max<int>(0, std::min<int>((int)line.size(), mState.mCursorPosition.mColumn + 1));
			if (aWordMode)
				mState.mCursorPosition = FindWordEnd(mState.mCursorPosition);
		}
	}

	if (aSelect)
	{
		if (oldPos == mInteractiveEnd)
			mInteractiveEnd = SanitizeCoordinates(mState.mCursorPosition);
		else if (oldPos == mInteractiveStart)
			mInteractiveStart = mState.mCursorPosition;
		else
		{
			mInteractiveStart = oldPos;
			mInteractiveEnd = mState.mCursorPosition;
		}
	}
	else
		mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
	SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

	EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(0, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			mInteractiveEnd = oldPos;
			mInteractiveStart = mState.mCursorPosition;
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::TextEditor::MoveBottom(bool aSelect)
{
	auto oldPos = GetCursorPosition();
	auto newPos = Coordinates((int)mLines.size() - 1, 0);
	SetCursorPosition(newPos);
	if (aSelect)
	{
		mInteractiveStart = oldPos;
		mInteractiveEnd = newPos;
	}
	else
		mInteractiveStart = mInteractiveEnd = newPos;
	SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else
			{
				mInteractiveStart = mState.mCursorPosition;
				mInteractiveEnd = oldPos;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::MoveEnd(bool aSelect)
{
	auto oldPos = mState.mCursorPosition;
	SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, (int)mLines[oldPos.mLine].size()));

	if (mState.mCursorPosition != oldPos)
	{
		if (aSelect)
		{
			if (oldPos == mInteractiveEnd)
				mInteractiveEnd = mState.mCursorPosition;
			else if (oldPos == mInteractiveStart)
				mInteractiveStart = mState.mCursorPosition;
			else
			{
				mInteractiveStart = oldPos;
				mInteractiveEnd = mState.mCursorPosition;
			}
		}
		else
			mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
		SetSelection(mInteractiveStart, mInteractiveEnd);
	}
}

void TextEditor::Delete()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);
		auto& line = mLines[pos.mLine];

		if (pos.mColumn == (int)line.size())
		{
			if (pos.mLine == (int)mLines.size() - 1)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			Advance(u.mRemovedEnd);

			auto& nextLine = mLines[pos.mLine + 1];
			line.insert(line.end(), nextLine.begin(), nextLine.end());
			RemoveLine(pos.mLine + 1);
		}
		else
		{
			u.mRemoved = line[pos.mColumn].mChar;
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			u.mRemovedEnd.mColumn++;

			line.erase(line.begin() + pos.mColumn);
		}

		mTextChanged = true;

		Colorize(pos.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::BackSpace()
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;


	UndoRecord u;
	u.mBefore = mState;

	if (HasSelection())
	{
		u.mRemoved = GetSelectedText();
		u.mRemovedStart = mState.mSelectionStart;
		u.mRemovedEnd = mState.mSelectionEnd;

		DeleteSelection();
	}
	else
	{
		auto pos = GetActualCursorCoordinates();
		SetCursorPosition(pos);

		if (mState.mCursorPosition.mColumn == 0)
		{
			if (mState.mCursorPosition.mLine == 0)
				return;

			u.mRemoved = '\n';
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			Advance(u.mRemovedEnd);

			auto& line = mLines[mState.mCursorPosition.mLine];
			auto& prevLine = mLines[mState.mCursorPosition.mLine - 1];
			auto prevSize = (int)prevLine.size();
			prevLine.insert(prevLine.end(), line.begin(), line.end());
			RemoveLine(mState.mCursorPosition.mLine);
			--mState.mCursorPosition.mLine;
			mState.mCursorPosition.mColumn = prevSize;
		}
		else
		{
			auto& line = mLines[mState.mCursorPosition.mLine];

			if (mCompleteBraces && pos.mColumn < line.size()) {
				if ((line[pos.mColumn - 1].mChar == '(' && line[pos.mColumn].mChar == ')') ||
					(line[pos.mColumn - 1].mChar == '{' && line[pos.mColumn].mChar == '}') ||
					(line[pos.mColumn - 1].mChar == '[' && line[pos.mColumn].mChar == ']'))
					Delete();
			}

			u.mRemoved = line[pos.mColumn - 1].mChar;
			u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
			--u.mRemovedStart.mColumn;

			--mState.mCursorPosition.mColumn;
			if (mState.mCursorPosition.mColumn < (int)line.size())
				line.erase(line.begin() + mState.mCursorPosition.mColumn);
		}

		mTextChanged = true;

		EnsureCursorVisible();
		Colorize(mState.mCursorPosition.mLine, 1);
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::SelectWordUnderCursor()
{
	auto c = GetCursorPosition();
	SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll()
{
	SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), 0));
}

bool TextEditor::HasSelection() const
{
	return mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::Copy()
{
	if (HasSelection())
	{
		ImGui::SetClipboardText(GetSelectedText().c_str());
	}
	else
	{
		if (!mLines.empty())
		{
			std::string str;
			auto& line = mLines[GetActualCursorCoordinates().mLine];
			for (auto& g : line)
				str.push_back(g.mChar);
			ImGui::SetClipboardText(str.c_str());
		}
	}
}

void TextEditor::Cut()
{
	if (IsReadOnly())
	{
		Copy();
	}
	else
	{
		if (HasSelection())
		{
			UndoRecord u;
			u.mBefore = mState;
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;

			Copy();
			DeleteSelection();

			u.mAfter = mState;
			AddUndo(u);
		}
	}
}

void TextEditor::Paste()
{
	auto clipText = ImGui::GetClipboardText();
	if (clipText != nullptr && strlen(clipText) > 0)
	{
		UndoRecord u;
		u.mBefore = mState;

		if (HasSelection())
		{
			u.mRemoved = GetSelectedText();
			u.mRemovedStart = mState.mSelectionStart;
			u.mRemovedEnd = mState.mSelectionEnd;
			DeleteSelection();
		}

		u.mAdded = clipText;
		u.mAddedStart = GetActualCursorCoordinates();

		InsertText(clipText);

		u.mAddedEnd = GetActualCursorCoordinates();
		u.mAfter = mState;
		AddUndo(u);
	}
}

bool TextEditor::CanUndo() const
{
	return mUndoIndex > 0;
}

bool TextEditor::CanRedo() const
{
	return mUndoIndex < (int)mUndoBuffer.size();
}

void TextEditor::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
		mUndoBuffer[--mUndoIndex].Undo(this);
}

void TextEditor::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		mUndoBuffer[mUndoIndex++].Redo(this);
}

const TextEditor::Palette & TextEditor::GetDarkPalette()
{
	static Palette p = { {
		0xffffffff,	// None
		0xffd69c56,	// Keyword	
		0xff00ff00,	// Number
		0xff7070e0,	// String
		0xff70a0e0, // Char literal
		0xffffffff, // Punctuation
		0xff409090,	// Preprocessor
		0xffaaaaaa, // Identifier
		0xff9bc64d, // Known identifier
		0xffc040a0, // Preproc identifier
		0xff206020, // Comment (single line)
		0xff406020, // Comment (multi line)
		0xff101010, // Background
		0xffe0e0e0, // Cursor
		0x80a06020, // Selection
		0x800020ff, // ErrorMarker
		0x40f08000, // Breakpoint
		0xff707000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40a0a0a0, // Current line edge
	} };
	return p;
}

const TextEditor::Palette & TextEditor::GetLightPalette()
{
	static Palette p = { {
		0xff000000,	// None
		0xffff0c06,	// Keyword	
		0xff008000,	// Number
		0xff2020a0,	// String
		0xff304070, // Char literal
		0xff000000, // Punctuation
		0xff409090,	// Preprocessor
		0xff404040, // Identifier
		0xff606010, // Known identifier
		0xffc040a0, // Preproc identifier
		0xff205020, // Comment (single line)
		0xff405020, // Comment (multi line)
		0xffffffff, // Background
		0xff000000, // Cursor
		0x80600000, // Selection
		0xa00010ff, // ErrorMarker
		0x80f08000, // Breakpoint
		0xff505000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40000000, // Current line edge
	} };
	return p;
}

const TextEditor::Palette & TextEditor::GetRetroBluePalette()
{
	static Palette p = { {
		0xff00ffff,	// None
		0xffffff00,	// Keyword	
		0xff00ff00,	// Number
		0xff808000,	// String
		0xff808000, // Char literal
		0xffffffff, // Punctuation
		0xff008000,	// Preprocessor
		0xff00ffff, // Identifier
		0xffffffff, // Known identifier
		0xffff00ff, // Preproc identifier
		0xff808080, // Comment (single line)
		0xff404040, // Comment (multi line)
		0xff800000, // Background
		0xff0080ff, // Cursor
		0x80ffff00, // Selection
		0xa00000ff, // ErrorMarker
		0x80ff8000, // Breakpoint
		0xff808000, // Line number
		0x40000000, // Current line fill
		0x40808080, // Current line fill (inactive)
		0x40000000, // Current line edge
	} };
	return p;
}


std::string TextEditor::GetText() const
{
	return GetText(Coordinates(), Coordinates((int)mLines.size(), 0));
}

std::string TextEditor::GetSelectedText() const
{
	return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::ProcessInputs()
{
}

void TextEditor::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)mLines.size() : std::min<int>((int)mLines.size(), aFromLine + aLines);
	mColorRangeMin = std::min<int>(mColorRangeMin, aFromLine);
	mColorRangeMax = std::max<int>(mColorRangeMax, toLine);
	mColorRangeMin = std::max<int>(0, mColorRangeMin);
	mColorRangeMax = std::max<int>(mColorRangeMin, mColorRangeMax);
	mCheckMultilineComments = true;
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine)
{
	if (mLines.empty() || aFromLine >= aToLine)
		return;

	std::string buffer;
	int endLine = std::max<int>(0, std::min<int>((int)mLines.size(), aToLine));
	for (int i = aFromLine; i < endLine; ++i)
	{
		bool preproc = false;
		auto& line = mLines[i];
		buffer.clear();
		for (auto g : mLines[i])
		{
			buffer.push_back(g.mChar);
			g.mColorIndex = PaletteIndex::Default;
		}

		std::match_results<std::string::const_iterator> results;
		auto last = buffer.cend();
		for (auto first = buffer.cbegin(); first != last; ++first)
		{
			for (auto& p : mRegexList)
			{
				if (std::regex_search<std::string::const_iterator>(first, last, results, p.first, std::regex_constants::match_continuous))
				{
					auto v = *results.begin();
					auto start = v.first - buffer.begin();
					auto end = v.second - buffer.begin();
					auto id = buffer.substr(start, end - start);
					auto color = p.second;
					if (color == PaletteIndex::Identifier)
					{
						if (!mLanguageDefinition.mCaseSensitive)
							std::transform(id.begin(), id.end(), id.begin(), ::toupper);

						if (!preproc)
						{
							if (mLanguageDefinition.mKeywords.find(id) != mLanguageDefinition.mKeywords.end())
								color = PaletteIndex::Keyword;
							else if (mLanguageDefinition.mIdentifiers.find(id) != mLanguageDefinition.mIdentifiers.end())
								color = PaletteIndex::KnownIdentifier;
							else if (mLanguageDefinition.mPreprocIdentifiers.find(id) != mLanguageDefinition.mPreprocIdentifiers.end())
								color = PaletteIndex::PreprocIdentifier;
						}
						else
						{
							if (mLanguageDefinition.mPreprocIdentifiers.find(id) != mLanguageDefinition.mPreprocIdentifiers.end())
								color = PaletteIndex::PreprocIdentifier;
							else
								color = PaletteIndex::Identifier;
						}
					}
					else if (color == PaletteIndex::Preprocessor)
					{
						preproc = true;
					}
					for (int j = (int)start; j < (int)end; ++j)
						line[j].mColorIndex = color;
					first += end - start - 1;
					break;
				}
			}
		}
	}
}

void TextEditor::ColorizeInternal()
{
	if (mLines.empty())
		return;
	
	if (mCheckMultilineComments)
	{
		auto end = Coordinates((int)mLines.size(), 0);
		auto commentStart = end;
		auto withinString = false;
		for (auto i = Coordinates(0, 0); i < end; Advance(i))
		{
			auto& line = mLines[i.mLine];
			if (!line.empty())
			{
				auto g = line[i.mColumn];
				auto c = g.mChar;

				bool inComment = commentStart <= i;

				if (withinString)
				{
					line[i.mColumn].mMultiLineComment = inComment;

					if (c == '\"')
					{
						if (i.mColumn + 1 < (int)line.size() && line[i.mColumn + 1].mChar == '\"')
						{
							Advance(i);
							if (i.mColumn < (int)line.size())
								line[i.mColumn].mMultiLineComment = inComment;
						}
						else
							withinString = false;
					}
					else if (c == '\\')
					{
						Advance(i);
						if (i.mColumn < (int)line.size())
							line[i.mColumn].mMultiLineComment = inComment;
					}
				}
				else
				{
					if (c == '\"')
					{
						withinString = true;
						line[i.mColumn].mMultiLineComment = inComment;
					}
					else
					{
						auto pred = [](const char& a, const Glyph& b) { return a == b.mChar; };
						auto from = line.begin() + i.mColumn;
						auto& startStr = mLanguageDefinition.mCommentStart;
						if (i.mColumn + startStr.size() <= line.size() &&
							equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
							commentStart = i;

						inComment = commentStart <= i;

						line[i.mColumn].mMultiLineComment = inComment;
						
						auto& endStr = mLanguageDefinition.mCommentEnd;
						if (i.mColumn + 1 >= (int)endStr.size() &&
							equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
							commentStart = end;
					}
				}
			}
		}
		mCheckMultilineComments = false;
		return;
	}

	if (mColorRangeMin < mColorRangeMax)
	{
		int to = std::min(mColorRangeMin + 10, mColorRangeMax);
		ColorizeRange(mColorRangeMin, to);
		mColorRangeMin = to;

		if (mColorRangeMax == mColorRangeMin)
		{
			mColorRangeMin = std::numeric_limits<int>::max();
			mColorRangeMax = 0;
		}
		return;
	}
}

int TextEditor::TextDistanceToLineStart(const Coordinates& aFrom) const
{
	auto& line = mLines[aFrom.mLine];
	auto len = 0;
	for (size_t it = 0u; it < line.size() && it < (unsigned)aFrom.mColumn; ++it)
		len = line[it].mChar == '\t' ? (len / mTabSize) * mTabSize + mTabSize : len + 1;
	return len;
}

void TextEditor::EnsureCursorVisible()
{
	if (!mWithinRender)
	{
		mScrollToCursor = true;
		return;
	}

	float scrollX = ImGui::GetScrollX();
	float scrollY = ImGui::GetScrollY();

	auto height = ImGui::GetWindowHeight();
	auto width = ImGui::GetWindowWidth();

	auto top = 1 + (int)ceil(scrollY / mCharAdvance.y);
	auto bottom = (int)ceil((scrollY + height) / mCharAdvance.y);

	auto left = (int)ceil(scrollX / mCharAdvance.x);
	auto right = (int)ceil((scrollX + width) / mCharAdvance.x);

	auto pos = GetActualCursorCoordinates();
	auto len = TextDistanceToLineStart(pos);

	if (pos.mLine < top)
		ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * mCharAdvance.y));
	if (pos.mLine > bottom - 4)
		ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 4) * mCharAdvance.y - height));
	if (len + GetTextStart() < left + 4)
		ImGui::SetScrollX(std::max(0.0f, (len + GetTextStart() - 4) * mCharAdvance.x));
	if (len + GetTextStart() > right - 4)
		ImGui::SetScrollX(std::max(0.0f, (len + GetTextStart() + 4) * mCharAdvance.x - width));
}

int TextEditor::GetPageSize() const
{
	auto height = ImGui::GetWindowHeight() - 20.0f;
	return (int)floor(height / mCharAdvance.y);
}

TextEditor::UndoRecord::UndoRecord(
	const std::string& aAdded,
	const TextEditor::Coordinates aAddedStart,
	const TextEditor::Coordinates aAddedEnd,
	const std::string& aRemoved,
	const TextEditor::Coordinates aRemovedStart,
	const TextEditor::Coordinates aRemovedEnd,
	TextEditor::EditorState& aBefore,
	TextEditor::EditorState& aAfter)
	: mAdded(aAdded)
	, mAddedStart(aAddedStart)
	, mAddedEnd(aAddedEnd)
	, mRemoved(aRemoved)
	, mRemovedStart(aRemovedStart)
	, mRemovedEnd(aRemovedEnd)
	, mBefore(aBefore)
	, mAfter(aAfter)
{
	assert(mAddedStart <= mAddedEnd);
	assert(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor * aEditor)
{
	if (!mAdded.empty())
	{
		aEditor->DeleteRange(mAddedStart, mAddedEnd);
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 2);
	}

	if (!mRemoved.empty())
	{
		auto start = mRemovedStart;
		aEditor->InsertTextAt(start, mRemoved.c_str());
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 2);
	}

	aEditor->mState = mBefore;
	aEditor->EnsureCursorVisible();

}

void TextEditor::UndoRecord::Redo(TextEditor * aEditor)
{
	if (!mRemoved.empty())
	{
		aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
		aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 1);
	}

	if (!mAdded.empty())
	{
		auto start = mAddedStart;
		aEditor->InsertTextAt(start, mAdded.c_str());
		aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 1);
	}

	aEditor->mState = mAfter;
	aEditor->EnsureCursorVisible();
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::CPlusPlus()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const cppKeywords[] = {
			"alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
			"compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
			"for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
			"register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local",
			"throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
		};
		for (auto& k : cppKeywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper",
			"std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("//.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";

		langDef.mCaseSensitive = true;

		langDef.mName = "C++";

		inited = true;
	}
	return langDef;
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::HLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
			"CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
			"export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
			"linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
			"pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
			"RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
			"static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
			"Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
			"VertexShader", "void", "volatile", "while",
			"bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
			"uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
			"float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
			"float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
			"half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
			"half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		m_HLSLDocumentation(langDef.mIdentifiers);

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("//.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";

		langDef.mCaseSensitive = true;

		langDef.mName = "HLSL";

		inited = true;
	}
	return langDef;
}
void TextEditor::LanguageDefinition::m_HLSLDocumentation(Identifiers& idents)
{
	std::function<Identifier(const std::string&)> desc = [](const std::string & str) {
		Identifier id;
		id.mDeclaration = str;
		return id;
	};

	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("abort", desc("Terminates the current draw or dispatch call being executed.")));
	idents.insert(std::make_pair("abs", desc("Absolute value (per component).")));
	idents.insert(std::make_pair("acos", desc("Returns the arccosine of each component of x.")));
	idents.insert(std::make_pair("all", desc("Test if all components of x are nonzero.")));
	idents.insert(std::make_pair("AllMemoryBarrier", desc("Blocks execution of all threads in a group until all memory accesses have been completed.")));
	idents.insert(std::make_pair("AllMemoryBarrierWithGroupSync", desc("Blocks execution of all threads in a group until all memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("any", desc("Test if any component of x is nonzero.")));
	idents.insert(std::make_pair("asdouble", desc("Reinterprets a cast value into a double.")));
	idents.insert(std::make_pair("asfloat", desc("Convert the input type to a float.")));
	idents.insert(std::make_pair("asin", desc("Returns the arcsine of each component of x.")));
	idents.insert(std::make_pair("asint", desc("Convert the input type to an integer.")));
	idents.insert(std::make_pair("asuint", desc("Convert the input type to an unsigned integer.")));
	idents.insert(std::make_pair("atan", desc("Returns the arctangent of x.")));
	idents.insert(std::make_pair("atan2", desc("Returns the arctangent of of two values (x,y).")));
	idents.insert(std::make_pair("ceil", desc("Returns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("CheckAccessFullyMapped", desc("Determines whether all values from a Sample or Load operation accessed mapped tiles in a tiled resource.")));
	idents.insert(std::make_pair("clamp", desc("Clamps x to the range [min, max].")));
	idents.insert(std::make_pair("clip", desc("Discards the current pixel, if any component of x is less than zero.")));
	idents.insert(std::make_pair("cos", desc("Returns the cosine of x.")));
	idents.insert(std::make_pair("cosh", desc("Returns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("countbits", desc("Counts the number of bits (per component) in the input integer.")));
	idents.insert(std::make_pair("cross", desc("Returns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("D3DCOLORtoUBYTE4", desc("Swizzles and scales components of the 4D vector x to compensate for the lack of UBYTE4 support in some hardware.")));
	idents.insert(std::make_pair("ddx", desc("Returns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_coarse", desc("Computes a low precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddx_fine", desc("Computes a high precision partial derivative with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("ddy", desc("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_coarse", desc("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("ddy_fine", desc("Computes a high precision partial derivative with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("degrees", desc("Converts x from radians to degrees.")));
	idents.insert(std::make_pair("determinant", desc("Returns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("DeviceMemoryBarrier", desc("Blocks execution of all threads in a group until all device memory accesses have been completed.")));
	idents.insert(std::make_pair("DeviceMemoryBarrierWithGroupSync", desc("Blocks execution of all threads in a group until all device memory accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("distance", desc("Returns the distance between two points.")));
	idents.insert(std::make_pair("dot", desc("Returns the dot product of two vectors.")));
	idents.insert(std::make_pair("dst", desc("Calculates a distance vector.")));
	idents.insert(std::make_pair("errorf", desc("Submits an error message to the information queue.")));
	idents.insert(std::make_pair("EvaluateAttributeAtCentroid", desc("Evaluates at the pixel centroid.")));
	idents.insert(std::make_pair("EvaluateAttributeAtSample", desc("Evaluates at the indexed sample location.")));
	idents.insert(std::make_pair("EvaluateAttributeSnapped", desc("Evaluates at the pixel centroid with an offset.")));
	idents.insert(std::make_pair("exp", desc("Returns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", desc("Base 2 exponent(per component).")));
	idents.insert(std::make_pair("f16tof32", desc("Converts the float16 stored in the low-half of the uint to a float.")));
	idents.insert(std::make_pair("f32tof16", desc("Converts an input into a float16 type.")));
	idents.insert(std::make_pair("faceforward", desc("Returns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("firstbithigh", desc("Gets the location of the first set bit starting from the highest order bit and working downward, per component.")));
	idents.insert(std::make_pair("firstbitlow", desc("Returns the location of the first set bit starting from the lowest order bit and working upward, per component.")));
	idents.insert(std::make_pair("floor", desc("Returns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("fma", desc("Returns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("fmod", desc("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("frac", desc("Returns the fractional part of x.")));
	idents.insert(std::make_pair("frexp", desc("Returns the mantissa and exponent of x.")));
	idents.insert(std::make_pair("fwidth", desc("Returns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("GetRenderTargetSampleCount", desc("Returns the number of render-target samples.")));
	idents.insert(std::make_pair("GetRenderTargetSamplePosition", desc("Returns a sample position (x,y) for a given sample index.")));
	idents.insert(std::make_pair("GroupMemoryBarrier", desc("Blocks execution of all threads in a group until all group shared accesses have been completed.")));
	idents.insert(std::make_pair("GroupMemoryBarrierWithGroupSync", desc("Blocks execution of all threads in a group until all group shared accesses have been completed and all threads in the group have reached this call.")));
	idents.insert(std::make_pair("InterlockedAdd", desc("Performs a guaranteed atomic add of value to the dest resource variable.")));
	idents.insert(std::make_pair("InterlockedAnd", desc("Performs a guaranteed atomic and.")));
	idents.insert(std::make_pair("InterlockedCompareExchange", desc("Atomically compares the input to the comparison value and exchanges the result.")));
	idents.insert(std::make_pair("InterlockedCompareStore", desc("Atomically compares the input to the comparison value.")));
	idents.insert(std::make_pair("InterlockedExchange", desc("Assigns value to dest and returns the original value.")));
	idents.insert(std::make_pair("InterlockedMax", desc("Performs a guaranteed atomic max.")));
	idents.insert(std::make_pair("InterlockedMin", desc("Performs a guaranteed atomic min.")));
	idents.insert(std::make_pair("InterlockedOr", desc("Performs a guaranteed atomic or.")));
	idents.insert(std::make_pair("InterlockedXor", desc("Performs a guaranteed atomic xor.")));
	idents.insert(std::make_pair("isfinite", desc("Returns true if x is finite, false otherwise.")));
	idents.insert(std::make_pair("isinf", desc("Returns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", desc("Returns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("ldexp", desc("Returns x * 2exp")));
	idents.insert(std::make_pair("length", desc("Returns the length of the vector v.")));
	idents.insert(std::make_pair("lerp", desc("Returns x + s(y - x).")));
	idents.insert(std::make_pair("lit", desc("Returns a lighting vector (ambient, diffuse, specular, 1)")));
	idents.insert(std::make_pair("log", desc("Returns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log10", desc("Returns the base-10 logarithm of x.")));
	idents.insert(std::make_pair("log2", desc("Returns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("mad", desc("Performs an arithmetic multiply/add operation on three values.")));
	idents.insert(std::make_pair("max", desc("Selects the greater of x and y.")));
	idents.insert(std::make_pair("min", desc("Selects the lesser of x and y.")));
	idents.insert(std::make_pair("modf", desc("Splits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("msad4", desc("Compares a 4-byte reference value and an 8-byte source value and accumulates a vector of 4 sums.")));
	idents.insert(std::make_pair("mul", desc("Performs matrix multiplication using x and y.")));
	idents.insert(std::make_pair("noise", desc("Generates a random value using the Perlin-noise algorithm.")));
	idents.insert(std::make_pair("normalize", desc("Returns a normalized vector.")));
	idents.insert(std::make_pair("pow", desc("Returns x^n.")));
	idents.insert(std::make_pair("printf", desc("Submits a custom shader message to the information queue.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsAvg", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMax", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("Process2DQuadTessFactorsMin", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessIsolineTessFactors", desc("Generates the rounded tessellation factors for an isoline.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsAvg", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMax", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessQuadTessFactorsMin", desc("Generates the corrected tessellation factors for a quad patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsAvg", desc("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMax", desc("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("ProcessTriTessFactorsMin", desc("Generates the corrected tessellation factors for a tri patch.")));
	idents.insert(std::make_pair("radians", desc("Converts x from degrees to radians.")));
	idents.insert(std::make_pair("rcp", desc("Calculates a fast, approximate, per-component reciprocal.")));
	idents.insert(std::make_pair("reflect", desc("Returns a reflection vector.")));
	idents.insert(std::make_pair("refract", desc("Returns the refraction vector.")));
	idents.insert(std::make_pair("reversebits", desc("Reverses the order of the bits, per component.")));
	idents.insert(std::make_pair("round", desc("Rounds x to the nearest integer")));
	idents.insert(std::make_pair("rsqrt", desc("Returns 1 / sqrt(x)")));
	idents.insert(std::make_pair("saturate", desc("Clamps x to the range [0, 1]")));
	idents.insert(std::make_pair("sign", desc("Computes the sign of x.")));
	idents.insert(std::make_pair("sin", desc("Returns the sine of x")));
	idents.insert(std::make_pair("sincos", desc("Returns the sineand cosine of x.")));
	idents.insert(std::make_pair("sinh", desc("Returns the hyperbolic sine of x")));
	idents.insert(std::make_pair("smoothstep", desc("Returns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("sqrt", desc("Square root (per component)")));
	idents.insert(std::make_pair("step", desc("Returns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("tan", desc("Returns the tangent of x")));
	idents.insert(std::make_pair("tanh", desc("Returns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("tex1D", desc("1D texture lookup.")));
	idents.insert(std::make_pair("tex1Dbias", desc("1D texture lookup with bias.")));
	idents.insert(std::make_pair("tex1Dgrad", desc("1D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex1Dlod", desc("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex1Dproj", desc("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex2D", desc("2D texture lookup.")));
	idents.insert(std::make_pair("tex2Dbias", desc("2D texture lookup with bias.")));
	idents.insert(std::make_pair("tex2Dgrad", desc("2D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex2Dlod", desc("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex2Dproj", desc("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("tex3D", desc("3D texture lookup.")));
	idents.insert(std::make_pair("tex3Dbias", desc("3D texture lookup with bias.")));
	idents.insert(std::make_pair("tex3Dgrad", desc("3D texture lookup with a gradient.")));
	idents.insert(std::make_pair("tex3Dlod", desc("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("tex3Dproj", desc("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texCUBE", desc("Cube texture lookup.")));
	idents.insert(std::make_pair("texCUBEbias", desc("Cube texture lookup with bias.")));
	idents.insert(std::make_pair("texCUBEgrad", desc("Cube texture lookup with a gradient.")));
	idents.insert(std::make_pair("texCUBElod", desc("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("texCUBEproj", desc("Cube texture lookup with projective divide.")));
	idents.insert(std::make_pair("transpose", desc("Returns the transpose of the matrix m.")));
	idents.insert(std::make_pair("trunc", desc("Truncates floating-point value(s) to integer value(s)")));
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::GLSL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local", "attribute", "uniform", "varying", "layout", "centroid", "flat", "smooth", "noperspective", "patch", "sample", "subroutine", "in", "out", "inout",
			"bool", "true", "false", "invariant", "mat2", "mat3", "mat4", "dmat2", "dmat3", "dmat4", "mat2x2", "mat2x3", "mat2x4", "dmat2x2", "dmat2x3", "dmat2x4", "mat3x2", "mat3x3", "mat3x4", "dmat3x2", "dmat3x3", "dmat3x4",
			"mat4x2", "mat4x3", "mat4x4", "dmat4x2", "dmat4x3", "dmat4x4", "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "dvec2", "dvec3", "dvec4", "uint", "uvec2", "uvec3", "uvec4",
			"lowp", "mediump", "highp", "precision", "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler1DShadow", "sampler2DShadow", "samplerCubeShadow", "sampler1DArray", "sampler2DArray", "sampler1DArrayShadow",
			"sampler2DArrayShadow", "isampler1D", "isampler2D", "isampler3D", "isamplerCube", "isampler1DArray", "isampler2DArray", "usampler1D", "usampler2D", "usampler3D", "usamplerCube", "usampler1DArray", "usampler2DArray",
			"sampler2DRect", "sampler2DRectShadow", "isampler2DRect", "usampler2DRect", "samplerBuffer", "isamplerBuffer", "usamplerBuffer", "sampler2DMS", "isampler2DMS", "usampler2DMS", "sampler2DMSArray", "isampler2DMSArray",
			"usampler2DMSArray", "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray", "usamplerCubeArray"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		m_GLSLDocumentation(langDef.mIdentifiers);

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("//.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";

		langDef.mCaseSensitive = true;

		langDef.mName = "GLSL";

		inited = true;
	}
	return langDef;
}
void TextEditor::LanguageDefinition::m_GLSLDocumentation(Identifiers& idents)
{
	std::function<Identifier(const std::string&)> desc = [](const std::string & str) {
		Identifier id;
		id.mDeclaration = str;
		return id;
	};

	/* SOURCE: https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-intrinsic-functions */

	idents.insert(std::make_pair("radians", desc("Converts x from degrees to radians.")));
	idents.insert(std::make_pair("degrees", desc("Converts x from radians to degrees.")));
	idents.insert(std::make_pair("sin", desc("Returns the sine of x")));
	idents.insert(std::make_pair("cos", desc("Returns the cosine of x.")));
	idents.insert(std::make_pair("tan", desc("Returns the tangent of x")));
	idents.insert(std::make_pair("asin", desc("Returns the arcsine of each component of x.")));
	idents.insert(std::make_pair("acos", desc("Returns the arccosine of each component of x.")));
	idents.insert(std::make_pair("atan", desc("Returns the arctangent of x.")));
	idents.insert(std::make_pair("sinh", desc("Returns the hyperbolic sine of x")));
	idents.insert(std::make_pair("cosh", desc("Returns the hyperbolic cosine of x.")));
	idents.insert(std::make_pair("tanh", desc("Returns the hyperbolic tangent of x")));
	idents.insert(std::make_pair("asinh", desc("Returns the arc hyperbolic sine of x")));
	idents.insert(std::make_pair("acosh", desc("Returns the arc hyperbolic cosine of x.")));
	idents.insert(std::make_pair("atanh", desc("Returns the arc hyperbolic tangent of x")));
	idents.insert(std::make_pair("pow", desc("Returns x^n.")));
	idents.insert(std::make_pair("exp", desc("Returns the base-e exponent.")));
	idents.insert(std::make_pair("exp2", desc("Base 2 exponent(per component).")));
	idents.insert(std::make_pair("log", desc("Returns the base-e logarithm of x.")));
	idents.insert(std::make_pair("log2", desc("Returns the base - 2 logarithm of x.")));
	idents.insert(std::make_pair("sqrt", desc("Square root (per component).")));
	idents.insert(std::make_pair("inversesqrt", desc("Returns rcp(sqrt(x)).")));
	idents.insert(std::make_pair("abs", desc("Absolute value (per component).")));
	idents.insert(std::make_pair("sign", desc("Computes the sign of x.")));
	idents.insert(std::make_pair("floor", desc("Returns the greatest integer which is less than or equal to x.")));
	idents.insert(std::make_pair("trunc", desc("Truncates floating-point value(s) to integer value(s)")));
	idents.insert(std::make_pair("round", desc("Rounds x to the nearest integer")));
	idents.insert(std::make_pair("roundEven", desc("Returns a value equal to the nearest integer to x. A fractional part of 0.5 will round toward the nearest even integer.")));
	idents.insert(std::make_pair("ceil", desc("Returns the smallest integer which is greater than or equal to x.")));
	idents.insert(std::make_pair("fract", desc("Returns the fractional part of x.")));
	idents.insert(std::make_pair("mod", desc("Modulus. Returns x – y ∗ floor (x/y).")));
	idents.insert(std::make_pair("modf", desc("Splits the value x into fractional and integer parts.")));
	idents.insert(std::make_pair("max", desc("Selects the greater of x and y.")));
	idents.insert(std::make_pair("min", desc("Selects the lesser of x and y.")));
	idents.insert(std::make_pair("clamp", desc("Clamps x to the range [min, max].")));
	idents.insert(std::make_pair("mix", desc("Returns x*(1-a)+y*a.")));
	idents.insert(std::make_pair("isinf", desc("Returns true if x is +INF or -INF, false otherwise.")));
	idents.insert(std::make_pair("isnan", desc("Returns true if x is NAN or QNAN, false otherwise.")));
	idents.insert(std::make_pair("smoothstep", desc("Returns a smooth Hermite interpolation between 0 and 1.")));
	idents.insert(std::make_pair("step", desc("Returns (x >= a) ? 1 : 0")));
	idents.insert(std::make_pair("floatBitsToInt", desc("Returns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("floatBitsToUint", desc("Returns a signed or unsigned integer value representing the encoding of a floating-point value. The floatingpoint value's bit-level representation is preserved.")));
	idents.insert(std::make_pair("intBitsToFloat", desc("Returns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("uintBitsToFloat", desc("Returns a floating-point value corresponding to a signed or unsigned integer encoding of a floating-point value.")));
	idents.insert(std::make_pair("fmod", desc("Returns the floating point remainder of x/y.")));
	idents.insert(std::make_pair("fma", desc("Returns the double-precision fused multiply-addition of a * b + c.")));
	idents.insert(std::make_pair("ldexp", desc("Returns x * 2exp")));
	idents.insert(std::make_pair("packUnorm2x16", desc("First, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packUnorm4x8", desc("First, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("packSnorm4x8", desc("First, converts each component of the normalized floating - point value v into 8 or 16bit integer values. Then, the results are packed into the returned 32bit unsigned integer.")));
	idents.insert(std::make_pair("unpackUnorm2x16", desc("First, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackUnorm4x8", desc("First, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("unpackSnorm4x8", desc("First, unpacks a single 32bit unsigned integer p into a pair of 16bit unsigned integers, four 8bit unsigned integers, or four 8bit signed integers.Then, each component is converted to a normalized floating point value to generate the returned two or four component vector.")));
	idents.insert(std::make_pair("packDouble2x32", desc("Returns a double-precision value obtained by packing the components of v into a 64-bit value.")));
	idents.insert(std::make_pair("unpackDouble2x32", desc("Returns a two-component unsigned integer vector representation of v.")));
	idents.insert(std::make_pair("length", desc("Returns the length of the vector v.")));
	idents.insert(std::make_pair("distance", desc("Returns the distance between two points.")));
	idents.insert(std::make_pair("dot", desc("Returns the dot product of two vectors.")));
	idents.insert(std::make_pair("cross", desc("Returns the cross product of two 3D vectors.")));
	idents.insert(std::make_pair("normalize", desc("Returns a normalized vector.")));
	idents.insert(std::make_pair("faceforward", desc("Returns -n * sign(dot(i, ng)).")));
	idents.insert(std::make_pair("reflect", desc("Returns a reflection vector.")));
	idents.insert(std::make_pair("refract", desc("Returns the refraction vector.")));
	idents.insert(std::make_pair("matrixCompMult", desc("Multiply matrix x by matrix y component-wise.")));
	idents.insert(std::make_pair("outerProduct", desc("Linear algebraic matrix multiply c * r.")));
	idents.insert(std::make_pair("transpose", desc("Returns the transpose of the matrix m.")));
	idents.insert(std::make_pair("determinant", desc("Returns the determinant of the square matrix m.")));
	idents.insert(std::make_pair("inverse", desc("Returns a matrix that is the inverse of m.")));
	idents.insert(std::make_pair("lessThan", desc("Returns the component-wise compare of x < y")));
	idents.insert(std::make_pair("lessThanEqual", desc("Returns the component-wise compare of x <= y")));
	idents.insert(std::make_pair("greaterThan", desc("Returns the component-wise compare of x > y")));
	idents.insert(std::make_pair("greaterThanEqual", desc("Returns the component-wise compare of x >= y")));
	idents.insert(std::make_pair("equal", desc("Returns the component-wise compare of x == y")));
	idents.insert(std::make_pair("notEqual", desc("Returns the component-wise compare of x != y")));
	idents.insert(std::make_pair("any", desc("Test if any component of x is nonzero.")));
	idents.insert(std::make_pair("all", desc("Test if all components of x are nonzero.")));
	idents.insert(std::make_pair("not", desc("Returns the component-wise logical complement of x.")));
	idents.insert(std::make_pair("uaddCarry", desc("Adds 32bit unsigned integer x and y, returning the sum modulo 2^32.")));
	idents.insert(std::make_pair("usubBorrow", desc("Subtracts the 32bit unsigned integer y from x, returning the difference if non-negatice, or 2^32 plus the difference otherwise.")));
	idents.insert(std::make_pair("umulExtended", desc("Multiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("imulExtended", desc("Multiplies 32bit integers x and y, producing a 64bit result.")));
	idents.insert(std::make_pair("bitfieldExtract", desc("Extracts bits [offset, offset + bits - 1] from value, returning them in the least significant bits of the result.")));
	idents.insert(std::make_pair("bitfieldInsert", desc("Returns the insertion the bits leas-significant bits of insert into base")));
	idents.insert(std::make_pair("bitfieldReverse", desc("Returns the reversal of the bits of value.")));
	idents.insert(std::make_pair("bitCount", desc("Returns the number of bits set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findLSB", desc("Returns the bit number of the least significant bit set to 1 in the binary representation of value.")));
	idents.insert(std::make_pair("findMSB", desc("Returns the bit number of the most significant bit in the binary representation of value.")));
	idents.insert(std::make_pair("textureSize", desc("Returns the dimensions of level lod  (if present) for the texture bound to sample.")));
	idents.insert(std::make_pair("textureQueryLod", desc("Returns the mipmap array(s) that would be accessed in the x component of the return value.")));
	idents.insert(std::make_pair("texture", desc("Use the texture coordinate P to do a texture lookup in the texture currently bound to sampler.")));
	idents.insert(std::make_pair("textureProj", desc("Do a texture lookup with projection.")));
	idents.insert(std::make_pair("textureLod", desc("Do a texture lookup as in texture but with explicit LOD.")));
	idents.insert(std::make_pair("textureOffset", desc("Do a texture lookup as in texture but with offset added to the (u,v,w) texel coordinates before looking up each texel.")));
	idents.insert(std::make_pair("texelFetch", desc("Use integer texture coordinate P to lookup a single texel from sampler.")));
	idents.insert(std::make_pair("texelFetchOffset", desc("Fetch a single texel as in texelFetch offset by offset.")));
	idents.insert(std::make_pair("texetureProjOffset", desc("Do a projective texture lookup as described in textureProj offset by offset as descrived in textureOffset.")));
	idents.insert(std::make_pair("texetureLodOffset", desc("Do an offset texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureProjLod", desc("Do a projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureLodOffset", desc("Do an offset texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureProjLodOffset", desc("Do an offset projective texture lookup with explicit LOD.")));
	idents.insert(std::make_pair("textureGrad", desc("Do a texture lookup as in texture but with explicit gradients.")));
	idents.insert(std::make_pair("textureGradOffset", desc("Do a texture lookup with both explicit gradient and offset, as described in textureGrad and textureOffset.")));
	idents.insert(std::make_pair("textureProjGrad", desc("Do a texture lookup both projectively and with explicit gradient.")));
	idents.insert(std::make_pair("textureProjGradOffset", desc("Do a texture lookup both projectively and with explicit gradient as well as with offset.")));
	idents.insert(std::make_pair("textureGather", desc("Built-in function.")));
	idents.insert(std::make_pair("textureGatherOffset", desc("Built-in function.")));
	idents.insert(std::make_pair("textureGatherOffsets", desc("Built-in function.")));
	idents.insert(std::make_pair("texture1D", desc("1D texture lookup.")));
	idents.insert(std::make_pair("texture1DLod", desc("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture1DProj", desc("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture1DProjLod", desc("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture2D", desc("2D texture lookup.")));
	idents.insert(std::make_pair("texture2DLod", desc("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture2DProj", desc("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture2DProjLod", desc("2D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("texture3D", desc("3D texture lookup.")));
	idents.insert(std::make_pair("texture3DLod", desc("3D texture lookup with LOD.")));
	idents.insert(std::make_pair("texture3DProj", desc("3D texture lookup with projective divide.")));
	idents.insert(std::make_pair("texture3DProjLod", desc("3D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("textureCube", desc("Cube texture lookup.")));
	idents.insert(std::make_pair("textureCubeLod", desc("Cube texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1D", desc("1D texture lookup.")));
	idents.insert(std::make_pair("shadow1DLod", desc("1D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow1DProj", desc("1D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow1DProjLod", desc("1D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("shadow2D", desc("2D texture lookup.")));
	idents.insert(std::make_pair("shadow2DLod", desc("2D texture lookup with LOD.")));
	idents.insert(std::make_pair("shadow2DProj", desc("2D texture lookup with projective divide.")));
	idents.insert(std::make_pair("shadow2DProjLod", desc("2D texture lookup with projective divide and with LOD.")));
	idents.insert(std::make_pair("dFdx", desc("Returns the partial derivative of x with respect to the screen-space x-coordinate.")));
	idents.insert(std::make_pair("dFdy", desc("Returns the partial derivative of x with respect to the screen-space y-coordinate.")));
	idents.insert(std::make_pair("fwidth", desc("Returns abs(ddx(x)) + abs(ddy(x))")));
	idents.insert(std::make_pair("interpolateAtCentroid", desc("Return the value of the input varying interpolant sampled at a location inside the both the pixel and the primitive being processed.")));
	idents.insert(std::make_pair("interpolateAtSample", desc("Return the value of the input varying interpolant at the location of sample number sample.")));
	idents.insert(std::make_pair("interpolateAtOffset", desc("Return the value of the input varying interpolant sampled at an offset from the center of the pixel specified by offset.")));
	idents.insert(std::make_pair("noise1", desc("Generates a random value")));
	idents.insert(std::make_pair("noise2", desc("Generates a random value")));
	idents.insert(std::make_pair("noise3", desc("Generates a random value")));
	idents.insert(std::make_pair("noise4", desc("Generates a random value")));
	idents.insert(std::make_pair("EmitStreamVertex", desc("Emit the current values of output variables to the current output primitive on stream stream.")));
	idents.insert(std::make_pair("EndStreamPrimitive", desc("Completes the current output primitive on stream stream and starts a new one.")));
	idents.insert(std::make_pair("EmitVertex", desc("Emit the current values to the current output primitive.")));
	idents.insert(std::make_pair("EndPrimitive", desc("Completes the current output primitive and starts a new one.")));
	idents.insert(std::make_pair("barrier", desc("For any given static instance of barrier(), all tessellation control shader invocations for a single input patch must enter it before any will be allowed to continue beyond it.")));
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::C()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
			"signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
			"_Noreturn", "_Static_assert", "_Thread_local"
		};
		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
			"ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("//.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";

		langDef.mCaseSensitive = true;

		langDef.mName = "C";

		inited = true;
	}
	return langDef;
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::SQL()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
			"AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE",
			"BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE",
			"CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE",
			"COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER",
			"CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE",
			"CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION",
			"DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
			"DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING","DROP", "OPENROWSET", "VIEW",
			"DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"ABS",  "ACOS",  "ADD_MONTHS",  "ASCII",  "ASCIISTR",  "ASIN",  "ATAN",  "ATAN2",  "AVG",  "BFILENAME",  "BIN_TO_NUM",  "BITAND",  "CARDINALITY",  "CASE",  "CAST",  "CEIL",
			"CHARTOROWID",  "CHR",  "COALESCE",  "COMPOSE",  "CONCAT",  "CONVERT",  "CORR",  "COS",  "COSH",  "COUNT",  "COVAR_POP",  "COVAR_SAMP",  "CUME_DIST",  "CURRENT_DATE",
			"CURRENT_TIMESTAMP",  "DBTIMEZONE",  "DECODE",  "DECOMPOSE",  "DENSE_RANK",  "DUMP",  "EMPTY_BLOB",  "EMPTY_CLOB",  "EXP",  "EXTRACT",  "FIRST_VALUE",  "FLOOR",  "FROM_TZ",  "GREATEST",
			"GROUP_ID",  "HEXTORAW",  "INITCAP",  "INSTR",  "INSTR2",  "INSTR4",  "INSTRB",  "INSTRC",  "LAG",  "LAST_DAY",  "LAST_VALUE",  "LEAD",  "LEAST",  "LENGTH",  "LENGTH2",  "LENGTH4",
			"LENGTHB",  "LENGTHC",  "LISTAGG",  "LN",  "LNNVL",  "LOCALTIMESTAMP",  "LOG",  "LOWER",  "LPAD",  "LTRIM",  "MAX",  "MEDIAN",  "MIN",  "MOD",  "MONTHS_BETWEEN",  "NANVL",  "NCHR",
			"NEW_TIME",  "NEXT_DAY",  "NTH_VALUE",  "NULLIF",  "NUMTODSINTERVAL",  "NUMTOYMINTERVAL",  "NVL",  "NVL2",  "POWER",  "RANK",  "RAWTOHEX",  "REGEXP_COUNT",  "REGEXP_INSTR",
			"REGEXP_REPLACE",  "REGEXP_SUBSTR",  "REMAINDER",  "REPLACE",  "ROUND",  "ROWNUM",  "RPAD",  "RTRIM",  "SESSIONTIMEZONE",  "SIGN",  "SIN",  "SINH",
			"SOUNDEX",  "SQRT",  "STDDEV",  "SUBSTR",  "SUM",  "SYS_CONTEXT",  "SYSDATE",  "SYSTIMESTAMP",  "TAN",  "TANH",  "TO_CHAR",  "TO_CLOB",  "TO_DATE",  "TO_DSINTERVAL",  "TO_LOB",
			"TO_MULTI_BYTE",  "TO_NCLOB",  "TO_NUMBER",  "TO_SINGLE_BYTE",  "TO_TIMESTAMP",  "TO_TIMESTAMP_TZ",  "TO_YMINTERVAL",  "TRANSLATE",  "TRIM",  "TRUNC", "TZ_OFFSET",  "UID",  "UPPER",
			"USER",  "USERENV",  "VAR_POP",  "VAR_SAMP",  "VARIANCE",  "VSIZE "
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\-\\-.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";

		langDef.mCaseSensitive = false;

		langDef.mName = "SQL";

		inited = true;
	}
	return langDef;
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::AngelScript()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for",
			"from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not",
			"null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
			"uint64", "void", "while", "xor"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE",
			"complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("//.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "/*";
		langDef.mCommentEnd = "*/";

		langDef.mCaseSensitive = true;

		langDef.mName = "AngelScript";

		inited = true;
	}
	return langDef;
}

TextEditor::LanguageDefinition TextEditor::LanguageDefinition::Lua()
{
	static bool inited = false;
	static LanguageDefinition langDef;
	if (!inited)
	{
		static const char* const keywords[] = {
			"and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
		};

		for (auto& k : keywords)
			langDef.mKeywords.insert(k);

		static const char* const identifiers[] = {
			"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring",  "next",  "pairs",  "pcall",  "print",  "rawequal",  "rawlen",  "rawget",  "rawset",
			"select",  "setmetatable",  "tonumber",  "tostring",  "type",  "xpcall",  "_G",  "_VERSION","arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", 
			"rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug","getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", 
			"getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen", 
			"read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger",
			"floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh",
			 "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock",
			 "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep",
			 "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern",
			 "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
		};
		for (auto& k : identifiers)
		{
			Identifier id;
			id.mDeclaration = "Built-in function";
			langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
		}

		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\-\\-.*", PaletteIndex::Comment));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
		langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

		langDef.mCommentStart = "\\-\\-\\[\\[";
		langDef.mCommentEnd = "\\]\\]";

		langDef.mCaseSensitive = true;

		langDef.mName = "Lua";

		inited = true;
	}
	return langDef;
}
