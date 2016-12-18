function pidigits()
  local z = require 'mp.z'

  local q, r, t, u, i = z(1), z(180), z(60), z(168), 2
  local y1, y2 = z(), z()
  return function()
    y1:div(r, t, y2)
    y2: addmul(q, 5*i-2)
    r:  mul(u, 10)
    r:  mul(r, y2)
    -- r = 10 * u * (q * (5*i-2) + r - y*t)
    q:  mul(q, i)
    q:  mul(q, 20*i-10)
    -- q = 10 * q*i * (2*i-1)
    t:  mul(t, u)
    -- t = t * u
    i = i + 1
    u:  add(u, 54*i)
    -- u = u + 54 * i
    return y1
  end
end

local digits = pidigits()

for i=1,15000 do
  io.write(tostring(digits()))
end

io.write('\n')
