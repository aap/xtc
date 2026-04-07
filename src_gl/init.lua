require("xmath")
require("xtc")
require("camera")
require("fennel").install().dofile("main.fnl")

--[[
function init()
	material = xtcMaterial()
	material.colorSelector = vec4(0, 0, 0, 1)
	material.ambient = vec4(0, 0, 0, 1)
	material.diffuse = vec4(0, 0, 0, 1)

	cam = Camera()
	cam.position = vec3(4,-6,4)*0.7
	cam.target = vec3(0,0,0)
	cam.up = vec3(0,0,1)

	setTexPath('/u/aap/3dmodels/gta3_textures')
	mdl = loadXModelChunk('kuruma.xm.chk')
--	print(package.path)
--	test()

	xtcSetAmbient(100, 100, 100)
	local l = xtcLight()
	l.enabled = 1;
	l.type = XTC_LIGHT_DIRECT;
	l.color = vec4(0.8, 0.8, 0.8, 1);
	l.specColor = vec4(1, 1, 1, 1);
	l.direction = vec3(-1, 1, -1):normalized()
	xtcSetLight(0, l)
	light = l

	time = 0
	print(print_and_add(1,2,3))
end
--]]

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
	xtcSetShader(xtcGetDefaultShader())
	xtcSetTexture(0, nil)
	xtcSetMaterial(material)

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

function rotZ(phi)
	local c = math.cos(phi)
	local s = math.sin(phi)
	local m = { { c, -s, 0, 0 },
	            { s, c, 0, 0 },
	            { 0, 0, 1, 0 },
	            { 0, 0, 0, 1 } }
	return mkmat4(m)
end

--[[
function draw()
	fnl_draw()
	return 0
	local io = imguiIO()
	local aspect = io.DisplaySize.x/io.DisplaySize.y
	dt = io.DeltaTime
	time = time + dt

	local phi = time*1.5
	local ld = vec3(math.cos(phi), math.sin(phi), -1):normalized()
	light.direction = ld
	xtcSetLight(0, light)

	cam.aspect = aspect
	cam.fov = 81.3
	cam:process()
	xtcSetProjectionMatrix(cam:getProjMat())
	xtcSetViewMatrix(cam:getViewMat())
	xtcSetWorldMatrix(rotZ(0.2*phi))

	xtcEnable(XTC_DEPTH_TEST)
	drawAxes()

	xtcSetShader(xtcGetDefaultShader())
	xtcSetTexture(0, nil)
	xtcSetMaterial(material)
	xtcBegin(XTC_LINELIST)
		xtcColor(255, 255, 255, 255)
		xtcVertex(0, 0, 0)
		xtcVertex(-ld.x*3, -ld.y*3, -ld.z*3)
	xtcEnd()

	xtcEnable(XTC_BLEND)
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA)
	mdl:draw()
--	print("dt", io.DeltaTime)
--	print("fps", io.Framerate)
--	print("mouse", io.MousePos.x, io.MousePos.y)
end
--]]
