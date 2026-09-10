#version 460

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_texCoord;
layout(location = 4) in ivec4 in_indices;
layout(location = 5) in vec4 in_weights;

out vec4 v_color;
out vec4 v_colorSpec;
out vec2 v_texCoord;

uniform mat4 u_world;
uniform vec4 u_colorClamp;
uniform vec4 u_colorScale;
uniform vec4 u_colorNorm;
uniform mat4 u_normal;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_eyePos;

uniform vec4 u_matColorSelector;
uniform vec4 u_matAmbient;
uniform vec4 u_matDiffuse;
uniform vec4 u_matSpecular;
uniform vec4 u_matEmissive;
uniform float u_matShininess;

uniform vec4 u_ambient;

// one hardcoded directional light
uniform vec4 u_lightDiffuse;
uniform vec4 u_lightSpecular;
uniform vec3 u_lightDirection;

uniform mat4 u_boneMatrices[64];

/*
material:
	ambient color
	diffuse color
	specular color
	emissive color
	specular power

light:
	ambient intensity
	diffuse intensity
	specular intensity
	position
	direction
	...

	global ambient

	emissive +
	global ambient * ambient color +
	sum	ambient intensity * ambient color +
		diffuse intensity * diffuse color * diffuse factor +
		specular intensity * specular color * (specular factor)^specular power +

	alpha = diffuse color alpha

color material
	each material color can be per vertex
*/

void main()
{
	vec3 SkinVertex = vec3(0.0, 0.0, 0.0);
	vec3 SkinNormal = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < 4; i++){
		SkinVertex += (u_boneMatrices[in_indices[i]] * vec4(in_pos, 1.0)).xyz * in_weights[i];
		SkinNormal += (mat3(u_boneMatrices[in_indices[i]]) * in_normal) * in_weights[i];
	}
	vec3 Vw = vec3(u_world * vec4(SkinVertex, 1.0));
	vec3 Nw = mat3(u_normal) * SkinNormal;

Nw = normalize(Nw);
	vec3 Vv = vec3(u_view * vec4(Vw, 1.0));
	gl_Position = u_proj * vec4(Vv, 1.0);

	v_texCoord = in_texCoord.xy;

	vec4 amb = mix(u_matAmbient, in_color, u_matColorSelector.x);
	vec4 diff = mix(u_matDiffuse, in_color, u_matColorSelector.y);
	vec4 spec = mix(u_matSpecular, in_color, u_matColorSelector.z);
	vec4 emiss = mix(u_matEmissive, in_color, u_matColorSelector.w);

	v_color = emiss + u_ambient*amb;
	v_colorSpec = vec4(0.0);

	float dl = max(0, dot(-u_lightDirection, Nw));
	v_color += u_lightDiffuse*diff*dl;
	v_color.a = diff.a;

	if(u_matShininess != 0.0 && dl != 0.0) {
		vec3 toLight = -u_lightDirection;
		vec3 toEye = normalize(u_eyePos - Vw);

		// blinn
		vec3 h = normalize(toLight + toEye);
		float sl = pow(max(0, dot(h, Nw)), u_matShininess);
/*
		// phong
		vec3 r = 2*dot(Nw, toLight)*Nw - toLight;
		float sl = pow(max(0, dot(r, toEye)), u_matShininess);
*/

		v_colorSpec.rgb += vec3(u_lightSpecular*spec*sl);
	}

	// the GS's view of it: clamp in 0..255, scale, and back to 1.0
	// being what the GS takes as 1.0 (see xtcColorMod)
	v_color = min(max(v_color, 0.0)*255.0, u_colorClamp)*u_colorScale/u_colorNorm;
	v_colorSpec = clamp(v_colorSpec, 0.0, 1.0);
}
