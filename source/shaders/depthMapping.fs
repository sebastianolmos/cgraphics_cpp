#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;

uniform bool orthographic;
uniform float nearPlane;
uniform float farPlane;

// required when using a perspective projection matrix
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));	
}

void main()
{             
    float depthValue = texture(depthMap, TexCoords).r;
    float dValue = orthographic ? depthValue : LinearizeDepth(depthValue)/farPlane;
    FragColor = vec4(vec3(dValue), 1.0); // orthographic
}