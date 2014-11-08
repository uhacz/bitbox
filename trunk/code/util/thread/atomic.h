#pragma once

#include "../type.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>


typedef CRITICAL_SECTION bxMutexHandle;
typedef HANDLE			 bxSemaphoreHandle;
typedef HANDLE			 bxThreadEventHandle;
	
namespace bxAtomic
{
    __forceinline atomic32 CAS( volatile atomic32* dst, volatile atomic32 cmp, atomic32 exc ) { return InterlockedCompareExchange( dst, exc, cmp ); }
    __forceinline bool CAS2( volatile atomic32* dst, volatile atomic32 cmp1, volatile atomic32 cmp2, atomic32 exc1, atomic32 exc2 ) 
    { 
    #ifdef x86
	    union Merger
	    {
		    struct
		    {
			    u32 v1;
			    u32 v2;
		    };
		    u64 _64;
	    };

	    Merger cmp = { cmp1, cmp2 };
	    Merger exc = { exc1, exc2 };

	    return InterlockedCompareExchange64( (volatile LONGLONG*)dst, exc._64, cmp._64 ) == cmp._64; 
    #else
	    (void)dst;
	    (void)cmp1;
	    (void)cmp2;
	    (void)exc1;
	    (void)exc2;
	    __debugbreak();
	    return 0;
    #endif
    }
    __forceinline i32 interlockedInc( atomic32* value ) { return InterlockedIncrement( value ); }
    __forceinline i32 interlockedDec( atomic32* value ) { return InterlockedDecrement( value ); }
    __forceinline i64 interlockedInc( atomic64* value ) { return InterlockedIncrement64( value ); }
    __forceinline i64 interlockedDec( atomic64* value ) { return InterlockedDecrement64( value ); }

    __forceinline i32 compareExchangeAcquire( atomic32* dst, i32 cmp, i32 exc ) { return InterlockedCompareExchangeAcquire( dst, exc, cmp ); }
    __forceinline i64 compareExchangeAcquire( atomic64* dst, i64 cmp, i64 exc ) { return InterlockedCompareExchangeAcquire64( dst, exc, cmp ); }
    __forceinline i32 exchangeRelease( atomic32* dst, i32 value ) { return InterlockedExchange( dst, value ); }
    __forceinline i64 exchangeRelease( atomic64* dst, i64 value ) { return InterlockedExchange64( dst, value ); }
}///


