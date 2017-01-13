#pragma once

#include <vector>
#include <util/type.h>
#include <util/time.h>

namespace bx
{

//////////////////////////////////////////////////////////////////////////
class GameState
{
public:
    virtual ~GameState() {}

    virtual const char* GetName() const = 0;
    virtual void OnStart () {}
    virtual void OnStop  () {}
    virtual void OnPause () {}
    virtual void OnResume() {}
    virtual void OnUpdate() {}
};

//////////////////////////////////////////////////////////////////////////
struct GameStateId
{
    u32 i;
};

//////////////////////////////////////////////////////////////////////////
class Game
{
public:
    GameStateId AddState( GameState* state );
    GameStateId FindState( const char* name );
    
    bool PushState( GameStateId stateId );
    bool PopState();

    virtual void StartUp () {}
    virtual void ShutDown() {}
    
    virtual void Update() {}
    virtual void Pause () {}
    virtual void Resume() {}

private:
    std::vector< GameState* > _state_stack;
    std::vector< GameState* > _states;

    u64 _time               = 0;
    u64 _time_not_paused    = 0;
    u64 _last_delta_time    = 0;
    bxTimeQuery _time_query = {};
};

}///