#version 330 core
in vec3 worldN;
in vec2 UV;
in vec3 worldPos;
in vec4 lightSpacePos;

out vec3 finalColor;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 envColor;
uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;

// Fog uniforms
uniform vec3 cameraPosition;
uniform vec3 fogColor;
uniform float fogDensity;
uniform bool fogEnabled;

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
	// environment (nebula background) contributes to the surface.
	// Approximate this by mapping the Y component of the normal
	// from [-1, 1] to [0, 1]:
	//   hemi = N.y * 0.5 + 0.5
	// Surfaces facing say upwards receive more environment light,
	// and those surfaces facing downwards receive less.
	float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 ambient = envColor * hemi;

	// Combine lighting with material color (albedo)
	// adapt environment lighting into Lambertian shading
	// by adding it as an ambient term:
	//   L_total = albedo * (L_ambient + L_diffuse)
	// This keeps the Lambertian model intact while improving
	// it with global illumination from the environment.
	vec3 albedo = texture(diffuseTexture, UV).rgb;
	vec3 color = albedo * (ambient + diffuse);

	// Convert from light clip space to NDC to UV coordinates
	vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
	vec2 shadowUV = proj.xy * 0.5 + 0.5;
	float depth = proj.z * 0.5 + 0.5;

	// Apply shadow if within shadow map bounds
	if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
		float existingDepth = texture(shadowMap, shadowUV).r;
		// Shadow test
		float shadow = (depth >= existingDepth + 0.003) ? 0.3 : 1.0; // had to increase bias to 0.003
		color *= shadow;											 // due to lots of shadow acne
	}

	// Tone mapping (Reinhard)
	// C_out = C / (C + 1)
	color = color / (color + vec3(1.0));

	// Gamma correction
	color = pow(color, vec3(1.0 / 2.2));

	// Apply fog if enabled
	if (fogEnabled) {
		// Calculate distance from camera to fragment
		float distance = length(worldPos - cameraPosition);

		// Exponential squared fog: fogFactor = exp(-(density * distance)^2)
		float fogFactor = exp(-pow(fogDensity * distance, 2.0));
		fogFactor = clamp(fogFactor, 0.0, 1.0);

		// combine color with fog
		// linear interpolation using mix(start range, end range, value to interpolate between)
		color = mix(fogColor, color, fogFactor);
	}

	finalColor = color;
}