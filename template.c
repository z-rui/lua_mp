static mp$_ptr $_new(lua_State *L)
{
	mp$_ptr z = lua_newuserdata(L, sizeof (mp$_t));
	if (luaL_getmetatable(L, "mp$_t") == LUA_TNIL)
		luaL_error(L, "mp$_t not registered");
	mp$_init(z);
	lua_setmetatable(L, -2);
	return z;
}

static void $__set_str(lua_State *L, mp$_ptr z, const char *s, int base)
{
	if (mp$_set_str(z, s, base) != 0) {
#if defined(MPZ)
		_conversion_error(L, "string", "integer", base);
#elif defined(MPQ)
		_conversion_error(L, "string", "rational", base);
#endif
	}
}

static mp$_ptr _checkmp$(lua_State *L, int i)
{
	void *z;

	z = luaL_checkudata(L, i, "mp$_t");
#ifdef MPZ
	if (lua_getuservalue(L, i) == LUA_TUSERDATA) {
		/* partial ref */
		z = *(mp$_ptr *) z;
	}
	lua_pop(L, 1);
#endif
	return (mp$_ptr) z;
}

static void $__set(lua_State *L, int i, mp$_ptr z)
{
	switch (lua_type(L, i)) {
		case LUA_TNUMBER: {
#if defined(MPZ)
			lua_Integer val;

			val = luaL_checkinteger(L, i);
			if (CAN_HOLD(long, val)) {
				mp$_set_si(z, (long int) val);
				break;
			}
			/* otherwise fall through */
#elif defined(MPQ)
			lua_Number val;

			val = lua_tonumber(L, i);
			luaL_argcheck(L, val == val && val * 0.0 == 0.0, i,
					"infinity or NaN");
			mp$_set_d(z, val);
			break;
#endif
		}
		case LUA_TSTRING:
			$__set_str(L, z, lua_tostring(L, i), 0);
			break;
		case LUA_TUSERDATA: {
			void *p;

			if ((p = luaL_testudata(L, i, "mpz_t")) != 0) {
#if defined(MPZ)
				mpz_set(z, p);
#elif defined(MPQ)
				mpq_set_z(z, p);
#endif
			} else if ((p = luaL_testudata(L, i, "mpq_t")) != 0) {
#if defined(MPZ)
				luaL_argcheck(L, mpz_sgn(mpq_denref((mpq_ptr) p)) > 0, i, "non-canonicalized rational");
				mpz_set_q(z, p);
#elif defined(MPQ)
				mpq_set(z, p);
#endif
			}
			break;
		}
		default:
			_conversion_error(L, luaL_typename(L, i),
#if defined(MPZ)
				"integer"
#elif defined(MPQ)
				"rational"
#endif
			, 0);
	}
}

static mp$_ptr _tomp$(lua_State *L, int i)
{
	mp$_ptr z;

	if ((z = luaL_testudata(L, i, "mp$_t")) == 0) {
		z = $_new(L);
		$__set(L, i, z);
		lua_replace(L, i);
	}
#ifdef MPQ
	/* sanity check to prevent fp exception */
	luaL_argcheck(L, mpz_sgn(mpq_denref(z)) > 0, i, "non-canonicalized rational");
#endif
	return z;
}

static int $_call(lua_State *L)
{
	mp$_ptr z;

	z = $_new(L);
	/* keep in mind that there is a
	 * 'self' argument at index 1;
	 * the stack now looks like:
	 * self | args... | z */
	switch (lua_gettop(L) - 2) {
	case 1:
		$__set(L, 2, z);
	case 0:
		break;
	default:
		$__set_str(L, z, luaL_checkstring(L, 2), _check_inbase(L, 3));
		break;
	}
	return 1;
}

static int $_set(lua_State *L)
{
	mp$_ptr z;

	z = _checkmp$(L, 1);
	$__set(L, 2, z);
	lua_settop(L, 1);
	return 1;
}

static int $_gc(lua_State *L)
{
	mp$_ptr z = luaL_checkudata(L, 1, "mp$_t");

	mp$_clear(z);
	lua_pushnil(L);
	lua_setmetatable(L, 1);
	return 0;
}

