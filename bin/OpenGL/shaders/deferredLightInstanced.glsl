
-- Vertex

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aInstanceParam;  // (RGB) light color and (A) is light radius
layout (location = 3) in mat4 aInstanceMatrix;


out vec3 lightColor;

uniform mat4 projection;
uniform mat4 view;

void main()
{
	// pass the instance light color to fragment shader
	//vec4 position = aInstanceMatrix[3];
	lightColor = aInstanceParam.rgb;
    gl_Position = projection * view * aInstanceMatrix  * vec4(aInstanceParam.w * aPos, 1.0);
}

-- Fragment

layout (location = 0) out vec4 FragColor;

in vec3 lightColor;

void main()
{           
    FragColor = vec4(lightColor, 1.0);
}