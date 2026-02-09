#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;
    mat4 modelMatrix;
} push;

void main() {
    gl_Position = push.lightSpaceMatrix * push.modelMatrix * vec4(inPosition, 1.0);
}
