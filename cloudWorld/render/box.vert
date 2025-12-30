#version 330 core
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec3 vertexNormal;
layout(location=2) in vec2 vertexUV;

out vec3 worldN;
out vec2 UV;
out vec3 worldPos;
out vec4 lightSpacePos; // shadow map

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 LightVP; // light view-projection matrix

void main(){
    worldN = mat3(transpose(inverse(M))) * vertexNormal;
    worldPos = (M * vec4(vertexPosition, 1.0)).xyz;
    UV = vertexUV;

    // Calculate position in light space for shadow mapping
    lightSpacePos = LightVP * vec4(worldPos, 1.0);

    gl_Position = MVP * vec4(vertexPosition,1.0);
}