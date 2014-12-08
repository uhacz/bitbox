
inline __m128 dotSoa( const __m128& xxxx, const __m128& yyyy, const __m128& zzzz,
                     const __m128& xxxx1, const __m128& yyyy1, const __m128& zzzz1 )
{
    return vec_madd( zzzz,zzzz1, vec_madd(xxxx,xxxx1, vec_mul(yyyy,yyyy1)) );
}

inline int viewFrustum_SphereIntersectLRBT( const bxGfxViewFrustumLRBT& f, const Vector4& sphere, const floatInVec& tolerance )
{
    const __m128 zero4 = _mm_set_ps1( 0.0f );
    const __m128 tolerance4 = ( tolerance.get128() );

    const __m128 xPlaneLRBT = ( f.xPlaneLRBT.get128() );
    const __m128 yPlaneLRBT = ( f.yPlaneLRBT.get128() );
    const __m128 zPlaneLRBT = ( f.zPlaneLRBT.get128() );
    const __m128 wPlaneLRBT = ( f.wPlaneLRBT.get128() ); // x left right bottom top, etc

    const __m128 sphereXYZW = sphere.get128();

    const __m128 xxxxSphere = vec_splat( sphereXYZW, 0 );
    const __m128 yyyySphere = vec_splat( sphereXYZW, 1 );
    const __m128 zzzzSphere = vec_splat( sphereXYZW, 2 );
    const __m128 wwwwSphere = _mm_set_ps1( 0.f );
    const __m128 rrrrSphere = vec_splat( sphereXYZW, 3 );

    const __m128 dotLRBT = dotSoa( xPlaneLRBT, yPlaneLRBT, zPlaneLRBT, xxxxSphere, yyyySphere, zzzzSphere );

    // add plane offset (distance from origin)
    __m128 dotwLRBT = vec_add( dotLRBT, wPlaneLRBT );
    dotwLRBT = vec_add( dotwLRBT, rrrrSphere );

    const __m128 dotPositive = vec_cmpgt( dotwLRBT, tolerance4 );

    const int maskBits = _mm_movemask_ps( dotPositive );
    const int res = maskBits == 0xF;
    // check against reference implementation

    return res;
}

