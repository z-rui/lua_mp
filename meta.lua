local mp = require('mp')
local z, q, f = mp.z, mp.q, mp.f

local getmetatable = getmetatable

local Z = getmetatable(z())
local Q = getmetatable(q())
local F = getmetatable(f())

local mptypes = {z, f, q, [Z] = 1, [F] = 2, [Q] = 3}
local function mphier(x)
  return mptypes[getmetatable(x)] or 0
end
local function promote(a, b)
  local h1, h2 = mphier(a), mphier(b)
  if h1 < h2 then
    return mptypes[h2], b
  end
  return mptypes[h1], a
end

-- type-generic binary operators (+, -, *)
local function tg_binop(fname)
  return function(a, b)
    local T = promote(a, b)
    return T[fname](T(), a, b)
  end
end

local tgadd = tg_binop "add"
local tgsub = tg_binop "sub"
local tgmul = tg_binop "mul"
local tgdiv = tg_binop "div"

Z.__add = tgadd
Z.__sub = tgsub
Z.__mul = tgmul

function Z.__div(a, b)
  if mphier(a) <= 1 and mphier(b) <= 1 then
    -- Z / Z is special case
    return q():div(a, b)
  end
  return tgdiv(a, b)
end

Q.__add = tgadd
Q.__sub = tgsub
Q.__mul = tgmul
Q.__div = tgdiv

F.__add = tgadd
F.__sub = tgsub
F.__mul = tgmul
F.__div = tgdiv

local function register(mt, list)
  local T = mt.__index
  for metamethod, fname in pairs(list) do
    local fun = T[fname]
    mt[metamethod] = function(...)
      return fun(T(), ...)
    end
  end
end

register(Z, {
  __unm  = "neg",
  __idiv = "div",
  __mod  = "mod",
  __pow  = "pow",
  __band = "band",
  __bor  = "bor",
  __bxor = "bxor",
  __bnot = "bnot",
  __shl  = "mul_2exp",
  __shr  = "div_2exp",
})

register(Q, {
  __unm = "neg",
  __shl = "mul_2exp",
  __shr = "div_2exp",
})

register(F, {
  __unm = "neg",
  __shl = "mul_2exp",
  __shr = "div_2exp",
  __pow = "pow",
})

local function tgless(a, b)
  local T, x = promote(a, b)
  if x == b then
    return b:cmp(a) > 0
  end
  return a:cmp(b) < 0
end

Z.__lt = tgless
Q.__lt = tgless
F.__lt = tgless

return mp
