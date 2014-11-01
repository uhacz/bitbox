#include "application.h"
#include "window.h"

bool bxApplication_startup( bxApplication* app, int argc, const char** argv )
{
    return app->startup( argc, argv );
}
void bxApplication_shutdown( bxApplication* app )
{
    app->shutdown();
}
int bxApplication_run( bxApplication* app )
{
    int ret = 0;
	
    bxWindow* win = bxWindow_get();

	do 
	{
		win->input.prev = win->input.curr;
		bxInput_clearState( &win->input.curr, false, false, true );
		MSG msg = {0};
		while( PeekMessage(&msg, win->hwnd, 0U, 0U, PM_REMOVE) != 0 )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		
		bxInput_updatePad( win->input.curr.pad, bxInput_State::eMAX_PAD_CONTROLLERS );
		bxInput_computeMouseDelta( &win->input.curr, win->input.prev );

		ret = app->update();
	} while ( ret );

	return ret;
}
