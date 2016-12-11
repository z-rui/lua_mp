function pidigits()
  local z = require 'mp.z'

  local q, r, t, u, i = z(1), z(180), z(60), z(168), z(2)
  local t1, y1, y2 = z(), z(), z()
  local ONE, TWO, FIVE, TEN, _54 = z(1), z(2), z(5), z(10), z(54)
  return function()
    z.div(y1, y2, r, t, 'fqr')
    t1: mul(FIVE, i)
    t1: sub(t1, TWO)
    y2: addmul(t1, q)
    r:  mul(TEN, u)
    r:  mul(r, y2)
    --r = TEN * u * (q * (FIVE*i-TWO) + r - y*t)
    t1: mul(TWO, i)
    t1: sub(t1, ONE)
    q:  mul(q, i)
    q:  mul(q, TEN)
    q:  mul(q, t1)
    --q = TEN * q*i * (TWO*i-ONE)
    t:  mul(t, u)
    --t = t * u
    i:  add(i, ONE)
    --i = i + ONE
    u:  addmul(_54, i)
    --u = u + 54 * i
    return y1
  end
end

local digits = pidigits()

for i=1,15000 do
  io.write(tostring(digits()))
end

io.write('\n')
