#pragma once

#include <vector>
#include <util/type.h>
#include <util/camera.h>
#include <rdi/rdi_backend.h>
#include "renderer_camera.h"

#include "game_time.h"
#include "profiler.h"

namespace bx
{

//////////////////////////////////////////////////////////////////////////
class Game;
class GameState
{
public:
    virtual ~GameState() {}

    virtual const char* GetName    () const                 = 0;
    virtual void OnStartUp         ()                       {}
    virtual void OnShutDown        ()                       {}
    virtual void OnPause           ()                       {}
    virtual void OnResume          ()                       {}
    virtual void OnUpdate          ( const GameTime& time ) {}
    virtual void OnBackgroundUpdate( const GameTime& time ) {}
    virtual void OnRender          ( const GameTime& time, rdi::CommandQueue* cmdq ) {}
    virtual void OnBackgroundRender( const GameTime& time, rdi::CommandQueue* cmdq ) {}

    Game* GetGame();
};

//////////////////////////////////////////////////////////////////////////
struct GameStateId
{
    u32 _i = UINT32_MAX;

    GameStateId() {}
    GameStateId( u32 id ): _i( id ) {}

    bool IsValid() const { return _i != UINT32_MAX; }
};

//////////////////////////////////////////////////////////////////////////
class Game
{
public:
    Game();
    virtual ~Game();

    GameStateId AddState( GameState* state );
    GameStateId FindState( const char* name );
    
    GameState* TopState ();
    void       PushState( const char* name );
    void       PushState( GameStateId stateId );
    bool       PopState ();

    void StartUp ();
    void ShutDown();
    
    bool Update();
    void Render();
    void Pause ();
    void Resume();

          gfx::Camera& GetDevCamera()       { return _dev_camera; }
    const gfx::Camera& GetDevCamera() const { return _dev_camera; }

    bool UseDevCamera() const { return _use_dev_camera; }

protected:
    virtual void StartUpImpl    () {}
    virtual void ShutDownImpl   () {}
    virtual bool PreUpdateImpl  ( const GameTime& time ) { return true; }
    virtual bool PostUpdateImpl ( const GameTime& time ) { return true; }
    virtual void PreRenderImpl  ( const GameTime& time, rdi::CommandQueue* cmdq ) {}
    virtual void PostRenderImpl ( const GameTime& time, rdi::CommandQueue* cmdq ) {}
    virtual void PauseImpl      () {}
    virtual void ResumeImpl     () {}

private:
    std::vector< GameState* > _state_stack;
    std::vector< GameState* > _states;

    bxTimeQuery _time_query = {};
    GameTime _time = {};
    bool _pause = false;

    Remotery* _rmt = nullptr;

protected:
    gfx::Camera             _dev_camera = {};
    gfx::CameraInputContext _dev_camera_input_ctx = {};

    bool _use_dev_camera = true;
};

}///