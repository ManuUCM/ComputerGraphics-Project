#version 330 core

in vec3 color;
in vec3 worldPosition;
in vec3 worldNormal;
// step 3 shades
in vec4 lightSpacePos;

out vec3 finalColor;

uniform vec3 lightPosition;
uniform vec3 lightIntensity;
// step 3 shades
uniform sampler2D shadowMap;


void main()
{
	// TODO: lighting, tone mapping, gamma correction
	// Normalize vectors
	vec3 N = normalize(worldNormal);
	vec3 L = normalize(lightPosition - worldPosition);
	float r = length(lightPosition - worldPosition);

	// Lambert light equation
	// L(x,*) = ρd/π * (N * L) * Φ/4πr2
	// Φ/4πr2 -> lightIntensity
	// "Assume Lambertian surfaces"
	// "Assume point light radiates equally in all directions"
	float diff = max(dot(N, L), 0.0);
	float attenuation = 1.0 / (4.0 * 3.1415926 * r * r);  // 1/(4πr²)
	vec3 lambert = diff * lightIntensity * attenuation;

	// Apply color
	finalColor = lambert * color;

	//TODO implement shadow
	// Convert from clip → NDC → UV
	vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
	vec2 uv = proj.xy * 0.5 + 0.5;
	// Depth in light space [0,1]
	float depth = proj.z * 0.5 + 0.5;

	// If fragment off shadow map's limits, do not apply shadow
	if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {
		// Read depth from shadow map
		float existingDepth = texture(shadowMap, uv).r;
		// Shadow test formula
		float shadow = (depth >= existingDepth + 1e-3) ? 0.2 : 1.0;
		// final color with shadow
		finalColor *= shadow;
	}
	// Tone mapping
	finalColor = finalColor / (finalColor + vec3(1.0));
	// Gamma correction
	finalColor = pow(finalColor, vec3(1.0/2.2));
}
