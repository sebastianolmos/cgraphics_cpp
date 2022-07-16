#version 330 core
out vec4 FragColor;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;    
    float shininess;
}; 

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    // attenuation
    float constant;
    float linear;
    float quadratic;
};

in vec3 FragPos;  
in vec2 FragTexCoords;
in vec3 Normal;  

uniform sampler2D texture_diffuse1;

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

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

    float distance = length(light.position - FragPos);
    float attenuation = light.constant + light.linear * distance + 
    		    light.quadratic * (distance * distance); 
    vec4 fragOriginalColor = texture(texture_diffuse1, FragTexCoords);

    vec3 result = (ambient + ((diffuse + specular)/attenuation) ) * fragOriginalColor.rgb;
    FragColor = vec4(result, fragOriginalColor[3]);
}
