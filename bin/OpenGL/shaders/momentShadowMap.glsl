
-- Vertex

layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}

-- Fragment

out vec4 FragColor;

void main()
{
    float depth = gl_FragCoord.z;	
    float squared = depth * depth;
    FragColor = vec4(depth, squared, depth * squared, squared * squared);
}