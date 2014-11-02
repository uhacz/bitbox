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
        bxInput_swap( &win->input );
		bxInput_clear( &win->input, false, false, true );

		//win->input.prev = win->input.curr;
		MSG msg = {0};
		while( PeekMessage(&msg, win->hwnd, 0U, 0U, PM_REMOVE) != 0 )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		
		bxInput_updatePad( win->input.pad.currentState(), 1 );
		bxInput_computeMouseDelta( win->input.mouse.currentState(), win->input.mouse.prevState() );

		ret = app->update();

	} while ( ret );

	return ret;
}
