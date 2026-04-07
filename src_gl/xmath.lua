local function copy_metamethods(methods, ...)
	local metatables = {...}
	for k, v in pairs(methods) do
--		if k:match("^__") then  -- If it's a metamethod
			for _, mt in ipairs(metatables) do
				mt[k] = v
			end
--		end
	end
end


function Deg2Rad(d)
	return d/180*math.pi
end

local vec2table = {}

function vec2table.__tostring(v)
	return "(" .. v.x .. ", " .. v.y .. ")"
end

function vec2table.__concat(a, b)
	return tostring(a) .. tostring(b)
end

function vec2table.neg(a)
	return vec2(-a.x, -a.y)
end

function vec2table.add(a, b)
	return vec2(a.x+b.x, a.y+b.y)
end

function vec2table.sub(a, b)
	return vec2(a.x-b.x, a.y-b.y)
end

function vec2table.scale(a, s)
	return vec2(s*a.x, s*a.y)
end

function vec2table.dot(a, b)
	return a.x*b.x + a.y*b.y
end

function vec2table.lensq(a)
	return a:dot(a)
end

function vec2table.len(a)
	return math.sqrt(a:lensq())
end

function vec2table.normalized(a)
	return a:scale(1/a:len())
end

function vec2table.__add(a, b)
	return a:add(b)
end

function vec2table.__sub(a, b)
	return a:sub(b)
end

function vec2table.__unm(a)
	return a:neg()
end

function vec2table.__mul(a, b)
	if type(a) == "number" then
		return b:scale(a)
	elseif type(b) == "number" then
		return a:scale(b)
	else
		return nil
	end
end

function vec2table.__div(a, b)
	if type(b) == "number" then
		return a:scale(1/b)
	else
		return nil
	end
end

copy_metamethods(vec2table, debug.getregistry()["Vec2"])


local vec3table = {}

function vec3table.__tostring(v)
	return "(" .. v.x .. ", " .. v.y .. ", " .. v.z .. ")"
end

function vec3table.__concat(a, b)
	return tostring(a) .. tostring(b)
end

function vec3table.neg(a)
	return vec3(-a.x, -a.y, -a.z)
end

function vec3table.add(a, b)
	return vec3(a.x+b.x, a.y+b.y, a.z+b.z)
end

function vec3table.sub(a, b)
	return vec3(a.x-b.x, a.y-b.y, a.z-b.z)
end

function vec3table.scale(a, s)
	return vec3(s*a.x, s*a.y, s*a.z)
end

function vec3table.dot(a, b)
	return a.x*b.x + a.y*b.y + a.z*b.z
end

function vec3table.cross(a, b)
	return vec3(a.y*b.z - a.z*b.y,
	            a.z*b.x - a.x*b.z,
	            a.x*b.y - a.y*b.x)
end

function vec3table.lensq(a)
	return a:dot(a)
end

function vec3table.len(a)
	return math.sqrt(a:lensq())
end

function vec3table.normalized(a)
	return a:scale(1/a:len())
end

function vec3table.__add(a, b)
	return a:add(b)
end

function vec3table.__sub(a, b)
	return a:sub(b)
end

function vec3table.__unm(a)
	return a:neg()
end

function vec3table.__mul(a, b)
	if type(a) == "number" then
		return b:scale(a)
	elseif type(b) == "number" then
		return a:scale(b)
	else
		return nil
	end
end

function vec3table.__div(a, b)
	if type(b) == "number" then
		return a:scale(1/b)
	else
		return nil
	end
end

function vec3table.quat(v)
	return quat(0, v.x, v.y, v.z)
end

copy_metamethods(vec3table, debug.getregistry()["Vec3"])



local vec4table = {}

function vec4table.__tostring(v)
	return "(" .. v.x .. ", " .. v.y .. ", " .. v.z .. ", " .. v.w .. ")"
end

