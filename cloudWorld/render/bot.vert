#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
// For skinning
layout(location = 3) in uvec4 joints; // Indices of the joints
layout(location = 4) in vec4 weights; // Weights of influence

// Output data, to be interpolated for each fragment
out vec3 worldPosition;
out vec3 worldNormal;
out vec4 lightSpacePos;

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 jointMatrices[100]; // Max joints

uniform vec3 modelCenter;
uniform float modelScale;
uniform vec3 skeletonOffset;
uniform mat4 LightVP;


void main() {
    // Linear blend skinning - combine joint transformations weighted by influence
    mat4 skinMatrix =
    weights.x * jointMatrices[joints.x] +
    weights.y * jointMatrices[joints.y] +
    weights.z * jointMatrices[joints.z] +
    weights.w * jointMatrices[joints.w];

    // Apply skinning transformation to vertex
    vec4 skinnedPosition = skinMatrix * vec4(vertexPosition, 1.0);
    // normalize model position: center at origin, align skeleton root, and scale to unit size
    // modelCenter: humanoid's geometric center from GLTF bounding box
    // skeletonOffset: offset from geometric center to skeleton root joint
    // modelScale: normalization factor to scale model to approx. 1 unit
    vec3 centered = (skinnedPosition.xyz + modelCenter + skeletonOffset) * modelScale;
    vec4 worldPos = M * vec4(centered, 1.0);

    worldPosition = worldPos.xyz;
    gl_Position = MVP * worldPos;

    // Calculate light space position
    lightSpacePos = LightVP * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(M)));
    worldNormal = normalMatrix * (mat3(skinMatrix) * vertexNormal);
}
