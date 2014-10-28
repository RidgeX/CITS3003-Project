#version 150

in vec4 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;
in ivec4 boneIDs;
in vec4 boneWeights;

out vec3 fL1, fL2;
out vec3 fE;
out vec3 fN;
out vec2 texCoord;

uniform mat4 ModelView;
uniform mat4 Projection;
uniform vec4 LightPosition1, LightPosition2;
uniform mat4 boneTransforms[64];

void main() {
	mat4 boneTransform = boneWeights[0] * boneTransforms[boneIDs[0]];
	boneTransform += boneWeights[1] * boneTransforms[boneIDs[1]];
	boneTransform += boneWeights[2] * boneTransforms[boneIDs[2]];
	boneTransform += boneWeights[3] * boneTransforms[boneIDs[3]];

	vec4 position = boneTransform * vPosition;
	vec3 normal = mat3(boneTransform) * vNormal;

	// Transform vertex position into eye coordinates
	vec3 pos = (ModelView * position).xyz;

	// The vector to the light from the vertex
	fL1 = LightPosition1.xyz - pos;
	fL2 = LightPosition2.xyz - pos;

	// The vector to the eye/camera from the vertex
	fE = -pos;

	// Transform vertex normal into eye coordinates (assumes scaling is uniform across dimensions)
	fN = (ModelView * vec4(normal, 0.0)).xyz;

	gl_Position = Projection * ModelView * position;
	texCoord = vTexCoord;
}
