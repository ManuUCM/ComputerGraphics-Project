#version 330 core
in vec3 worldN;
in vec2 UV;
out vec3 finalColor;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 envColor;

void main(){
	// Normalize the surface normal
	vec3 N = normalize(worldN);

	// Lambertian diffuse lighting
	// Formula:
	// L_d = max( dot(N, L), 0 )
	// where:
	// N = surface normal
	// L = light direction

	// This models how much light hits the surface depending
	// on its orientation relative to the light source.

	float ndl = max(dot(N, -lightDir), 0.0);
	vec3 diffuse = ndl * lightColor;

	// Environment (hemispherical) lighting
	// Idea:
	// Use the normal direction to estimate how much of the
	// environment (sky / nebula) contributes to the surface.
	// Approximate this by mapping the Y component of the normal
	// from [-1, 1] to [0, 1]:
	//   hemi = N.y * 0.5 + 0.5
	// Surfaces facing "up" receive more environment light,
	// surfaces facing "down" receive less.
	float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 ambient = envColor * hemi;

	// Combine lighting with material color (albedo)
	// We adapt environment lighting into Lambertian shading
	// by ADDING it as an ambient term:
	//   L_total = albedo * (L_ambient + L_diffuse)
	// This keeps the Lambertian model intact while improving
	// it with global illumination from the environment.
	vec3 albedo = vec3(0.7, 0.7, 0.8);
	vec3 color = albedo * (ambient + diffuse);

	finalColor = color;
}