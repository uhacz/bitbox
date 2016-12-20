#pragma once

#include <util/type.h>

struct bxWindow;
struct bxInput;
class bxResourceManager;
struct bxGdiDeviceBackend;
struct bxGdiContextBackend;
struct bxGdiContext;

namespace tjdb
{
    class App : public bxApplication
    {
    public:
        App();

        virtual bool startup( int argc, const char** argv );
        virtual void shutdown();

        virtual bool update( u64 deltaTimeUS );

        bxResourceManager* _resourceManager;
        bxGdiDeviceBackend* _gdiDev;
        bxGdiContext* _gdiCtx;
    };


    void startup( bxWindow* win, bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );
    void shutdown( bxGdiDeviceBackend* dev, bxResourceManager* resourceManager );

    void tick( const bxInput* input, bxGdiContext* ctx, u64 deltaTimeMS );
    void draw( bxGdiContext* ctx );


    struct Metronome
    {
        Metronome();

        void init( int bpm, float meter );
        void update( u64 deltaTimeUS );

        u64 clickUS_;
        u64 timerUS_;
        u64 clickTimerUS_;

        u32 flag_click_;
    };

}///