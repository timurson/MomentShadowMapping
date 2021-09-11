-- _global

precision highp float;
precision highp int;

layout(rgba32f, binding = 0, location = 0) uniform image2D uTex0;
layout(rgba32f, binding = 1, location = 1) uniform image2D uTex1;

uniform int ComputeKernelSize;

int cKernelSize           = ComputeKernelSize; // added on the cpp side
int cKernelHalfDist       = cKernelSize/2;
float recKernelSize       = 1.0 / float(cKernelSize);

-- ComputeH


layout( local_size_x = CS_THREAD_GROUP_SIZE, local_size_y = 1, local_size_z = 1 ) in;

void main() 
{
  int y = int(gl_GlobalInvocationID.x);
    
    // avoid processing pixels that are out of texture dimensions!
    if( y >= (cRTScreenSizeI.w) ) return;

    vec4 colorSum = imageLoad( uTex0, ivec2( 0, y ) ) * float(cKernelHalfDist);
    for( int x = 0; x <= cKernelHalfDist; x++ )
        colorSum += imageLoad( uTex0, ivec2( x, y ) );
	

    for( int x = 0; x < cRTScreenSizeI.z; x++ )
    {
        imageStore( uTex1, ivec2( x, y ), colorSum * recKernelSize );

        // move window to the next 
        vec4 leftBorder     = imageLoad( uTex0, ivec2( max( x-cKernelHalfDist, 0 ), y ) );
        vec4 rightBorder    = imageLoad( uTex0, ivec2( min( x+cKernelHalfDist+1, cRTScreenSizeI.z-1 ), y ) );

        colorSum -= leftBorder;
        colorSum += rightBorder;
    }
}

-- ComputeV

layout( local_size_x = CS_THREAD_GROUP_SIZE, local_size_y = 1, local_size_z = 1 ) in;

void main()
{
    // x and y are swapped for vertical

    int y = int(gl_GlobalInvocationID.x);

    // avoid processing pixels that are out of texture dimensions!
    if( y >= cRTScreenSizeI.z ) return;

    vec4 colorSum = imageLoad( uTex0, ivec2( y, 0 ) ) * float(cKernelHalfDist);
    for( int x = 0; x <= cKernelHalfDist; x++ )
        colorSum += imageLoad( uTex0, ivec2( y, x ) );
    
    for( int x = 0; x < cRTScreenSizeI.w; x++ )
    {
        imageStore( uTex1, ivec2( y, x ), colorSum * recKernelSize );
		
        // move window to the next 
        vec4 leftBorder     = imageLoad( uTex0, ivec2( y, max( x-cKernelHalfDist, 0 ) ) );
        vec4 rightBorder    = imageLoad( uTex0, ivec2( y, min( x+cKernelHalfDist+1, cRTScreenSizeI.w-1 ) ) );
    
        colorSum -= leftBorder;
        colorSum += rightBorder;
    }
}