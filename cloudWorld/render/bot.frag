#version 330 core

in vec3 worldPosition;
in vec3 worldNormal; 

out vec3 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;

// Fog uniforms
uniform vec3 cameraPosition;
uniform vec3 fogColor;
uniform float fogDensity;
uniform bool fogEnabled;

void main()
{
	// Lighting
	vec3 lightDir = lightPosition - worldPosition;
	float lightDist = dot(lightDir, lightDir);
	lightDir = normalize(lightDir);
	vec3 v = lightIntensity * clamp(dot(lightDir, worldNormal), 0.0, 1.0) / lightDist;

	// Tone mapping
	v = v / (1.0 + v);

	// Gamma correction
	vec3 color = pow(v, vec3(1.0 / 2.2));

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