static int $_tostring(lua_State *L)	/** tostring(x) */
{
	mp$_ptr z;
	int base;
	luaL_Buffer B;
	size_t sz;
	char *s;

	z = _checkmp$(L, 1);
	base = _check_outbase(L, 2);
#if defined(MPZ)
	sz = mpz_sizeinbase(z, abs(base)) + 2;
#elif defined(MPQ)
	sz = mpz_sizeinbase(mpq_numref(z), abs(base))
	   + mpz_sizeinbase(mpq_denref(z), abs(base))
	   + 3;
#endif
	s = mp$_get_str(luaL_buffinitsize(L, &B, sz), base, z);

	luaL_pushresultsize(&B, strlen(s));
	return 1;
}

static int $_tonumber(lua_State *L)
{
	mp$_ptr z;
	double val;

	z = _checkmp$(L, 1);
#ifdef MPZ
	/* FIXME 'signed long' maybe shorter than 'lua_Integer' */
	if (mpz_fits_slong_p(z)) {
		long val;

		val = mpz_get_si(z);
		lua_pushinteger(L, val);
		return 1;
	}
#endif
	val = mp$_get_d(z);
	lua_pushnumber(L, val);
	return 1;
}

static int $__cmp(lua_State *L)
{
	return mp$_cmp(_tomp$(L, 1), _tomp$(L, 2));
}

static int $_cmp(lua_State *L)
{
	lua_pushinteger(L, $__cmp(L));
	return 1;
}

static int $_eq(lua_State *L)
{
	lua_pushboolean(L, $__cmp(L) == 0);
	return 1;
}

static int $_lt(lua_State *L)
{
	lua_pushboolean(L, $__cmp(L) < 0);
	return 1;
}

static int $_swap(lua_State *L)
{
	mp$_ptr a, b;

	a = _checkmp$(L, 1);
	b = _checkmp$(L, 2);
	mp$_swap(a, b);
	return 0;
}

static void $__checkops(lua_State *L, int ops)
{
	int top;

	top = lua_gettop(L);
	switch (top - ops) {
		case 0:
			$_new(L);
			lua_insert(L, 1);
		case 1:
			break;
		default:
			luaL_error(L, "expect %d or %d arguments, got %d", ops, ops+1, top);
	}
}

static int $_unop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr))
{
	$__checkops(L, 1);
	(*op)(_checkmp$(L, 1), _tomp$(L, 2));
	lua_settop(L, 1);
	return 1;
}

static int $_binop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr, mp$_srcptr))
{
	$__checkops(L, 2);
	(*op)(_checkmp$(L, 1), _tomp$(L, 2), _tomp$(L, 3));
	lua_settop(L, 1);
	return 1;
}

#define OP_DCL(typ, fun) \
static int $_##fun(lua_State *L) { return $_##typ(L, mp$_##fun); }

OP_DCL(unop, abs)
OP_DCL(unop, neg)
/* Pitfall here! */
/* Lua's __unm takes an additional argument,
 * behaving like a binary op */
static int $_neg_wrap(lua_State *L)
{
	lua_settop(L, 1);
	return $_neg(L);
}

OP_DCL(binop, add)
OP_DCL(binop, sub)
OP_DCL(binop, mul)

#ifdef MPZ /* integer specific functions */
OP_DCL(unop, nextprime)
OP_DCL(unop, com)
OP_DCL(binop, gcd)
OP_DCL(binop, lcm)
OP_DCL(binop, and)
OP_DCL(binop, ior)
OP_DCL(binop, xor)

static int z__partial_ref(lua_State *L, int i, mpz_ptr z)
{
	mpz_ptr *p = lua_newuserdata(L, sizeof (mpz_ptr));
	if (luaL_getmetatable(L, "mp$_t") == LUA_TNIL)
		luaL_error(L, "mp$_t not registered");
	*p = z;
	lua_setmetatable(L, -2);
	lua_pushvalue(L, i);
	lua_setuservalue(L, -2);
	return 1;
}

