#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <windows.h>
#include "input.h"

struct bxWindow
{
	bxWindow()
		: name(0)
        , hwnd(0), parent_hwnd(0), hinstance(0), hdc(0)
		, width(0), height(0), full_screen(0)
	{ 
	}

	char* name;
	HWND hwnd;
	HWND parent_hwnd;
	HINSTANCE hinstance;
	HDC hdc;

	unsigned width;
	unsigned height;
	unsigned full_screen : 1;

	bxInput input;
};

bxWindow* bxWindow_get();

bxWindow* bxWindow_create( const char* name, unsigned width, unsigned height, bool full_screen, HWND parent_hwnd );
void bxWindow_release();

void bxWindow_setSize( bxWindow* win, unsigned width, unsigned height );
