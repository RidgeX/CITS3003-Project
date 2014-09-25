#version 150

in vec3 fL;
in vec3 fE;
in vec3 fN;
in vec2 texCoord;  // The third coordinate is always 0.0 and is discarded

out vec4 fColor;

uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform float Shininess;
uniform sampler2D texture;

void main() {
	// Unit direction vectors for Blinn-Phong shading calculation
	vec3 L = normalize(fL);  // Direction to the light source
	vec3 E = normalize(fE);  // Direction to the eye/camera
	vec3 H = normalize(L + E);  // Halfway vector
	vec3 N = normalize(fN);  // Normal vector

	// Compute terms in the illumination equation
	vec3 ambient = AmbientProduct;

	float Kd = max(dot(L, N), 0.0);
	vec3 diffuse = Kd * DiffuseProduct;

	float Ks = pow(max(dot(N, H), 0.0), Shininess);
	vec3 specular = Ks * SpecularProduct;

	if (dot(L, N) < 0.0) {
		specular = vec3(0.0, 0.0, 0.0);
	}

	// globalAmbient is independent of distance from the light source
	vec3 globalAmbient = vec3(0.1, 0.1, 0.1);
	vec4 color = vec4(globalAmbient + ambient + diffuse + specular, 1.0);

	fColor = color * texture2D(texture, texCoord * 2.0);
}