static int z_sizeinbase(lua_State *L)
{
	mpz_ptr z;
	lua_Integer base;

	z = _tompz(L, 1);
	base = luaL_checkinteger(L, 2);
	luaL_argcheck(L, 2 <= base && base <= 62, 2, "base must be between 2 and 62");
	lua_pushinteger(L, mpz_sizeinbase(z, base));
	return 1;
}

static int $_ternop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr, mp$_srcptr))
{
	int top;

	top = lua_gettop(L);
	if (top != 3)
		luaL_error(L, "too %s arguments", (top > 3) ? "many" : "few");
	(*op)(_checkmp$(L, 1), _tomp$(L, 2), _tomp$(L, 3));
	lua_settop(L, 1);
	return 1;
}

OP_DCL(ternop, addmul)
OP_DCL(ternop, submul)

static int z__intdiv_s(lua_State *L, int mode)
{
	static void (*divops[])(mpz_ptr, mpz_srcptr, mpz_srcptr) = {
		mpz_cdiv_q, mpz_cdiv_r,
		mpz_fdiv_q, mpz_fdiv_r,
		mpz_tdiv_q, mpz_tdiv_r,
		mpz_divexact
	};
	mpz_ptr a, b, c;

	z__checkops(L, 2);
	a = _checkmpz(L, 1);
	b = _tompz(L, 2);
	c = _tompz(L, 3);
	_check_divisor(L, c);
	(*divops[mode])(a, b, c);
	lua_settop(L, 1);
	return 1;
}

static int z__intdiv_d(lua_State *L, int mode)
{
	static void (*divops[])(mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr) = {
		mpz_cdiv_qr, mpz_fdiv_qr, mpz_tdiv_qr
	};
	mpz_ptr a, b, c, d;

	switch (lua_gettop(L)) {
		case 2:
			z_new(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 3:
			z_new(L);
			lua_insert(L, 2);
		case 4:
			break;
		default:
			luaL_error(L, "wrong number of arguments");
	}
	a = _checkmpz(L, 1);
	b = _checkmpz(L, 2);
	c = _tompz(L, 3);
	d = _tompz(L, 4);
	_check_divisor(L, d);
	(*divops[mode])(a, b, c, d);
	lua_settop(L, 2);
	return 2;
}

static int z_intdiv(lua_State *L)
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
	return (mode < 8) ? z__intdiv_s(L, mode) : z__intdiv_d(L, mode/2 - 4);
}

static int z_fdiv_q(lua_State *L) { return z__intdiv_s(L, 2); }
static int z_fdiv_r(lua_State *L) { return z__intdiv_s(L, 3); }

static mpq_ptr q_new(lua_State *L);

/* z_ratdiv is always called as an operator */
static int z_ratdiv(lua_State *L)
{
	mpq_ptr a;
	mpz_ptr b, c;

	a = q_new(L);
	b = _tompz(L, 1);
	c = _tompz(L, 2);
	_check_divisor(L, c);
	mpz_set(mpq_numref(a), b);
	mpz_set(mpq_denref(a), c);
	mpq_canonicalize(a);
	return 1;
}

#define PRED_DCL(op) \
static int z_##op(lua_State *L) { lua_pushboolean(L, mpz_##op(_tompz(L, 1))); return 1; }

PRED_DCL(odd_p)
PRED_DCL(even_p)
PRED_DCL(perfect_power_p)
PRED_DCL(perfect_square_p)

static int z_probab_prime_p(lua_State *L)
{
	lua_pushboolean(L, mpz_probab_prime_p(_tompz(L, 1), luaL_checkinteger(L, 2)));
	return 1;
}

static int z_divisible_p(lua_State *L)
{
	lua_pushboolean(L, mpz_divisible_p(_tompz(L, 1), _tompz(L, 2)));
	return 1;
}

static int z_gcdext(lua_State *L)
{
	mpz_ptr a, b, s, t, g;

	g = _checkmpz(L, 1);
	s = (lua_type(L, 2) == LUA_TNIL) ? NULL : _checkmpz(L, 2);
	t = (lua_type(L, 3) == LUA_TNIL) ? NULL : _checkmpz(L, 3);
	a = _tompz(L, 4);
	b = _tompz(L, 5);
	mpz_gcdext(g, s, t, a, b);
	lua_settop(L, 3);
	return 3;
}

