#include "window.h"
#include <stdio.h>
#include <util/debug.h>
#include <util/memory.h>
#include <util/string_util.h>

LRESULT CALLBACK default_window_message_proc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static bxWindow* __window;
bxWindow* bxWindow_get() { return __window; }
const bxInput* bxInput_get()
{
    return &__window->input;
}


namespace
{
    void _AdjustWindowSize( HWND hwnd, HWND parent_hwnd, u32 width, u32 height, bool full_screen )
    {
	    int screen_width = GetSystemMetrics(SM_CXSCREEN);
	    int screen_height = GetSystemMetrics(SM_CYSCREEN); 

	    if( !full_screen )
	    {
		    if( !parent_hwnd )
		    {
			    DWORD window_style = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

			    RECT client;
			    client.left = 0;
			    client.top = 0;
			    client.right = width;
			    client.bottom = height;

			    BOOL ret = AdjustWindowRectEx( &client, window_style, FALSE, 0 );
			    SYS_ASSERT( ret );
			    const int tmp_width = client.right - client.left;
			    const int tmp_height = client.bottom - client.top;
			    const int x = 0; //screen_width - tmp_width;
                const int y = screen_height - tmp_height;

			    MoveWindow( hwnd, x, y, tmp_width, tmp_height, FALSE );
		    }
		    else
		    {
			    DWORD window_style = WS_CHILD;// | WS_BORDER;
			    WINDOWINFO win_info;
			    BOOL res = GetWindowInfo( parent_hwnd, &win_info );
			    SYS_ASSERT( res );

			    RECT client;
			    client.left = 0;
			    client.top = 0;
			    client.right = win_info.rcWindow.right;
			    client.bottom = win_info.rcWindow.bottom;

			    //BOOL ret = AdjustWindowRectEx( &client, window_style, FALSE, 0 );
			    //assert( ret );

			    MoveWindow( hwnd, 0, 0, client.right, client.bottom, FALSE );
		    }
	    }
	    else
	    {
		    MoveWindow( hwnd, 0, 0, screen_width, screen_height, FALSE );

		    DEVMODE screen_settings;
		    memset(&screen_settings, 0, sizeof(screen_settings));

		    screen_settings.dmSize         = sizeof(screen_settings);     
		    screen_settings.dmPelsWidth    = screen_width;
		    screen_settings.dmPelsHeight   = screen_height;
		    screen_settings.dmBitsPerPel   = 32;
		    screen_settings.dmFields       = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		    ChangeDisplaySettings( &screen_settings, CDS_FULLSCREEN );

		    ShowCursor( FALSE );
	    }

	    UpdateWindow( hwnd );
    }
}

bxWindow* bxWindow_create( const char* name, unsigned width, unsigned height, bool full_screen, HWND parent_hwnd )
{
    SYS_ASSERT( __window == 0 );
	
	HINSTANCE hinstance = GetModuleHandle(NULL);
	HDC hdc = 0;
	HWND hwnd = 0;

	{ // create window
		WNDCLASS wndClass;
		wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
		wndClass.lpfnWndProc = default_window_message_proc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = hinstance;
		wndClass.hIcon = 0;
		wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
		wndClass.hbrBackground = NULL;
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = name;

		if( ! RegisterClass(&wndClass) )
		{
			DWORD err = GetLastError();
			printf( "create_window: Error with RegisterClass!\n" );
			return false;
		}

		if( full_screen && !parent_hwnd )
		{
			hwnd = CreateWindowEx( 
				WS_EX_APPWINDOW, name, name, WS_POPUP,
				0, 0,
				10, 10,
				NULL, NULL, hinstance, 0 );
		}
		else
		{
			DWORD style = 0;
			DWORD ex_style = 0;
			if( parent_hwnd )
			{
				style = WS_CHILD | WS_BORDER;
				ex_style = WS_EX_TOPMOST|WS_EX_NOPARENTNOTIFY;
			}
			else
			{
                style = WS_BORDER | WS_THICKFRAME;
			} 
			hwnd = CreateWindowEx( 
				ex_style, name, name, style,
				10, 10,
				100, 100, 
				parent_hwnd, NULL, hinstance, 0 );
		}

		if( hwnd == NULL )
		{
			printf( "create_window: Error while creating window (Err: 0x%x)\n", GetLastError() );
			return 0;
		}
	
		hdc = GetDC( hwnd );
	}
	
    bxWindow* win = BX_NEW( bxDefaultAllocator(), bxWindow );
    win->name = string::duplicate( win->name, name );
    __window = win;

	win->hwnd = hwnd;
	win->parent_hwnd = parent_hwnd;
	win->hinstance = hinstance;
	win->hdc = hdc;
	
	win->full_screen = full_screen;

	{ // setup window
		_AdjustWindowSize( hwnd, parent_hwnd, width, height, full_screen );		
		ShowWindow( hwnd, SW_SHOW );

		RECT win_rect;
		GetClientRect( hwnd, &win_rect );
		width = win_rect.right - win_rect.left;
		height = win_rect.bottom - win_rect.top;
	}

    win->width = width;
	win->height = height;
	return __window;
}

