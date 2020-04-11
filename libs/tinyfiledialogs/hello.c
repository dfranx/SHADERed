/*_________
 /         \ hello.c v3.4.3 [Dec 8, 2019] zlib licence
 |tiny file| Hello World file created [November 9, 2014]
 | dialogs | Copyright (c) 2014 - 2018 Guillaume Vareille http://ysengrin.com
 \____  ___/ http://tinyfiledialogs.sourceforge.net
      \|     git clone http://git.code.sf.net/p/tinyfiledialogs/code tinyfd
		 ____________________________________________
		|                                            |
		|   email: tinyfiledialogs at ysengrin.com   |
		|____________________________________________|
         _________________________________________________________________
        |                                                                 |
        | this file is for windows and unix (osx linux, bsd, solaris ...) |
        |_________________________________________________________________|
	 
Please upvote my stackoverflow answer https://stackoverflow.com/a/47651444

tiny file dialogs (cross-platform C C++)
InputBox PasswordBox MessageBox ColorPicker
OpenFileDialog SaveFileDialog SelectFolderDialog
Native dialog library for WINDOWS MAC OSX GTK+ QT CONSOLE & more
SSH supported via automatic switch to console mode or X11 forwarding

one C file + a header (add them to your C or C++ project) with 8 functions:
- beep
- notify popup (tray)
- message & question
- input & password
- save file
- open file(s)
- select folder
- color picker

complements OpenGL Vulkan GLFW GLUT GLUI VTK SFML TGUI
SDL Ogre Unity3d ION OpenCV CEGUI MathGL GLM CPW GLOW
Open3D IMGUI MyGUI GLT NGL STB & GUI less programs

NO INIT
NO MAIN LOOP
NO LINKING
NO INCLUDE

the dialogs can be forced into console mode

Windows (XP to 10) ASCII MBCS UTF-8 UTF-16
- native code & vbs create the graphic dialogs
- enhanced console mode can use dialog.exe from
http://andrear.altervista.org/home/cdialog.php
- basic console input

Unix (command line calls) ASCII UTF-8
- applescript, kdialog, zenity
- python (2 or 3) + tkinter + python-dbus (optional)
- dialog (opens a console if needed)
- basic console input
the same executable can run across desktops & distributions

C89 & C++98 compliant: tested with C & C++ compilers
VisualStudio MinGW-gcc GCC Clang TinyCC OpenWatcom-v2 BorlandC SunCC ZapCC
on Windows Mac Linux Bsd Solaris Minix Raspbian
using Gnome Kde Enlightenment Mate Cinnamon Budgie Unity Lxde Lxqt Xfce
WindowMaker IceWm Cde Jds OpenBox Awesome Jwm Xdm

Bindings for LUA and C# dll, Haskell
Included in LWJGL(java), Rust, Allegrobasic

- License -

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software.  If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


/*
- Here is the Hello World:
    if a console is missing, it will use graphic dialogs
    if a graphical display is absent, it will use console dialogs
		(on windows the input box may take some time to open the first time)
*/


#include <stdio.h>
#include <string.h>
#include "tinyfiledialogs.h"