function vec4table.__concat(a, b)
	return tostring(a) .. tostring(b)
end

function vec4table.neg(a)
	return vec4(-a.x, -a.y, -a.z, -a.w)
end

function vec4table.add(a, b)
	return vec4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w)
end

function vec4table.sub(a, b)
	return vec4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w)
end

function vec4table.scale(a, s)
	return vec4(s*a.x, s*a.y, s*a.z, s*a.w)
end

function vec4table.dot(a, b)
	return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w
end

function vec4table.lensq(a)
	return a:dot(a)
end

function vec4table.len(a)
	return math.sqrt(a:lensq())
end

function vec4table.normalized(a)
	return a:scale(1/a:len())
end

function vec4table.__add(a, b)
	return a:add(b)
end

function vec4table.__sub(a, b)
	return a:sub(b)
end

function vec4table.__unm(a)
	return a:neg()
end

function vec4table.__mul(a, b)
	if type(a) == "number" then
		return b:scale(a)
	elseif type(b) == "number" then
		return a:scale(b)
	else
		return nil
	end
end

function vec4table.__div(a, b)
	if type(b) == "number" then
		return a:scale(1/b)
	else
		return nil
	end
end

function vec4table.quat(v)
	return quat(v.w, v.x, v.y, v.z)
end

copy_metamethods(vec4table, debug.getregistry()["Vec4"])
copy_metamethods(vec4table, debug.getregistry()["Vec4p"])


mat4table = debug.getregistry()["Mat4"]

function mat4table.__tostring(m)
	return
		"/" .. m[1][1] .. " " .. m[2][1] .. " " .. m[3][1] .. " " .. m[4][1] .. "\\\n" ..
		"|" .. m[1][2] .. " " .. m[2][2] .. " " .. m[3][2] .. " " .. m[4][2] .. "|\n" ..
		"|" .. m[1][3] .. " " .. m[2][3] .. " " .. m[3][3] .. " " .. m[4][3] .. "|\n" ..
		"\\" .. m[1][4] .. " " .. m[2][4] .. " " .. m[3][4] .. " " .. m[4][4] .. "/"
end

function mat4table.__concat(a, b)
	return tostring(a) .. tostring(b)
end

function mat4table.scale(a, s)
	local m = mat4()
	for i = 1,4 do
		for j = 1,4 do
			m[i][j] = s*m[i][j]
		end
	end
	return m
end

function mat4table.add(a, b)
	local m = mat4()
	for i = 1,4 do
		for j = 1,4 do
			m[i][j] = a[i][j] + b[i][j]
		end
	end
	return m
end

function mat4table.mul(a, b)
	local m = mat4(0)
	for i = 1,4 do
		for j = 1,4 do
			for k = 1,4 do
				m[i][j] = m[i][j] + a[k][j] * b[i][k]
			end
		end
	end
	return m
end

function mat4table.invOrtho(m)
	return mkmat4({{ m[1][1], m[1][2], m[1][3], -m[1]:dot(m[4]) },
	               { m[2][1], m[2][2], m[2][3], -m[2]:dot(m[4]) },
	               { m[3][1], m[3][2], m[3][3], -m[3]:dot(m[4]) },
	               { 0, 0, 0, 1}})
end

function mat4table.xform(m, v)
	return vec4(
		m[1][1]*v.x + m[1][2]*v.y + m[1][3]*v.z + m[1][4]*v.w,
		m[2][1]*v.x + m[2][2]*v.y + m[2][3]*v.z + m[2][4]*v.w,
		m[3][1]*v.x + m[3][2]*v.y + m[3][3]*v.z + m[3][4]*v.w,
		m[4][1]*v.x + m[4][2]*v.y + m[4][3]*v.z + m[4][4]*v.w
	)
end

