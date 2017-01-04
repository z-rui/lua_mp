local f = require 'mp.f'

local N = 15000
f.precision = math.ceil(N/math.log(2,10))

local a, b, t = f(1), f():sqrt(.5), f(.25)
local tmp = f()

for i = 0, 15 do
	tmp: add(a, b)
	tmp: div_2exp(tmp, 1)
	b: mul(a, b)
	b: sqrt(b)
	a: swap(tmp) -- tmp is now old a
	tmp: sub(tmp, a)
	tmp: mul(tmp, tmp)
	tmp: mul_2exp(tmp, i)
	t: sub(t, tmp)
end

tmp: mul(a, a)
tmp: div(tmp, t)

print(tmp:tostring(nil, N))
