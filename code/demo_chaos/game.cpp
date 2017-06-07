#include "game.h"
#include <util\debug.h>
#include <util\string_util.h>
#include <util\memory.h>
#include <system/window.h>
#include <rdi\rdi_backend_dx11.h>

#include "imgui\imgui.h"
#include "renderer_camera.h"
#include "game_util.h"

namespace bx
{

    static Game* __game = nullptr;

    Game::Game()
    {
        SYS_ASSERT( __game == nullptr );
        __game = this;
    }

    Game::~Game()
    {
        __game = nullptr;
    }

    GameStateId Game::AddState( GameState* state )
{
    if( std::find( _states.begin(), _states.end(), state ) == _states.end() )
    {
        u32 index = (u32)_states.size();
        _states.push_back( state );

        return GameStateId( index );
    }
    SYS_NOT_IMPLEMENTED;
    return GameStateId();
}

GameStateId Game::FindState( const char* name )
{
    for( size_t i = 0; i < _states.size(); ++i )
    {
        if( string::equal( _states[i]->GetName(), name ) )
            return GameStateId( (u32)i );
    }

    return GameStateId();
}

GameState* Game::TopState()
{
    return ( _state_stack.empty() ) ? nullptr : _state_stack.back();
}

void Game::PushState( GameStateId stateId )
{
    SYS_ASSERT( stateId._i < _states.size() );

    GameState* state = _states[stateId._i];
    SYS_ASSERT( std::find( _state_stack.begin(), _state_stack.end(), state ) == _state_stack.end() );

    _state_stack.push_back( state );
}

void Game::PushState( const char* name )
{
    GameStateId id = FindState( name );
    if( id.IsValid() )
        PushState( id );
}

bool Game::PopState()
{
    if( _state_stack.empty() )
        return false;

    _state_stack.pop_back();
    return true;
}

void Game::StartUp()
{
    {
        rmt_CreateGlobalInstance( &_rmt );
    }

    StartUpImpl();

    for( GameState* state : _states )
    {
        state->OnStartUp();
    }
}

void Game::ShutDown()
{
    for( GameState* state : _states )
    {
        state->OnShutDown();
    }
    ShutDownImpl();

    while( !_states.empty() )
    {
        BX_DELETE( bxDefaultAllocator(), _states.back() );
        _states.pop_back();
    }

    {
        rmt_DestroyGlobalInstance( _rmt );
    }
}

bool Game::Update()
{
    rmt_ScopedCPUSample( Update, 0 );

    if( _time_query.isRunning() )
    {
        bxTimeQuery::end( &_time_query );
    }

    const u64 deltaTimeUS = _time_query.durationUS;
    _time_query = bxTimeQuery::begin();



    _time = {};
    if( !_pause )
    {
        _time.delta_time_us = deltaTimeUS;
        _time.time_us += deltaTimeUS;
    }
    _time.time_not_paused_us += deltaTimeUS;
    

    { // dev camera
        bxWindow* win = bxWindow_get();
        if( bxInput_isKeyPressedOnce( &win->input.kbd, '1' ) )
        {
            _use_dev_camera = !_use_dev_camera;
        }
        if( _use_dev_camera )
        {
            game_util::DevCameraCollectInput( &_dev_camera_input_ctx, _time.DeltaTimeSec(), 0.005f );
            _dev_camera.world = _dev_camera_input_ctx.computeMovement( _dev_camera.world, 0.05f );

            gfx::computeMatrices( &_dev_camera );
        }
    }

    bool result = true;
    {
        rmt_ScopedCPUSample( PreUpdate, 0 );
        result &= PreUpdateImpl( _time );
    }

    {
        rmt_ScopedCPUSample( StateUpdate, 0 )
        if( !_state_stack.empty() )
        {
            std::vector<GameState*>::reverse_iterator it = _state_stack.rbegin();
            ( *it )->OnUpdate( _time );
            ++it;
            for( ; it < _state_stack.rend(); ++it )
            {
                ( *it )->OnBackgroundUpdate( _time );
            }
        }
    }

    {
        rmt_ScopedCPUSample( PostUpdate, 0 );
        result &= PostUpdateImpl( _time );
    }

    if( ImGui::Begin( "GameUpdate" ) )
    {
        const float dt = _time.DeltaTimeSec();
        const float dt_inv = ( dt>FLT_EPSILON ) ? 1.f / dt : 0.f;
        ImGui::Text( "DeltaTime: %f", dt );
        ImGui::Text( "FPS: %.2f", dt_inv );
    }
    ImGui::End();

    return result;
}

void Game::Render()
{
    if( _state_stack.empty() )
        return;
    
    rmt_ScopedCPUSample( Render, 0 );

    rdi::CommandQueue* cmdq = nullptr;
    rdi::frame::Begin( &cmdq );


    {
        rmt_ScopedCPUSample( PreRender, 0 );
        PreRenderImpl( _time, cmdq );
    }


    {
        rmt_ScopedCPUSample( StateRender, 0 );
        if( _state_stack.size() == 1 )
        {
            _state_stack[0]->OnRender( _time, cmdq );
        }
        else
        {
            const size_t n = _state_stack.size();
            for( size_t i = 0; i < n - 1; ++i )
            {
                _state_stack[i]->OnBackgroundRender( _time, cmdq );
            }

            _state_stack.back()->OnRender( _time, cmdq );
        }
    }

    {
        rmt_ScopedCPUSample( PostRender, 0 );
        PostRenderImpl( _time, cmdq );
    }


    rdi::frame::End( &cmdq );
}

void Game::Pause()
{
    _pause = true;
    for( auto state : _state_stack )
    {
        state->OnPause();
    }

    PauseImpl();
}

void Game::Resume()
{
    ResumeImpl();

    for( auto state : _state_stack )
    {
        state->OnResume();
    }
    _pause = false;
}

Game* GameState::GetGame()
{
    return __game;
}

}//