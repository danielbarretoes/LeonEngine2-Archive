#type vertex
#version 330 core

layout(location = 0) in vec3 aPos;

out vec3 v_TexCoords;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    v_TexCoords = aPos;
    // Strip camera translation by casting view matrix to mat3
    vec4 pos = u_Projection * mat4(mat3(u_View)) * vec4(aPos, 1.0);
    // Setting z = w ensures z / w = 1.0 (drawn at maximum depth / far plane)
    gl_Position = pos.xyww;
}

#type fragment
#version 330 core

layout(location = 0) out vec4 FragColor;

in vec3 v_TexCoords;

uniform vec3 u_SunDir;
uniform vec3 u_SkyColor;
uniform vec3 u_HorizonColor;
uniform vec3 u_GroundColor;
uniform vec3 u_SunColor;
uniform float u_SunIntensity;
uniform float u_Exposure = 1.0;

uniform sampler2D u_HDREnvironmentMap;
uniform int u_UseHDREnvironmentMap = 0;

const float PI = 3.14159265358979323846;

void main() {
    vec3 dir = normalize(v_TexCoords);

    if (u_UseHDREnvironmentMap == 1) {
        // Spherical equirectangular coordinates
        float u = 0.5 + atan(dir.z, dir.x) / (2.0 * PI);
        float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
        vec3 hdrSample = texture(u_HDREnvironmentMap, vec2(u, v)).rgb;
        FragColor = vec4(hdrSample * u_Exposure, 1.0);
        return;
    }

    vec3 sunDir = normalize(u_SunDir);
    float height = dir.y;
    
    // Procedural Physical Atmospheric Sky Gradient
    vec3 sky;
    if (height >= 0.0) {
        float horizonFactor = pow(1.0 - height, 4.0);
        sky = mix(u_SkyColor, u_HorizonColor, horizonFactor);
    } else {
        float groundFactor = clamp(-height * 3.0, 0.0, 1.0);
        sky = mix(u_HorizonColor, u_GroundColor, groundFactor);
    }

    // Solar Disc & Mie Atmospheric Scattering Halo
    float cosTheta = max(dot(dir, sunDir), 0.0);
    float sunDisc = step(0.9995, cosTheta);
    float sunHalo = pow(cosTheta, 32.0) * 1.5 + pow(cosTheta, 256.0) * 3.0;
    
    vec3 sunContribution = (u_SunColor * u_SunIntensity) * (sunDisc * 8.0 + sunHalo);
    vec3 hdrColor = (sky + sunContribution) * u_Exposure;

    // Linear HDR output (Post-Processing Pass handles Tonemapping & Gamma)
    FragColor = vec4(hdrColor, 1.0);
}
