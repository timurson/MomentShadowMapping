
-- Vertex

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}

-- Fragment

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gDiffuse;
uniform sampler2D gSpecular;
uniform int gBufferMode;

void main()
{             
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gDiffuse, TexCoords).rgb;
    vec3 Specular = texture(gSpecular, TexCoords).rgb;
	vec3 outColor = vec3(0.0);
	
	if(gBufferMode == 1) // world position
	{
		outColor = FragPos;
	}
	else if(gBufferMode == 2) // world normal
	{
		outColor = Normal;
	}
	else if(gBufferMode == 3) // diffuse
	{
		outColor = Diffuse;
	}
	else if(gBufferMode == 4) // specular
	{
		outColor = Specular;
	}
	FragColor = vec4(outColor, 1.0);
}