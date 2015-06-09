#include "application.h"
#include "window.h"
#include <util\time.h>

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

    bxTimeQuery timeQuery = bxTimeQuery::begin();
	do 
	{
        bxTimeQuery::end( &timeQuery );        
        const u64 deltaTimeUS = timeQuery.durationUS;
        timeQuery = bxTimeQuery::begin();

        

		MSG msg = {0};
		while( PeekMessage(&msg, win->hwnd, 0U, 0U, PM_REMOVE) != 0 )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		
        bxInput* input = &win->input;
        bxInput_KeyboardState* kbdState = input->kbd.currentState();
        for( int i = 0; i < 256; ++i )
        {
            kbdState->keys[i] = ( GetAsyncKeyState( i ) & 0x8000 ) != 0;
        }

		bxInput_updatePad( win->input.pad.currentState(), 1 );
		bxInput_computeMouseDelta( win->input.mouse.currentState(), win->input.mouse.previousState() );

		ret = app->update( deltaTimeUS );

        bxInput_swap( &win->input );
        bxInput_clear( &win->input, true, false, true );

	} while ( ret );

	return ret;
}
