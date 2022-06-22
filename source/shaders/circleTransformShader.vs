#version 330 core
layout (location = 0) in vec3 aPos;

uniform float pointRadius; 
uniform float pointScale;
uniform mat4 transform;

void main()
{	
	vec3 posEye = vec3(transform * vec4(aPos, 1.0));
	float dist = length(posEye);
	gl_PointSize = pointRadius * pointScale;
	gl_Position = transform * vec4(aPos, 1.0);
}