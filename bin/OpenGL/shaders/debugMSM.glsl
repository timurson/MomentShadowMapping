
-- Vertex

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 transform;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = transform * vec4(aPos, 1.0);
}

-- Fragment

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform float zNear;
uniform float zFar;

// required when using a perspective projection matrix
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * zNear * zFar) / (zFar + zNear - z * (zFar - zNear));	
}

void main()
{             
    vec3 moments = texture(depthMap, TexCoords).rgb;
	
    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / zFar), 1.0); // perspective
    FragColor = vec4(moments, 1.0); // orthographic
}