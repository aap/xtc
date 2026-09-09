require("xmath")
require("xtc")
require("camera")
-- the scene script: main.fnl (the model viewer) unless -script says otherwise
local script = "main.fnl"
for i, a in ipairs(arg) do
	if a == "-script" then script = arg[i+1] end
end
require("fennel").install().dofile(script)


function test()
	local A = vec3(1,2)
	local a = vec3(1,2,3)
	local b = vec3(30,20,10)

	local m = mat4(1)
	print(m[1])
	print(m[2])
	print(m[3])
	print(m[4])
	print(m)
	local r1 = mkmat4({{ 0, 0, 0, 1},
	                   { 0, 0, -1, 0},
	                   { 0, 1, 0, 0},
	                   { -1, 0, 0, 0}})
	local r2 = mkmat4({{ 0, 0, -1, 0},
	                   { 0, 0, 0, -1},
	                   { 1, 0, 0, 0},
	                   { 0, 1, 0, 0}})
	local r3 = mkmat4({{ 0, 1, 0, 0},
	                   { -1, 0, 0, 0},
	                   { 0, 0, 0, -1},
	                   { 0, 0, 1, 0}})
	print("r1")
	print(r1)
	print("r2")
	print(r2)
	print("r3")
	print(r3)
local xxx = debug.getregistry()["Vec4"]
print(xxx.__methods)
	print(r1 * r2)
	print(r3 * 4)

	local v = vec4(1, 2, 3, 4)
	print(r1 * v)

	a = vec3(4, 5, 6)
	a.x = 10
	a.y = 20
	a.z = 30
	print(a.x)
	print(a.y)
	print(a.z)

	local xxx = m
	xxx[1] = v
	xxx[1][3] = 123
	print(m)
	print(xxx)

	local q = quat(1, 2, 3, 4)
	local p = quat(4, 1, 0, 2)
	print(q)
	print(p)
	q = rotor(0.3, vec3(0.5, 0.2, 0.1):normalized());
	print(q)
end

function drawAxes(scale)
	xtcSetPipeline(defaultPipeline)
	xtcSetTexture(nil)
	xtcSetStdMaterial(material)
	xtcSetColorMaterial(XTC_EMISSIVE)

	local s = scale or 1
	xtcBegin(XTC_LINELIST)
		xtcColor(255, 0, 0, 255)
		xtcVertex(0, 0, 0)
		xtcVertex(s, 0, 0)

		xtcColor(0, 255, 0, 255)
		xtcVertex(0, 0, 0)
		xtcVertex(0, s, 0)

		xtcColor(0, 0, 255, 255)
		xtcVertex(0, 0, 0)
		xtcVertex(0, 0, s)
	xtcEnd()
end

function rotX(phi)
	local c = math.cos(phi)
	local s = math.sin(phi)
	local m = { { 1, 0, 0, 0 },
	            { 0, c, -s, 0 },
	            { 0, s, c, 0 },
	            { 0, 0, 0, 1 } }
	return mkmat4(m)
end

function rotZ(phi)
	local c = math.cos(phi)
	local s = math.sin(phi)
	local m = { { c, -s, 0, 0 },
	            { s, c, 0, 0 },
	            { 0, 0, 1, 0 },
	            { 0, 0, 0, 1 } }
	return mkmat4(m)
end

