#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outNormal;

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform MaterialProperties {
    float roughness;
    float metallic;
} mp;

void main() {
    outColor = vec4(texture(texSampler, fragTexCoord).rgb, mp.roughness);
    outPosition = vec4(fragPosition, mp.metallic);
    outNormal = vec4(normalize(fragNormal), 1.0);
}