function mat4table.__mul(a, b)
	if getmetatable(a) == mat4table and
	   getmetatable(b) == mat4table then
		return a:mul(b)
	elseif getmetatable(a) == mat4table and
	       getmetatable(b) == vec4table then
		return a:xform(b)
	elseif type(a) == "number" then
		return b:scale(a)
	elseif type(b) == "number" then
		return a:scale(b)
	else
		return nil
	end
end

function lookat(pos, target, up)
	local z = (pos-target):normalized()
	local x = up:cross(z):normalized()
	local y = z:cross(x):normalized()
	return mkmat4({{ x.x, y.x, z.x, pos.x },
	               { x.y, y.y, z.y, pos.y },
	               { x.z, y.z, z.z, pos.z },
	               { 0.0, 0.0, 0.0, 1.0 }})
end

function perspective(fov, aspect, n, f)
	fov = Deg2Rad(fov)
	local w = math.tan(fov/2)
	local h = w/aspect
	w = 1/w
	h = 1/h
	local a = (n+f)/(n-f)
	local b = 2*n*f/(n-f)
	return mkmat4({{ w, 0, 0, 0 },
	               { 0, h, 0, 0 },
	               { 0, 0, a, b },
	               { 0, 0, -1, 0 }})
end

function mkmat4(mat)
	local m = mat4(0)
	for i = 1,4 do
		for j = 1,4 do
			m[i][j] = mat[j][i]
		end
	end
	return m
end



quattable = debug.getregistry()["Quat"]

function quattable.__tostring(v)
	return "H(" .. v.w .. ", " .. v.x .. ", " .. v.y .. ", " .. v.z .. ")"
end

function quattable.__concat(a, b)
	return tostring(a) .. tostring(b)
end

function quattable.neg(a)
	return quat(-a.w, -a.x, -a.y, -a.z)
end

function quattable.add(a, b)
	return quat(a.w+b.w, a.x+b.x, a.y+b.y, a.z+b.z)
end

function quattable.sub(a, b)
	return quat(a.w-b.w, a.x-b.x, a.y-b.y, a.z-b.z)
end

function quattable.scale(a, s)
	return quat(s*a.w, s*a.x, s*a.y, s*a.z)
end

function quattable.conj(a)
	return quat(a.w, -a.x, -a.y, -a.z)
end

function quattable.mul(a, b)
	return quat(a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
	            a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
	            a.w*b.y + a.y*b.w + a.z*b.x - a.x*b.z,
	            a.w*b.z + a.z*b.w + a.x*b.y - a.y*b.x)
end

function quattable.inner(a, b)
	return a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z
end

function quattable.scalar(a, b)
	return a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
end

function quattable.normsq(a)
	return a:inner(a)
end

function quattable.norm(a)
	return math.sqrt(a:normsq())
end

function quattable.inv(a)
	return a:conj():scale(1/a:normsq())
end

function quattable.normalized(a)
	return a:scale(1/a:norm())
end

function quattable.sandwich(q, v)
	return q:mul(v:mul(q:conj()))
end

function quattable.sandwichVec3(q, v)
	return q:mul(v:quat():mul(q:conj())):vec3()
end

function quattable.__add(a, b)
	return a:add(b)
end

function quattable.__sub(a, b)
	return a:sub(b)
end

function quattable.__unm(a)
	return a:neg()
end

function quattable.__mul(a, b)
	if type(a) == "number" then
		return b:scale(a)
	elseif type(b) == "number" then
		return a:scale(b)
	else
		return a:mul(b)
	end
end

function quattable.__div(a, b)
	if type(b) == "number" then
		return a:scale(1/b)
	else
		return a:mul(b:inv())
	end
end

function quattable.vec3(q)
	return vec3(q.x, q.y, q.z)
end

function quattable.vec4(q)
	return vec4(q.x, q.y, q.z, q.w)
end

function rotor(angle, axis)
	local c = math.cos(angle)
	local s = math.sin(angle)
	return quat(c, s*axis.x, s*axis.y, s*axis.z)
end
