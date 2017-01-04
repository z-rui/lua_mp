local f = require 'mp.f'

local N = 15000
f.precision = math.ceil(N/math.log(2,10))

local a = f(1)
local b = f(1/2); b:sqrt(b) -- b = 1/sqrt(2)
local t = f(1/4)
local p = f(1, nil, 0) -- p does not need much precision
local tmp = f()

for i = 1,16 do
	tmp: add(a, b)
	tmp: div_2exp(tmp, 1)
	b: mul(a, b)
	b: sqrt(b)
	a: swap(tmp) -- tmp is now old a
	tmp: sub(tmp, a)
	tmp: mul(tmp, tmp)
	tmp: mul(tmp, p)
	t: sub(t, tmp)
	p: mul_2exp(p, 1)
end

tmp: mul(a, a)
tmp: div(tmp, t)

print(tmp:tostring(nil, N))
