local mp = require('mp')
local z, q = mp.z, mp.q

local getmetatable = getmetatable

local Z = getmetatable(z())
local Q = getmetatable(q())

local function mp_op(constructor, fun)
  return function(...)
    return fun(constructor(), ...)
  end
end

-- type-generic binary operators (+, -, *)
local function tg_binop(fname)
  return function(a, b)
    local t = z
    if getmetatable(b) == Q then
      t = q
    end
    return t[fname](t(), a, b)
  end
end
Z.__add = tg_binop "add"
Z.__sub = tg_binop "sub"
Z.__mul = tg_binop "mul"
-- division always gives a rational
Z.__div = mp_op(q, q.div)

local z_ops = {
  __unm  = z.neg,
  __idiv = z.div,
  __pow  = z.pow,
  __band = z.band,
  __bor  = z.bor,
  __bxor = z.bxor,
  __bnot = z.bnot,
  __shl  = z.mul_2exp,
  __shr  = z.div_2exp,
}

local q_ops = {
  __unm  = q.neg,
  __add  = q.add,
  __sub  = q.sub,
  __mul  = q.mul,
  __div  = q.div,
}

for name, fun in pairs(z_ops) do
  Z[name] = mp_op(z, fun)
end

for name, fun in pairs(q_ops) do
  Q[name] = mp_op(q, fun)
end

function Z.__lt(a, b)
  local ta, tb = getmetatable(a), getmetatable(b)

  if ta == nil or tb == Q then
    return b:cmp(a) > 0 -- b > a
  end
  return a:cmp(b) < 0 -- a < b
end

function Q.__lt(a, b)
  local ta, tb = getmetatable(a), getmetatable(b)

  if ta == nil then
    return b:cmp(a) > 0 -- b > a
  end
  return a:cmp(b) < 0 -- a < b
end

return mp
