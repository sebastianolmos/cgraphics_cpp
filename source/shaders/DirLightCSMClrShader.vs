#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

#define NUM_CASCADES 3

out vec3 FragPos;
out vec3 Normal;
out vec4 LightSpacePos[NUM_CASCADES];

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 FragPosLP[NUM_CASCADES]; //FragPosLightSpace

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    gl_Position = projection * view * vec4(FragPos, 1.0);

    for (int i = 0 ; i < NUM_CASCADES ; i++) {
        LightSpacePos[i] = FragPosLP[i] * vec4(FragPos, 1.0);
    }
}