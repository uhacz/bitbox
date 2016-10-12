#include "rdi_backend.h"

#include "rdi_backend_dx11.h"


namespace bx{ namespace rdi{ 

void Startup( uptr hWnd, int winWidth, int winHeight, int fullScreen )
{
    StartupDX11( hWnd, winWidth, winHeight, fullScreen );
}

void Shutdown()
{
    ShutdownDX11();
}

}
}///