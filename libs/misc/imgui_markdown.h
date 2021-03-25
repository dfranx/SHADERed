#pragma once

// License: zlib
// Copyright (c) 2019 Juliette Foucaut & Doug Binks
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

/*
API BREAKING CHANGES
====================
- 2019/02/01 - Changed LinkCallback parameters, see https://github.com/juliettef/imgui_markdown/issues/2
- 2019/02/05 - Added ImageCallback parameter to ImGui::MarkdownConfig
- 2019/02/06 - Added useLinkCallback member variable to MarkdownImageData to configure using images as links
*/

/*
imgui_markdown https://github.com/juliettef/imgui_markdown
Markdown for Dear ImGui

A permissively licensed markdown single-header library for https://github.com/ocornut/imgui

imgui_markdown currently supports the following markdown functionality:
 - Wrapped text
 - Headers H1, H2, H3
 - Indented text, multi levels
 - Unordered lists and sub-lists
 - Links
 - Images
 
Syntax

Wrapping: 
Text wraps automatically. To add a new line, use 'Return'.

Headers:
# H1
## H2
### H3

Indents: 
On a new line, at the start of the line, add two spaces per indent.
··Indent level 1
····Indent level 2

Unordered lists: 
On a new line, at the start of the line, add two spaces, an asterisks and a space. 
For nested lists, add two additional spaces in front of the asterisk per list level increment.
··*·Unordered List level 1
····*·Unordered List level 2

Links:
[link description](https://...)

Images:
![image alt text](image identifier e.g. filename)

===============================================================================

// Example use on Windows with links opening in a browser

#include "ImGui.h"                // https://github.com/ocornut/imgui
#include "imgui_markdown.h"       // https://github.com/juliettef/imgui_markdown
#include "IconsFontAwesome5.h"    // https://github.com/juliettef/IconFontCppHeaders

// Following includes for Windows LinkCallback
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Shellapi.h"
#include <string>

void LinkCallback( ImGui::MarkdownLinkCallbackData data_ );
inline ImGui::MarkdownImageData ImageCallback( ImGui::MarkdownLinkCallbackData data_ );

// You can make your own Markdown function with your prefered string container and markdown config.
static ImGui::MarkdownConfig mdConfig{ LinkCallback, ImageCallback, ICON_FA_LINK, { { NULL, true }, { NULL, true }, { NULL, false } } };

void LinkCallback( ImGui::MarkdownLinkCallbackData data_ )
{
    std::string url( data_.link, data_.linkLength );
    if( !data_.isImage )
    {
        ShellExecuteA( NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL );
    }
}

inline ImGui::MarkdownImageData ImageCallback( ImGui::MarkdownLinkCallbackData data_ )
{
    // In your application you would load an image based on data_ input. Here we just use the imgui font texture.
    ImTextureID image = ImGui::GetIO().Fonts->TexID;
    ImGui::MarkdownImageData imageData{ true, false, image, ImVec2( 40.0f, 20.0f ) };
    return imageData;
}

void LoadFonts( float fontSize_ = 12.0f )
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    // Base font
    io.Fonts->AddFontFromFileTTF( "myfont.ttf", fontSize_ );
    // Bold headings H2 and H3
    mdConfig.headingFormats[ 1 ].font = io.Fonts->AddFontFromFileTTF( "myfont-bold.ttf", fontSize_ );
    mdConfig.headingFormats[ 2 ].font = mdConfig.headingFormats[ 1 ].font;
    // bold heading H1
    float fontSizeH1 = fontSize_ * 1.1f;
    mdConfig.headingFormats[ 0 ].font = io.Fonts->AddFontFromFileTTF( "myfont-bold.ttf", fontSizeH1 );
}

void Markdown( const std::string& markdown_ )
{
    // fonts for, respectively, headings H1, H2, H3 and beyond
    ImGui::Markdown( markdown_.c_str(), markdown_.length(), mdConfig );
}

void MarkdownExample()
{
    const std::string markdownText = u8R"(
# H1 Header: Text and Links
You can add [links like this one to enkisoftware](https://www.enkisoftware.com/) and lines will wrap well.
## H2 Header: indented text.
  This text has an indent (two leading spaces).
    This one has two.
### H3 Header: Lists
  * Unordered lists
    * Lists can be indented with two extra spaces.
  * Lists can have [links like this one to Avoyd](https://www.avoyd.com/)
)";
    Markdown( markdownText );
}

===============================================================================
*/


