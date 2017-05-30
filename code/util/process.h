#pragma once

namespace bx
{
    bool     IsProcessRunning     ( const char* processName );
    unsigned CountProcessInstances( const char* processName );
    bool     LaunchProcess        ( const char* rootPath, const char* processName, const char* commandLine = nullptr );

}//
