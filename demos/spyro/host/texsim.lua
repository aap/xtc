#!/usr/bin/env lua
--[[
texsim.lua -- what would a texture cache have had to do?

	lua demos/spyro/host/texsim.lua [-pages N] [-sorted] trace.txt

trace.txt is the console's log with the lines the `trace' argument of
demos/spyro prints: `bind <pointer> <pages>' for every texture bind of
one frame, between `bind frame' and `bind end'.  The frame is taken as
the steady state: it repeats, and a cache that holds the working set
uploads nothing after the first time round.

Policies, all placing textures at page granularity in an arena of
-pages GS pages (default 89, what is left beside 640x448 buffers):

	reset	what src/xtctex.c does today: hand pages out in order
		until they run out, then forget everything and start over
	fifo	evict the oldest resident texture until the new one fits
	lru	evict the least recently bound
	belady	evict the one bound furthest in the future (the optimum,
		needs the future, so a bound to compare against)

-sorted runs the same binds sorted by texture, which is what sorting
the draws by material would give the cache.

The numbers per policy: uploads per frame in the steady state, and the
pages those uploads move.
]]

local pages = 89
local sorted = false
local path
local args = {...}
local i = 1
while i <= #args do
	if args[i] == "-pages" then pages = tonumber(args[i+1]); i = i + 2
	elseif args[i] == "-sorted" then sorted = true; i = i + 1
	else path = args[i]; i = i + 1 end
end
if not path then
	io.stderr:write("usage: texsim.lua [-pages N] [-sorted] trace.txt\n")
	os.exit(1)
end

-- the binds of one frame: a list of texture ids, and each texture's size
local binds, size = {}, {}
local names = {}
local inframe = false
for line in io.lines(path) do
	line = line:gsub("^%[%s*[%d.]+%]%s*", "")	-- pcsx2's timestamps
	if line:match("^bind frame") then inframe = true
	elseif line:match("^bind end") then inframe = false
	elseif inframe then
		local ptr, n = line:match("^bind (%S+) (%d+)")
		if ptr then
			if not names[ptr] then
				names[ptr] = #size + 1
				size[#size+1] = tonumber(n)
			end
			binds[#binds+1] = names[ptr]
		end
	end
end
if #binds == 0 then
	io.stderr:write("no binds in " .. path .. "\n")
	os.exit(1)
end
if sorted then table.sort(binds) end

local total = 0
for _, s in ipairs(size) do total = total + s end
print(string.format("%d binds of %d textures, %d pages in all, arena %d pages%s",
	#binds, #size, total, pages, sorted and ", binds sorted" or ""))

-- run a policy over the frame repeated, count the third time round
local function run(policy)
	local resident = {}	-- id -> true
	local used = 0
	local order = {}	-- ids in the policy's order (oldest first)
	local uploads, moved = 0, 0
	local rounds = 3
	local seq = {}
	for r = 1, rounds do for _, b in ipairs(binds) do seq[#seq+1] = b end end
	local counting = false

	local function evictIndex(pos)
		if policy == "fifo" or policy == "lru" then return 1 end
		-- belady: the resident texture whose next use is furthest away
		local far, faridx = -1, 1
		for k, id in ipairs(order) do
			local nxt = math.huge
			for p = pos + 1, #seq do
				if seq[p] == id then nxt = p; break end
			end
			if nxt > far then far, faridx = nxt, k end
		end
		return faridx
	end

	for pos, id in ipairs(seq) do
		if pos > 2 * #binds then counting = true end
		if resident[id] then
			if policy == "lru" then
				for k, v in ipairs(order) do
					if v == id then table.remove(order, k); break end
				end
				order[#order+1] = id
			end
		else
			if policy == "reset" then
				if used + size[id] > pages then
					resident, order, used = {}, {}, 0
				end
			else
				while used + size[id] > pages and #order > 0 do
					local k = evictIndex(pos)
					local victim = table.remove(order, k)
					resident[victim] = nil
					used = used - size[victim]
				end
			end
			resident[id] = true
			order[#order+1] = id
			used = used + size[id]
			if counting then
				uploads = uploads + 1
				moved = moved + size[id]
			end
		end
	end
	return uploads, moved
end

print(string.format("%-8s %8s %8s", "policy", "uploads", "pages"))
for _, p in ipairs({ "reset", "fifo", "lru", "belady" }) do
	local u, m = run(p)
	print(string.format("%-8s %8d %8d", p, u, m))
end
