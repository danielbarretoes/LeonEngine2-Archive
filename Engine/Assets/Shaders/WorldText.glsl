#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

out vec2 v_TexCoord;
out vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

void main() {
    v_TexCoord = aTexCoord;
    v_Color = aColor;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_FontAtlas;

void main() {
    float alpha = texture(u_FontAtlas, v_TexCoord).a;
    if (alpha < 0.01) {
        discard;
    }

    FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
}