int main( int argc , char * argv[] )
{
	int lIntValue;
	char const * lTmp;
	char const * lTheSaveFileName;
	char const * lTheOpenFileName;
	char const * lTheSelectFolderName;
	char const * lTheHexColor;
	char const * lWillBeGraphicMode;
	unsigned char lRgbColor[3];
	FILE * lIn;
	char lBuffer[1024];
	char lString[1024];
	char const * lFilterPatterns[2] = { "*.txt", "*.text" };

	tinyfd_verbose = argc - 1;
    tinyfd_silent = 1;

#ifdef _WIN32
	tinyfd_winUtf8 = 0; /* on windows, you decide if char holds 0(default): MBCS or 1: UTF-8 */
#endif

	lWillBeGraphicMode = tinyfd_inputBox("tinyfd_query", NULL, NULL);

#ifdef _MSC_VER
#pragma warning(disable:4996) /* silences warning about strcpy strcat fopen*/
#endif

	strcpy(lBuffer, "v");
	strcat(lBuffer, tinyfd_version);
	if (lWillBeGraphicMode)
	{
		strcat(lBuffer, "\ngraphic mode: ");
	}
	else
	{
		strcat(lBuffer, "\nconsole mode: ");
	}
	strcat(lBuffer, tinyfd_response);
	strcat(lBuffer, "\n");
	strcat(lBuffer, tinyfd_needs+78);
	strcpy(lString, "hello");
	tinyfd_messageBox(lString, lBuffer, "ok", "info", 0);

	tinyfd_notifyPopup("the title", "the message\n\tfrom outer-space", "info");

	/*tinyfd_forceConsole = 1;*/
	if ( lWillBeGraphicMode && ! tinyfd_forceConsole )
	{
		lIntValue = tinyfd_messageBox("Hello World",
			"graphic dialogs [yes] / console mode [no]?",
			"yesno", "question", 1);
		tinyfd_forceConsole = ! lIntValue ;
	
		/*lIntValue = tinyfd_messageBox("Hello World",
			"graphic dialogs [yes] / console mode [no]?",
			"yesnocancel", "question", 1);
		tinyfd_forceConsole = (lIntValue == 2);*/
	}

	lTmp = tinyfd_inputBox(
		"a password box", "your password will be revealed", NULL);

	if (!lTmp) return 1 ;

	/* copy lTmp because saveDialog would overwrites
	inputBox static buffer in basicinput mode */

	strcpy(lString, lTmp);

	lTheSaveFileName = tinyfd_saveFileDialog(
		"let us save this password",
		"passwordFile.txt",
		2,
		lFilterPatterns,
		NULL);

	if (! lTheSaveFileName)
	{
		tinyfd_messageBox(
			"Error",
			"Save file name is NULL",
			"ok",
			"error",
			1);
		return 1 ;
	}

	lIn = fopen(lTheSaveFileName, "w");
	if (!lIn)
	{
		tinyfd_messageBox(
			"Error",
			"Can not open this file in write mode",
			"ok",
			"error",
			1);
		return 1 ;
	}
	fputs(lString, lIn);
	fclose(lIn);

	lTheOpenFileName = tinyfd_openFileDialog(
		"let us read the password back",
		"",
		2,
		lFilterPatterns,
		NULL,
		0);

	if (! lTheOpenFileName)
	{
		tinyfd_messageBox(
			"Error",
			"Open file name is NULL",
			"ok",
			"error",
			1);
		return 1 ;
	}

	lIn = fopen(lTheOpenFileName, "r");

#ifdef _MSC_VER
#pragma warning(default:4996)
#endif

	if (!lIn)
	{
		tinyfd_messageBox(
			"Error",
			"Can not open this file in read mode",
			"ok",
			"error",
			1);
		return(1);
	}
	lBuffer[0] = '\0';
	fgets(lBuffer, sizeof(lBuffer), lIn);
	fclose(lIn);

	tinyfd_messageBox("your password is",
			lBuffer, "ok", "info", 1);

	lTheSelectFolderName = tinyfd_selectFolderDialog(
		"let us just select a directory", NULL);

	if (!lTheSelectFolderName)
	{
		tinyfd_messageBox(
			"Error",
			"Select folder name is NULL",
			"ok",
			"error",
			1);
		return 1;
	}

	tinyfd_messageBox("The selected folder is",
		lTheSelectFolderName, "ok", "info", 1);

	lTheHexColor = tinyfd_colorChooser(
		"choose a nice color",
		"#FF0077",
		lRgbColor,
		lRgbColor);

	if (!lTheHexColor)
	{
		tinyfd_messageBox(
			"Error",
			"hexcolor is NULL",
			"ok",
			"error",
			1);
		return 1;
	}

	tinyfd_messageBox("The selected hexcolor is",
		lTheHexColor, "ok", "info", 1);

	tinyfd_beep();

	return 0;
}

/*
OSX :
$ clang -o hello.app hello.c tinyfiledialogs.c
( or gcc )

UNIX :
$ gcc -o hello hello.c tinyfiledialogs.c
( or clang tcc owcc cc CC  )

Windows :
	MinGW needs gcc >= v4.9 otherwise some headers are incomplete:
	> gcc -o hello.exe hello.c tinyfiledialogs.c -LC:/mingw/lib -lcomdlg32 -lole32

	TinyCC needs >= v0.9.27 (+ tweaks - contact me) otherwise some headers are missing
	> tcc -o hello.exe hello.c tinyfiledialogs.c
	    -isystem C:\tcc\winapi-full-for-0.9.27\include\winapi
	    -lcomdlg32 -lole32 -luser32 -lshell32

	Borland C: > bcc32c -o hello.exe hello.c tinyfiledialogs.c

	OpenWatcom v2: create a character-mode executable project.

	VisualStudio :
	  Create a console application project,
	  it links against Comdlg32.lib & Ole32.lib.
*/