#include <stdint.h>

namespace ImGui
{
    //-----------------------------------------------------------------------------
    // Basic types
    //-----------------------------------------------------------------------------

    struct Link;
    struct MarkdownConfig;

    struct MarkdownLinkCallbackData                                 // for both links and images
    {
        const char*             text;                               // text between square brackets []
        int                     textLength;
        const char*             link;                               // text between brackets ()
        int                     linkLength;
        void*                   userData;
        bool                    isImage;                            // true if '!' is detected in front of the link syntax
    };
    
    struct MarkdownImageData
    {
        bool                    isValid = false;                    // if true, will draw the image
        bool                    useLinkCallback = false;            // if true, linkCallback will be called when image is clicked
        ImTextureID             user_texture_id;                    // see ImGui::Image
        const ImVec2            size;                               // see ImGui::Image
        const ImVec2            uv0 = ImVec2( 0, 0 );               // see ImGui::Image
        const ImVec2            uv1 = ImVec2( 1, 1 );               // see ImGui::Image
        const ImVec4            tint_col = ImVec4( 1, 1, 1, 1 );    // see ImGui::Image
        const ImVec4            border_col = ImVec4( 0, 0, 0, 0 );  // see ImGui::Image
    };

    typedef void                MarkdownLinkCallback( MarkdownLinkCallbackData data );
    typedef MarkdownImageData   MarkdownImageCallback( MarkdownLinkCallbackData data );

    struct MarkdownHeadingFormat
    {   
        ImFont*                 font;                               // ImGui font
        bool                    separator;                          // if true, an underlined separator is drawn after the header
    };

    // Configuration struct for Markdown
    // - linkCallback is called when a link is clicked on
    // - linkIcon is a string which encode a "Link" icon, if available in the current font (e.g. linkIcon = ICON_FA_LINK with FontAwesome + IconFontCppHeaders https://github.com/juliettef/IconFontCppHeaders)
    // - HeadingFormat controls the format of heading H1 to H3, those above H3 use H3 format
    struct MarkdownConfig
    {
        static const int        NUMHEADINGS = 3;

        MarkdownLinkCallback*   linkCallback = NULL;
        MarkdownImageCallback*  imageCallback = NULL;
        const char*             linkIcon = "";                      // icon displayd in link tooltip
        MarkdownHeadingFormat   headingFormats[ NUMHEADINGS ] = { { NULL, true }, { NULL, true }, { NULL, true } };
        void*                   userData = NULL;
    };

    //-----------------------------------------------------------------------------
    // External interface
    //-----------------------------------------------------------------------------

    inline void Markdown( const char* markdown_, size_t markdownLength_, const MarkdownConfig& mdConfig_ );

    //-----------------------------------------------------------------------------
    // Internals
    //-----------------------------------------------------------------------------

    struct TextRegion;
    struct Line;
    inline void UnderLine( ImColor col_ );
    inline void RenderLine( const char* markdown_, Line& line_, TextRegion& textRegion_, const MarkdownConfig& mdConfig_ );

    struct TextRegion
    {
        TextRegion() : indentX( 0.0f )
        {
        }
        ~TextRegion()
        {
            ResetIndent();
        }

        // ImGui::TextWrapped will wrap at the starting position
        // so to work around this we render using our own wrapping for the first line
        void RenderTextWrapped( const char* text_, const char* text_end_, bool bIndentToHere_ = false )
        {
            const float scale = 1.0f;
            float       widthLeft = GetContentRegionAvail().x;
            const char* endLine = ImGui::GetFont()->CalcWordWrapPositionA( scale, text_, text_end_, widthLeft );
            ImGui::TextUnformatted( text_, endLine );
            if( bIndentToHere_ )
            {
                float indentNeeded = GetContentRegionAvail().x - widthLeft;
                if( indentNeeded )
                {
                    ImGui::Indent( indentNeeded );
                    indentX += indentNeeded;
                }
            }
            widthLeft = GetContentRegionAvail().x;
            while( endLine < text_end_ )
            {
                text_ = endLine;
                if( *text_ == ' ' ) { ++text_; }    // skip a space at start of line
                endLine = ImGui::GetFont()->CalcWordWrapPositionA( scale, text_, text_end_, widthLeft );
                if( text_ == endLine ) 
                {
                    endLine++;
                }
                ImGui::TextUnformatted( text_, endLine );
            }
        }

