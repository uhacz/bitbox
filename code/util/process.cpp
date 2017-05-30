#include "process.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Tlhelp32.h"
#include <sstream>

#include <Psapi.h>

namespace bx
{
bool IsProcessRunning( const char* processName )
{
    // Get the list of process identifiers.
    DWORD aProcesses[8 * 1024], cbNeeded, cProcesses;

    if( !EnumProcesses( aProcesses, sizeof( aProcesses ), &cbNeeded ) )
    {
        printf( "IsProcessRunning(%s): EnumProcesses failed!", processName );
        return false;
    }

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof( DWORD );

    // Get the name for each process and try find picoHub
    for( unsigned int i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            DWORD processID = aProcesses[i];

            // Get a handle to the process.
            HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );

            char szProcessName[MAX_PATH] = "";

            // Get the process name.
            if( NULL != hProcess )
            {
                HMODULE hMod;
                DWORD cbNeeded2;

                if( EnumProcessModules( hProcess, &hMod, sizeof( hMod ), &cbNeeded2 ) )
                    GetModuleBaseNameA( hProcess, hMod, szProcessName, sizeof( szProcessName ) );

                // Release the handle to the process.
                CloseHandle( hProcess );
            }

            // Check if this is our process
            if( 0 == strcmp( szProcessName, processName ) )
                return true;
        }
    }

    return false;
}

unsigned CountProcessInstances( const char* processName )
{
    // Get the list of process identifiers.
    DWORD aProcesses[8 * 1024], cbNeeded, cProcesses;

    if( !EnumProcesses( aProcesses, sizeof( aProcesses ), &cbNeeded ) )
    {
        printf( "CountProcessInstances(%s) : EnumProcesses failed!", processName );
        return 0;
    }

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof( DWORD );

    unsigned nProc = 0;

    // Get the name for each process and try find picoHub
    for( unsigned int i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            DWORD processID = aProcesses[i];

            // Get a handle to the process.
            HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );

            char szProcessName[MAX_PATH] = "";

            // Get the process name.
            if( NULL != hProcess )
            {
                HMODULE hMod;
                DWORD cbNeeded2;

                if( EnumProcessModules( hProcess, &hMod, sizeof( hMod ), &cbNeeded2 ) )
                    GetModuleBaseNameA( hProcess, hMod, szProcessName, sizeof( szProcessName ) );

                // Release the handle to the process.
                CloseHandle( hProcess );
            }

            // Check if this is our process
            if( 0 == strcmp( szProcessName, processName ) )
                nProc += 1;
        }
    }

    return nProc;
}

bool LaunchProcess( const char* rootPath, const char* processName, const char* commandLine /*= nullptr */ )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    std::string sb;
    if( rootPath )
    {
        sb.append( rootPath );
        sb.append( "\\" );
    }

    sb.append( processName );
    if( commandLine )
    {
        sb.append( " " );
        sb.append( commandLine );
    }

    

    // Start the child process. 
    if( !CreateProcess(
        nullptr,		// Module name
        (char*)sb.c_str(),		// Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        CREATE_NEW_CONSOLE,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
        )
    {
        DWORD err = GetLastError();
        (void)err;
        printf( "CreateProcess failed. Err=%u", err );
        return false;
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    return true;
}

}//
