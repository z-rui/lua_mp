#define CAN_HOLD(type, val) ((type) (val) == (val))

static mpz_ptr Znew(lua_State *L)
{
	mpz_ptr z = lua_newuserdata(L, sizeof (mpz_t));
	if (luaL_getmetatable(L, "mpz_t") == LUA_TNIL)
		luaL_error(L, "mp.z not loaded properly");
	mpz_init(z);
	lua_setmetatable(L, -2);
	return z;
}

static mpz_ptr Z_fromstring(lua_State *L, const char *s, int base)
{
	mpz_ptr z;

	z = Znew(L);
	if (mpz_set_str(z, s, base) != 0) {
		if (base)
			luaL_error(L, "cannot convert string to integer in base %d", base);
		else
			luaL_error(L, "cannot convert string to integer");
	}
	return z;
}

static mpz_ptr Zget(lua_State *L, int i)
{
	mpz_ptr z;
	lua_Integer val;

	switch (lua_type(L, i))
	{
		case LUA_TNUMBER:
			val = luaL_checkinteger(L, i);
			if (CAN_HOLD(long, val)) {
				z = Znew(L);
				mpz_set_si(z, (long int) val);
				lua_replace(L, i);
				return z;
			}
			/* otherwise fall through */
		case LUA_TSTRING:
			z = Z_fromstring(L, lua_tostring(L, i), 0);
			lua_replace(L, i);
			return z;
		default:
			return luaL_checkudata(L, i, "mpz_t");
	}
	return 0;
}

static int Zcall(lua_State *L)
{
	lua_Integer base;

	lua_remove(L, 1); /* self */
	switch (lua_gettop(L)) {
	case 0:
		Znew(L);
		break;
	case 1:
		Zget(L, 1);
		break;
	case 2:
		base = luaL_checkinteger(L, 2);
		luaL_argcheck(L, base == 0 || (2 <= base && base <= 62), 2, "base neither 0 nor in range [2,62]");
		Z_fromstring(L, luaL_checkstring(L, 1), (int) base);
		break;
	default:
		return luaL_error(L, "too many arguments");
	}
	return 1;
}

static int Zgc(lua_State *L)
{
	mpz_ptr z = Zget(L, 1);

	mpz_clear(z);
	lua_pushnil(L);
	lua_setmetatable(L, 1);
	return 0;
}

static int Ztostring(lua_State *L)	/** tostring(x) */
{
	mpz_ptr z;
	lua_Integer base;
	luaL_Buffer B;
	size_t sz;
	char *s;

	z = Zget(L, 1);
	base = luaL_optinteger(L, 2, 10);
	luaL_argcheck(L, (-36 <= base && base <= -2) || (2 <= base && base <= 62), 2, "base not in range [-36,-2] and [2,62]");

	sz = mpz_sizeinbase(z, abs(base)) + 2;
	s = mpz_get_str(luaL_buffinitsize(L, &B, sz), base, z);

	luaL_pushresultsize(&B, strlen(s));
	return 1;
}

static int Z_cmp(lua_State *L)
{
	mpz_srcptr x = Zget(L, 1);
	mpz_srcptr y = Zget(L, 2);

	return mpz_cmp(x, y);
}

static int Zeq(lua_State *L)
{
	lua_pushboolean(L, Z_cmp(L) == 0);
	return 1;
}

static int Zlt(lua_State *L)
{
	lua_pushboolean(L, Z_cmp(L) < 0);
	return 1;
}

static int Zswap(lua_State *L)
{
	mpz_ptr a, b;

	a = luaL_checkudata(L, 1, "mpz_t");
	b = luaL_checkudata(L, 2, "mpz_t");
	mpz_swap(a, b);
	return 0;
}

static void Z_checkops(lua_State *L, int ops)
{
	int top;

	top = lua_gettop(L);
	switch (top - ops) {
		case 0:
			Znew(L);
			lua_insert(L, 1);
		case 1:
			break;
		default:
			luaL_error(L, "expect %d or %d arguments, got %d", ops, ops+1, top);
	}
}

static int Zunop(lua_State *L, void (*op)(mpz_ptr, mpz_srcptr))
{
	Z_checkops(L, 1);
	(*op)(luaL_checkudata(L, 1, "mpz_t"), Zget(L, 2));
	lua_settop(L, 1);
	return 1;
}

static int Zbinop(lua_State *L, void (*op)(mpz_ptr, mpz_srcptr, mpz_srcptr))
{
	Z_checkops(L, 2);
	(*op)(luaL_checkudata(L, 1, "mpz_t"), Zget(L, 2), Zget(L, 3));
	lua_settop(L, 1);
	return 1;
}

