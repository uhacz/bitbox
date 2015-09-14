
#if defined( bx__vertex__ )
in vec3 POSITION;
in vec3 NORMAL;
in vec4 COLOR;

out block
{
    vec4 hPosition;
    vec3 wPosition;
    vec3 wNormal;
    vec4 color;
} OUT;

void main()
{
   gl_Position = _cameraViewProj * vec4( POSITION, 1.0 );
   OUT.wPosition = POSITION;
   OUT.wNormal = NORMAL;
   OUT.color = COLOR;
}
#endif

#if defined( bx__fragment__ )

in block
{
    vec4 hPosition;
    vec3 wPosition;
    vec3 wNormal;
    vec4 color;
} IN;
out vec4 _color;
void main()
{
   _color = IN.color;
}
#endif
