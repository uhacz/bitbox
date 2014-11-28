#pragma once

template<typename Type>
Type signalFilter_lowPass( const Type& rawInput, const Type& lastInput, float rc, float dt )
{
    const float a = dt / ( rc + dt );
    return lastInput * ( 1.f - a ) + rawInput * a;
}