static mp$_ptr $_new(lua_State *L)
{
	mp$_ptr z = lua_newuserdata(L, sizeof (mp$_t));
	if (luaL_getmetatable(L, "mp$_t") == LUA_TNIL)
		luaL_error(L, "mp$_t not registered");
	mp$_init(z);
	lua_setmetatable(L, -2);
	return z;
}

#if defined(MPZ)
# define MPNAME "integer"
#elif defined(MPQ)
# define MPNAME "rational"
#endif

static void $__set_str(lua_State *L, mp$_ptr z, int i, int base)
{
	const char *s;

	s = lua_tostring(L, i);
	if (mp$_set_str(z, s, base) != 0) {
		_conversion_error(L, i, MPNAME, base);
	}
}

#ifdef MPZ
static void z__set_int(lua_State *L, mpz_ptr z, int i)
{
	lua_Integer val;

	val = lua_tonumber(L, i);
	if (CAN_HOLD(long, val)) {
		mpz_set_si(z, (long) val);
	} else {
		z__set_str(L, z, i, 0);
	}
}
#endif

static mp$_ptr _checkmp$(lua_State *L, int i, int raise)
{
	void *z;

	z = (raise) ? luaL_checkudata(L, i, "mp$_t")
		    : luaL_testudata(L, i, "mp$_t");
#ifdef MPZ
	if (z) {
		if (lua_getuservalue(L, i) == LUA_TUSERDATA
			&& luaL_testudata(L, -1, "mpq_t")) {
			/* partial ref */
			z = *(mp$_ptr *) z;
		}
		lua_pop(L, 1); /* remove uservalue */
	}
#endif
	return (mp$_ptr) z;
}

