#type vertex
#version 450 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;

uniform mat4 u_ViewProjection;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    v_TexCoord = aTexCoord;
    v_Color = aColor;
    gl_Position = u_ViewProjection * vec4(aPos, 0.0, 1.0);
}

#type fragment
#version 450 core

in vec2 v_TexCoord;
in vec4 v_Color;

out vec4 FragColor;

uniform sampler2D u_FontTexture;
uniform bool u_UseTexture = true;

void main() {
    if (u_UseTexture) {
        float alpha = texture(u_FontTexture, v_TexCoord).a;
        FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
    } else {
        FragColor = v_Color;
    }
}
