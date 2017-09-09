#include "puzzle_physics_util.h"
#include "puzzle_physics.h"
#include "puzzle_physics_pbd.h"

#include "sdf.h"
#include "voxelize.h"

#include <util/array.h>
#include <util/bbox.h>
#include <util/poly/poly_shape.h>
#include <util/random.h>
#include "util/intersect.h"
#include "util/camera.h"
#include "puzzle_physics_gfx.h"

namespace bx{ namespace puzzle{
namespace physics{

BodyId CreateRope( Solver* solver, const Vector3F& attach, const Vector3F& axis, float len, float particleMass )
{
    const float pradius = physics::GetParticleRadius( solver );

    const u32 num_points = (u32)( len / ( pradius * 2.f ) );
    const float step = ( len / (float)( num_points ) );
    const float particle_mass_inv = ( particleMass > FLT_EPSILON ) ? 1.f / particleMass : 0.f;
    BodyId rope = physics::CreateBody( solver, num_points );

    Vector3F* points = MapPosition( solver, rope );
    f32* mass_inv = MapMassInv( solver, rope );
    
    points[0] = attach;
    mass_inv[0] = particle_mass_inv;
    for( u32 i = 1; i < num_points; ++i )
    {
        points[i] = points[i - 1] + axis * step;
        mass_inv[i] = particle_mass_inv;
    }

    physics::Unmap( solver, mass_inv );
    physics::Unmap( solver, points );

    array_t< DistanceCInfo >c_info;
    array::reserve( c_info, num_points - 1 );
    for( u32 i = 0; i < num_points - 1; ++i )
    {
        DistanceCInfo& info = c_info[i];
        info.i0 = i;
        info.i1 = i + 1;
    }
    SetDistanceConstraints( solver, rope, c_info.begin(), num_points - 1 );

    return rope;
}

BodyId CreateCloth( Solver* solver, const Vector3F& attach, const Vector3F& axis, float width, float height, float particleMass )
{
    return{0};
}


#if 0
static inline bool EdgeDetect( u32 ix, u32 iy, u32 iz, u32 w, u32 d, u32 h )
{
    bool is_on_edge = false;
    is_on_edge |= iz == 0 || iz == ( d - 1 );
    is_on_edge |= iy == 0 || iy == ( h - 1 );
    is_on_edge |= ix == 0 || ix == ( w - 1 );

    return is_on_edge;
}
BodyId CreateSoftBox( Solver* solver, const Matrix4F& pose, float width, float depth, float height, float particleMass, bool shell )
{
    const f32 pradius = physics::GetParticleRadius( solver );
    const f32 pradius2 = pradius * 2.f;

    const u32 w = (u32)( width / pradius2 );
    const u32 h = (u32)( height / pradius2 );
    const u32 d = (u32)( depth / pradius2 );
    
    u32 num_particles = 0;
    if( shell )
    {
        for( u32 iz = 0; iz < d; ++iz )
        {
            for( u32 iy = 0; iy < h; ++iy )
            {
                for( u32 ix = 0; ix < w; ++ix )
                {
                    if( !EdgeDetect( ix, iy, iz, w, h, d ) )
                        continue;

                    ++num_particles;
                }
            }
        }
    }
    else
    {
        num_particles = w*h*d;
    }

    //const u32 num_particles = 2 * ( w*h + h*d + w*d );
    BodyId id = CreateBody( solver, num_particles );

    const Vector3F begin_pos_ls =
        -Vector3F(
            (f32)(w/2) - ( 1 - ( w % 2 ) ) * 0.5f,
            (f32)(h/2) - ( 1 - ( h % 2 ) ) * 0.5f,
            (f32)(d/2) - ( 1 - ( d % 2 ) ) * 0.5f
            ) * pradius2;

    const f32 particle_mass_inv = ( particleMass > FLT_EPSILON ) ? 1.f / particleMass : 0.f;
    Vector3F* pos = MapPosition( solver, id );
    f32* mass_inv = MapMassInv( solver, id );

    array_t< ShapeMatchingCInfo > shape_match_cinfo;
    array::reserve( shape_match_cinfo, num_particles );

   
    u32 pcounter = 0;
    for( u32 iz = 0; iz < d; ++iz )
    {
        for( u32 iy = 0; iy < h; ++iy )
        {
            for( u32 ix = 0; ix < w; ++ix )
            {
                if( shell )
                {
                    if( !EdgeDetect( ix, iy, iz, w, h, d ) )
                        continue;
                }

                const Vector3F pos_ls = begin_pos_ls + Vector3F( (f32)ix, (f32)iy, (f32)iz ) * pradius2;
                const Vector3F pos_ws = ( pose * Point3F( pos_ls ) ).getXYZ();

                pos[pcounter] = pos_ws;
                mass_inv[pcounter] = particle_mass_inv;

                ShapeMatchingCInfo info;
                info.i = pcounter;
                info.rest_pos = pos_ls;
                info.mass = particleMass;
                array::push_back( shape_match_cinfo, info );


                //u32 voxel_index = voxel_grid.index( ix + 1, iy + 1, iz + 1 );
                const u32 voxel_index = pcounter;
                pcounter += 1;
            }
        }
    }

    SYS_ASSERT( pcounter == num_particles );

    Unmap( solver, mass_inv );
    Unmap( solver, pos );

    CalculateLocalPositions( solver, id );
    //SetShapeMatchingConstraints( solver, id, shape_match_cinfo.begin(), shape_match_cinfo.size );
    return id;
}
#endif

static AABBF ComputeAABB( const Vector3F* points, u32 numPoints )
{
    AABBF aabb = AABBF::prepare();
    for( u32 i = 0; i < numPoints; ++i )
        aabb = AABBF::extend( aabb, points[i] );
    return aabb;
}

BodyId CreateFromShape( Solver* solver, const Matrix4F& pose, const Vector3F& scale, const Vector3F* srcPos, u32 numPositions, const u32* srcIndices, u32 numIndices, float particleMass, float spacingFactor, float jitter )
{
    bxRandomGen rnd( 0xDEADCEE1 );

    array_t<Vector3F> positions;
    array::reserve( positions, numPositions );
    
    // --- prepare positions
    for( u32 i = 0; i < numPositions; ++i )
        positions[i] = srcPos[i];

    AABBF local_aabb         = ComputeAABB( positions.begin(), numPositions );
    Vector3F local_aabb_size = AABBF::size( local_aabb );
    float max_edge           = maxElem( local_aabb_size );

    // put mesh at the origin and scale to specified size
    const Matrix4F xform = Matrix4F::scale( scale * ( 1.f / max_edge ) ); // prependScale( scale * ( 1.f / max_edge ), Matrix4F::translation( -local_aabb.min ) );
    for( u32 i = 0; i < numPositions; ++i )
        positions[i] = ( xform * Point3F( positions[i] ) ).getXYZ();

    // recompute bounds
    local_aabb      = ComputeAABB( positions.begin(), numPositions );
    local_aabb_size = AABBF::size( local_aabb );
    max_edge        = maxElem( local_aabb_size );
    

    // tweak spacing to avoid edge cases for particles laying on the boundary
    // just covers the case where an edge is a whole multiple of the spacing.
    const float pradius = GetParticleRadius( solver );
    const float spacing = pradius * spacingFactor;
    const float spacing_eps = spacing * ( 1.0f - 1e-4f );

    // make sure to have at least one particle in each dimension
    int dx, dy, dz;
    dx = spacing > local_aabb_size.x ? 1 : int( local_aabb_size.x / spacing_eps );
    dy = spacing > local_aabb_size.y ? 1 : int( local_aabb_size.y / spacing_eps );
    dz = spacing > local_aabb_size.z ? 1 : int( local_aabb_size.z / spacing_eps );
    int max_dim = maxOfPair( maxOfPair( dx, dy ), dz );
    
    // used to arrange particle 
    const Vector3F spacing_fine = divPerElem( local_aabb_size, Vector3F( (f32)dx, (f32)dy, (f32)dz ) );

    // expand border by two voxels to ensure adequate sampling at edges
    local_aabb.min -= 2.0f*Vector3F( spacing );
    local_aabb.max += 2.0f*Vector3F( spacing );
    max_dim += 4;

    const u32 max_dim_pow3 = max_dim*max_dim*max_dim;

    array_t<u32> voxels;
    array::reserve( voxels, max_dim_pow3 );

    // we shift the voxelization bounds so that the voxel centers
    // lie symmetrically to the center of the object. this reduces the 
    // chance of missing features, and also better aligns the particles
    // with the mesh
    Vector3F meshOffset;
    meshOffset.x = 0.5f * ( spacing - ( local_aabb_size.x - ( dx - 1 )*spacing ) );
    meshOffset.y = 0.5f * ( spacing - ( local_aabb_size.y - ( dy - 1 )*spacing ) );
    meshOffset.z = 0.5f * ( spacing - ( local_aabb_size.z - ( dz - 1 )*spacing ) );
    local_aabb.min -= meshOffset;

    // --- voxelize
    Voxelize( 
        (const float*)positions.begin(), numPositions, (const int*)srcIndices, numIndices, 
        max_dim, max_dim, max_dim, &voxels[0], local_aabb.min, local_aabb.min + Vector3F( max_dim*spacing ) 
        );
    // --- 

    // --- make sdf
    array_t<i32> indices;
    array_t<f32> sdf;
    array::reserve( indices, max_dim_pow3 );
    array::reserve( sdf, max_dim_pow3 );

    MakeSDF( &voxels[0], max_dim, max_dim, max_dim, &sdf[0] );
    // --- 

    u32 num_particles = 0;
    for( u32 i = 0; i < voxels.capacity; ++i )
        num_particles += ( voxels[i] ) ? 1 : 0;

    BodyId id = CreateBody( solver, num_particles );

    const f32 particle_mass_inv = ( particleMass > FLT_EPSILON ) ? 1.f / particleMass : 0.f;
    Vector3F* body_pos = MapPosition( solver, id );
    f32*      body_mass_inv = MapMassInv( solver, id );

    array_t<Vector4F> sdf_data;
    
    u32 body_particle_index = 0;
    const Vector3F translation = pose.getTranslation();
    for( int x = 0; x < max_dim; ++x )
    {
        for( int y = 0; y < max_dim; ++y )
        {
            for( int z = 0; z < max_dim; ++z )
            {
                const int index = z*max_dim*max_dim + y*max_dim + x;
                indices[index] = UINT32_MAX;
                if( !voxels[index] )
                    continue;

                const Vector3F grid_pos = Vector3F( float( x ) + 0.5f, float( y ) + 0.5f, float( z ) + 0.5f );
                const Vector3F jitter_pos = bxRand::unitVector( rnd ) * jitter;
                //const Vector3F pos_ls = local_aabb.min + spacing*grid_pos + jitter_pos;
                const Vector3F pos_ls = local_aabb.min + mulPerElem( spacing_fine, grid_pos ) + jitter_pos;
                const Vector3F pos_ws = (pose * Point3F(pos_ls)).getXYZ();

                // normalize the sdf value and transform to world scale
                Vector3F sdf_grad;
                SampleSDFGrad( &sdf_grad.x, sdf.begin(), max_dim, x, y, z );
                const Vector3F n = normalizeSafeF( sdf_grad );
                const float d = sdf[index] * max_edge;
                const Vector4F nrm_ls( n, d );

                body_pos[body_particle_index] = pos_ws;
                body_mass_inv[body_particle_index] = particle_mass_inv;
                
                array::push_back( sdf_data, nrm_ls );
                indices[index] = body_particle_index;

                body_particle_index += 1;
            }
        }
    }


    const float springStiffness = 1.f;
    if( springStiffness > 0.0f )
    {
        array_t<DistanceCInfo> cinfo_array;
        // construct cross link springs to occupied cells
        for( int x = 0; x < max_dim; ++x )
        {
            for( int y = 0; y < max_dim; ++y )
            {
                for( int z = 0; z < max_dim; ++z )
                {
                    const int centerCell = z*max_dim*max_dim + y*max_dim + x;

                    // if voxel is marked as occupied the add a particle
                    if( voxels[centerCell] )
                    {
                        const int width = 1;

                        // create springs to all the neighbors within the width
                        for( int i = x - width; i <= x + width; ++i )
                        {
                            for( int j = y - width; j <= y + width; ++j )
                            {
                                for( int k = z - width; k <= z + width; ++k )
                                {
                                    const int neighborCell = k*max_dim*max_dim + j*max_dim + i;

                                    if( neighborCell > 0 && neighborCell < int( voxels.capacity ) && voxels[neighborCell] && neighborCell != centerCell )
                                    {
                                        DistanceCInfo cinfo;
                                        cinfo.i0 = indices[centerCell];
                                        cinfo.i1 = indices[neighborCell];
                                        array::push_back( cinfo_array, cinfo );
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if( cinfo_array.size )
        {
            SetDistanceConstraints( solver, id, cinfo_array.begin(), cinfo_array.size, springStiffness );
        }
    }

    SYS_ASSERT( num_particles == body_particle_index );

    CalculateLocalPositions( solver, id );
    SetSDFData( solver, id, sdf_data.begin(), sdf_data.size );

    Unmap( solver, body_mass_inv );
    Unmap( solver, body_pos );

    return id;
}


//
//BodyId CreateFromPolyShape( Solver* solver, const Matrix4F& pose, const Vector3F& scale, const bxPolyShape& shape, float particleMass, float spacingFactor, float jitter )
//{
//    const Vector3F* positions = (Vector3F*)shape.positions;
//    const u32* indices = shape.indices;
//    BodyId id = CreateFromShape( solver, pose, scale, positions, shape.num_vertices, indices, shape.num_indices, particleMass, spacingFactor, jitter );
//    
//    return id;
//}

BodyId CreateFromParShapes( Solver* solver, const Matrix4F& pose, const Vector3F& scale, par_shapes_mesh* mesh, float particleMass, float spacingFactor, float jitter )
{
    array_t<u32> indices32;
    array::reserve( indices32, mesh->ntriangles * 3 );
    for( int i = 0; i < mesh->ntriangles * 3; ++i )
        array::push_back( indices32, (u32)mesh->triangles[i] );


    ParShapesMeshMakeSymetric( mesh );
    
    const Vector3F* points = (Vector3F*)mesh->points;
    const u32 npoints = mesh->npoints;
    const u32* indices = indices32.begin();
    const u32  nindices = indices32.size;
    
    BodyId id = CreateFromShape( solver, pose, scale, points, npoints, indices, nindices, particleMass, 1.5f, 0.f );
    return id;
}


BodyId CreateBox( Solver* solver, const Matrix4F& pose, const Vector3F& extents, float particleMass )
{
    par_shapes_mesh* mesh = par_shapes_create_cube();
    BodyId id = CreateFromParShapes( solver, pose, extents*2.f, mesh, particleMass, 1.5f, 0.f );
    par_shapes_free_mesh( mesh );

    //bxPolyShape shape;
    //bxPolyShape_createBox( &shape, 1 );
    //BodyId id = CreateFromPolyShape( solver, pose, extents*2.f, shape, particleMass, 1.5f, 0.f );
    //bxPolyShape_deallocateShape( &shape );
    return id;
}

BodyId CreateSphere( Solver* solver, const Matrix4F& pose, float radius, float particleMass, int subdiv )
{
    par_shapes_mesh* mesh = par_shapes_create_subdivided_sphere( subdiv );
    BodyId id = CreateFromParShapes( solver, pose, Vector3F( radius *2.f ), mesh, particleMass, 1.5f, 0.f );
    par_shapes_free_mesh( mesh );
    //bxPolyShape shape;
    //bxPolyShape_createShpere( &shape, 16 );
    //BodyId id = CreateFromPolyShape( solver, pose, Vector3F(radius*2.f), shape, particleMass, 1.f, 0.005f );
    //bxPolyShape_deallocateShape( &shape );
    return id;
}

}

}}///



#include "../imgui/imgui.h"
#include <rdi/rdi_debug_draw.h>
namespace bx {namespace puzzle {
namespace physics
{
    enum ECreateStage
    {
        NONE,
        INIT,
        DEFINE_SCALE1,
        DEFINE_SCALE2,
        DEFINE_POSE,
        COMMIT,
        CANCEL,
    };
struct GUIContext
{
    Solver* solver = nullptr;
    Gfx* gfx = nullptr;

    BodyId selected_body_id = BodyIdInvalid();
    
    ECreateStage create_stage = ECreateStage::NONE;
    Vector3F create_point[3] = {};
    u8 create_point_valid[4] = {};
    Matrix4F create_pose = Matrix4F::identity();
    par_shapes_mesh* create_mesh = nullptr;

};

//////////////////////////////////////////////////////////////////////////
void physics::CreateGUI( GUIContext ** gui, Solver * solver, Gfx * gfx )
{
    GUIContext* g = BX_NEW( bxDefaultAllocator(), GUIContext );
    g->solver = solver;
    g->gfx = gfx;

    gui[0] = g;
}
void physics::DestroyGUI( GUIContext ** gui )
{
    BX_DELETE0( bxDefaultAllocator(), gui[0] );
}
void ShowGUI( GUIContext* gui, const gfx::Camera& camera )
{
    if( !ImGui::Begin( "Solver Edit" ) )
    {
        ImGui::End();
        return;
    }

    const Vector4F ground_plane = makePlane( Vector3F::yAxis(), Vector3F( 0.f ) );

    Solver* solver = gui->solver;
    if( gui->create_stage == ECreateStage::NONE )
    {
        if( ImGui::Button( "CreateBox" ) )
            gui->create_stage = ECreateStage::INIT;
    }
    else
    {
        const ImGuiIO& io = ImGui::GetIO();

        const float mouse_screen_pos_x = clamp( io.MousePos.x / io.DisplaySize.x, 0.f, 1.f );
        const float mouse_screen_pos_y = 1.f - clamp( io.MousePos.y / io.DisplaySize.y, 0.f, 1.f );
        const float click_screen_pos_x = clamp( io.MouseClickedPos[0].x / io.DisplaySize.x, 0.f, 1.f );
        const float click_screen_pos_y = 1.f - clamp( io.MouseClickedPos[0].y / io.DisplaySize.y, 0.f, 1.f );
        const Matrix4 camera_vp_inv = inverse( camera.view_proj );


        switch( gui->create_stage )
        {
        case ECreateStage::INIT:
            {
                if( io.MouseClicked[0] )
                {
                    gui->create_stage = ECreateStage::DEFINE_SCALE1;
                }
            }break;
        case ECreateStage::DEFINE_SCALE1:
            {
                const bool points_valid = gui->create_point_valid[0] && gui->create_point_valid[1];

                if( points_valid && io.MouseClicked[0] )
                    gui->create_stage = ECreateStage::DEFINE_SCALE2;
                else if( io.MouseClicked[0] || io.MouseClicked[1] || io.MouseClicked[2] )
                    gui->create_stage = ECreateStage::CANCEL;
                else
                {
                    struct Ray{ Vector3F ro, rd; };

                    //////////////////////////////////////////////////////////////////////////
                    auto L_create_ray = []( float screenX, float screenY, const Matrix4& vpInv ) -> Ray
                    {
                        const Vector3 from = gfx::cameraUnprojectNormalized( Vector3( screenX, screenY, 0.f ), vpInv );
                        const Vector3 to = gfx::cameraUnprojectNormalized( Vector3( screenX, screenY, 1.f ), vpInv );

                        return{ toVector3F( from ), normalizeSafeF( toVector3F( to - from ) ) };
                    };

                    auto L_cast_ray = [&gui]( u32 i, const Ray& r, const Vector4F& plane )
                    {
                        float t;
                        if( IntersectRayPlane( r.ro, r.rd, plane, t ) )
                        {
                            const Vector3F point = r.ro + r.rd * t;
                            gui->create_point[i] = point;
                            gui->create_point_valid[i] = 1;
                        }
                    };
                    //////////////////////////////////////////////////////////////////////////

                    const Ray r0 = L_create_ray( click_screen_pos_x, click_screen_pos_y, camera_vp_inv );
                    const Ray r1 = L_create_ray( mouse_screen_pos_x, mouse_screen_pos_y, camera_vp_inv );

                    L_cast_ray( 0, r0, ground_plane );
                    L_cast_ray( 1, r1, ground_plane );
                }
            }break;
        case ECreateStage::DEFINE_SCALE2:
            {
                if( io.MouseClicked[0] )
                    gui->create_stage = ECreateStage::COMMIT;
                else if( io.MouseClicked[1] )
                    gui->create_stage = ECreateStage::CANCEL;
                else
                {
                    const Vector3F p0 = gui->create_point[0];
                    const Vector3F p1 = gui->create_point[1];
                    const Vector3F center = lerp( 0.5f, p0, p1 );
                    const Vector4F center_ss = toVector4F( camera.view_proj * toVector4( Vector4F( center, 1.0f ) ) );
                    const float z = center_ss.z / center_ss.w;

                    const Vector3 from = gfx::cameraUnprojectNormalized( Vector3( click_screen_pos_x, click_screen_pos_y, z ), camera_vp_inv );
                    const Vector3 to = gfx::cameraUnprojectNormalized( Vector3( mouse_screen_pos_x, mouse_screen_pos_y, z ), camera_vp_inv );
                    const Vector3F v = toVector3F( to - from );
                    const float d = dot( v, ground_plane.getXYZ() );
                    const Vector3F p2 = center + ground_plane.getXYZ() * d;

                    gui->create_point[2] = p2;
                    gui->create_point_valid[2] = 1;
                }
            }break;
        case ECreateStage::DEFINE_POSE:
            {
            }break;

        case ECreateStage::COMMIT:
            {
                const Vector3F& p0 = gui->create_point[0];
                const Vector3F& p1 = gui->create_point[1];
                const Vector3F& p2 = gui->create_point[2];

                const Vector3F pmin = minPerElem( p0, minPerElem( p1, p2 ) );
                const Vector3F pmax = maxPerElem( p0, maxPerElem( p1, p2 ) );

                const Vector3F center = lerp( 0.5f, pmin, pmax );
                const Vector3F ext = ( pmax - pmin ) * 0.5f;
                const Matrix4F pose = Matrix4F::translation( center );

                BodyId id = CreateBox( solver, pose, ext, 1.f );
                AddBody( gui->gfx, id );

                gui->create_stage = ECreateStage::CANCEL;


            }break;

        case ECreateStage::CANCEL:
            {
                memset( gui->create_point_valid, 0x00, sizeof( gui->create_point_valid ) );
                gui->create_stage = ECreateStage::NONE;
            }break;

        default:
            break;
        }



        auto L_draw_box = []( const Vector3F& p0, const Vector3F& p1, const Vector3F& p2, u32 color )
        {
            const Vector3F pmin = minPerElem( p0, minPerElem( p1, p2 ) );
            const Vector3F pmax = maxPerElem( p0, maxPerElem( p1, p2 ) );

            const Vector3F center = lerp( 0.5f, pmin, pmax );
            const Vector3F ext = ( pmax - pmin ) * 0.5f;

            rdi::debug_draw::AddBox( Matrix4F::translation( center ), ext, color, 1 );
        };

        u32 points_valid_mask = 0;
        for( u32 i = 0; i < 3; ++i )
            points_valid_mask |= ( gui->create_point_valid[i] ) ? 1 << i : 0;

        if( points_valid_mask == 0x1 )
        {
            rdi::debug_draw::AddSphere( Vector4F( gui->create_point[0], 0.1f ), 0xFF00FFFF, 1 );
        }
        else if( points_valid_mask == 0x3 )
        {
            const Vector3F p0 = gui->create_point[0];
            const Vector3F p1 = gui->create_point[1];
            
            L_draw_box( p0, p1, p0, 0xFF00FFFF );
        }
        else if( points_valid_mask == 0x7 )
        {
            const Vector3F& p0 = gui->create_point[0];
            const Vector3F& p1 = gui->create_point[1];
            const Vector3F& p2 = gui->create_point[2];

            L_draw_box( p0, p1, p2, 0xFF00FFFF );
        }
    }
        



    ImGui::Separator();
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 2, 2 ) );
    ImGui::Columns( 2 );
    ImGui::Separator();
    
    const u32 n_bodies = GetNbBodies( solver );
    for( u32 i = 0; i < n_bodies; ++i )
    {
        const BodyId body_id = GetBodyId( solver, i );
        const char* body_name = GetName( solver, body_id );
            
        ImGui::PushID( body_id.i );                      // Use object uid as identifier. Most commonly you could also use the object pointer as a base ID.
        ImGui::AlignFirstTextHeightToWidgets();         // Text and Tree nodes are less high than regular widgets, here we add vertical spacing to make the tree lines equal high.
        
        ImGuiTreeNodeFlags tree_flags = 0;
        const bool is_body_id_selected = body_id == gui->selected_body_id;
        if( is_body_id_selected )
            tree_flags = ImGuiTreeNodeFlags_Selected;

        ImGui::SetNextTreeNodeOpen( is_body_id_selected, ImGuiSetCond_Always );
        bool node_open = ImGui::TreeNodeEx( "Object", tree_flags, "%s", body_name );

        if( node_open )
        {
            ImGui::NextColumn();
            ImGui::AlignFirstTextHeightToWidgets();
            if( ImGui::Button( "Delete" ) )
            {
                DestroyBody( solver, body_id );
            }
            else
            {
                gui->selected_body_id = body_id;
            }
            
            ImGui::NextColumn();
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::Columns( 1 );
    ImGui::Separator();
    ImGui::PopStyleVar();
    ImGui::End();


    if( IsBodyAlive( solver, gui->selected_body_id ) )
    {
        const BodyAABB& body_aabb = GetAABB( solver, gui->selected_body_id );
        const Vector3F size = BodyAABB::size( body_aabb );
        const Vector3F center = BodyAABB::center( body_aabb );

        rdi::debug_draw::AddBox( Matrix4F::translation( center ), size*0.5f, 0xFF0000FF, 1 );
    }
}

}}}///
