
-- Vertex

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform float lightRadius;

void main()
{
    gl_Position = projection * view * model  * vec4(lightRadius * aPos, 1.0);
}

-- Fragment

layout (location = 0) out vec4 FragColor;

uniform vec3 lightColor;

void main()
{           
    FragColor = vec4(lightColor, 1.0);
}