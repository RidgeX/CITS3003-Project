#version 150

in vec3 fL1, fL2;
in vec3 fE;
in vec3 fN;
in vec2 texCoord;  // The third coordinate is always 0.0 and is discarded

out vec4 fColor;

uniform vec3 LightColor1, LightColor2;
uniform float LightBrightness1, LightBrightness2;
uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform float Shininess;
uniform sampler2D texture;

void main() {
	// Unit direction vectors for Blinn-Phong shading calculation
	vec3 L1 = normalize(fL1);  // Direction to the light source
	vec3 L2 = normalize(fL2);
	vec3 E = normalize(fE);  // Direction to the eye/camera
	vec3 H1 = normalize(L1 + E);  // Halfway vector
	vec3 H2 = normalize(L2 + E);
	vec3 N = normalize(fN);  // Normal vector

	// [G] Compute terms in the illumination equation
	vec3 ambient1 = (LightColor1 * LightBrightness1) * AmbientProduct;
	vec3 ambient2 = (LightColor2 * LightBrightness2) * AmbientProduct;

	float Kd1 = max(dot(L1, N), 0.0);
	float Kd2 = max(dot(L2, N), 0.0);
	vec3 diffuse1 = Kd1 * (LightColor1 * LightBrightness1) * DiffuseProduct;
	vec3 diffuse2 = Kd2 * (LightColor2 * LightBrightness2) * DiffuseProduct;

	// [H] Only use brightness for specular highlights
	float Ks1 = pow(max(dot(N, H1), 0.0), Shininess);
	float Ks2 = pow(max(dot(N, H2), 0.0), Shininess);
	vec3 specular1 = Ks1 * LightBrightness1 * SpecularProduct;
	vec3 specular2 = Ks2 * LightBrightness2 * SpecularProduct;

	if (dot(L1, N) < 0.0) {
		specular1 = vec3(0.0, 0.0, 0.0);
	}
	if (dot(L2, N) < 0.0) {
		specular2 = vec3(0.0, 0.0, 0.0);
	}

	// globalAmbient is independent of distance from the light source
	vec3 globalAmbient = vec3(0.1, 0.1, 0.1);

	vec4 color = vec4(globalAmbient + ambient1 + ambient2 + diffuse1 + diffuse2, 1.0);
	fColor = color * texture2D(texture, texCoord * 2.0) + vec4(specular1 + specular2, 1.0);
}
