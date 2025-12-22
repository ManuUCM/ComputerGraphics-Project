#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec3 vertexNormal;

// Output data, to be interpolated for each fragment
out vec3 color;
out vec3 worldPosition;
out vec3 worldNormal;
// Step 3 shades
out vec4 lightSpacePos;


uniform mat4 MVP;
// Step 2 lighting
uniform mat4 Model;
// Step 3 shades
uniform mat4 LightVP;

void main() {
    // Transform vertex
    gl_Position =  MVP * vec4(vertexPosition, 1);
    
    // Pass vertex color to the fragment shader
    color = vertexColor;

    // World-space geometry 
    worldPosition = vec3(Model * vec4(vertexPosition, 1.0));
    worldNormal = mat3(transpose(inverse(Model))) * vertexNormal;

    lightSpacePos = LightVP * vec4(worldPosition, 1.0);
}
