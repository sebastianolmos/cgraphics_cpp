#version 330 core
out vec4 FragColor;

#define NUM_CASCADES 3

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;    
    float shininess;
}; 

struct Light {
    // We need to render the scene from a light's perspective and thus render the scene from a 
    // position somewhere along the lines of the light direction.
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 FragPos;  
in vec3 Normal;
in vec4 LightSpacePos[NUM_CASCADES];
in float ClipSpacePosZ;

// texture samplers
uniform sampler2D shadowMap[NUM_CASCADES];
  
uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform vec3 color;
uniform float cascadeEndClipSpace[NUM_CASCADES];

float ShadowCalculation(int cascadeIndex, vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.005);
    // check whether current frag pos is in shadow
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap[cascadeIndex], 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap[cascadeIndex], projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}

void main()
{
    // ambient
    vec3 ambient = light.ambient * material.ambient;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);  

    // calculate shadow
    float shadow = 0.0;
    for (int i = 0 ; i < NUM_CASCADES ; i++) {
        if (ClipSpacePosZ <= cascadeEndClipSpace[i]) {
            shadow = ShadowCalculation(i, LightSpacePos[i]);
            break;
        }
    }
        
    vec3 result = (ambient + (1.0 - shadow)*(diffuse + specular)) * color;
    FragColor = vec4(result, 1.0);
} 