#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <windows.h>
#include "input.h"

typedef int (*bxWindow_MsgCallbackPtr)( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

struct bxWindow
{
	enum 
    {
        eMAX_MSG_CALLBACKS = 4,
    };

    bxWindow()
		: name(0)
        , hwnd(0), parent_hwnd(0), hinstance(0), hdc(0)
		, width(0), height(0), full_screen(0)
        , numWinMsgCallbacks(0)
	{ 
	}

	char* name;
	HWND hwnd;
	HWND parent_hwnd;
	HINSTANCE hinstance;
	HDC hdc;

	u32 width;
	u32 height;
	u32 full_screen : 1;

	bxInput input;

    
    bxWindow_MsgCallbackPtr winMsgCallbacks[ eMAX_MSG_CALLBACKS ];
    i32 numWinMsgCallbacks;
};

bxWindow* bxWindow_get();

bxWindow* bxWindow_create( const char* name, unsigned width, unsigned height, bool full_screen, HWND parent_hwnd );
void bxWindow_release();

void bxWindow_setSize( bxWindow* win, unsigned width, unsigned height );
int bxWindow_addWinMsgCallback( bxWindow* win, bxWindow_MsgCallbackPtr ptr );
void bxWindow_removeWinMsgCallback( bxWindow* win, bxWindow_MsgCallbackPtr ptr );