static void $__set(lua_State *L, int i, mp$_ptr z)
{
	switch (lua_type(L, i)) {
		case LUA_TNUMBER:
#if defined(MPZ)
			luaL_checkinteger(L, i);
			z__set_int(L, z, i);
#elif defined(MPQ)
			if (lua_isinteger(L, i)) {
				z__set_int(L, mpq_numref(z), i);
				mpz_set_si(mpq_denref(z), 1);
			} else {
				lua_Number val;

				val = lua_tonumber(L, i);
				luaL_argcheck(L, val == val && val * 0.0 == 0.0, i,
						"infinity or NaN");
				mp$_set_d(z, val);
			}
#endif
			break;
		case LUA_TSTRING:
			$__set_str(L, z, i, 0);
			break;
		case LUA_TUSERDATA: {
			void *p;

			if ((p = _checkmpz(L, i, 0)) != 0) {
#if defined(MPZ)
				mpz_set(z, p);
#elif defined(MPQ)
				mpq_set_z(z, p);
#endif
			} else if ((p = _checkmpq(L, i, 0)) != 0) {
#if defined(MPZ)
				mpz_ptr num, den;

				num = mpq_numref((mpq_ptr) p);
				den = mpq_denref((mpq_ptr) p);
				if (mpz_cmp_si(den, 1) == 0) {
					mpz_set(z, num);
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
			_conversion_error(L, i, MPNAME, 0);
	}
}

#ifdef MPQ
static void q_checksanity(lua_State *L, int i, mpq_ptr z)
{
	/* sanity check to prevent fp exception */
	luaL_argcheck(L, mpz_sgn(mpq_denref(z)) > 0, i, "non-canonicalized rational");
}
#endif

static mp$_ptr _tomp$(lua_State *L, int i)
{
	mp$_ptr z;

	if ((z = _checkmp$(L, i, 0)) == 0) {
		luaL_checkany(L, i);
		z = $_new(L);
		$__set(L, i, z);
		lua_replace(L, i);
	}
#ifdef MPQ
	q_checksanity(L, i, z);
#endif
	return z;
}

static int $_set(lua_State *L)
{
	mp$_ptr z;

	z = _checkmp$(L, 1, 1);
	if (lua_type(L, 2) == LUA_TSTRING && lua_isinteger(L, 3)) {
		int base;
		/* set(str, base) */
		base = _check_inbase(L, 3);
		$__set_str(L, z, 2, base);
	} else {
		$__set(L, 2, z);
	}
	lua_settop(L, 1);
	return 1;
}

static int $_call(lua_State *L)
{
	$_new(L);
	lua_replace(L, 1); /* replace the 'self' argument (the table) */
	if (lua_gettop(L) > 1)
		return $_set(L);
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

	z = _checkmp$(L, 1, 1);
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

	z = _checkmp$(L, 1, 1);
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

static int $_cmp(lua_State *L)
{
	mp$_ptr a;
	void *b;
	int ret, isint;
	lua_Integer si;

	a = _checkmp$(L, 1, 1);
#ifdef MPQ
	q_checksanity(L, 1, a);
#endif
	si = lua_tointegerx(L, 2, &isint);
	if (isint && CAN_HOLD(long, si)) {
#if defined(MPQ)
		lua_Integer si2;

		if (lua_isnone(L, 3)) {
			si2 = 1;
		} else {
			si2 = luaL_checkinteger(L, 3);
			if (!CAN_HOLD(long, si2))
				goto overflow_long;
		}
		ret = mpq_cmp_si(a, si, si2);
	} else if (isint) { /* integer, but overflows long */
		/* this can only happen when lua_Integer is longer
		 * than long */
		if (!lua_isnone(L, 3) && luaL_checkinteger(L, 3)) {
			mpz_ptr num, den;
overflow_long:
			b = q_new(L);
			num = mpq_numref((mpq_ptr) b);
			den = mpq_denref((mpq_ptr) b);
			z__set_str(L, num, 2, 0);
			z__set_int(L, den, 3);
			q_checksanity(L, 3, b);
			goto q_cmp_q;
		}
		b = z_new(L);
		z__set_str(L, b, 2, 0);
		goto q_cmp_z;
	} else if ((b = _checkmpz(L, 2, 0))) {
q_cmp_z:
		ret = mpq_cmp_z(a, b);
#elif defined(MPZ)
		ret = mpz_cmp_si(a, si);
	} else if (!isint && lua_isnumber(L, 2)) {
		ret = mpz_cmp_d(a, lua_tonumber(L, 2));
#endif
	} else { /* general case */
		b = _tomp$(L, 2);
#ifdef MPQ
q_cmp_q:
#endif
		ret = mp$_cmp(a, b);
	}
	lua_pushinteger(L, ret);
	return 1;
}

static int $_swap(lua_State *L)
{
	mp$_ptr a, b;

	a = _checkmp$(L, 1, 1);
	b = _checkmp$(L, 2, 1);
	mp$_swap(a, b);
	return 0;
}

static int $_mul_2exp(lua_State *L)
{
	mp$_ptr a, b;
	lua_Integer n;

	a = _checkmp$(L, 1, 1);
	b = _tomp$(L, 2);
	n = luaL_checkinteger(L, 3);
	luaL_argcheck(L, n >= 0, 3, "expect non-negative");
	mp$_mul_2exp(a, b, n);
	lua_settop(L, 1);
	return 1;
}

#ifdef MPZ
static const char *z_div_lst[] = {
	"cq", "fq", "tq",
	"cr", "fr", "tr",
	0,
	"cqr", "fqr", "tqr",
0};
#endif

static int $_div_2exp(lua_State *L)
{
#ifdef MPZ
	static void (*ops[])(mpz_ptr, mpz_srcptr, mp_bitcnt_t) = {
		mpz_cdiv_q_2exp, mpz_fdiv_q_2exp, mpz_tdiv_q_2exp,
		mpz_cdiv_r_2exp, mpz_fdiv_r_2exp, mpz_tdiv_r_2exp,
	};
	int mode;
#endif
	mp$_ptr r, a;
	lua_Integer b;

#ifdef MPZ
	mode = luaL_checkoption(L, 4, "fq", z_div_lst);
#endif
	r = _checkmp$(L, 1, 1);
	a = _tomp$(L, 2);
	b = luaL_checkinteger(L, 3);
	luaL_argcheck(L, b >= 0, 3, "expect non-negative");
#ifdef MPZ
	(*ops[mode])(r, a, b);
#else
	mp$_div_2exp(r, a, b);
#endif
	lua_settop(L, 1);
	return 1;
}

static int $_unop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr))
{
	(*op)(_checkmp$(L, 1, 1), _tomp$(L, 2));
	lua_settop(L, 1);
	return 1;
}

static int $_binop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr, mp$_srcptr))
{
	(*op)(_checkmp$(L, 1, 1), _tomp$(L, 2), _tomp$(L, 3));
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

	r = _checkmp$(L, 1, 1);
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

#define OP_UI(fun) \
static int $_##fun(lua_State *L) { return $_uiop(L, mp$_##fun, mp$_##fun##_ui); }

OP_UI(add)
OP_UI(sub)
OP_UI(mul)
OP_UI(divexact)
OP_UI(lcm)

OP_UI(addmul)
OP_UI(submul)

static int z__partial_ref(lua_State *L, int i, mpz_ptr z)
{
	mpz_ptr *p = lua_newuserdata(L, sizeof (mpz_ptr));
	if (luaL_getmetatable(L, "mp$_t") == LUA_TNIL)
		luaL_error(L, "mp$_t not registered");
	*p = z;
	/* temporarily remove the __gc method from metatable */
	lua_pushnil(L);
	lua_setfield(L, -2, "__gc");
	/* set metatable (without __gc method) */
	lua_pushvalue(L, -1);
	lua_setmetatable(L, -3);
	/* restore __gc method */
	lua_pushcfunction(L, z_gc);
	lua_setfield(L, -2, "__gc");
	/* pop metatable */
	lua_pop(L, 1);

	lua_pushvalue(L, i);
	lua_setuservalue(L, -2); /* reference the object */
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

static int z_idiv(lua_State *L)
{
	int mode;
	mpz_ptr q, r, a, b;

	q = _checkmpz(L, 1, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	_check_divisor(L, b);
	if ((r = _checkmpz(L, 4, 0)) != 0) {
		/* return both quotient and remainder */
		static void (*ops[])(mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr) = {
			mpz_cdiv_qr, mpz_fdiv_qr, mpz_tdiv_qr
		};
		mode = luaL_checkoption(L, 5, "fqr", z_div_lst + 7);
		(*ops[mode])(q, r, a, b);
		lua_rotate(L, 2, -2);
		lua_settop(L, 2);
		return 2;
	} else {
		/* return either quotient or remainder */
		static void (*ops[])(mpz_ptr, mpz_srcptr, mpz_srcptr) = {
			mpz_cdiv_q, mpz_fdiv_q, mpz_tdiv_q,
			mpz_cdiv_r, mpz_fdiv_r, mpz_tdiv_r,
		};
		mode = luaL_checkoption(L, 4, "fq", z_div_lst);
		(*ops[mode])(q, a, b);
		lua_settop(L, 1);
		return 1;
	}
}

static int z_mod(lua_State *L) { lua_pushliteral(L, "fr"); return z_idiv(L); }

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
	g = _checkmpz(L, 1, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	if (lua_isnone(L, 4)) {
		mpz_gcd(g, a, b);
	} else {
		s = _checkmpz(L, 4, 1);
		t = lua_isnone(L, 5) ? NULL : _checkmpz(L, 5, 1);
		mpz_gcdext(g, s, t, a, b);
	}
	lua_rotate(L, 2, -2);
	lua_settop(L, top-2);
	return top-2;
}

static int z_fac(lua_State *L)
{
	mpz_ptr z;
	unsigned long n, m;

	z = _checkmpz(L, 1, 1);
	n = _castulong(L, 2);
	if (lua_isnone(L, 3))
		goto simple;
	m = _castulong(L, 3);
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

	z = _checkmpz(L, 1, 1);
	n = _tompz(L, 2);
	k = _castulong(L, 3);
	mpz_bin_ui(z, n, k);
	lua_settop(L, 1);
	return 1;
}

static int z_fib(lua_State *L)
{
	mpz_ptr z, z1;
	unsigned long n;

	z = _checkmpz(L, 1, 1);
	n = _castulong(L, 2);
	if (lua_isnone(L, 3)) {
		mpz_fib_ui(z, n);
		lua_settop(L, 1);
		return 1;
	} else {
		z1 = _checkmpz(L, 3, 1);
		mpz_fib2_ui(z, z1, n);
		lua_rotate(L, 2, -1);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_invert(lua_State *L)
{
	mpz_ptr r, a, b;

	r = _checkmpz(L, 1, 1);
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
	lua_Integer val;
	int isnum;

	r = _checkmpz(L, 1, 1);
	a = _tompz(L, 2);
	val = lua_tointegerx(L, 3, &isnum);
	c = _tompz(L, 4);
	if (isnum && val >= 0 && CAN_HOLD(unsigned long, val)) {
		mpz_powm_ui(r, a, val, c);
	} else {
		b = _tompz(L, 3);
		if (mpz_sgn(b) < 0) {
			mpz_t tmp;

			mpz_init(tmp);
			if (!mpz_invert(tmp, a, c)) {
				mpz_clear(tmp);
				luaL_error(L, "inverse does not exist");
			}
			mpz_neg(b, b);
			mpz_powm(r, tmp, b, c);
			mpz_neg(b, b);
			mpz_clear(tmp);
		} else {
			mpz_powm(r, a, b, c);
		}
	}

	lua_settop(L, 1);
	return 1;
}

static int z_pow(lua_State *L)
{
	mpz_ptr z, base;
	unsigned long exp;

	z = _checkmpz(L, 1, 1);
	base = _tompz(L, 2);
	exp = _castulong(L, 3);

	mpz_pow_ui(z, base, exp);
	lua_settop(L, 1);
	return 1;
}

static int z_root(lua_State *L)
{
	mpz_ptr root, rem, z;
	lua_Integer n;

	root = _checkmpz(L, 1, 1);
	z = _tompz(L, 2);
	n = _castulong(L, 3);
	if (lua_isnone(L, 4)) {
		mpz_root(root, z, n);
		lua_settop(L, 1);
		return 1;
	} else {
		rem = _checkmpz(L, 4, 1);
		mpz_rootrem(root, rem, z, n);
		lua_rotate(L, 2, -2);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_sqrt(lua_State *L)
{
	mpz_ptr root, rem, z;

	root = _checkmpz(L, 1, 1);
	z = _tompz(L, 2);
	if (lua_isnone(L, 3)) {
		mpz_sqrt(root, z);
		lua_settop(L, 1);
		return 1;
	} else {
		rem = _checkmpz(L, 3, 1);
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

	q = _checkmpq(L, 1, 1);
	r = _tompq(L, 2);
	_check_divisor(L, mpq_numref(q));
	mpq_inv(q, r);
	lua_settop(L, 1);
	return 1;
}

static int q_div(lua_State *L)
{
	mpq_ptr a, b, c;
	mpz_ptr zb, zc;

	a = _checkmpq(L, 1, 1);
	if (lua_isinteger(L, 2) && lua_isinteger(L, 3)) {
		z__set_int(L, mpq_numref(a), 2);
		z__set_int(L, mpq_denref(a), 3);
	} else if ((zb = _checkmpz(L, 2, 0)) && (zc = _checkmpz(L, 3, 0))) {
		mpz_set(mpq_numref(a), zb);
		mpz_set(mpq_denref(a), zc);
	} else {
		b = _tompq(L, 2);
		c = _tompq(L, 3);
		_check_divisor(L, mpq_numref(c));
		mpq_div(a, b, c);
	}
	lua_settop(L, 1);
	return 1;
}

static int q_canonicalize(lua_State *L)
{
	mpq_ptr q;

	q = _checkmpq(L, 1, 1);
	_check_divisor(L, mpq_denref(q));
	mpq_canonicalize(q);
	lua_settop(L, 1);
	return 1;
}

static int q_numref(lua_State *L)
{
	return z__partial_ref(L, 1, mpq_numref(_checkmpq(L, 1, 1)));
}

static int q_denref(lua_State *L)
{
	return z__partial_ref(L, 1, mpq_denref(_checkmpq(L, 1, 1)));
}

static int $_equal(lua_State *L)
{
	mpq_ptr a, b;
	int ret;

	a = _checkmpq(L, 1, 1);
	b = _tompq(L, 2);
	ret = mpq_equal(a, b);
	lua_pushboolean(L, ret);
	return 1;
}

#endif

static int $_inp_str(lua_State *L)
{
	mp$_ptr z;
	FILE *f;
	int base;
	size_t n;

	z = _checkmp$(L, 1, 1);
	f = _checkfile(L, 2);
	base = _check_inbase(L, 3);

	n = mp$_inp_str(z, f, base);
	lua_pushinteger(L, (lua_Integer) n);
	return 1;
}

static int $_out_str(lua_State *L)
{
	mp$_ptr z;
	FILE *f;
	int base;
	size_t n;

	z = _checkmp$(L, 1, 1);
	f = _checkfile(L, 2);
	base = _check_outbase(L, 3);

	n = mp$_out_str(f, base, z);
	lua_pushinteger(L, (lua_Integer) n);
	return 1;
}

#define METHOD_ALIAS(name, fun) \
	{ #name,	$_##fun	}
#define METHOD(name) METHOD_ALIAS(name, name)
#define METAMETHOD_ALIAS(name, fun) \
	{ "__"#name,	$_##fun	}
#define METAMETHOD(name) METAMETHOD_ALIAS(name, name)

static const luaL_Reg $_Meta[] = {
	/* Note: keep __gc the first */
	METAMETHOD(gc),
	METAMETHOD(tostring),
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
	METHOD(mul_2exp),
	METHOD(div_2exp),

	METHOD(cmp),
#if defined(MPZ)
	METHOD(addmul),
	METHOD(submul),
	METHOD_ALIAS(div, idiv),
	METHOD(divexact),
	METHOD(mod),
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
	METHOD(div),
	METHOD(inv),
	METHOD(canonicalize),
	METHOD(numref),
	METHOD(denref),
	METHOD(equal),
#endif
	METHOD(inp_str),
	METHOD(out_str),
	{ NULL,		NULL	}
};

LUALIB_API int luaopen_mp_$(lua_State *L)
{
	lua_pushcfunction(L, $_call);
	luaL_newlib(L, $_Reg);
	luaL_newmetatable(L, "mp$_t");
	luaL_setfuncs(L, $_Meta, 0);

	return _open_common(L);
}
