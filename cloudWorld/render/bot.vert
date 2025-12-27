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

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 jointMatrices[100]; // Max joints

void main() {
    // Linear Blend Skinning formula:
    // P_skinned = sum( Weight_i * JointMatrix_i ) * P_original

    mat4 skinMatrix =
    weights.x * jointMatrices[joints.x] +
    weights.y * jointMatrices[joints.y] +
    weights.z * jointMatrices[joints.z] +
    weights.w * jointMatrices[joints.w];

    vec4 skinnedPosition = skinMatrix * vec4(vertexPosition, 1.0);

    // Apply model matrix
    vec4 worldPos = M * skinnedPosition;

    gl_Position = MVP * worldPos;

    // World-space geometry
    worldPosition = skinnedPosition.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(skinMatrix)));
    worldNormal = normalMatrix * vertexNormal;
}