static int z_fac(lua_State *L)
{
	mpz_ptr z;
	lua_Integer n, m;

	if (luaL_testudata(L, 1, "mpz_t") == NULL) {
		z = z_new(L);
		lua_insert(L, 1);
	} else {
		z = _checkmpz(L, 1);
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

static int z_bin(lua_State *L)
{
	mpz_ptr z, n;
	lua_Integer k;

	z__checkops(L, 2);
	z = _checkmpz(L, 1);
	n = _tompz(L, 2);
	k = luaL_checkinteger(L, 3);
	luaL_argcheck(L, CAN_HOLD(unsigned long, k), 3, "k overflow");
	mpz_bin_ui(z, n, k);
	lua_settop(L, 1);
	return 1;
}

static int z_fib(lua_State *L)
{
	mpz_ptr z, z1;
	lua_Integer n;

	switch(lua_gettop(L)) {
		case 1:
			z_new(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 2:
			z = _checkmpz(L, 1);
			n = luaL_checkinteger(L, 2);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 2, "n overflow");
			mpz_fib_ui(z, n);
			lua_settop(L, 1);
			return 1;
		case 3:
			z = _checkmpz(L, 1);
			z1 = _checkmpz(L, 2);
			n = luaL_checkinteger(L, 3);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 3, "n overflow");
			mpz_fib2_ui(z, z1, n);
			lua_settop(L, 2);
			return 2;
	}
	return luaL_error(L, "wrong number of arguments");
}

static int z_pow(lua_State *L)
{
	mpz_ptr base;
	lua_Integer exp;

	z__checkops(L, 2);
	base = _tompz(L, 2);
	exp = luaL_checkinteger(L, 3);
	luaL_argcheck(L, CAN_HOLD(unsigned long, exp), 3, "exp overflow");
	mpz_pow_ui(_checkmpz(L, 1), base, exp);
	lua_settop(L, 1);
	return 1;
}

static int z_root(lua_State *L)
{
	mpz_ptr root, rem, z;
	lua_Integer n;

	switch(lua_gettop(L)) {
		case 2:
			z_new(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 3:
			root = _checkmpz(L, 1);
			z = _tompz(L, 2);
			n = luaL_checkinteger(L, 3);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 3, "n overflow");
			mpz_root(root, z, n);
			lua_settop(L, 1);
			return 1;
		case 4:
			root = _checkmpz(L, 1);
			rem = _checkmpz(L, 2);
			z = _tompz(L, 3);
			n = luaL_checkinteger(L, 4);
			luaL_argcheck(L, CAN_HOLD(unsigned long, n), 4, "n overflow");
			mpz_rootrem(root, rem, z, n);
			lua_settop(L, 2);
			return 2;
	}
	return luaL_error(L, "wrong number of arguments");
}

static int z_sqrt(lua_State *L)
{
	mpz_ptr root, rem, z;

	switch(lua_gettop(L)) {
		case 1:
			z_new(L);
			lua_insert(L, 1);
			/* fallthrough */
		case 2:
			root = _checkmpz(L, 1);
			z = _tompz(L, 2);
			mpz_sqrt(root, z);
			lua_settop(L, 1);
			return 1;
		case 3:
			root = _checkmpz(L, 1);
			rem = _checkmpz(L, 2);
			z = _tompz(L, 3);
			mpz_sqrtrem(root, rem, z);
			lua_settop(L, 2);
			return 2;
	}
	return luaL_error(L, "wrong number of arguments");
}
#endif
#ifdef MPQ /* rational specfic functions */
static int q_inv(lua_State *L)
{
	mpq_ptr q, r;

	q__checkops(L, 1);
	q = _checkmpq(L, 1);
	r = _tompq(L, 2);
	_check_divisor(L, mpq_numref(q));
	mpq_inv(q, r);
	lua_settop(L, 1);
	return 1;
}

static int q_div(lua_State *L)
{
	mpq_ptr a, b, c;

	q__checkops(L, 2);
	a = _checkmpq(L, 1);
	b = _tompq(L, 2);
	c = _tompq(L, 3);
	_check_divisor(L, mpq_numref(c));
	mpq_div(a, b, c);
	lua_settop(L, 1);
	return 1;
}

static int q_canonicalize(lua_State *L)
{
	mpq_ptr q;

	q = _checkmpq(L, 1);
	_check_divisor(L, mpq_denref(q));
	if (lua_type(L, 2) == LUA_TBOOLEAN && !lua_toboolean(L, 2)) {
		/* only fix the sign of denominator, */
		if (mpz_sgn(mpq_denref(q)) < 0) {
			mpz_neg(mpq_denref(q), mpq_denref(q));
			mpz_neg(mpq_numref(q), mpq_numref(q));
		}
	} else {
		/* do a full canonicalization */
		mpq_canonicalize(q);
	}
	lua_settop(L, 1);
	return 1;
}

static int q_numref(lua_State *L)
{
	mpq_ptr q;

	q = _checkmpq(L, 1);
	return z__partial_ref(L, 1, mpq_numref(q));
}

static int q_denref(lua_State *L)
{
	mpq_ptr q;

	q = _checkmpq(L, 1);
	return z__partial_ref(L, 1, mpq_denref(q));
}
#endif

#define METHOD_ALIAS(name, fun) \
	{ #name,	$_##fun	}
#define METHOD(name) METHOD_ALIAS(name, name)
#define METAMETHOD_ALIAS(name, fun) \
	{ "__"#name,	$_##fun	}
#define METAMETHOD(name) METAMETHOD_ALIAS(name, name)

static const luaL_Reg $_Meta[] = {
	METAMETHOD(gc),
	METAMETHOD(tostring),
	METAMETHOD_ALIAS(unm, neg_wrap),
	METAMETHOD(add),
	METAMETHOD(sub),
	METAMETHOD(mul),
	METAMETHOD(eq),
	METAMETHOD(lt),
#if defined(MPZ)
	METAMETHOD_ALIAS(div, ratdiv),
	METAMETHOD_ALIAS(idiv, fdiv_q),
	METAMETHOD_ALIAS(mod, fdiv_r),
	METAMETHOD(pow),
	METAMETHOD_ALIAS(band, and),
	METAMETHOD_ALIAS(bor, ior),
	METAMETHOD_ALIAS(bxor, xor),
	METAMETHOD_ALIAS(bnot, com),
#elif defined(MPQ)
	METAMETHOD(div),
#endif
	{ NULL,		NULL	}
};

static const luaL_Reg $_Reg[] =
{
	METHOD(tostring),
	METHOD(tonumber),

	METHOD(set),
	METHOD(swap),

	METHOD(abs),
	METHOD(neg),
	METHOD(add),
	METHOD(sub),
	METHOD(mul),
	METHOD(cmp),
#if defined(MPZ)
	METHOD(addmul),
	METHOD(submul),
	METHOD_ALIAS(div, intdiv),
	METHOD(pow),
	METHOD(sqrt),
	METHOD(root),

	METHOD(nextprime),
	METHOD(gcd),
	METHOD(gcdext),
	METHOD(lcm),
	METHOD(probab_prime_p),
	METHOD(fac),
	METHOD(bin),
	METHOD(fib),

	METHOD_ALIAS(band, and),
	METHOD_ALIAS(bor, ior),
	METHOD_ALIAS(bxor, xor),
	METHOD_ALIAS(bnot, com),

	METHOD(even_p),
	METHOD(odd_p),
	METHOD(perfect_power_p),
	METHOD(perfect_square_p),
	METHOD(divisible_p),

	METHOD(sizeinbase),
#elif defined(MPQ)
	METHOD(inv),
	METHOD(canonicalize),
	METHOD(numref),
	METHOD(denref),
#endif
	{ NULL,		NULL	}
};

LUALIB_API int luaopen_mp_$(lua_State *L)
{
	lua_pushcfunction(L, $_call);
	luaL_newmetatable(L, "mp$_t");
	luaL_setfuncs(L, $_Meta, 0);
	luaL_newlib(L, $_Reg);

	return _open_common(L);
}
