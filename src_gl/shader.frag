#version 460

in vec4 v_color;
in vec4 v_colorSpec;
out vec4 frag_color;

void main()
{
	frag_color = v_color + v_colorSpec;
}
