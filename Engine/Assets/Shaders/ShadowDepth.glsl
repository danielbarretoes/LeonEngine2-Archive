#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in vec3 aColor;

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;

void main() {
    gl_Position = u_LightSpaceMatrix * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 450 core

void main() {
    // Depth is automatically recorded to the shadow framebuffer
}
