#version 150

in vec4 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

out vec3 fL1, fL2;
out vec3 fE;
out vec3 fN;
out vec2 texCoord;

uniform mat4 ModelView;
uniform mat4 Projection;
uniform vec4 LightPosition1, LightPosition2;

void main() {
	// Transform vertex position into eye coordinates
	vec3 pos = (ModelView * vPosition).xyz;

	// The vector to the light from the vertex
	fL1 = LightPosition1.xyz - pos;
	fL2 = LightPosition2.xyz - pos;

	// The vector to the eye/camera from the vertex
	fE = -pos;

	// Transform vertex normal into eye coordinates (assumes scaling is uniform across dimensions)
	fN = (ModelView * vec4(vNormal, 0.0)).xyz;

	gl_Position = Projection * ModelView * vPosition;
	texCoord = vTexCoord;
}
