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
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 FragPos;  
in vec3 Normal;  
in vec2 FragTexCoords;
in vec4 LightSpacePos[NUM_CASCADES];

// texture samplers
uniform sampler2D texture_diffuse0;
uniform sampler2D shadowMap[NUM_CASCADES];
  
uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform float cascadeEndClipSpace[NUM_CASCADES];


uniform mat4 view;

void main()
{
    // ambient
    vec3 ambient = light.ambient * material.ambient;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-light.direction);  
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(norm, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);  

    vec4 fragOriginalColor = texture(texture_diffuse0, FragTexCoords);

    // Cascade debug color
    vec3 cascadeIndicator = vec3(0.0, 0.0, 0.0);
    for (int i = 0 ; i < NUM_CASCADES ; i++) {
        vec4 fragPosViewSpace = view * vec4(FragPos, 1.0);
        float depthValue = abs(fragPosViewSpace.z);
        if (depthValue <= cascadeEndClipSpace[i]) {
            //ShadowFactor = CalcShadowFactor(i, LightSpacePos[i]);
            if (i == 0) 
                cascadeIndicator = vec3(0.1, 0.0, 0.0);
            else if (i == 1)
                cascadeIndicator = vec3(0.0, 0.1, 0.0);
            else if (i == 2)
                cascadeIndicator = vec3(0.0, 0.0, 0.1);
            break;
        }
    }
        
    vec3 result = (ambient + diffuse + specular) * fragOriginalColor.rgb + cascadeIndicator*2.0;
    FragColor = vec4(result, fragOriginalColor[3]);
} 