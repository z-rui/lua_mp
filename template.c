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
				mpz_ptr num, den;

				num = mpq_numref((mpq_ptr) p);
				den = mpq_denref((mpq_ptr) p);
				if (mpz_sgn(den) != 0 && mpz_divisible_p(num, den)) {
					mpz_divexact(z, num, den);
				} else {
					goto error;
				}
#elif defined(MPQ)
				mpq_set(z, p);
#endif
			} else {
				goto error;
			}
			break;
		}
		default:
error:
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
		luaL_checkany(L, i);
		z = $_new(L);
		$__set(L, i, z);
		lua_replace(L, i);
	}
#ifdef MPZ
	if (lua_getuservalue(L, i) == LUA_TUSERDATA) {
		/* partial ref */
		z = *(mp$_ptr *) z;
	}
	lua_pop(L, 1);
#endif
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

static int $_swap(lua_State *L)
{
	mp$_ptr a, b;

	a = _checkmp$(L, 1);
	b = _checkmp$(L, 2);
	mp$_swap(a, b);
	return 0;
}

static void $__fixmeta(lua_State *L)
{
	if (lua_toboolean(L, lua_upvalueindex(1))) {
		$_new(L);
		lua_insert(L, 1);
	}
}

static int $_unop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr))
{
	$__fixmeta(L);
	(*op)(_checkmp$(L, 1), _tomp$(L, 2));
	lua_settop(L, 1);
	return 1;
}

static int $_binop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr, mp$_srcptr))
{
	$__fixmeta(L);
	(*op)(_checkmp$(L, 1), _tomp$(L, 2), _tomp$(L, 3));
	lua_settop(L, 1);
	return 1;
}

#define OP_DCL(typ, fun) \
static int $_##fun(lua_State *L) { return $_##typ(L, mp$_##fun); }

OP_DCL(unop, abs)
OP_DCL(unop, neg)

#ifdef MPZ /* integer specific functions */
OP_DCL(unop, nextprime)
OP_DCL(unop, com)
OP_DCL(binop, and)
OP_DCL(binop, ior)
OP_DCL(binop, xor)

static int $_uiop(lua_State *L, void (*fun)(mp$_ptr, mp$_srcptr, mp$_srcptr), void (*fun_ui)(mp$_ptr, mp$_srcptr, unsigned long))
{
	mp$_ptr r, a;
	lua_Integer val;
	int isnum;

	r = _checkmp$(L, 1);
	a = _tomp$(L, 2);
	val = lua_tointegerx(L, 3, &isnum);
	if (isnum && val >= 0 && CAN_HOLD(unsigned long, val)) {
		(*fun_ui)(r, a, val);
	} else {
		(*fun)(r, a, _tomp$(L, 3));
	}
	lua_settop(L, 1);
	return 1;
}

#define OP_BIN_UI(fun) \
static int $_##fun(lua_State *L) { $__fixmeta(L); return $_uiop(L, mp$_##fun, mp$_##fun##_ui); }

OP_BIN_UI(add)
OP_BIN_UI(sub)
OP_BIN_UI(mul)
OP_BIN_UI(divexact)
OP_BIN_UI(lcm)

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

#define OP_TERN_UI(fun) \
static int $_##fun(lua_State *L) { return $_uiop(L, mp$_##fun, mp$_##fun##_ui); }

OP_TERN_UI(addmul)
OP_TERN_UI(submul)

static int z_mul_2exp(lua_State *L)
{
	mpz_ptr a, b;
	lua_Integer n;

	z__fixmeta(L);
	a = _checkmpz(L, 1);
	b = _tompz(L, 2);
	n = luaL_checkinteger(L, 3);
	luaL_argcheck(L, n >= 0, 3, "expect non-negative");
	mpz_mul_2exp(a, b, n);
	lua_settop(L, 1);
	return 1;
}

static int q_div(lua_State *L);

