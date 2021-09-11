
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
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;
uniform float glossiness;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
	float Radius;
};

uniform Light gLight;
uniform vec3 viewPos;
uniform int shadowMethod; // 0 - standard, 1 - MSM

float calculateShadow(vec3 fragPos, vec3 normal)
{
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // calculate bias
    vec3 lightDir = normalize(gLight.Position - fragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	
    float shadowDepth = texture(shadowMap, projCoords.xy).r; 
	
    float shadowCoef = projCoords.z - bias > shadowDepth  ? 0.0 : 1.0;
	
    // keep the shadow at 1.0 when outside the zFar region of the light's frustum.
    if(projCoords.z > 1.0)
        shadowCoef = 1.0;
	
    return shadowCoef;
}

float linstep(float min, float max, float v)  
{
    return clamp ((v - min) / (max - min), 0.0, 1.0);
}  

float reduceLightBleeding(float p_max, float amount)  
{  
    return linstep(amount, 1, p_max);  
}  

float calculateMSMHamburger(vec4 moments, float frag_depth, float depth_bias, float moment_bias)
{
    // Bias input data to avoid artifacts
    vec4 b = mix(moments, vec4(0.5f, 0.5f, 0.5f, 0.5f), moment_bias);
    vec3 z;
    z[0] = frag_depth - depth_bias;

    // Compute a Cholesky factorization of the Hankel matrix B storing only non-
    // trivial entries or related products
    float L32D22 = fma(-b[0], b[1], b[2]);
    float D22 = fma(-b[0], b[0], b[1]);
    float squaredDepthVariance = fma(-b[1], b[1], b[3]);
    float D33D22 = dot(vec2(squaredDepthVariance, -L32D22), vec2(D22, L32D22));
    float InvD22 = 1.0f / D22;
    float L32 = L32D22 * InvD22;

    // Obtain a scaled inverse image of bz = (1,z[0],z[0]*z[0])^T
    vec3 c = vec3(1.0f, z[0], z[0] * z[0]);

    // Forward substitution to solve L*c1=bz
    c[1] -= b.x;
    c[2] -= b.y + L32 * c[1];

    // Scaling to solve D*c2=c1
    c[1] *= InvD22;
    c[2] *= D22 / D33D22;

    // Backward substitution to solve L^T*c3=c2
    c[1] -= L32 * c[2];
    c[0] -= dot(c.yz, b.xy);

    // Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions
    // z[1] and z[2]
    float p = c[1] / c[2];
    float q = c[0] / c[2];
    float D = (p * p * 0.25f) - q;
    float r = sqrt(D);
    z[1] =- p * 0.5f - r;
    z[2] =- p * 0.5f + r;

    // Compute the shadow intensity by summing the appropriate weights
    vec4 switchVal = (z[2] < z[0]) ? vec4(z[1], z[0], 1.0f, 1.0f) :
                      ((z[1] < z[0]) ? vec4(z[0], z[1], 0.0f, 1.0f) :
                      vec4(0.0f,0.0f,0.0f,0.0f));
    float quotient = (switchVal[0] * z[2] - b[0] * (switchVal[0] + z[2]) + b[1])/((z[2] - switchVal[1]) * (z[0] - z[1]));
    float shadowIntensity = switchVal[2] + switchVal[3] * quotient;
    return 1.0f - clamp(shadowIntensity, 0.0f, 1.0f);
}

float calculateShadow4MSM(vec3 fragPos)
{
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // keep the shadow at 1.0 when outside the zFar region of the light's frustum.
    if(projCoords.z > 1.0)
        return 1.0;
	
    float currentDepth = projCoords.z;
	
    return calculateMSMHamburger(texture(shadowMap, projCoords.xy), currentDepth, 0.0000, 0.0003);
}

void main()
{             
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gDiffuse, TexCoords).rgb;
    vec4 Specular = texture(gSpecular, TexCoords);
	
    // do Phong lighting calculation
    vec3 ambient  = Diffuse * 0.2; // hard-coded ambient component
    vec3 viewDir  = normalize(viewPos - FragPos);
	
    // diffuse
    vec3 lightDir = normalize(gLight.Position - FragPos);
    vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * gLight.Color;
    // specular
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), glossiness) * Specular.a;
    vec3 specular = gLight.Color * spec * Specular.rgb;
    // attenuation
    float distance = length(gLight.Position - FragPos);
    float attenuation = 1.0 / (1.0 + gLight.Linear * distance + gLight.Quadratic * distance * distance);
    diffuse *= attenuation;
    specular *= attenuation;
	
    float shadowFactor = 1.0;
    if(shadowMethod == 1) {
    	// calculate shadow using Moment Shadow Map
	shadowFactor = calculateShadow4MSM(FragPos);
	// use linear step function to reduce light bleeding more
	shadowFactor = reduceLightBleeding(shadowFactor, 0.2);	
    }
    else {
        // use standard shadow map method
	shadowFactor = calculateShadow(FragPos, Normal);	
    }
	
    vec3 result = ambient + (diffuse + specular) * shadowFactor;
			
    FragColor = vec4(result, 1.0);
}