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
in vec4 LightSpacePos[NUM_CASCADES];
in float ClipSpacePosZ;
  
uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform vec3 color;
uniform sampler2D shadowMap[NUM_CASCADES];
uniform float cascadeEndClipSpace[NUM_CASCADES];

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

    // Cascade debug color
    vec3 cascadeIndicator = vec3(0.0, 0.0, 0.0);
    for (int i = 0 ; i < NUM_CASCADES ; i++) {
        if (ClipSpacePosZ <= cascadeEndClipSpace[i]) {
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
        
    vec3 result = (ambient + diffuse + specular) * color+ cascadeIndicator*2.0;
    FragColor = vec4(result, 1.0);
} 