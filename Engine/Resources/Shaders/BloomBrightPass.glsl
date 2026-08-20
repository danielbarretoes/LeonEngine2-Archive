#type vertex
#version 450 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord = aTexCoord;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_TexCoord;

layout(binding = 0) uniform sampler2D u_HDRTexture;
uniform float u_Threshold = 1.0;
uniform float u_SoftKnee = 0.5;

void main() {
    vec3 color = texture(u_HDRTexture, v_TexCoord).rgb;
    
    // Perceptual luminance (Rec. 709 / sRGB linear)
    float luma = max(dot(color, vec3(0.2126, 0.7152, 0.0722)), 0.0);

    // Soft-knee thresholding to prevent harsh boundary transitions
    float knee = max(u_SoftKnee, 0.0001);
    float soft = luma - u_Threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee + 0.00001);
    
    float contribution = max(soft, luma - u_Threshold);
    contribution /= max(luma, 0.0001);

    vec3 brightColor = color * max(contribution, 0.0);
    FragColor = vec4(brightColor, 1.0);
}