        void RenderListTextWrapped( const char* text_, const char* text_end_ )
        {
            ImGui::Bullet();
            ImGui::SameLine();
            RenderTextWrapped( text_, text_end_, true );
        }

        bool RenderLinkText( const char* text_, const char* text_end_, const Link& link_, const ImGuiStyle& style_, 
            const char* markdown_, const MarkdownConfig& mdConfig_, const char** linkHoverStart_ );

        void RenderLinkTextWrapped( const char* text_, const char* text_end_, const Link& link_, const ImGuiStyle& style_,
            const char* markdown_, const MarkdownConfig& mdConfig_, const char** linkHoverStart_, bool bIndentToHere_ = false );

        void ResetIndent()
        {
            if( indentX > 0.0f )
            {
                ImGui::Unindent( indentX );
            }
            indentX = 0.0f;
        }

    private:
        float       indentX;
    };

    // Text that starts after a new line (or at beginning) and ends with a newline (or at end)
    struct Line {
        bool isHeading = false;
        bool isUnorderedListStart = false;
        bool isLeadingSpace = true;     // spaces at start of line
        int  leadSpaceCount = 0;
        int  headingCount = 0;
        int  lineStart = 0;
        int  lineEnd   = 0;
        int  lastRenderPosition = 0;     // lines may get rendered in multiple pieces
    };

    struct TextBlock {                  // subset of line
        int start = 0;
        int stop  = 0;
        int size() const
        {
            return stop - start;
        }
    };

    struct Link {
        enum LinkState {
            NO_LINK,
            HAS_SQUARE_BRACKET_OPEN,
            HAS_SQUARE_BRACKETS,
            HAS_SQUARE_BRACKETS_ROUND_BRACKET_OPEN,
        };
        LinkState state = NO_LINK;
        TextBlock text;
        TextBlock url;
        bool isImage = false;
    };

    inline void UnderLine( ImColor col_ )
    {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.y = max.y;
        ImGui::GetWindowDrawList()->AddLine( min, max, col_, 1.0f );
    }

    inline void RenderLine( const char* markdown_, Line& line_, TextRegion& textRegion_, const MarkdownConfig& mdConfig_ )
    {
        // indent
        int indentStart = 0;
        if( line_.isUnorderedListStart )    // ImGui unordered list render always adds one indent
        { 
            indentStart = 1; 
        }
        for( int j = indentStart; j < line_.leadSpaceCount / 2; ++j )    // add indents
        {
            ImGui::Indent();
        }

        // render
        int textStart = line_.lastRenderPosition + 1;
        int textSize = line_.lineEnd - textStart;
        if( line_.isUnorderedListStart )    // render unordered list
        {
            const char* text = markdown_ + textStart + 1;
            textRegion_.RenderListTextWrapped( text, text + textSize - 1 );
        }
        else if( line_.isHeading )          // render heading
        {
            MarkdownHeadingFormat fmt;
            if( line_.headingCount > mdConfig_.NUMHEADINGS )
            {
                fmt = mdConfig_.headingFormats[ mdConfig_.NUMHEADINGS - 1 ];
            }
            else
            {
                 fmt = mdConfig_.headingFormats[ line_.headingCount - 1 ];
            }

            bool popFontRequired = false;
            if( fmt.font && fmt.font != ImGui::GetFont() )
            {
                ImGui::PushFont( fmt.font );
                popFontRequired = true;
            }
            const char* text = markdown_ + textStart + 1;
            ImGui::NewLine();
            textRegion_.RenderTextWrapped( text, text + textSize - 1 );
            if( fmt.separator )
            {
                ImGui::Separator();
            }
            ImGui::NewLine();
            if( popFontRequired )
            {
                ImGui::PopFont();
            }
        }
        else                                // render a normal paragraph chunk
        {
            const char* text = markdown_ + textStart;
            textRegion_.RenderTextWrapped( text, text + textSize );
        }
            
        // unindent
        for( int j = indentStart; j < line_.leadSpaceCount / 2; ++j )
        {
            ImGui::Unindent();
        }
    }
    
