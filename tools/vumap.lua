-- vumap.lua -- the VU1 data memory as a microcode's equates lay it out
--
--	lua tools/vumap.lua src/vu1/stdPipe.dsm [vertexTop=0x2d0 ...]
--
-- Evaluates the .equ lines (with any overrides given), then prints the
-- regions it knows the names of -- the double-buffered input, the two
-- output buffers, the clip scratch, bone matrices, and every single
-- qword constant -- from 0 to 0x400, with the gaps between them.
local file = arg[1]
local over = {}
local prefix = ""
for i = 2, #arg do
	local n, v = arg[i]:match("^([%w_]+)=(.+)$")
	if n then over[n] = v
	elseif arg[i]:match("_$") then prefix = arg[i] end	-- a layout: std_ or skin_
end
if not file then io.stderr:write("usage: vumap.lua file.dsm [NAME=VALUE...]\n") os.exit(1) end


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

local eq = {}
local order = {}
readDefines(file, eq)
for n, v in pairs(eq) do order[#order+1] = n end
table.sort(order)
for line in io.lines(file) do
	line = line:gsub(";.*", "")
	local name, expr = line:match("^%s*%.equ%s+([%w_]+)%s*,%s*(.-)%s*$")
	if name then
		if over[name] then expr = over[name] end
		expr = expr:gsub("/", "//")
		local fn, err = load("return " .. expr, name, "t", eq)
		if fn then
			local ok, v = pcall(fn)
			if ok and type(v) == "number" then eq[name] = v; order[#order+1] = name end
		end
	end
end

-- the regions: name, start, size (qwords).  The clipping entry and the
-- plain one lay the same memory out differently, so two maps.
local named = { vertexTop=1, vertCount=1, offset=1, outBuf1=1, outBuf2=1, outSize=1,
	clipVertCount=1, clipOutSize=1, clipOutBuf2=1, clipBuf=1, boneMatrices=1,
	clipVertLimitTL=1, clipVertLimitTS=1, numInAttribs=1, numUnpackAttribs=1,
	numOutAttribs=1, numOutBuf=1 }
-- the layout's values under their plain names
for _, n in ipairs({ "vertexTop", "numUnpackAttribs", "vertCount", "offset", "outBuf1", "outBuf2", "outSize",
		"clipVertCount", "clipOutSize", "clipOutBuf2", "clipBuf", "clipVertLimitTL", "clipVertLimitTS" }) do
	if eq[prefix .. n] then eq[n] = eq[prefix .. n] end
end

local function regionsFor(mode)
	local regions = {}
	local function add(name, s, n) if s and n and n > 0 then regions[#regions+1] = { name = name, s = s, e = s + n } end end
	local vc, off = eq.vertCount, eq.offset
	local nIn = eq.numUnpackAttribs or eq.numInAttribs
	if vc and off then
		add("input buffer 0 (" .. vc .. " verts x " .. nIn .. ")", 0, off)
		add("input buffer 1", off, off)
	end
	if mode == "plain" then
		if eq.outBuf1 and eq.outSize then add("output buffer 1", eq.outBuf1, eq.outSize) end
		if eq.outBuf2 and eq.outSize then add("output buffer 2", eq.outBuf2, eq.outSize) end
	else
		if eq.clipOutBuf2 and eq.clipOutSize then
			add("clip output 1 (" .. (eq.clipVertCount or "?") .. " verts)", eq.outBuf1, eq.clipOutSize)
			add("clip output 2", eq.clipOutBuf2, eq.clipOutSize)
		end
		if eq.clipBuf then add("clip scratch", eq.clipBuf, (eq.vertexTop or 0x3d0) - eq.clipBuf) end
	end
	if eq.boneMatrices and eq.vertexTop and eq.vertexTop <= eq.boneMatrices then
		add("bone matrices (64 x 4)", eq.boneMatrices, 256)
	end
	local consts = {}
	for _, n in ipairs(order) do
		local v = eq[n]
		if not named[n] and not n:match("^std_") and not n:match("^skin_") and v >= (eq.vertexTop or 0x3d0) and v < 0x400 then consts[#consts+1] = { name = n, s = v } end
	end
	-- a block alias (STD_LIGHTS for STD_LIGHTDIRX) shares an address
	-- with its first member: keep the member, the longer name
	table.sort(consts, function(a, b) return a.s < b.s or (a.s == b.s and #a.name > #b.name) end)
	local uniq = {}
	for _, c in ipairs(consts) do
		if #uniq == 0 or uniq[#uniq].s ~= c.s then uniq[#uniq+1] = c end
	end
	consts = uniq
	for i, c in ipairs(consts) do
		local nxt = consts[i+1] and consts[i+1].s or 0x400
		add(c.name, c.s, math.min(nxt, 0x400) - c.s)
	end
	table.sort(regions, function(a, b) return a.s < b.s end)
	return regions
end

print(string.format("%s %s (vertexTop 0x%x, vertCount %s)", file, prefix, eq.vertexTop or 0, tostring(eq.vertCount)))
for _, mode in ipairs({ "plain", "clipping" }) do
	print(string.format("-- %s entry", mode))
	print(string.format("%-6s %-6s %6s  %s", "from", "to", "qwords", "what"))
	local pos = 0
	for _, r in ipairs(regionsFor(mode)) do
		if r.s > pos then print(string.format("%04x   %04x   %6d  -- gap", pos, r.s - 1, r.s - pos)) end
		if r.s < pos then print(string.format("                     (overlaps the previous by %d)", pos - r.s)) end
		print(string.format("%04x   %04x   %6d  %s", r.s, r.e - 1, r.e - r.s, r.name))
		pos = math.max(pos, r.e)
	end
	if pos < 0x400 then print(string.format("%04x   %04x   %6d  -- gap", pos, 0x3ff, 0x400 - pos)) end
end
