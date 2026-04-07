camtable = {}
camtable.__index = camtable

function camtable.__tostring(c)
	return "Camera(" .. c.position .. ", " .. c.target .. ", " .. c.up .. ")"
end

function camtable.process(c)
	local io = imguiIO()
	local d = getDragMode()
	if d ~= 0 then
		local scale = 1/io.Framerate
		local mouseSens = scale*15
		local zoomSens = 0.5

		local dx = -io.MouseDelta.x
		local dy = -io.MouseDelta.y
		if d == 1 then
			c:orbit(Deg2Rad(dx)*mouseSens, -Deg2Rad(dy)*mouseSens)
		elseif d == 2 then
			c:zoom(dy*mouseSens*zoomSens)
		elseif d == 3 then
			dist = c:distanceToTarget()
			c:pan(dx*mouseSens*dist/100, -dy*mouseSens*dist/100)
		end
	end
end

function camtable.distanceToTarget(c)
	return (c.position - c.target):len()
end

function camtable.getProjMat(c)
	return perspective(c.fov, c.aspect, c.near, c.far)
end

function camtable.getViewMat(c)
	local m = lookat(c.position, c.target, c.up);
	return m:invOrtho()
end

function camtable.orbit(c, yaw, pitch)
	local dir = c.target - c.position
	local zaxis = vec3(0,0,1)
	local r = rotor(yaw, zaxis)
	dir = r:sandwichVec3(dir)
	c.localup = r:sandwichVec3(c.localup)

	local right = dir:cross(c.localup):normalized()
	r = rotor(-pitch, right)
	dir = r:sandwichVec3(dir)
	c.localup = right:cross(dir):normalized()
	if c.localup.z >= 0.0 then c.up.z = 1.0
	else c.up.z = -1.0
	end

	c.position = c.target - dir
end

function camtable.zoom(c, dist)
	local dir = c.target - c.position
	local curdist = dir:len()
	if dist >= curdist then 
		dist = curdist-0.3
	end
	dir = dir:normalized()*dist
	c.position = c.position + dir
end

function camtable.pan(c, x, y)
	local dir = (c.target - c.position):normalized()
	local right = dir:cross(c.up):normalized()
	local localup = right:cross(dir):normalized()
	dir = right*x + localup*y
	c.position = c.position + dir
	c.target = c.target + dir
end

function Camera()
	local c = {
		fov = 70,
		aspect = 1,
		near = 0.1,
		far = 100.0,
		position = vec3(0, 1, 0),
		target = vec3(0, 0, 0),
		up = vec3(0, 0, 1),
		localup = vec3(0, 0, 1),
		projMat = mat4(1),
		viewMat = mat4(1)
	}
	setmetatable(c, camtable)
	return c
end