    // render markdown
    inline void Markdown( const char* markdown_, size_t markdownLength_, const MarkdownConfig& mdConfig_ )
    {
        static const char* linkHoverStart = NULL; // we need to preserve status of link hovering between frames
        ImGuiStyle& style = ImGui::GetStyle();
        Line        line;
        Link        link;
        TextRegion  textRegion;

        char c = 0;
        for( int i=0; i < (int)markdownLength_; ++i )
        {
            c = markdown_[i];               // get the character at index
            if( c == 0 ) { break; }         // shouldn't happen but don't go beyond 0.

            // If we're at the beginning of the line, count any spaces
            if( line.isLeadingSpace )
            {
                if( c == ' ' )
                {
                    ++line.leadSpaceCount;
                    continue;
                }
                else
                {
                    line.isLeadingSpace = false;
                    line.lastRenderPosition = i - 1;
                    if(( c == '*' ) && ( line.leadSpaceCount >= 2 ))
                    {
                        if(( (int)markdownLength_ > i + 1 ) && ( markdown_[ i + 1 ] == ' ' ))    // space after '*'
                        {
                            line.isUnorderedListStart = true;
                            ++i;
                            ++line.lastRenderPosition;
                        }
                        continue;
                    }
                    else if( c == '#' )
                    {
                        line.headingCount++;
                        bool bContinueChecking = true;
                        uint32_t j = i;
                        while( ++j < (int)markdownLength_ && bContinueChecking )
                        {
                            c = markdown_[j];
                            switch( c )
                            {
                            case '#':
                                line.headingCount++;
                                break;
                            case ' ':
                                line.lastRenderPosition = j - 1;
                                i = j;
                                line.isHeading = true;
                                bContinueChecking = false;
                                break;
                            default:
                                line.isHeading = false;
                                bContinueChecking = false;
                                break;
                            }
                        }
                        if( line.isHeading ) { continue; }
                    }
                }
            }

            // Test to see if we have a link
            switch( link.state )
            {
            case Link::NO_LINK:
                if( c == '[' )
                {
                    link.state = Link::HAS_SQUARE_BRACKET_OPEN;
                    link.text.start = i + 1;
                    if( i > 0 && markdown_[i - 1] == '!' )
                    {
                        link.isImage = true;
                    }
                }
                break;
            case Link::HAS_SQUARE_BRACKET_OPEN:
                if( c == ']' )
                {
                    link.state = Link::HAS_SQUARE_BRACKETS;
                    link.text.stop = i;
                }
                break;
            case Link::HAS_SQUARE_BRACKETS:
                if( c == '(' )
                {
                    link.state = Link::HAS_SQUARE_BRACKETS_ROUND_BRACKET_OPEN;
                    link.url.start = i + 1;
                }
                break;
            case Link::HAS_SQUARE_BRACKETS_ROUND_BRACKET_OPEN:
                if( c == ')' )
                {
                    // render previous line content
                    line.lineEnd = link.text.start - ( link.isImage ? 2 : 1 );
                    RenderLine( markdown_, line, textRegion, mdConfig_ );
                    line.leadSpaceCount = 0;
                    link.url.stop = i;
                    line.isUnorderedListStart = false;    // the following text shouldn't have bullets
                    ImGui::SameLine( 0.0f, 0.0f );
                    if( link.isImage )   // it's an image, render it.
                    {
                        bool drawnImage = false;
                        bool useLinkCallback = false;
                        if( mdConfig_.imageCallback )
                        {
                            MarkdownImageData imageData = mdConfig_.imageCallback({ markdown_ + link.text.start, link.text.size(), markdown_ + link.url.start, link.url.size(), mdConfig_.userData });
                            useLinkCallback = imageData.useLinkCallback;
                            if( imageData.isValid )
                            {
                                ImGui::Image( imageData.user_texture_id, imageData.size, imageData.uv0, imageData.uv1, imageData.tint_col, imageData.border_col );
                                drawnImage = true;
                            }
                        }
                        if( !drawnImage )
                        {
                            ImGui::Text( "( Image %.*s not loaded )", link.url.size(), markdown_ + link.url.start );
                        }
                        if( ImGui::IsItemHovered() )
                        {
                            if( ImGui::IsMouseClicked( 0 ) && mdConfig_.linkCallback && useLinkCallback )
                            {
                                mdConfig_.linkCallback( { markdown_ + link.text.start, link.text.size(), markdown_ + link.url.start, link.url.size(), mdConfig_.userData, true } );
                            }
                            if( link.text.size() > 0 )
                            {
                                ImGui::SetTooltip( "%.*s", link.text.size(), markdown_ + link.text.start );
                            }
                        }
                    }
                    else                 // it's a link, render it.
                    {
                        textRegion.RenderLinkTextWrapped( markdown_ + link.text.start, markdown_ + link.text.start + link.text.size(), link, style, markdown_, mdConfig_, &linkHoverStart, false );
                    }
                    ImGui::SameLine( 0.0f, 0.0f );
                    // reset the link by reinitializing it
                    link = Link();
                    line.lastRenderPosition = i;
                    break;
                }
            }

            // handle end of line (render)
            if( c == '\n' )
            {
                // render the line
                line.lineEnd = i;
                RenderLine( markdown_, line, textRegion, mdConfig_ );

                // reset the line
                line = Line();
                line.lineStart = i + 1;
                line.lastRenderPosition = i;

                textRegion.ResetIndent();
                
                // reset the link
                link = Link();
            }
        }

        // render any remaining text if last char wasn't 0
        if( markdownLength_ && line.lineStart < (int)markdownLength_ && markdown_[ line.lineStart ] != 0 )
        {
            // handle both null terminated and non null terminated strings
            line.lineEnd = (int)markdownLength_;
            if( 0 == markdown_[ line.lineEnd - 1 ] )
            {
                --line.lineEnd;
            }
            RenderLine( markdown_, line, textRegion, mdConfig_ );
        }
    }