static int Zternop(lua_State *L, void (*op)(mpz_ptr, mpz_srcptr, mpz_srcptr))
{
	int top;

	top = lua_gettop(L);
	if (top != 3)
		luaL_error(L, "too %s arguments", (top > 3) ? "many" : "few");
	(*op)(luaL_checkudata(L, 1, "mpz_t"), Zget(L, 2), Zget(L, 3));
	lua_settop(L, 1);
	return 1;
}

static void Z_check_divisor(lua_State *L, mpz_ptr divisor)
{
	if (mpz_sgn(divisor) == 0)
		luaL_error(L, "division by zero");
}

static int Z_intdiv_s(lua_State *L, int mode)
{
	static void (*divops[])(mpz_ptr, mpz_srcptr, mpz_srcptr) = {
		mpz_cdiv_q, mpz_cdiv_r, mpz_fdiv_q, mpz_fdiv_r, mpz_tdiv_q, mpz_tdiv_r, mpz_divexact
	};
	mpz_ptr a, b, c;

	Z_checkops(L, 2);
	a = luaL_checkudata(L, 1, "mpz_t");
	b = Zget(L, 2);
	c = Zget(L, 3);
	Z_check_divisor(L, c);
	(*divops[mode])(a, b, c);
	lua_settop(L, 1);
	return 1;
}

