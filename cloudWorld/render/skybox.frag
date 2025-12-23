#version 330 core

in vec2 UV;
out vec3 finalColor;
uniform sampler2D textureSampler;

void main(){
    finalColor = texture(textureSampler,UV).rgb;
}