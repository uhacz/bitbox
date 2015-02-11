#include <util/vectormath/vectormath.h>

namespace bxPathTracer
{
    
    Vector2 worldIntersect( const Vector3& ro, const Vector3& rd, const floatInVec& maxLen );
    floatInVec worldShadow( const Vector3& ro, const Vector3& rd, const floatInVec& maxLen );

    Vector3 worldGetNormal( const Vector3& po, float objectID );
    Vector3 worldGetColor( const Vector3& po, const Vector3& no, float objectID );
    Vector3 worldGetBackgound( const Vector3& rd );

    Vector3 worldApplyLighting( const Vector3& pos, const Vector3& nor );
    Vector3 worldGetBRDFRay( const Vector3& pos, const Vector3& nor, const Vector3& eye, float materialID );


}///



int main()
{
    
    return 0;
}