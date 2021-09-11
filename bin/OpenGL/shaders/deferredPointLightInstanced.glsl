
-- Vertex

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec4 aInstanceParam;  // (RGB) light color and (A) is light radius
layout (location = 3) in mat4 aInstanceMatrix;

out vec3 lightColor;
out vec3 lightPosition;
out float lightRadius;

uniform mat4 projection;
uniform mat4 view;

void main()
{
	lightColor = aInstanceParam.rgb;
	lightRadius = aInstanceParam.w;
	// extract light position from the instance model matrix
	lightPosition = vec3(aInstanceMatrix[3]);
    gl_Position = projection * view * aInstanceMatrix * vec4(lightRadius * aPos, 1.0);
}

-- Fragment

layout (location = 0) out vec4 FragColor;

in vec3 lightColor;
in float lightRadius;
in vec3 lightPosition;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gDiffuse;
uniform sampler2D gSpecular;
uniform vec3 viewPos;
uniform float lightIntensity;
uniform vec2 screenSize;
uniform float glossiness;

void main()
{
	vec2 uvCoords = gl_FragCoord.xy / screenSize;
	vec3 FragPos = texture(gPosition, uvCoords).rgb;
	vec3 Normal = texture(gNormal, uvCoords).rgb;
	vec3 Diffuse = texture(gDiffuse, uvCoords).rgb;
	vec4 Specular = texture(gSpecular, uvCoords);
	
	// do Phong lighting calculation
	vec3 ambient  = Diffuse * 0.2; // ambient contribution
	vec3 viewDir  = normalize(viewPos - FragPos);
	
	// diffuse
	vec3 lightDir = normalize(lightPosition - FragPos);
	vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lightColor;
	// specular
	vec3 halfwayDir = normalize(lightDir + viewDir);  
	float spec = pow(max(dot(Normal, halfwayDir), 0.0), glossiness) * Specular.a;
	vec3 specular = lightColor * spec * Specular.rgb;
	// attenuation
	float distToL = length(lightPosition - FragPos);
	float attenuation = 1.0 - pow(smoothstep(0.0, 1.0, clamp(distToL/lightRadius, 0.0, 1.0)), 4.0);
	vec3 result = ambient + diffuse + specular;
	float noZTestFix = step(0.0, lightRadius - distToL); //0.0 if distToL > radius, 1.0 otherwise
	vec4 outColor = vec4(result, noZTestFix) * attenuation * lightIntensity;
	
    FragColor = outColor;
}