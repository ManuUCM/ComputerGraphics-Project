#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec4 lightSpacePos;

out vec3 finalColor;

// Directional light (like planets)
uniform vec3 lightDir;      // Direction TO the light
uniform vec3 lightColor;    // Light color/intensity

//shadow
uniform sampler2D shadowMap;

// Environment lighting
uniform vec3 envColor;

//uniform vec3 lightPosition;
//uniform vec3 lightIntensity;

// Fog uniforms
uniform vec3 cameraPosition;
uniform vec3 fogColor;
uniform float fogDensity;
uniform bool fogEnabled;

void main()
{
	vec3 N = normalize(worldNormal);

	// Directional (sun) lighting - same as planets
	float ndl = max(dot(N, -lightDir), 0.0);  // -lightDir because lightDir points TO light
	vec3 diffuse = ndl * lightColor;

	// Ambient lighting (similar to planets)
	// Add hemispherical environment lighting
	float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
	vec3 ambient = envColor * hemi;

	// Combine lighting
	vec3 albedo = vec3(1.0, 1.0, 1.0);  // White
	vec3 color = albedo * (ambient + diffuse);

	// Shadow calculation
	vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
	vec2 shadowUV = proj.xy * 0.5 + 0.5;
	float depth = proj.z * 0.5 + 0.5;

	if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
		float existingDepth = texture(shadowMap, shadowUV).r;
		float shadow = (depth >= existingDepth + 0.003) ? 0.3 : 1.0;
		color *= shadow;
	}

	// Tone mapping
	color = color / (color + vec3(1.0));

	// Gamma correction
	color = pow(color, vec3(1.0 / 2.2));

	// Apply fog if enabled
	if (fogEnabled) {
		// Calculate distance from camera to fragment
		float distance = length(worldPosition - cameraPosition);

		// Exponential squared fog: fogFactor = exp(-(density * distance)^2)
		float fogFactor = exp(-pow(fogDensity * distance, 2.0));
		fogFactor = clamp(fogFactor, 0.0, 1.0);

		// Mix color with fog: finalColor = mix(fogColor, objectColor, fogFactor)
		// fogFactor = 1.0 means no fog (close), fogFactor = 0.0 means full fog (far)
		color = mix(fogColor, color, fogFactor);
	}

	finalColor = color;
}