static int z__intdiv(lua_State *L, int mode)
{
	static void (*ops1[])(mpz_ptr, mpz_srcptr, mpz_srcptr) = {
		mpz_cdiv_q, mpz_fdiv_q, mpz_tdiv_q,
		mpz_cdiv_r, mpz_fdiv_r, mpz_tdiv_r,
	};
	static void (*ops2[])(mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr) = {
		mpz_cdiv_qr, mpz_fdiv_qr, mpz_tdiv_qr
	};
	mpz_ptr q, r, a, b;

	q = _checkmpz(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	_check_divisor(L, b);
	if (mode > 5) {
		r = _checkmpz(L, 4);
		(*ops2[mode-6])(q, r, a, b);
		lua_rotate(L, 2, -2);
		lua_settop(L, 2);
		return 2;
	} else {
		(*ops1[mode])(q, a, b);
		lua_settop(L, 1);
		return 1;
	}
}

static int z_intdiv(lua_State *L)
{
	const char *modestr;
	int mode;

	if (!lua_isstring(L, 2)) {
no_modestr:
		return lua_isnone(L, 4)
			? z__intdiv(L, 1)	/* fq */
			: z__intdiv(L, 7);	/* fqr */
	}
	modestr = lua_tostring(L, 2);
	/* try parsing modestr */
	if (modestr[0] == 'c') {
		mode = 0;
	} else if (modestr[0] == 'f') {
		mode = 1;
	} else if (modestr[0] == 't') {
		mode = 2;
	} else {
		/* not a valid modestr
		 * maybe it is a number? */
		goto no_modestr;
	}
	if (modestr[1] == 'q') {
		if (modestr[2] == '\0') {
			; /* ok */
		} else if (modestr[2] == 'r' && modestr[3] == '\0') {
			mode += 6;
		} else {
			goto mode_error;
		}
	} else if (modestr[1] == 'r' && modestr[2] == '\0') {
		mode += 3;
	} else {
mode_error:
		luaL_argerror(L, 2, "expect one of: cq cr cqr fq fr fqr tq tr tqr");
	}
	lua_remove(L, 2);
	return z__intdiv(L, mode);
}

static int z_fdiv_q(lua_State *L) { z_new(L); lua_insert(L, 1); return z__intdiv(L, 1); }
static int z_fdiv_r(lua_State *L) { z_new(L); lua_insert(L, 1); return z__intdiv(L, 4); }

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
	int top;

	top = lua_gettop(L);
	g = _checkmpz(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	s = lua_isnone(L, 4) ? NULL : _checkmpz(L, 4);
	t = lua_isnone(L, 5) ? NULL : _checkmpz(L, 5);
	if (s)
		mpz_gcdext(g, s, t, a, b);
	else
		mpz_gcd(g, a, b);
	lua_rotate(L, 2, -2);
	lua_settop(L, top-2);
	return top-2;
}

static int z_fac(lua_State *L)
{
	mpz_ptr z;
	unsigned long n, m;

	z = _checkmpz(L, 1);
	n = _checkulong(L, 2);
	if (lua_isnone(L, 3))
		goto simple;
	m = _checkulong(L, 3);
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
	unsigned long k;

	z = _checkmpz(L, 1);
	n = _tompz(L, 2);
	k = _checkulong(L, 3);
	mpz_bin_ui(z, n, k);
	lua_settop(L, 1);
	return 1;
}

static int z_fib(lua_State *L)
{
	mpz_ptr z, z1;
	unsigned long n;

	z = _checkmpz(L, 1);
	n = _checkulong(L, 2);
	if (lua_isnone(L, 3)) {
		mpz_fib_ui(z, n);
		lua_settop(L, 1);
		return 1;
	} else {
		z1 = _checkmpz(L, 3);
		mpz_fib2_ui(z, z1, n);
		lua_rotate(L, 2, -1);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_invert(lua_State *L)
{
	mpz_ptr r, a, b;

	r = _checkmpz(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	if (mpz_invert(r, a, b) == 0) {
		return luaL_error(L, "inverse does not exist");
	}
	lua_settop(L, 1);
	return 1;
}

static int z_powm(lua_State *L)
{
	mpz_ptr r, a, b, c;

	r = _checkmpz(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	c = _tompz(L, 4);
	luaL_argcheck(L, mpz_sgn(b) >= 0, 3, "expect non-negative");

	mpz_powm(r, a, b, c);
	lua_settop(L, 1);
	return 1;
}

static int z_pow(lua_State *L)
{
	mpz_ptr z, base;
	unsigned long exp;

	z__fixmeta(L);
	z = _checkmpz(L, 1);
	base = _tompz(L, 2);
	exp = _checkulong(L, 3);

	mpz_pow_ui(z, base, exp);
	lua_settop(L, 1);
	return 1;
}

static int z_root(lua_State *L)
{
	mpz_ptr root, rem, z;
	lua_Integer n;

	root = _checkmpz(L, 1);
	z = _tompz(L, 2);
	n = _checkulong(L, 3);
	if (lua_isnone(L, 4)) {
		mpz_root(root, z, n);
		lua_settop(L, 1);
		return 1;
	} else {
		rem = _checkmpz(L, 4);
		mpz_rootrem(root, rem, z, n);
		lua_rotate(L, 2, -2);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_sqrt(lua_State *L)
{
	mpz_ptr root, rem, z;

	root = _checkmpz(L, 1);
	z = _tompz(L, 2);
	if (lua_isnone(L, 3)) {
		mpz_sqrt(root, z);
		lua_settop(L, 1);
		return 1;
	} else {
		rem = _checkmpz(L, 3);
		mpz_sqrtrem(root, rem, z);
		lua_rotate(L, 2, -1);
		lua_settop(L, 2);
		return 2;
	}
}
#endif
#ifdef MPQ /* rational specfic functions */
OP_DCL(binop, add)
OP_DCL(binop, sub)
OP_DCL(binop, mul)

static int q_inv(lua_State *L)
{
	mpq_ptr q, r;

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

	q__fixmeta(L);
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
	METAMETHOD_ALIAS(unm, neg),
	METAMETHOD(add),
	METAMETHOD(sub),
	METAMETHOD(mul),
	{ "__eq",	mp_eq	},
	{ "__lt",	mp_lt	},
#if defined(MPZ)
	METAMETHOD_ALIAS(shl, mul_2exp),
	{ "__div",	q_div	},
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
	{ "cmp",	mp_cmp	},
#if defined(MPZ)
	METHOD(addmul),
	METHOD(submul),
	METHOD(mul_2exp),
	METHOD_ALIAS(div, intdiv),
	METHOD(divexact),
	METHOD(pow),
	METHOD(sqrt),
	METHOD(root),

	METHOD(nextprime),
	METHOD_ALIAS(gcd, gcdext),
	METHOD(lcm),
	METHOD(probab_prime_p),
	METHOD(fac),
	METHOD(bin),
	METHOD(fib),
	METHOD(invert),
	METHOD(powm),

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
	lua_pushboolean(L, 1);
	luaL_setfuncs(L, $_Meta, 1);
	luaL_newlib(L, $_Reg);

	return _open_common(L);
}
