#type vertex
#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model = mat4(1.0);

out vec4 v_Color;

void main() {
    v_Color = aColor;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 450 core

in vec4 v_Color;
out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
