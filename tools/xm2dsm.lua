#!/usr/bin/env lua
--[[
xm2dsm.lua -- an .xm model as dvp-as source, to be linked into a chunk.

	lua tools/xm2dsm.lua [-pipes src/vu1] [-name SYM] [-strips model.strips]
	                     [-nogeo] model.xm > model.dsm
	ee-dvp-as -Itools model.dsm -o model.o
	ee-ld -T tools/chk.ld -o model.chk model.o

-strips takes the tri strips tools/xstrip wrote for the model and makes
the prim lists strips instead of lists.  -nogeo leaves the xGeometry
and the skin's per vertex data out: only the chains, the structures,
the inverse bone matrices; the bounding sphere then has nothing to
measure and comes out as the unit sphere.

Writes the xModel structures of common/xmodel.h as data (the PS2 layout:
32 bit pointers, Mat4 as 16 floats), every pointer through the `ptr'
macro of tools/chunk.inc so the linker collects the fixups, and each
mesh's prim list as a ref chain into per-attribute arrays for the std or
skin pipeline, whose batch size and input layout are read from the
microcode's own .dsm.  Pipelines and textures are `global' entries,
resolved by name when the chunk is loaded.

buildXModel leaves a mesh alone once it has a prim list, so nothing
rebuilds what the chunk brought.
]]

local pipesDir = "src/vu1"
local symName = nil
local path = nil
local stripsPath = nil
local noGeo = false

local args = {...}
local i = 1
while i <= #args do
	local a = args[i]
	if a == "-pipes" then pipesDir = args[i+1]; i = i + 2
	elseif a == "-name" then symName = args[i+1]; i = i + 2
	elseif a == "-strips" then stripsPath = args[i+1]; i = i + 2
	elseif a == "-nogeo" then noGeo = true; i = i + 1
	elseif a:sub(1, 1) == "-" then error("unknown option " .. a)
	else path = a; i = i + 1 end
end
if not path then
	io.stderr:write("usage: xm2dsm.lua [-pipes DIR] [-name SYM] [-strips F] [-nogeo] model.xm > model.dsm\n")
	os.exit(1)
end
if not symName then
	symName = path:match("([^/]+)%.xm$") or path:match("([^/]+)$")
	symName = symName:gsub("%W", "_")
end

-- ---- the microcode's batch layout ------------------------------------------

local USAGE = { XTCP_POSITION = "pos", XTCP_TEXCOORD = "uv", XTCP_COLOR = "rgba",
                XTCP_NORMAL = "normal", XTCP_SKINDATA = "skin" }
-- VIF UNPACK: command byte, bytes per vertex
local FORMATS = {
	UNPACK_V4_32 = { 0x6C, 16 }, UNPACK_V3_32 = { 0x68, 12 }, UNPACK_V2_32 = { 0x64, 8 },
	UNPACK_V4_16 = { 0x6D, 8 },  UNPACK_V3_16 = { 0x69, 6 },  UNPACK_V2_16 = { 0x65, 4 },
	UNPACK_V4_8  = { 0x6E, 4 },  UNPACK_V3_8  = { 0x6A, 3 },
}


-- #define NAME VALUE lines of the files a microcode #includes (next to
-- it), for layouts that live in a header the assembler sees through cpp
local function readDefines(file, eq)
	local dir = file:match("^(.*)/[^/]*$") or "."
	for line in io.lines(file) do
		local inc = line:match('^%s*#include%s+"([^"]+)"')
		if inc then
			local f = io.open(dir .. "/" .. inc)
			if f then
				for l in f:lines() do
					local name, val = l:match("^%s*#define%s+([%w_]+)%s+([%w_x]+)")
					if name then
						local v = tonumber(val) or eq[val]
						if v then eq[name] = v end
					end
				end
				f:close()
			end
		end
	end
end