inline int viewFrustum_AABBIntersectLRTB( const bxGfxViewFrustum& f, const Vector3& minCorner, const Vector3& maxCorner, const floatInVec& tolerance )
{
    const __m128 zero4 = _mm_set_ps1( 0.0f );
    const __m128 minc = ( minCorner.get128() );
    const __m128 maxc = ( maxCorner.get128() );
    const __m128 tolerance4 = ( tolerance.get128() );

    const __m128 xPlaneLRBT = ( f.xPlaneLRBT.get128() );
    const __m128 yPlaneLRBT = ( f.yPlaneLRBT.get128() );
    const __m128 zPlaneLRBT = ( f.zPlaneLRBT.get128() );
    const __m128 wPlaneLRBT = ( f.wPlaneLRBT.get128() ); // x left right bottom top, etc

    // convert min and max corners to soa format
    __m128 xxxxMinc, yyyyMinc, zzzzMinc;
    __m128 xxxxMaxc, yyyyMaxc, zzzzMaxc;

    xxxxMinc = vec_splat(minc,0);
    yyyyMinc = vec_splat(minc,1);
    zzzzMinc = vec_splat(minc,2);
    xxxxMaxc = vec_splat(maxc,0);
    yyyyMaxc = vec_splat(maxc,1);
    zzzzMaxc = vec_splat(maxc,2);

    // test left, right, bottom and top planes

    // prepare selection masks for each component
    __m128 xSelLRBT = vec_cmpgt(xPlaneLRBT, zero4);
    __m128 ySelLRBT = vec_cmpgt(yPlaneLRBT, zero4);
    __m128 zSelLRBT = vec_cmpgt(zPlaneLRBT, zero4);

    // select component from min or max, according to masks, min corner is selected when bits are set to 0x00000000
    // max when bits are set to 0xffffffff
    __m128 xMaxLRBT = vec_sel(xxxxMinc, xxxxMaxc, xSelLRBT);
    __m128 yMaxLRBT = vec_sel(yyyyMinc, yyyyMaxc, ySelLRBT);
    __m128 zMaxLRBT = vec_sel(zzzzMinc, zzzzMaxc, zSelLRBT);

    // compute dot product for left, right, bottom and top plane at once
    __m128 dotLRBT = dotSoa(xMaxLRBT, yMaxLRBT, zMaxLRBT, xPlaneLRBT, yPlaneLRBT, zPlaneLRBT);

    // add plane offset (distance from origin)
    __m128 dotwLRBT = vec_add(dotLRBT, wPlaneLRBT);
    __m128 cmpgtLRBT = vec_cmpgt(dotwLRBT, tolerance4);

    // this is similar to vec_all_gt, returns true, when all elements of cmpgtLRBTNFNF vector are 0xffffffff
    int maskBits = _mm_movemask_ps( cmpgtLRBT );
    int res = maskBits == 0xF;
    // check against reference implementation

    return res;
}
inline boolInVec viewFrustum_AABBIntersect( const bxGfxViewFrustum& f, const Vector3& minCorner, const Vector3& maxCorner, const floatInVec& tolerance )
{
    const __m128 zero4 = _mm_set_ps1( 0.0f );

    const __m128 minc = minCorner.get128();
    const __m128 maxc = maxCorner.get128();
    const __m128 tolerance4 = ( tolerance.get128() );

    const __m128 xPlaneLRBT = ( f.xPlaneLRBT.get128() );
    const __m128 yPlaneLRBT = ( f.yPlaneLRBT.get128() );
    const __m128 zPlaneLRBT = ( f.zPlaneLRBT.get128() );
    const __m128 wPlaneLRBT = ( f.wPlaneLRBT.get128() ); // x left right bottom top, etc
    const __m128 xPlaneNFNF = ( f.xPlaneNFNF.get128() );
    const __m128 yPlaneNFNF = ( f.yPlaneNFNF.get128() );
    const __m128 zPlaneNFNF = ( f.zPlaneNFNF.get128() );
    const __m128 wPlaneNFNF = ( f.wPlaneNFNF.get128() ); // x near far 0 0, etc

    //toSoa( xPlaneLRBT, yPlaneLRBT, zPlaneLRBT, wPlaneLRBT, planes[0].get128(), planes[1].get128(), planes[2].get128(), planes[3].get128() );
    //toSoa( xPlaneNF00, yPlaneNF00, zPlaneNF00, wPlaneNF00, planes[4].get128(), planes[5].get128(), zero4, zero4 );

    // convert min and max corners to soa format
    __m128 xxxxMinc, yyyyMinc, zzzzMinc;
    __m128 xxxxMaxc, yyyyMaxc, zzzzMaxc;

    xxxxMinc = vec_splat(minc,0);
    yyyyMinc = vec_splat(minc,1);
    zzzzMinc = vec_splat(minc,2);
    xxxxMaxc = vec_splat(maxc,0);
    yyyyMaxc = vec_splat(maxc,1);
    zzzzMaxc = vec_splat(maxc,2);

    // test left, right, bottom and top planes

    // prepare selection masks for each component
    __m128 xSelLRBT = vec_cmpgt(xPlaneLRBT, zero4);
    __m128 ySelLRBT = vec_cmpgt(yPlaneLRBT, zero4);
    __m128 zSelLRBT = vec_cmpgt(zPlaneLRBT, zero4);

    // select component from min or max, according to masks, min corner is selected when bits are set to 0x00000000
    // max when bits are set to 0xffffffff
    __m128 xMaxLRBT = vec_sel(xxxxMinc, xxxxMaxc, xSelLRBT);
    __m128 yMaxLRBT = vec_sel(yyyyMinc, yyyyMaxc, ySelLRBT);
    __m128 zMaxLRBT = vec_sel(zzzzMinc, zzzzMaxc, zSelLRBT);

    // compute dot product for left, right, bottom and top plane at once
    __m128 dotLRBT = dotSoa(xMaxLRBT, yMaxLRBT, zMaxLRBT, xPlaneLRBT, yPlaneLRBT, zPlaneLRBT);

    // add plane offset (distance from origin)
    __m128 dotwLRBT = vec_add(dotLRBT, wPlaneLRBT);
    __m128 cmpgtLRBT = vec_cmpgt(dotwLRBT, tolerance4);

    // now, do the same for near and far planes, third and fourth component of vector are zeros

    // prepare selection masks for each component
    __m128 xSelNFNF = vec_cmpgt(xPlaneNFNF, zero4);
    __m128 ySelNFNF = vec_cmpgt(yPlaneNFNF, zero4);
    __m128 zSelNFNF = vec_cmpgt(zPlaneNFNF, zero4);

    // select component from min or max, according to masks, min corner is selected when bits are set to 0x00000000
    // max when bits are set to 0xffffffff
    __m128 xMaxNFNF = vec_sel(xxxxMinc, xxxxMaxc, xSelNFNF);
    __m128 yMaxNFNF = vec_sel(yyyyMinc, yyyyMaxc, ySelNFNF);
    __m128 zMaxNFNF = vec_sel(zzzzMinc, zzzzMaxc, zSelNFNF);

    // compute dot product for left, right, bottom and top plane at once
    __m128 dotNFNF = dotSoa(xMaxNFNF, yMaxNFNF, zMaxNFNF, xPlaneNFNF, yPlaneNFNF, zPlaneNFNF);

    // add plane offset (distance from origin)
    __m128 dotwNFNF = vec_add(dotNFNF, wPlaneNFNF);
    __m128 cmpgtNFNF = vec_cmpgt(dotwNFNF, tolerance4);

    // gather results of comparison against all planes
    // previous solution was to check if all bits of cmpgtLRBT are ones, then check if all bits of cmpgtNFNF are ones, and
    // then and'ing them to get final result
    // now this result is anded using vector registers to reduce switching between general purpose and vector registers
    // we could do this 'and' using regular registers but this would require transferring comparison results to memory, and then to
    // general purpose registers, and then anding
    // altivec's vec_all_gt set condition flags in condition registers instead of writing to general purpose registers or memory,
    // and to perform 'and' it requires extracting bits from condition registers, which is very slow
    __m128 cmpgtLRBTNFNF = vec_and(cmpgtLRBT, cmpgtNFNF);

    // if at least one value is 0, then we should return false
    // anding all components is a simple way to check this
    //
    cmpgtLRBTNFNF = ( vec_and(
        vec_and( vec_splat( cmpgtLRBTNFNF, 0 ), vec_splat( cmpgtLRBTNFNF, 1 ) ),
        vec_and( vec_splat( cmpgtLRBTNFNF, 2 ), vec_splat( cmpgtLRBTNFNF, 3 ) ) ) );

    return boolInVec( floatInVec( cmpgtLRBTNFNF ) );
}
