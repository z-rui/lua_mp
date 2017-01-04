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

-- type-generic binary operators (+, -, *, /)
local function tg_binop(fname)
  return function(a, b)
    local T = promote(a, b)
    return T[fname](T(), a, b)
  end
end

local tg = {
  __add = tg_binop "add",
  __sub = tg_binop "sub",
  __mul = tg_binop "mul",
  __div = tg_binop "div",
  __lt = function (a, b)
    local T, x = promote(a, b)
    if x == b then
      return b:cmp(a) > 0
    end
    return a:cmp(b) < 0
  end,
}

for k, v in pairs(tg) do
  Z[k], Q[k], F[k] = v, v, v
end

-- rewrite Z.__div
function Z.__div(a, b)
  if mphier(a) <= 1 and mphier(b) <= 1 then
    -- Z / Z is special case
    return q():div(a, b)
  end
  return tgdiv(a, b)
end

-- type-specific methods
local ts = {
  [Z] = {
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
  },
  [Q] = {
    __unm = "neg",
    __shl = "mul_2exp",
    __shr = "div_2exp",
  },
  [F] = {
    __unm = "neg",
    __shl = "mul_2exp",
    __shr = "div_2exp",
    __pow = "pow",
  },
}

for mt, list in pairs(ts) do
  local T = mt.__index
  for metamethod, fname in pairs(list) do
    local fun = T[fname]
    mt[metamethod] = function(...)
      return fun(T(), ...)
    end
  end
end

return mp
