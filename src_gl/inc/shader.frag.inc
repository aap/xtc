const char *shader_frag_src =
"#version 460\n"
"\n"
"in vec4 v_color;\n"
"in vec4 v_colorSpec;\n"
"out vec4 frag_color;\n"
"\n"
"void main()\n"
"{\n"
"	frag_color = v_color + v_colorSpec;\n"
"}\n"
;