-- the .equ lines (C expressions over earlier equates) and the layout's
-- input descriptor.  A microcode holds its layouts as prefixed equates
-- (std_vertCount, skin_vertCount) and descriptors (std_inputDesc:);
-- prefix picks one.  An unprefixed file is its own single layout.
local function readPipeline(file, prefix)
	prefix = prefix or ""
	local eq = {}
	readDefines(file, eq)
	local attribs = {}
	local stride
	local inblock = prefix == ""
	local f = assert(io.open(file), "no microcode " .. file)
	for line in f:lines() do
		line = line:gsub(";.*", "")
		local name, expr = line:match("^%s*%.equ%s+([%w_]+)%s*,%s*(.-)%s*$")
		if name then
			expr = expr:gsub("/", "//")
			local fn = assert(load("return " .. expr, name, "t", eq))
			eq[name] = fn()
		end
		if prefix ~= "" then
			local label = line:match("^([%w_]+)inputDesc:")
			if label then inblock = label == prefix end
		end
		local usage, off, fmt = line:match("^%s*%.word%s+(XTCP_%w+)%s*,%s*(%d+)%s*,%s*(.-)%s*$")
		if usage and inblock then
			local usn = fmt:find("UNPACK_USN") ~= nil
			local fmtname = fmt:match("UNPACK_V%d_%d+")
			local cmd, size = table.unpack(FORMATS[fmtname])
			attribs[#attribs+1] = {
				usage = USAGE[usage], offset = tonumber(off),
				cmd = cmd, size = size, usn = usn,
				-- the immediate: TOPS-relative address, unsigned flag
				imm = 0x8000 | (usn and 0x4000 or 0) | tonumber(off),
				name = fmtname:sub(8),
			}
		end
		local s, n = line:match("^%s*%.word%s+(%d+)%s*,%s*(%d+)%s*$")
		if s and inblock and not stride then stride = tonumber(s) end
	end
	f:close()
	local vertCount = assert(eq[prefix .. "vertCount"], "no " .. prefix .. "vertCount in " .. file)
	-- quantized positions and texcoords want the dequantization
	-- constants: three qwords, xyz scale, xyz offset, uv scale, at the
	-- address the microcode names
	eq.unXYZScale = eq.unXYZScale or eq.STD_UNXYZSCALE
	eq.unXYZOff = eq.unXYZOff or eq.STD_UNXYZOFF
	eq.unUVScale = eq.unUVScale or eq.STD_UNUVSCALE
	local quant = false
	for _, at in ipairs(attribs) do
		-- V4_16/V3_16 positions, V2_16 texcoords
		if (at.usage == "pos" and at.size <= 8) or (at.usage == "uv" and at.size <= 4) then quant = true end
	end
	if quant then
		assert(eq.unXYZScale and eq.unXYZOff == eq.unXYZScale + 1 and eq.unUVScale == eq.unXYZScale + 2,
			file .. ": quantized input wants .equ unXYZScale, unXYZOff, unUVScale, in that order")
	end
	return {
		file = file .. (prefix ~= "" and " (" .. prefix:sub(1, -2) .. ")" or ""),
		stride = stride, attribs = attribs, vertCount = vertCount,
		quant = quant, unAddr = eq.unXYZScale,
		-- the microcode footer's numVerts: a tri list batch is a multiple
		-- of 4 and of 3, a strip batch a multiple of 4 plus the 2 repeated
		triListVerts = ((vertCount // 3) & ~3) * 3,
		triStripVerts = (((vertCount & ~3) - 2) & ~3) + 2,
	}
end

-- ---- the .xm reader ---------------------------------------------------------

local function tokenize(line)
	local t = {}
	for tok in line:gmatch('"[^"]*"') do end	-- (quoted names below)
	local pos = 1
	while true do
		local s, e = line:find("%S+", pos)
		if not s then break end
		if line:sub(s, s) == '"' then
			local q = line:find('"', s + 1, true)
			t[#t+1] = line:sub(s + 1, q - 1)
			pos = q + 1
		else
			t[#t+1] = line:sub(s, e)
			pos = e + 1
		end
	end
	return t
end

local function readMatrix(t, base)
	local m = {}
	for k = 1, 12 do m[k] = tonumber(t[base + k - 1]) end
	return m
end

local function readXm(file)
	local mdl = { materials = {}, meshes = {}, root = nil }
	local mat, mesh, node
	local f = assert(io.open(file), "can't read " .. file)
	for line in f:lines() do
		local t = tokenize(line)
		local c = t[1]
		if not c then goto continue end
		if c == "numMaterials" then mdl.numMaterials = tonumber(t[2])
		elseif c == "numMeshes" then
			if node then node.meshes = {} else mdl.numMeshes = tonumber(t[2]) end
		elseif c == "material" then
			if mesh then mesh.material = tonumber(t[2])
			else
				mat = { ambient = {1,1,1,1}, diffuse = {1,1,1,1}, specular = {1,1,1,0},
				        emissive = {0,0,0,0}, colormaterial = 1 }
				mdl.materials[tonumber(t[2]) + 1] = mat
			end
		elseif c == "endmaterial" then mat = nil
		elseif c == "ambient" or c == "diffuse" or c == "specular" or c == "emissive" then
			mat[c] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]), tonumber(t[5]) }
		elseif c == "shininess" then mat.specular[4] = tonumber(t[2])
		elseif c == "colormaterial" then mat.colormaterial = tonumber(t[2])
		elseif c == "diffusetex" then mat.tex = t[2]
		elseif c == "mesh" then
			if node then node.meshes[#node.meshes+1] = tonumber(t[2])
			else
				mesh = { v = {}, n = {}, t = {}, c = {}, f = {} }
				mdl.meshes[tonumber(t[2]) + 1] = mesh
			end
		elseif c == "endmesh" then mesh = nil
		elseif c == "numVerts" then mesh.numVerts = tonumber(t[2])
		elseif c == "numFaces" then mesh.numFaces = tonumber(t[2])
		elseif c == "v" then mesh.v[#mesh.v+1] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]) }
		elseif c == "n" then mesh.n[#mesh.n+1] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]) }
		elseif c == "t" then mesh.t[#mesh.t+1] = { tonumber(t[2]), tonumber(t[3]) }
		elseif c == "c" then mesh.c[#mesh.c+1] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]), tonumber(t[5]) }
		elseif c == "f" then mesh.f[#mesh.f+1] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]) }
		elseif c == "skin" then mesh.skin = { numBones = tonumber(t[2]), w = {}, i = {}, inv = {} }
		elseif c == "w" then
			local s = mesh.skin
			s.w[#s.w+1] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]), tonumber(t[5]) }
		elseif c == "i" then
			local s = mesh.skin
			s.i[#s.i+1] = { tonumber(t[2]), tonumber(t[3]), tonumber(t[4]), tonumber(t[5]) }
		elseif c == "invmat" then
			local s = mesh.skin
			s.inv[#s.inv+1] = readMatrix(t, 2)
		elseif c == "node" then
			local n = { name = t[2], tag = -1, children = {}, meshes = {}, parent = node }
			if node then node.children[#node.children+1] = n else mdl.root = n end
			node = n
		elseif c == "tag" then node.tag = tonumber(t[2])
		elseif c == "xform" then node.xform = readMatrix(t, 2)
		elseif c == "endnode" then node = node.parent
		elseif c == "skeleton" then
			node.skel = { numBones = tonumber(t[2]), bones = {} }
			mdl.skel = node.skel
			mdl.skelNode = node
		elseif c == "bone" then
			local b = node.skel.bones
			b[#b+1] = { name = t[2], flag = tonumber(t[3]), tag = tonumber(t[4]), m = readMatrix(t, 5) }
		else
			error("unknown command " .. c)
		end
		::continue::
	end
	f:close()
	return mdl
end

-- the strips tools/xstrip wrote: `mesh N COUNT' then the indices
local function readStrips(file)
	local strips = {}
	local cur
	local f = assert(io.open(file), "can't read " .. file)
	for line in f:lines() do
		local n, count = line:match("^mesh (%d+) (%d+)")
		if n then
			cur = {}
			strips[tonumber(n) + 1] = cur
		elseif cur then
			for tok in line:gmatch("%d+") do cur[#cur+1] = tonumber(tok) end
		end
	end
	f:close()
	return strips
end

-- ---- the writer -------------------------------------------------------------

local out = {}
local function emit(s) out[#out+1] = s end
local function emitf(fmt, ...) out[#out+1] = string.format(fmt, ...) end

local function fmtfloat(x)
	-- enough digits to round trip a single
	local s = string.format("%.9g", x)
	if not s:find("[%.eEn]") then s = s .. ".0" end
	return s
end

local function floats(list)
	local t = {}
	for k, x in ipairs(list) do t[k] = fmtfloat(x) end
	return table.concat(t, ", ")
end

-- a Mat4 from the 12 numbers of the text format: three columns and
-- the translation, w padded like readXMatrix does
local function emitMatrix(m)
	emitf("\t.float %s, 0.0", floats({ m[1], m[2], m[3] }))
	emitf("\t.float %s, 0.0", floats({ m[4], m[5], m[6] }))
	emitf("\t.float %s, 0.0", floats({ m[7], m[8], m[9] }))
	emitf("\t.float %s, 1.0", floats({ m[10], m[11], m[12] }))
end

local function emitPtr(sym)
	if sym then emitf("\tptr %s", sym) else emit("\t.int 0") end
end

-- one string pool, deduplicated
local strings, stringList = {}, {}
local function str(s)
	if not strings[s] then
		local sym = string.format("%s_str%d", symName, #stringList)
		strings[s] = sym
		stringList[#stringList+1] = { sym = sym, s = s }
	end
	return strings[s]
end

local function label(sym, what)
	emit("")
	emitf("%s:\t; %s", sym, what)
end

-- the skin pipe's input: the four weights as floats, the low byte of
-- each replaced by 4*index (xtcpipe.c does the same at vertex kick)
local function packSkin(w, idx)
	local words = {}
	for k = 1, 4 do
		local bits = string.unpack("<I4", string.pack("<f", w[k]))
		bits = (bits & ~0x3FF) | ((idx[k] << 2) & 0xFF)
		words[k] = string.format("0x%08X", bits)
	end
	return "\t.int " .. table.concat(words, ", ")
end

-- quantization of a mesh: pos = q*xyzScale + xyzOff, uv = q*uvScale,
-- q a signed 16 bit integer.  the offset is the centre of the bounds
-- and the scale the half range over 32767, per axis; texcoords keep
-- their origin, the scale is the largest magnitude over 32767
local function quantize(mesh)
	local lo, hi = { 1e30, 1e30, 1e30 }, { -1e30, -1e30, -1e30 }
	local uvmax = 0
	for _, v in ipairs(mesh.v) do
		for k = 1, 3 do
			if v[k] < lo[k] then lo[k] = v[k] end
			if v[k] > hi[k] then hi[k] = v[k] end
		end
	end
	for _, t in ipairs(mesh.t) do
		uvmax = math.max(uvmax, math.abs(t[1]), math.abs(t[2]))
	end
	local q = { off = {}, scale = {}, uvscale = math.max(uvmax, 1e-6) / 32767 }
	for k = 1, 3 do
		q.off[k] = (lo[k] + hi[k]) / 2
		q.scale[k] = math.max((hi[k] - lo[k]) / 2, 1e-6) / 32767
	end
	return q
end

local function q16(x)
	x = math.floor(x + 0.5)
	if x > 32767 then x = 32767 elseif x < -32768 then x = -32768 end
	return x
end

-- one attribute of one vertex as a data line
local function dataLine(attrib, mesh, vi)
	local u = attrib.usage
	if u == "pos" then
		local v = mesh.v[vi]
		if attrib.size <= 8 then
			local q = mesh.quant
			local a = q16((v[1] - q.off[1]) / q.scale[1])
			local b = q16((v[2] - q.off[2]) / q.scale[2])
			local c = q16((v[3] - q.off[3]) / q.scale[3])
			if attrib.size == 8 then return string.format("\t.short %d, %d, %d, 0", a, b, c) end
			return string.format("\t.short %d, %d, %d", a, b, c)
		end
		return "\t.float " .. floats({ v[1], v[2], v[3], 0 })
	elseif u == "uv" then
		local t = mesh.t[vi] or { 0, 0 }
		if attrib.size == 4 then
			local q = mesh.quant
			return string.format("\t.short %d, %d", q16(t[1] / q.uvscale), q16(t[2] / q.uvscale))
		end
		return "\t.float " .. floats(t)
	elseif u == "rgba" then
		local c = mesh.c[vi] or { 255, 255, 255, 255 }
		return string.format("\t.byte %d, %d, %d, %d", c[1], c[2], c[3], c[4])
	elseif u == "normal" then
		local n = mesh.n[vi] or { 0, 0, 1 }
		local b = {}
		for k = 1, 3 do
			-- (int)(n*127) like xtcNormal: truncated towards zero
			local x = n[k] * 127.0
			x = x >= 0 and math.floor(x) or math.ceil(x)
			if x > 127 then x = 127 elseif x < -127 then x = -127 end
			b[k] = x
		end
		return string.format("\t.byte %d, %d, %d", b[1], b[2], b[3])
	elseif u == "skin" then
		return packSkin(mesh.skin.w[vi], mesh.skin.i[vi])
	end
	error("attribute " .. u)
end

-- the prim list: a tri list of the mesh's faces, batched like
-- xtcpGetBatchInfo does, the chain refs into per-attribute arrays.
-- each batch's slice of an array is padded to a qword so any batch size
-- works (the arrays are per pipeline anyway, they live in the chunk).
local function emitPrimList(sym, mesh, pipe, strip)
	local verts = {}
	if pipe.quant then mesh.quant = quantize(mesh) end
	if strip then
		for _, i in ipairs(strip) do verts[#verts+1] = i + 1 end
	else
		for _, f in ipairs(mesh.f) do
			verts[#verts+1] = f[1] + 1
			verts[#verts+1] = f[2] + 1
			verts[#verts+1] = f[3] + 1
		end
	end
	local n = #verts
	-- xtcpGetBatchInfo: a strip batch repeats the last two vertices of
	-- the one before, a list batch holds whole triangles
	local repeat_ = strip and 2 or 0
	local batchSize = strip and pipe.triStripVerts or pipe.triListVerts
	local numPrims = strip and n - 2 or n // 3
	local primsPerBatch = strip and batchSize - 2 or batchSize // 3
	local numBatches = (numPrims + primsPerBatch - 1) // primsPerBatch
	assert(numBatches > 0, "empty mesh")
	local last = n - (numBatches - 1) * (batchSize - repeat_)

	-- slice offsets per attribute, qword padded
	local offsets = {}
	for a = 1, #pipe.attribs do offsets[a] = {} end
	local base = 0
	local batches = {}
	for b = 1, numBatches do
		local count = b == numBatches and last or batchSize
		batches[b] = { base = base, count = count }
		base = base + count - repeat_
	end
	for a, at in ipairs(pipe.attribs) do
		local off = 0
		for b = 1, numBatches do
			offsets[a][b] = off
			off = off + ((batches[b].count * at.size + 15) & ~15)
		end
	end

	emitf("")
	emitf("; prim list for %s: tri %s, %d vertices in batches of %d",
		pipe.file:match("[^/]+$"), strip and "strip" or "list", n, batchSize)
	emitf("; input: stride %d, %s", pipe.stride, (function()
		local t = {}
		for _, at in ipairs(pipe.attribs) do
			t[#t+1] = string.format("%s %s%s @%d", at.usage, at.name, at.usn and "u" or "", at.offset)
		end
		return table.concat(t, ", ")
	end)())
	emit(".align 4")
	emitf("%s:", sym)
	if pipe.quant then
		local q = mesh.quant
		emit("; dequantization constants for this list: pos = q*scale + off, uv = q*uvscale")
		emit("DMAcnt *")
		emit("stcycl 1, 1")
		emitf("unpack V4_32, 0x%x, *", pipe.unAddr)
		emitf("\t.float %s, 0.0\t; xyz scale", floats(q.scale))
		emitf("\t.float %s, 0.0\t; xyz offset", floats(q.off))
		emitf("\t.float %s, %s, 0.0, 0.0\t; uv scale", fmtfloat(q.uvscale), fmtfloat(q.uvscale))
		emit(".EndUnpack")
		emit(".EndDmaData")
	end
	for b, bt in ipairs(batches) do
		local lastBatch = b == numBatches
		emitf("; batch %d: vertices %d..%d", b - 1, bt.base, bt.base + bt.count - 1)
		for a, at in ipairs(pipe.attribs) do
			local qwc = (bt.count * at.size + 15) // 16
			emitf("chkref %d, %s_%s+%d", qwc, sym, at.usage, offsets[a][b])
			emit(a == 1 and string.format("stcycl 1, %d", pipe.stride) or "vifnop")
			emitf("unpackref 0x%02X, 0x%04X, %d\t; unpack[r%s] %s, %d",
				at.cmd, at.imm, bt.count, at.usn and "u" or "", at.name, at.offset)
		end
		emit(lastBatch and "DMAret *" or "DMAcnt *")
		emitf("itop %d", bt.count)
		emit(b == 1 and "mscalf 0" or "mscnt")
		if lastBatch then emit("flush") end
		emit(".EndDmaData")
	end
	for a, at in ipairs(pipe.attribs) do
		emit("")
		emitf("; %s, %s, %d bytes per vertex", at.usage, at.name, at.size)
		emit(".align 4")
		emitf("%s_%s:", sym, at.usage)
		for b, bt in ipairs(batches) do
			for k = bt.base + 1, bt.base + bt.count do
				emit(dataLine(at, mesh, verts[k]))
			end
			emit(".align 4")
		end
	end
end

local function emitMaterial(sym, mat, k)
	label(sym, string.format("xMaterial %d", k - 1))
	emitf("\t.float %s\t; emissive", floats(mat.emissive))
	emitf("\t.float %s\t; ambient", floats(mat.ambient))
	emitf("\t.float %s\t; diffuse", floats(mat.diffuse))
	emitf("\t.float %s\t; specular, w is the power", floats(mat.specular))
	emitf("\t.int %d\t; colorMaterial", mat.colormaterial)
	emitPtr(mat.tex and sym .. "_tex")
	emit(".align 4")
	if mat.tex then
		label(sym .. "_tex", "xTexture")
		emitPtr(str(mat.tex))
		emitf('\tglobal "texture", "%s"', mat.tex)
		emit(".align 4")
	end
end

local function emitMesh(sym, mesh, k, pipes, strip)
	local skinned = mesh.skin ~= nil
	label(sym, string.format("xMesh %d", k - 1))
	emitPtr(sym .. "_prims")
	emitPtr(string.format("%s_mat%d", symName, mesh.material))
	emitPtr(not noGeo and sym .. "_geo")
	emitPtr(skinned and sym .. "_skin")

	label(sym .. "_prims", "xtcPrimList")
	emitf('\tglobal "pipeline", "%s"', skinned and "skin" or "std")
	emit(strip and "\t.int 4\t; XTC_TRISTRIP" or "\t.int 3\t; XTC_TRILIST")
	emit("\t.int 0\t; size, only the runtime's dumps want it")
	emitPtr(sym .. "_chain")
	-- what the data asks of the pipeline: XTCP_ST_ bits of xtci.h
	local quant = (skinned and pipes.skin or pipes.std).quant
	emitf("\t.int %d\t; stages: %s", (quant and 1 or 0) + (skinned and 2 or 0),
		(quant and "dequantize " or "") .. (skinned and "skin" or "none"))

	if not noGeo then
		label(sym .. "_geo", "xGeometry")
		emitf("\t.int %d, %d\t; numVertices, numIndices", #mesh.v, 3 * #mesh.f)
		emitPtr(sym .. "_verts")
		emitPtr(sym .. "_indices")

		emit("")
		emit(".align 4")
		emitf("%s_verts:\t; xVertex[]: vtx, nrm, tex, col", sym)
		for vi = 1, #mesh.v do
			local v, n, t, c = mesh.v[vi], mesh.n[vi] or {0,0,1}, mesh.t[vi] or {0,0}, mesh.c[vi] or {255,255,255,255}
			emitf("\t.float %s,  %s,  %s", floats(v), floats(n), floats(t))
			emitf("\t.byte %d, %d, %d, %d", c[1], c[2], c[3], c[4])
		end
		emit("")
		emit(".align 4")
		emitf("%s_indices:", sym)
		for _, f in ipairs(mesh.f) do
			emitf("\t.int %d, %d, %d", f[1], f[2], f[3])
		end
	end

	if skinned then
		local s = mesh.skin
		label(sym .. "_skin", "xSkin")
		emitf("\t.int %d\t; numBones", s.numBones)
		emitPtr(sym .. "_invmat")
		emitPtr(not noGeo and sym .. "_skinidx")
		emitPtr(not noGeo and sym .. "_skinwt")
		emit("")
		emit(".align 4")
		emitf("%s_invmat:\t; Mat4[]", sym)
		for _, m in ipairs(s.inv) do emitMatrix(m) end
		if not noGeo then
			emit("")
			emit(".align 4")
			emitf("%s_skinidx:\t; uint8[4] per vertex", sym)
			for _, x in ipairs(s.i) do emitf("\t.byte %d, %d, %d, %d", x[1], x[2], x[3], x[4]) end
			emit("")
			emit(".align 4")
			emitf("%s_skinwt:\t; float[4] per vertex", sym)
			for _, w in ipairs(s.w) do emitf("\t.float %s", floats(w)) end
		end
	end

	emitPrimList(sym .. "_chain", mesh, skinned and pipes.skin or pipes.std, strip)
end

-- nodes get numbered in file order; bones refer to them by name
local nodeSyms = {}
local nodeList = {}
local function numberNodes(node)
	nodeList[#nodeList+1] = node
	node.sym = string.format("%s_node%d", symName, #nodeList - 1)
	nodeSyms[node.name] = node.sym
	for _, c in ipairs(node.children) do numberNodes(c) end
end

local function emitNode(node)
	local parent = node.parent
	local child = node.children[1]
	local next = nil
	if parent then
		for k, c in ipairs(parent.children) do
			if c == node then next = parent.children[k+1] end
		end
	end
	label(node.sym, string.format('xNode "%s"', node.name))
	emitMatrix(node.xform or { 1,0,0, 0,1,0, 0,0,1, 0,0,0 })
	emitPtr(str(node.name))
	emitPtr(parent and parent.sym)
	emitPtr(child and child.sym)
	emitPtr(next and next.sym)
	emitPtr(#node.meshes > 0 and node.sym .. "_meshes")
	emitf("\t.int %d\t; numMeshes", #node.meshes)
	emit("\t.int 0\t; hidden")
	emitPtr(node.skel and node.sym .. "_skel")
	emitf("\t.int %d\t; tag", node.tag)
	emit(".align 4")
	if #node.meshes > 0 then
		label(node.sym .. "_meshes", "xMesh *[]")
		for _, m in ipairs(node.meshes) do
			emitPtr(string.format("%s_mesh%d", symName, m))
		end
		emit(".align 4")
	end
	if node.skel then
		local s = node.skel
		local sym = node.sym .. "_skel"
		label(sym, "xSkeleton")
		emitf("\t.int %d\t; numBones", s.numBones)
		emitPtr(sym .. "_bones")
		emitPtr(sym .. "_matrices")
		emit(".align 4")
		label(sym .. "_bones", "xBone[]: flag, tag, node")
		for _, b in ipairs(s.bones) do
			emitf("\t.int %d, %d", b.flag, b.tag)
			emitPtr(assert(nodeSyms[b.name], "bone without node: " .. b.name))
		end
		emit(".align 4")
		label(sym .. "_matrices", "Mat4[]")
		for _, b in ipairs(s.bones) do emitMatrix(b.m) end
	end
end

local function main()
	local pipes = {
		std = readPipeline(pipesDir .. "/stdPipe.dsm", "std_"),
		skin = readPipeline(pipesDir .. "/stdPipe.dsm", "skin_"),
	}
	local mdl = readXm(path)
	local strips = stripsPath and readStrips(stripsPath) or {}
	numberNodes(mdl.root)

	emitf("; %s as an xtc chunk, generated by tools/xm2dsm.lua from %s%s%s", symName, path,
		stripsPath and " with strips from " .. stripsPath or "", noGeo and ", no geometry" or "")
	emitf("; %d materials, %d meshes, %d nodes%s", #mdl.materials, #mdl.meshes, #nodeList,
		mdl.skel and string.format(", %d bones", mdl.skel.numBones) or "")
	emit("")
	emit('.include "chunk.inc"')
	emit("chkheader")
	emit("")
	emit(".data")
	emit(".align 4")
	emitf(".global %s", symName)
	emitf("%s:\t; xModel", symName)
	emitf("\t.int %d, %d\t; numMeshes, numMaterials", #mdl.meshes, #mdl.materials)
	emitPtr(symName .. "_meshes")
	emitPtr(symName .. "_materials")
	emitPtr(mdl.root.sym)
	emitPtr(mdl.skel and mdl.skelNode.sym .. "_skel")
	emit(".align 4")

	label(symName .. "_meshes", "xMesh *[]")
	for k = 1, #mdl.meshes do emitPtr(string.format("%s_mesh%d", symName, k - 1)) end
	emit(".align 4")
	label(symName .. "_materials", "xMaterial *[]")
	for k = 1, #mdl.materials do emitPtr(string.format("%s_mat%d", symName, k - 1)) end
	emit(".align 4")

	for k, mat in ipairs(mdl.materials) do
		emitMaterial(string.format("%s_mat%d", symName, k - 1), mat, k)
	end
	for _, node in ipairs(nodeList) do emitNode(node) end
	for k, mesh in ipairs(mdl.meshes) do
		emitMesh(string.format("%s_mesh%d", symName, k - 1), mesh, k, pipes, strips[k])
	end

	emit("")
	emit("; strings")
	for _, s in ipairs(stringList) do
		emitf('%s:\t.asciz "%s"', s.sym, s.s)
	end
	emit(".align 4")
	emit("")
	io.write(table.concat(out, "\n"), "\n")
end

main()