static int Z_intdiv_d(lua_State *L, int mode)
{
	static void (*divops[])(mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr) = {
		mpz_cdiv_qr, mpz_fdiv_qr, mpz_tdiv_qr
	};
	mpz_ptr a, b, c, d;

	switch (lua_gettop(L)) {
		case 2:
			Znew(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 3:
			Znew(L);
			lua_insert(L, 2);
		case 4:
			break;
		default:
			luaL_error(L, "wrong number of arguments");
	}
	a = luaL_checkudata(L, 1, "mpz_t");
	b = luaL_checkudata(L, 2, "mpz_t");
	c = Zget(L, 3);
	d = Zget(L, 4);
	Z_check_divisor(L, d);
	(*divops[mode])(a, b, c, d);
	lua_settop(L, 2);
	return 2;
}

static int Zintdiv(lua_State *L)
{
	const char *modestr;
	int mode;

	if (lua_type(L, -1) == LUA_TSTRING) {
		modestr = luaL_checkstring(L, -1);
	} else {
try_default:
		switch (lua_gettop(L)) {
			case 2:
			case 3:
				modestr = lua_pushliteral(L, "fq");
				break;
			case 4:
				modestr = lua_pushliteral(L, "fqr");
				break;
			default:
				return luaL_error(L, "wrong number of arguments");
		}
	}

	if (strcmp(modestr, "exact") == 0) {
		mode = 6;
		goto mode_parse_end;
	}
	if (modestr[0] == 'c') {
		mode = 0;
	} else if (modestr[0] == 'f') {
		mode = 2;
	} else if (modestr[0] == 't') {
		mode = 4;
	} else {
		/* modestr does not match a valid pattern.
		 * maybe this is a number in string form?
		 * (instead of a modestr) */
		goto try_default;
	}
	if (modestr[1] == 'q') {
		if (modestr[2] == '\0') {
			; /* ok */
		} else if (modestr[2] == 'r' && modestr[3] == '\0') {
			mode += 8;
		}
	} else if (modestr[1] == 'r' && modestr[2] == '\0') {
		mode += 1;
	} else {
		luaL_argerror(L, -1, "mode must be one of: cq cr cqr fq fr fqr tq tr tqr exact");
	}
mode_parse_end:
	lua_pop(L, 1); /* remove modestr from stack */
	return (mode < 8) ? Z_intdiv_s(L, mode) : Z_intdiv_d(L, mode/2 - 4);
}

static int Zfdiv_q(lua_State *L) { return Z_intdiv_s(L, 2); }
static int Zfdiv_r(lua_State *L) { return Z_intdiv_s(L, 3); }

#define Z_DCL(typ, alias, op) \
static int Z##alias(lua_State *L) { return Z##typ(L, mpz_##op); }

#define Z_UNOP_DCL(op) Z_DCL(unop, op, op)
#define Z_BINOP_DCL(op) Z_DCL(binop, op, op)
#define Z_TERNOP_DCL(op) Z_DCL(ternop, op, op)

Z_UNOP_DCL(abs)
Z_UNOP_DCL(neg)
/* Pitfall here! */
/* Lua's __unm takes an additional argument,
 * behaving like a binary op */
static int Zneg_wrap(lua_State *L)
{
	lua_settop(L, 1);
	return Zneg(L);
}
Z_UNOP_DCL(nextprime)
Z_UNOP_DCL(set)
Z_DCL(unop, bnot, com)

Z_BINOP_DCL(add)
Z_BINOP_DCL(sub)
Z_BINOP_DCL(mul)
Z_BINOP_DCL(gcd)
Z_BINOP_DCL(lcm)
Z_DCL(binop, band, and)
Z_DCL(binop, bor, ior)
Z_DCL(binop, bxor, xor)

Z_TERNOP_DCL(addmul)
Z_TERNOP_DCL(submul)

static void Q_checkops(lua_State *L, int ops);

static int Zratdiv(lua_State *L)
{
	mpq_ptr a;
	mpz_ptr b, c;

	Q_checkops(L, 2);
	a = luaL_checkudata(L, 1, "mpq_t");
	b = Zget(L, 2);
	c = Zget(L, 3);
	Z_check_divisor(L, c);
	mpz_set(mpq_numref(a), b);
	mpz_set(mpq_denref(a), c);
	mpq_canonicalize(a);
	lua_settop(L, 1);
	return 1;
}

#define Z_PRED_DCL(op) \
static int Z##op(lua_State *L) { return lua_pushboolean(L, mpz_##op(Zget(L, 1))), 1; }

Z_PRED_DCL(odd_p)
Z_PRED_DCL(even_p)
Z_PRED_DCL(perfect_power_p)
Z_PRED_DCL(perfect_square_p)

static int Zprobab_prime_p(lua_State *L)
{
	mpz_ptr z;
	int reps;

	z = Zget(L, 1);
	reps = luaL_checkinteger(L, 2);
	lua_pushboolean(L, mpz_probab_prime_p(z, reps));
	return 1;
}

static int Zdivisible_p(lua_State *L)
{
	mpz_ptr a, b;

	a = Zget(L, 1);
	b = Zget(L, 2);
	lua_pushboolean(L, mpz_divisible_p(a, b));
	return 1;
}

static int Zgcdext(lua_State *L)
{
	mpz_ptr a, b, s, t, g;

	g = luaL_checkudata(L, 1, "mpz_t");
	s = (lua_type(L, 2) == LUA_TNIL) ? NULL : luaL_checkudata(L, 2, "mpz_t");
	t = (lua_type(L, 3) == LUA_TNIL) ? NULL : luaL_checkudata(L, 3, "mpz_t");
	a = Zget(L, 4);
	b = Zget(L, 5);
	mpz_gcdext(g, s, t, a, b);
	lua_settop(L, 3);
	return 3;
}

static int Zfac(lua_State *L)
{
	mpz_ptr z;
	lua_Integer n, m;

	if (luaL_testudata(L, 1, "mpz_t") == NULL) {
		z = Znew(L);
		lua_insert(L, 1);
	} else {
		z = luaL_checkudata(L, 1, "mpz_t");
	}
	n = luaL_checkinteger(L, 2);
	luaL_argcheck(L, CAN_HOLD(unsigned long, n), 2, "n overflow");
	if (lua_type(L, 3) != LUA_TNONE)
		m = luaL_checkinteger(L, 3);
	else
		goto simple;
	luaL_argcheck(L, CAN_HOLD(unsigned long, m), 3, "m overflow");
	switch (m) {
		case 1:
simple:
			mpz_fac_ui(z, n);
			break;
		case 2:
			mpz_2fac_ui(z, n);
			break;
		default:
			mpz_mfac_uiui(z, n, m);
			break;
	}
	lua_settop(L, 1);
	return 1;
}

static int Zbin(lua_State *L)
{
	mpz_ptr z, n;
	lua_Integer k;

	Z_checkops(L, 2);
	z = luaL_checkudata(L, 1, "mpz_t");
	n = Zget(L, 2);
	k = luaL_checkinteger(L, 3);
	luaL_argcheck(L, CAN_HOLD(unsigned long, k), 3, "k overflow");
	mpz_bin_ui(z, n, k);
	lua_settop(L, 1);
	return 1;
}

static int Zfib(lua_State *L)
{
	mpz_ptr z, z1;
	lua_Integer n;

	switch(lua_gettop(L)) {
		case 1:
			Znew(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 2:
			z = luaL_checkudata(L, 1, "mpz_t");
			n = luaL_checkinteger(L, 2);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 2, "n overflow");
			mpz_fib_ui(z, n);
			lua_settop(L, 1);
			return 1;
		case 3:
			z = luaL_checkudata(L, 1, "mpz_t");
			z1 = luaL_checkudata(L, 2, "mpz_t");
			n = luaL_checkinteger(L, 3);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 3, "n overflow");
			mpz_fib2_ui(z, z1, n);
			lua_settop(L, 2);
			return 2;
	}
	return luaL_error(L, "wrong number of arguments");
}

