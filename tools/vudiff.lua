-- vudiff.lua -- the comparing half of tools/vudiff.sh: two ee-objdump
-- disassemblies of .vutext and the two assembler listings; prints the
-- instructions whose bytes differ, with the listing's source line.

local adis, bdis, alst, blst = ...

-- objdump: "     1b8:\t01 e1 10 00 ff 02 00 00 \tmnemonic ..." possibly
-- over two lines (the 4+4 byte split); gather bytes per address
local function readdis(path)
	local ins, order = {}, {}
	local cur
	for line in io.lines(path) do
		local addr, bytes, rest = line:match("^%s*(%x+):%s+([%x ]+)%s*\t?(.*)$")
		if addr then
			addr = tonumber(addr, 16)
			bytes = bytes:gsub("%s+$", "")
			if cur and addr == cur.addr + cur.n and rest == "" then
				cur.bytes = cur.bytes .. " " .. bytes
				cur.n = cur.n + #bytes:gsub("%s", "") // 2
			else
				cur = { addr = addr, bytes = bytes, text = rest, n = #bytes:gsub("%s", "") // 2 }
				ins[addr] = cur
				order[#order+1] = addr
			end
		end
	end
	return ins, order
end

-- listing: " 121 01b8 0010E101 \t source" -- address in the section
local function readlst(path)
	local src = {}
	for line in io.lines(path) do
		local addr, text = line:match("^%s*%d+%s+(%x%x%x%x)%s+%x+%s+(.*)$")
		if addr and text and text ~= "" and not src[tonumber(addr, 16)] then
			src[tonumber(addr, 16)] = text
		end
	end
	return src
end

local a, aorder = readdis(adis)
local b = readdis(bdis)
local asrc = readlst(alst)
local bsrc = readlst(blst)

-- a directive of several words (.word a, b, c, d) is listed at its
-- first address only; the following qwords inherit its line
local function fill(src)
	local last
	for _, addr in ipairs(aorder) do
		if src[addr] then last = src[addr]
		elseif last and last:match("^%s*%.%a+") then src[addr] = last end
	end
end
fill(asrc)
fill(bsrc)

-- code and data apart: a .word in the footer changing is expected,
-- an instruction changing is what the question is about
local function isdata(src)
	-- directives, and the DMA and VIF words of the upload chain: not VU code
	return src ~= nil and (src:match("^%s*%.%a+") ~= nil or
		src:match("^%s*[Uu]npack") ~= nil or src:match("^%s*[Ss]tcycl") ~= nil or
		src:match("^%s*DMA") ~= nil or src:match("^%s*MPG") ~= nil or
		src:match("^%s*vifnop") ~= nil or src:match("^%s*mscal") ~= nil or src:match("^%s*flush") ~= nil)
end

local ncode, ndata, total = 0, 0, 0
local report = { code = {}, data = {} }
for _, addr in ipairs(aorder) do
	local x, y = a[addr], b[addr]
	local src = asrc[addr]
	total = total + 1
	if y == nil or x.bytes ~= y.bytes then
		local kind = isdata(src) and "data" or "code"
		local t = report[kind]
		t[#t+1] = string.format("%04x: %s", addr, src or "?")
		if y == nil then
			t[#t+1] = "      only in the original"
		else
			t[#t+1] = string.format("      was  %-24s %s", x.bytes, kind == "code" and x.text or "")
			t[#t+1] = string.format("      now  %-24s %s", y.bytes, kind == "code" and y.text or "")
			if bsrc[addr] and bsrc[addr] ~= src then
				t[#t+1] = string.format("      src  %s", bsrc[addr])
			end
		end
		if kind == "data" then ndata = ndata + 1 else ncode = ncode + 1 end
	end
end
for addr in pairs(b) do
	if a[addr] == nil then
		report.code[#report.code+1] = string.format("%04x: only in the variant: %s", addr, b[addr].text)
		ncode = ncode + 1
	end
end
if #report.code > 0 then
	print("instructions:")
	for _, l in ipairs(report.code) do print(l) end
end
if #report.data > 0 then
	print("data:")
	for _, l in ipairs(report.data) do print(l) end
end
print(string.format("%d instructions and %d data words differ, of %d", ncode, ndata, total))
