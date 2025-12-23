#version 330 core
in vec3 worldN;
in vec2 UV;
out vec3 finalColor;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 envColor;

void main(){
	vec3 N = normalize(worldN);
	float ndl = max(dot(N,-lightDir),0.0);

	vec3 albedo = vec3(0.6,0.6,0.7);
	float hemi = clamp(N.y*0.5+0.5,0.0,1.0);
	vec3 ambient = envColor * hemi;

	vec3 c = albedo * (ambient + ndl*lightColor);
	c = c/(vec3(1.0)+c);
	c = pow(c,vec3(1.0/2.2));
	finalColor = c;
}