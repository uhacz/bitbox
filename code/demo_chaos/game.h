#pragma once

#include <vector>
#include <util/type.h>
#include <rdi/rdi_backend.h>

#include "game_time.h"

namespace bx
{

//////////////////////////////////////////////////////////////////////////
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
    virtual ~Game() {}

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

protected:
    virtual void StartUpImpl    () {}
    virtual void ShutDownImpl   () {}
    virtual bool PreUpdateImpl  ( const GameTime& time ) { return true; }
    virtual bool PostUpdateImpl ( const GameTime& time ) { return true; }
    virtual void PauseImpl      () {}
    virtual void ResumeImpl     () {}

private:
    std::vector< GameState* > _state_stack;
    std::vector< GameState* > _states;

    bxTimeQuery _time_query = {};
    GameTime _time = {};
    bool _pause = false;
};

}///