static int Zpow(lua_State *L)
{
	mpz_ptr base;
	lua_Integer exp;

	Z_checkops(L, 2);
	base = Zget(L, 2);
	exp = luaL_checkinteger(L, 3);
	luaL_argcheck(L, CAN_HOLD(unsigned long, exp), 3, "exp overflow");
	mpz_pow_ui(luaL_checkudata(L, 1, "mpz_t"), base, exp);
	lua_settop(L, 1);
	return 1;
}

static int Zroot(lua_State *L)
{
	mpz_ptr root, rem, z;
	lua_Integer n;

	switch(lua_gettop(L)) {
		case 2:
			Znew(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 3:
			root = luaL_checkudata(L, 1, "mpz_t");
			z = Zget(L, 2);
			n = luaL_checkinteger(L, 3);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 3, "n overflow");
			mpz_root(root, z, n);
			lua_settop(L, 1);
			return 1;
		case 4:
			root = luaL_checkudata(L, 1, "mpz_t");
			rem = luaL_checkudata(L, 2, "mpz_t");
			z = Zget(L, 3);
			n = luaL_checkinteger(L, 4);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 4, "n overflow");
			mpz_rootrem(root, rem, z, n);
			lua_settop(L, 2);
			return 2;
	}
	return luaL_error(L, "wrong number of arguments");
}

static int Zsqrt(lua_State *L)
{
	mpz_ptr root, rem, z;

	switch(lua_gettop(L)) {
		case 1:
			Znew(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 2:
			root = luaL_checkudata(L, 1, "mpz_t");
			z = Zget(L, 2);
			mpz_sqrt(root, z);
			lua_settop(L, 1);
			return 1;
		case 3:
			root = luaL_checkudata(L, 1, "mpz_t");
			rem = luaL_checkudata(L, 2, "mpz_t");
			z = Zget(L, 3);
			mpz_sqrtrem(root, rem, z);
			lua_settop(L, 2);
			return 2;
	}
	return luaL_error(L, "wrong number of arguments");
}

static const luaL_Reg ZReg[] =
{
	{ "__gc",	Zgc	},
	{ "__tostring",	Ztostring	},
	{ "__unm",	Zneg_wrap	},
	{ "__add",	Zadd	},
	{ "__sub",	Zsub	},
	{ "__mul",	Zmul	},
	{ "__div",	Zratdiv	},
	{ "__idiv",	Zfdiv_q	},
	{ "__pow",	Zpow	},
	{ "__eq",	Zeq	},
	{ "__lt",	Zlt	},
	{ "__mod",	Zfdiv_r	},
	{ "__band",	Zband	},
	{ "__bor",	Zbor	},
	{ "__bxor",	Zbxor	},
	{ "__bnot",	Zbnot	},

	{ "tostring",	Ztostring	},

	{ "abs",	Zabs	},
	{ "neg",	Zneg	},
	{ "set",	Zset	},
	{ "add",	Zadd	},
	{ "sub",	Zsub	},
	{ "mul",	Zmul	},
	{ "addmul",	Zaddmul	},
	{ "submul",	Zsubmul	},
	{ "div",	Zintdiv	},
	{ "pow",	Zpow	},
	{ "sqrt",	Zsqrt	},
	{ "root",	Zroot	},

	{ "nextprime",	Znextprime	},
	{ "gcd",	Zgcd	},
	{ "gcdext",	Zgcdext	},
	{ "lcm",	Zlcm	},
	{ "probab_prime_p",	Zprobab_prime_p	},
	{ "fac",	Zfac	},
	{ "bin",	Zbin	},
	{ "fib",	Zfib	},

	{ "band",	Zband	},
	{ "bor",	Zbor	},
	{ "bxor",	Zbxor	},
	{ "bnot",	Zbnot	},

	{ "even_p",	Zeven_p	},
	{ "odd_p",	Zodd_p	},
	{ "perfect_power_p",	Zperfect_power_p	},
	{ "perfect_square_p",	Zperfect_square_p	},
	{ "divisible_p",	Zdivisible_p	},

	{ "swap",	Zswap	},
	{ NULL,		NULL	}
};

LUALIB_API int luaopen_mp_z(lua_State *L)
{
	luaL_newmetatable(L, "mpz_t");
	luaL_setfuncs(L, ZReg, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_newtable(L);
	lua_newtable(L);
	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, Zcall);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);

	return 1;
}
