#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

layout(push_constant, std430) uniform ModelViewProjection {
    mat4 cameraMat;
    mat4 transform;
} mvp;

void main() {
    gl_Position = mvp.cameraMat * mvp.transform * vec4(inPosition, 1.0);
    fragColor = inColor;
}