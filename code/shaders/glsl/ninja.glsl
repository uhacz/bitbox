#if defined( bx__vertex__ )
in vec3 POSITION;
void main()
{
   gl_Position = _cameraViewProj * vec4( POSITION, 1.0 );
}
#endif

#if defined( bx__fragment__ )
out vec4 _color;
void main()
{
   _color = vec4( 1.0, 0.0, 0.0, 1.0 );
}
#endif