    inline bool TextRegion::RenderLinkText( const char* text_, const char* text_end_, const Link& link_, const ImGuiStyle& style_,
        const char* markdown_, const MarkdownConfig& mdConfig_, const char** linkHoverStart_ )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, style_.Colors[ImGuiCol_ButtonHovered] );
        ImGui::PushTextWrapPos( -1.0f );
        ImGui::TextUnformatted( text_, text_end_ );
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        bool bThisItemHovered = ImGui::IsItemHovered();
        if(bThisItemHovered)
        {
            *linkHoverStart_ = markdown_ + link_.text.start;
        }
        bool bHovered = bThisItemHovered || ( *linkHoverStart_ == ( markdown_ + link_.text.start ) );

        if(bHovered)
        {
            if(ImGui::IsMouseClicked( 0 ) && mdConfig_.linkCallback)
            {
                mdConfig_.linkCallback({ markdown_ + link_.text.start, link_.text.size(), markdown_ + link_.url.start, link_.url.size(), mdConfig_.userData, false });
            }
            ImGui::UnderLine( style_.Colors[ImGuiCol_ButtonHovered] );
            ImGui::SetTooltip( "%s Open in browser\n%.*s", mdConfig_.linkIcon, link_.url.size(), markdown_ + link_.url.start );
        }
        else
        {
            ImGui::UnderLine( style_.Colors[ImGuiCol_Button] );
        }
        return bThisItemHovered;
    }

    inline void TextRegion::RenderLinkTextWrapped( const char* text_, const char* text_end_, const Link& link_, const ImGuiStyle& style_,
        const char* markdown_, const MarkdownConfig& mdConfig_, const char** linkHoverStart_, bool bIndentToHere_ )
        {
            const float scale = 1.0f;
            float       widthLeft = GetContentRegionAvail().x;
            const char* endLine = ImGui::GetFont()->CalcWordWrapPositionA( scale, text_, text_end_, widthLeft );
            bool bHovered = RenderLinkText( text_, endLine, link_, style_, markdown_, mdConfig_, linkHoverStart_ );
            if( bIndentToHere_ )
            {
                float indentNeeded = GetContentRegionAvail().x - widthLeft;
                if( indentNeeded )
                {
                    ImGui::Indent( indentNeeded );
                    indentX += indentNeeded;
                }
            }
            widthLeft = GetContentRegionAvail().x;
            while( endLine < text_end_ )
            {
                text_ = endLine;
                if( *text_ == ' ' ) { ++text_; }    // skip a space at start of line
                endLine = ImGui::GetFont()->CalcWordWrapPositionA( scale, text_, text_end_, widthLeft );
                if( text_ == endLine ) 
                {
                    endLine++;
                }
                bool bThisLineHovered = RenderLinkText( text_, endLine, link_, style_, markdown_, mdConfig_, linkHoverStart_ );
                bHovered = bHovered || bThisLineHovered;
            }
            if( !bHovered && *linkHoverStart_ == markdown_ + link_.text.start )
            {
                *linkHoverStart_ = NULL;
            }
        }
}

