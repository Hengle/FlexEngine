#version 450

layout (location = 0) in vec3 in_Position;
layout (location = 1) in vec3 in_Velocity; // Unused here but necessary for compute pass to store inter-frame values
layout (location = 2) in vec4 in_Colour;
layout (location = 3) in vec3 in_Scale;
layout (location = 4) in vec4 in_ExtraVec4; // X: lifetime, Y: initial_lifetime, ZW: unused

layout (binding = 1) uniform UBODynamic
{
	mat4 model;
} uboDynamic;

layout (location = 0) out VSO
{
    vec3 position;
    vec4 colour;
    vec3 size;
} outputs;

void main()
{
	outputs.position = (uboDynamic.model * vec4(in_Scale * in_Position, 1.0)).xyz;
	outputs.colour = in_Colour;
	outputs.size = in_Scale;
}