void bxWindow_release()
{
	if ( __window->hwnd != NULL )
	{
		DestroyWindow( __window->hwnd );
		__window->hwnd = NULL;
	}
	UnregisterClass( __window->name, __window->hinstance );
    string::free( __window->name );
    BX_DELETE0( bxDefaultAllocator(), __window );

}
void bxWindow_setSize( bxWindow* win, unsigned width, unsigned height )
{
	_AdjustWindowSize( win->hwnd, win->parent_hwnd, width, height, (win->full_screen) ? true : false );
	win->width = width;
	win->height = height;
}

LRESULT CALLBACK default_window_message_proc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    if( !__window )
    {
        return DefWindowProc( hwnd, msg, wParam, lParam );
    }

    int processed = 0;
    for( int i = 0; i < __window->numWinMsgCallbacks && !processed; ++i )
    {
        processed = (*__window->winMsgCallbacks[i])(hwnd, msg, wParam, lParam);
    }
    if( processed )
    {
        return DefWindowProc( hwnd, msg, wParam, lParam );
    }
        


    static bool lButtonPressed = false;
    bxInput* input = &__window->input;
    bxInput_KeyboardState* kbdState = input->kbd.currentState();
    bxInput_MouseState* mouseState = input->mouse.currentState();

    //GetKeyboardState( kbdState->keys );
    
	switch( msg )
	{
	//case WM_KEYUP:
	//	{
	//		if ( wParam < 256 )
	//		{
	//			kbdState->keys[wParam] = false;
	//		}
	//		break;
	//	}
	//case WM_KEYDOWN:
	//	{
	//		if ( wParam < 256 )
	//		{
 //               kbdState->keys[wParam] = true;
	//		}
	//		break;
	//	}

	case WM_LBUTTONDOWN:
		{
			mouseState->lbutton = 1;
			break;
		}

	case WM_LBUTTONUP:
		{
			mouseState->lbutton = 0;
			break;
		}
    case WM_MBUTTONDOWN:
    {
        mouseState->mbutton = 1;
        break;
    }

    case WM_RBUTTONUP:
    {
        mouseState->rbutton = 0;
        break;
    }

    case WM_RBUTTONDOWN:
    {
        mouseState->rbutton = 1;
        break;
    }

    case WM_MBUTTONUP:
    {
        mouseState->mbutton = 0;
        break;
    }

	case WM_MOUSEMOVE:
		{
			POINTS pt = MAKEPOINTS( lParam );
            //const unsigned short x = GET_X_LPARAM(lParam);
			//const unsigned short y = GET_Y_LPARAM(lParam);
            const unsigned short x = pt.x;
            const unsigned short y = pt.y;

			mouseState->x = x;
			mouseState->y = y;
			
			break;
		}

	case WM_SIZE:
		{
			RECT clientRect;
			GetClientRect( __window->hwnd, &clientRect);

			unsigned int clientWindowWidth = clientRect.right - clientRect.left;
			unsigned int clientWindowHeight = clientRect.bottom - clientRect.top;

			__window->width = clientWindowWidth;
			__window->height = clientWindowHeight;

			//_push_event( &g_window, msg, lParam, wParam );

			break;
		}
	case WM_CLOSE:
		//_push_event( &g_window, msg, lParam, wParam );
		PostQuitMessage( 0 );
		break;

	case WM_DESTROY:	
		PostQuitMessage(0);
		break;
	case WM_PARENTNOTIFY:
		PostQuitMessage(0);
		break;
	case WM_SYSCOMMAND:
		{
			switch (wParam)											
			{
			case SC_SCREENSAVE:	
			case SC_MONITORPOWER:
				break;
			default:
				break;
			}
		}
		break;
	}
	
	return DefWindowProc( hwnd, msg, wParam, lParam );
}

int bxWindow_addWinMsgCallback( bxWindow* win, bxWindow_MsgCallbackPtr ptr )
{
    if( win->numWinMsgCallbacks >= bxWindow::eMAX_MSG_CALLBACKS )
        return -1;

    int index = win->numWinMsgCallbacks++;
    win->winMsgCallbacks[ index ] = ptr;
    return index;
}

void bxWindow_removeWinMsgCallback( bxWindow* win, bxWindow_MsgCallbackPtr ptr )
{
    for( int i = 0 ; i < win->numWinMsgCallbacks; ++i )
    {
        if( ptr == win->winMsgCallbacks[i] )
        {
            SYS_ASSERT( win->numWinMsgCallbacks > 0 );
            win->winMsgCallbacks[ i ] = 0;
            win->winMsgCallbacks[ i ] = win->winMsgCallbacks[ --win->numWinMsgCallbacks ];
            break;
        }
    }
}

