#!/usr/bin/env lua
--[[
xan2dsm.lua -- an .xan animation list as dvp-as source, to be linked
into a chunk.  The companion of tools/xm2dsm.lua, and much simpler:
an xAnimList is nothing but structs and key arrays, no DMA chains, no
pipelines, no textures, so there are no `global' entries either.

	lua tools/xan2dsm.lua [-name SYM] [-clips A,B,C] [-clip A] [-link] anims.xan > anims.dsm
	ee-dvp-as -Itools anims.dsm -o anims.o
	ee-ld -T tools/chk.ld -o anims.chk anims.o

Without -clips/-clip every clip in the file is written, in file order;
with them only the named ones, in the order they are named -- the same
cut tools/xancut.py makes on the text, so a demo can carry the clips it
plays and nothing else.  (samples/ is a submodule: the cut belongs
here, not in the asset tree.)

Writes the structures of common/xmodel.h in the PS2 layout (32 bit
pointers), every pointer through the `ptr' macro of tools/chunk.inc so
the linker collects the fixups:

	xAnimList     8	 { int numAnims; xAnimation *anims; }
	xAnimation   16	 { char *name; float duration;
	                   int numChannels; xAnimChannel *channels; }
	xAnimChannel 32	 { char *name; int id;
	                   int numRotKeys, numTransKeys, numScaleKeys;
	                   xRotKey *rotKeys; xVecKey *transKeys, *scaleKeys; }
	xRotKey      20	 { float time; Quat rot; }	 (x y z w)
	xVecKey      16	 { float time; Vec3 v; }

Key arrays are qword aligned because that is what the loader's blocks
were, and because the EE likes it.  `id' is the bone index the exporter
already resolved against the model's skeleton, or -1 for a channel that
drives a plain node (RigRoot); xAnimPlayerSetAnim falls back to a name
lookup for -1, so it has to be carried through verbatim.

Floats are written as decimal with enough digits to round trip, and
each one is checked to actually round trip before it goes out -- the
chunk has to be bit for bit what loadXAnimListText would have built,
or the two loads would not draw the same frame.
]]

local symName = nil
local linkFlag = false	-- -link: an object for the ELF, not a chunk file
local path = nil
local clipNames = nil		-- nil: every clip

local function wantClip(name)
	if not clipNames then return true end
	for _, n in ipairs(clipNames) do
		if n == name then return true end
	end
	return false
end

local args = {...}
local i = 1
while i <= #args do
	local a = args[i]
	if a == "-name" then symName = args[i+1]; i = i + 2
	elseif a == "-link" then linkFlag = true; i = i + 1
	elseif a == "-clip" then
		clipNames = clipNames or {}
		clipNames[#clipNames+1] = args[i+1]; i = i + 2
	elseif a == "-clips" then
		clipNames = clipNames or {}
		for n in args[i+1]:gmatch("[^,]+") do clipNames[#clipNames+1] = n end
		i = i + 2
	elseif a:sub(1, 1) == "-" then error("unknown option " .. a)
	else path = a; i = i + 1 end
end
if not path then
	io.stderr:write("usage: xan2dsm.lua [-name SYM] [-clips A,B,C] [-clip A] [-link] anims.xan > anims.dsm\n")
	os.exit(1)
end
if not symName then
	symName = path:match("([^/]+)%.xan$") or path:match("([^/]+)$")
	symName = symName:gsub("%W", "_")
end

-- ---- the .xan reader --------------------------------------------------------

-- the text format is one `anim "Name" duration numChannels' per clip,
-- then `channel "Name" id numRot numTrans numScale' and the keys
local function readXan(file, want)
	local anims = {}
	local anim, chan
	local skipping = false
	local f = assert(io.open(file), "can't read " .. file)
	for line in f:lines() do
		local c = line:match("^%s*(%S+)")
		if c == "anim" then
			local name = line:match('^%s*anim%s+"([^"]*)"')
			local dur, nch = line:match('"%s+(%S+)%s+(%S+)%s*$')
			skipping = not want(name)
			if skipping then
				anim = nil
			else
				anim = { name = name, duration = tonumber(dur),
				         numChannels = tonumber(nch), channels = {} }
				anims[#anims+1] = anim
			end
			chan = nil
		elseif skipping then
			-- nothing
		elseif c == "channel" then
			local name = line:match('^%s*channel%s+"([^"]*)"')
			local id, nr, nt, ns = line:match('"%s+(%-?%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s*$')
			chan = { name = name, id = tonumber(id),
			         numRot = tonumber(nr), numTrans = tonumber(nt),
			         numScale = tonumber(ns),
			         rot = {}, trans = {}, scale = {} }
			anim.channels[#anim.channels+1] = chan
		elseif c == "rot" then
			local t, x, y, z, w = line:match("^%s*rot%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
			chan.rot[#chan.rot+1] = { tonumber(t), tonumber(x), tonumber(y), tonumber(z), tonumber(w) }
		elseif c == "trans" then
			local t, x, y, z = line:match("^%s*trans%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
			chan.trans[#chan.trans+1] = { tonumber(t), tonumber(x), tonumber(y), tonumber(z) }
		elseif c == "scale" then
			local t, x, y, z = line:match("^%s*scale%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
			chan.scale[#chan.scale+1] = { tonumber(t), tonumber(x), tonumber(y), tonumber(z) }
		elseif c == "numAnimations" or c == nil then
			-- the count is implied by what we write, blank lines ignored
		else
			error("unknown command " .. c)
		end
	end
	f:close()
	-- sanity: the counts in the file are what we are about to write out
	for _, a in ipairs(anims) do
		assert(#a.channels == a.numChannels,
			string.format("%s: %d channels, header says %d", a.name, #a.channels, a.numChannels))
		for _, ch in ipairs(a.channels) do
			assert(#ch.rot == ch.numRot and #ch.trans == ch.numTrans and #ch.scale == ch.numScale,
				string.format("%s/%s: key count mismatch", a.name, ch.name))
		end
	end
	return anims
end

-- ---- the writer -------------------------------------------------------------

local out = {}
local function emit(s) out[#out+1] = s end
local function emitf(fmt, ...) out[#out+1] = string.format(fmt, ...) end

-- enough digits to round trip a single, and checked: the assembler has
-- to end up with the float the C loader's atof would have produced
local function fmtfloat(x)
	local s = string.format("%.9g", x)
	if not s:find("[%.eEn]") then s = s .. ".0" end
	assert(string.pack("<f", tonumber(s)) == string.pack("<f", x),
		"float does not round trip: " .. tostring(x))
	return s
end

local function floats(list)
	local t = {}
	for k, x in ipairs(list) do t[k] = fmtfloat(x) end
	return table.concat(t, ", ")
end

local function emitPtr(sym)
	if not sym then emit("\t.int 0")
	elseif linkFlag then emitf("\t.int %s", sym)
	else emitf("\tptr %s", sym) end
end

-- one string pool, deduplicated: 39 channel names repeated over every
-- clip are 39 strings, not 39 times the number of clips
local strings, stringList = {}, {}
local function str(s)
	if not strings[s] then
		local sym = string.format("%s_str%d", symName, #stringList)
		strings[s] = sym
		stringList[#stringList+1] = { sym = sym, s = s }
	end
	return strings[s]
end

local function emitRotKeys(sym, keys)
	emit("")
	emitf("; %s: xRotKey[%d], 20 bytes each: time, quaternion x y z w", sym, #keys)
	emit(".align 4")
	emitf("%s:", sym)
	for _, k in ipairs(keys) do
		emitf("\t.float %s", floats(k))
	end
end

local function emitVecKeys(sym, keys, what)
	emit("")
	emitf("; %s: xVecKey[%d], 16 bytes each: time, %s x y z", sym, #keys, what)
	emit(".align 4")
	emitf("%s:", sym)
	for _, k in ipairs(keys) do
		emitf("\t.float %s", floats(k))
	end
end

local function main()
	local anims = readXan(path, wantClip)
	if clipNames then
		-- keep the order the clips were named in, and complain about
		-- one that is not in the file rather than silently dropping it
		local byName = {}
		for _, a in ipairs(anims) do byName[a.name] = a end
		local sorted = {}
		for _, n in ipairs(clipNames) do
			sorted[#sorted+1] = assert(byName[n], "not in " .. path .. ": " .. n)
		end
		anims = sorted
	end
	assert(#anims > 0, "no clips")

	local nkeys, nchan = 0, 0
	for _, a in ipairs(anims) do
		nchan = nchan + #a.channels
		for _, c in ipairs(a.channels) do
			nkeys = nkeys + #c.rot + #c.trans + #c.scale
		end
	end

	emitf("; %s as an xtc %s, generated by tools/xan2dsm.lua from %s", symName,
		linkFlag and "object for the ELF" or "chunk", path)
	emitf("; %d clips, %d channels, %d keys", #anims, nchan, nkeys)
	emit("")
	if linkFlag then
		emit(".section .data")	-- see xm2dsm.lua
	else
		emit('.include "chunk.inc"')
		emit("chkheader")
		emit("")
		emit(".data")
	end
	emit(".align 4")
	emitf(".global %s", symName)
	emitf("%s:\t; xAnimList", symName)
	emitf("\t.int %d\t; numAnims", #anims)
	emitPtr(symName .. "_anims")

	emit("")
	emit(".align 2")
	emitf("%s_anims:\t; xAnimation[%d]", symName, #anims)
	for k, a in ipairs(anims) do
		emitf("\t; %d: \"%s\", %g seconds, %d channels", k-1, a.name, a.duration, #a.channels)
		emitPtr(str(a.name))
		emitf("\t.float %s\t; duration", fmtfloat(a.duration))
		emitf("\t.int %d\t; numChannels", #a.channels)
		emitPtr(string.format("%s_a%d_chans", symName, k-1))
	end

	for k, a in ipairs(anims) do
		local asym = string.format("%s_a%d", symName, k-1)
		emit("")
		emit(".align 2")
		emitf("%s_chans:\t; xAnimChannel[%d] of \"%s\"", asym, #a.channels, a.name)
		for j, c in ipairs(a.channels) do
			local csym = string.format("%s_c%d", asym, j-1)
			emitf("\t; %d: \"%s\", bone %d, %d/%d/%d rot/trans/scale keys",
				j-1, c.name, c.id, #c.rot, #c.trans, #c.scale)
			emitPtr(str(c.name))
			emitf("\t.int %d\t; id", c.id)
			emitf("\t.int %d, %d, %d\t; numRotKeys, numTransKeys, numScaleKeys",
				#c.rot, #c.trans, #c.scale)
			emitPtr(#c.rot > 0 and csym .. "_rot")
			emitPtr(#c.trans > 0 and csym .. "_trans")
			emitPtr(#c.scale > 0 and csym .. "_scale")
		end
		for j, c in ipairs(a.channels) do
			local csym = string.format("%s_c%d", asym, j-1)
			if #c.rot > 0 then emitRotKeys(csym .. "_rot", c.rot) end
			if #c.trans > 0 then emitVecKeys(csym .. "_trans", c.trans, "translation") end
			if #c.scale > 0 then emitVecKeys(csym .. "_scale", c.scale, "scale") end
		end
	end

	emit("")
	emitf("; strings, %d of them", #stringList)
	for _, s in ipairs(stringList) do
		emitf('%s:\t.asciz "%s"', s.sym, s.s)
	end
	emit(".align 4")
	emit("")
	io.write(table.concat(out, "\n"), "\n")
end

main()
