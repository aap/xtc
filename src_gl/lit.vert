// the lit4/lit8 pipelines: global ambient, NLIGHTS directional diffuse
// lights and one directional specular light with an infinite viewer.
//
// the lights arrive in object space, packed the way VU1 wants them:
// row i of u_lightMat is the direction towards light i, column i of
// u_lightColMat its colour.  so per group of 4 lights the diffuse term
// is one matrix multiply (4 dot products), one clamp, and one matrix
// multiply summing the 4 colours.  normals are never transformed.
//
// NLIGHTS is defined by the pipeline (4 or 8), the #version line too.

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_texCoord;

out vec4 v_color;
out vec4 v_colorSpec;
out vec2 v_texCoord;

uniform mat4 u_world;
uniform mat4 u_view;
uniform mat4 u_proj;

uniform vec4 u_matColorSelector;
uniform vec4 u_matAmbient;
uniform vec4 u_matDiffuse;
uniform vec4 u_matSpecular;
uniform vec4 u_matEmissive;
uniform float u_matShininess;

uniform vec4 u_ambient;
uniform mat4 u_lightMat[NLIGHTS/4];
uniform mat4 u_lightColMat[NLIGHTS/4];
uniform vec3 u_specDir;		// half vector of the specular light
uniform vec4 u_specCol;		// its specular colour

void main()
{
	gl_Position = u_proj * u_view * u_world * vec4(in_pos, 1.0);
	v_texCoord = in_texCoord.xy;

	vec4 amb = mix(u_matAmbient, in_color, u_matColorSelector.x);
	vec4 diff = mix(u_matDiffuse, in_color, u_matColorSelector.y);
	vec4 spec = mix(u_matSpecular, in_color, u_matColorSelector.z);
	vec4 emiss = mix(u_matEmissive, in_color, u_matColorSelector.w);

	// diffuse: 4 lights per matrix pair
	vec4 n = vec4(in_normal, 0.0);
	vec3 light = vec3(0.0);
	for(int i = 0; i < NLIGHTS/4; i++) {
		vec4 I = max(u_lightMat[i] * n, 0.0);
		light += vec3(u_lightColMat[i] * I);
	}

	v_color.rgb = emiss.rgb + u_ambient.rgb*amb.rgb + light*diff.rgb;
	v_color.a = diff.a;

	// specular: light 0 only, and only where it lights the surface
	v_colorSpec = vec4(0.0);
	float dl = dot(vec3(u_lightMat[0][0][0], u_lightMat[0][1][0], u_lightMat[0][2][0]), in_normal);
	if(u_matShininess > 0.0 && dl > 0.0) {
		float sl = pow(max(dot(u_specDir, in_normal), 0.0), u_matShininess);
		v_colorSpec.rgb = u_specCol.rgb*spec.rgb*sl;
	}

	v_color = clamp(v_color, 0.0, 1.0);
	v_colorSpec = clamp(v_colorSpec, 0.0, 1.0);
}
