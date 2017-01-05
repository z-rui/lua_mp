#ifdef MPF
static mp_bitcnt_t f__get_default_prec(lua_State *L)
{
	lua_Integer val;
	int isint;

	luaL_getmetatable(L, "mp$_t");
	lua_getfield(L, -1, "__index"); /* lib table */
	lua_getfield(L, -1, "precision");
	val = lua_tointegerx(L, -1, &isint);
	if (!isint || !CAN_HOLD(mp_bitcnt_t, val))
		luaL_error(L, "precision cannot be represented as mp_bitcnt_t");
	lua_pop(L, 3); /* metatable, lib table and precision */
	return (mp_bitcnt_t) val;
}

static mpf_ptr f_new(lua_State *L, mp_bitcnt_t prec)
#else
static mp$_ptr $_new(lua_State *L)
#endif
{
	mp$_ptr z = lua_newuserdata(L, sizeof (mp$_t));
	if (luaL_getmetatable(L, "mp$_t") == LUA_TNIL)
		luaL_error(L, "mp$_t not registered");
#ifdef MPF
	mpf_init2(z, prec);
#else
	mp$_init(z);
#endif
	lua_setmetatable(L, -2);
	return z;
}

#if defined(MPZ)
# define MPNAME "integer"
#elif defined(MPQ)
# define MPNAME "rational"
#elif defined(MPF)
# define MPNAME "float"
#endif

static void $__set_str(lua_State *L, mp$_ptr z, int i, int base)
{
	const char *s;

	s = lua_tostring(L, i);
	if (mp$_set_str(z, s, base) != 0) {
		_conversion_error(L, i, MPNAME, base);
	}
}

#ifndef MPQ
static void $__set_int(lua_State *L, mp$_ptr z, int i)
{
	lua_Integer val;

	val = lua_tointeger(L, i);
	if (CAN_HOLD(long, val)) {
		mp$_set_si(z, (long) val);
	} else {
		$__set_str(L, z, i, 0);
	}
}
#endif

static void $__set(lua_State *L, int i, mp$_ptr z)
{
	switch (lua_type(L, i)) {
		case LUA_TNUMBER:
#ifdef MPZ
			luaL_checkinteger(L, i);
			z__set_int(L, z, i);
#else
			if (lua_isinteger(L, i)) {
# ifdef MPQ
				z__set_int(L, mpq_numref(z), i);
				mpz_set_si(mpq_denref(z), 1);
# else /* MPF */
				$__set_int(L, z, i);
# endif
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

			if ((p = _checkmp(L, i, 0, 'z'))) {
#ifdef MPZ
				mpz_set(z, p);
#else
				mp$_set_z(z, p);
#endif
			} else if ((p = _checkmp(L, i, 0, 'q'))) {
#if defined(MPZ)
				mpz_ptr num, den;

				num = mpq_numref((mpq_ptr) p);
				den = mpq_denref((mpq_ptr) p);
				if (mpz_cmp_si(den, 1) != 0)
					goto error;
				mpz_set(z, num);
#elif defined(MPQ)
				mpq_set(z, p);
#elif defined(MPF)
				mpf_set_q(z, p);
#endif
			} else if ((p = _checkmp(L, i, 0, 'f'))) {
#ifdef MPF
				mpf_set(z, p);
#else
# ifdef MPZ
				if (!mpf_integer_p(p))
					goto error;
# endif
				mp$_set_f(z, p);
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

static mp$_ptr $__check(lua_State *L, int i)
{
	return _checkmp(L, i, 1, '$');
}

static mp$_ptr _tomp$(lua_State *L, int i)
{
	mp$_ptr z;

	if ((z = _checkmp(L, i, 0, '$')) == 0) {
		luaL_checkany(L, i);
#ifdef MPF
		z = f_new(L, f__get_default_prec(L));
#else
		z = $_new(L);
#endif
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

	z = $__check(L, 1);
	if (!lua_isnoneornil(L, 3)) {
		int base;
		/* 2 arguments: z:set(str, base) */
		luaL_checkstring(L, 2);
		base = _check_inbase(L, 3);
		$__set_str(L, z, 2, base);
	} else {
		/* 1 argument z:set(value) */
		$__set(L, 2, z);
	}
	lua_settop(L, 1);
	return 1;
}

static int $_call(lua_State *L)
{
#ifdef MPF
	mp_bitcnt_t prec;

	if (lua_isnoneornil(L, 4))
		prec = f__get_default_prec(L);
	else
		prec = _castbitcnt(L, 4);
	f_new(L, prec);
#else
	$_new(L);
#endif
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
	int base, absbase;
	luaL_Buffer B;
	size_t sz;
	char *s;
#ifdef MPF
	size_t n_digits;
	mp_exp_t exp;
#endif

	z = $__check(L, 1);
	base = _check_outbase(L, 2);
	absbase = abs(base);
#if defined(MPZ)
	sz = mpz_sizeinbase(z, absbase) + 2;
#elif defined(MPQ)
	sz = mpz_sizeinbase(mpq_numref(z), absbase)
	   + mpz_sizeinbase(mpq_denref(z), absbase)
	   + 3;
#elif defined(MPF)
	n_digits = (size_t) luaL_optinteger(L, 3, 0);
	if (n_digits == 0) {
		/* guess buffer size */
		n_digits = absbase;
		sz = 0;
		while (n_digits >>= 1) ++sz;
		sz = mpf_get_prec(z) / sz + 10;
	} else {
		sz = n_digits + 4;
	}
#endif
	s = luaL_buffinitsize(L, &B, sz);
#ifdef MPF
	mpf_get_str(s+1, &exp, base, n_digits, z);
	/* assert(strlen(s+1) < sz-1); */
	sz = strlen(s+1);
	if (sz == 0) {
		memcpy(s, "0.", 2);
		luaL_addsize(&B, 2);
	} else {
		size_t n;
		int scientific = 0;

		n = s[1] == '-';
		if (0 <= exp && exp <= sz) {
			n += exp;
		} else {
			n++;
			scientific = 1;
		}
		memmove(s, s+1, n);
		s[n] = '.';
		luaL_addsize(&B, sz+1);
		if (scientific) {
			luaL_addchar(&B, (absbase > 10) ? '@' : 'e');
			lua_pushfstring(L, "%I", (lua_Integer) exp - 1);
			luaL_addvalue(&B);
		}
	}
	luaL_pushresult(&B);
#else
	mp$_get_str(s, base, z);
	luaL_pushresultsize(&B, strlen(s));
#endif

	return 1;
}

static int $_tonumber(lua_State *L)
{
	mp$_ptr z;
	double val;

	z = $__check(L, 1);
#ifdef MPZ
	/* FIXME 'signed long' maybe shorter than 'lua_Integer' */
	if (mpz_fits_slong_p(z)) {
		long val;

		val = mpz_get_si(z);
		lua_pushinteger(L, val);
		return 1;
	}
#endif
#ifdef MPQ
	q_checksanity(L, 1, z);
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

	a = $__check(L, 1);
#ifdef MPQ
	q_checksanity(L, 1, a);
#endif
	si = lua_tointegerx(L, 2, &isint);
	if (isint && CAN_HOLD(long, si)) {
#ifdef MPQ
		ret = mpq_cmp_si(a, si, 1);
#else
		ret = mp$_cmp_si(a, si);
#endif
#ifndef MPZ
	} else if ((b = _checkmp(L, 2, 0, 'z'))) {
		ret = mp$_cmp_z(a, b);
#endif
#ifndef MPQ /* there is no mpq_cmp_d */
	} else if (!isint && lua_type(L, 2) == LUA_TNUMBER) {
		lua_Number val;

		val = lua_tonumber(L, 2);
		luaL_argcheck(L, val == val, 2, "NaN");
		ret = mp$_cmp_d(a, val);
#endif
	} else { /* general case */
		b = _tomp$(L, 2);
		ret = mp$_cmp(a, b);
	}
	lua_pushinteger(L, ret);
	return 1;
}

static int $_sgn(lua_State *L)
{
	mp$_ptr z;

	/* Careful, mp$_sgn is a macro that evaluate its
	 * argument multiple times */
	z = _tomp$(L, 1);
	lua_pushinteger(L, mp$_sgn(z));
	return 1;
}

static int $_swap(lua_State *L)
{
	mp$_ptr a, b;

	a = $__check(L, 1);
	b = $__check(L, 2);
	mp$_swap(a, b);
	return 0;
}

static int $_mul_2exp(lua_State *L)
{
	mp$_ptr a, b;
	mp_bitcnt_t n;

	a = $__check(L, 1);
	b = _tomp$(L, 2);
	n = _castbitcnt(L, 3);
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
	mp_bitcnt_t b;

#ifdef MPZ
	mode = luaL_checkoption(L, 4, "fq", z_div_lst);
#endif
	r = $__check(L, 1);
	a = _tomp$(L, 2);
	b = _castbitcnt(L, 3);
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
	(*op)($__check(L, 1), _tomp$(L, 2));
	lua_settop(L, 1);
	return 1;
}

#ifndef MPF
static int $_binop(lua_State *L, void (*op)(mp$_ptr, mp$_srcptr, mp$_srcptr))
{
	(*op)($__check(L, 1), _tomp$(L, 2), _tomp$(L, 3));
	lua_settop(L, 1);
	return 1;
}
#endif

#define OP_DCL(typ, fun) \
static int $_##fun(lua_State *L) { return $_##typ(L, mp$_##fun); }

OP_DCL(unop, abs)
OP_DCL(unop, neg)

#if defined(MPZ) || defined(MPF)
static int $_uiop(lua_State *L, void (*fun)(mp$_ptr, mp$_srcptr, mp$_srcptr), void (*fun_ui)(mp$_ptr, mp$_srcptr, unsigned long))
{
	mp$_ptr r, a;
	lua_Integer val;
	int isnum;

	r = $__check(L, 1);
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

static int $_urandomb(lua_State *L)
{
	mp$_ptr r;
	gmp_randstate_t *state;
	mp_bitcnt_t n;

	r = $__check(L, 1);
	state = luaL_checkudata(L, 2, "gmp_randstate_t");
	n = _castbitcnt(L, 3);
	mp$_urandomb(r, *state, n);
	lua_settop(L, 1);
	return 1;
}

#endif

#if defined(MPZ) /* integer specific functions */
OP_DCL(unop, nextprime)
OP_DCL(unop, com)
OP_DCL(binop, and)
OP_DCL(binop, ior)
OP_DCL(binop, xor)

static void z_push_bitcnt_res(lua_State *L, mp_bitcnt_t res)
{
	if (!~res) {
		/* res is the "largest possible" mp_bitcnt_t,
		 * i.e. all 1s in binary form */
		lua_pushnil(L);
	} else {
		lua_pushinteger(L, res);
	}
}

static int z_popcount(lua_State *L)
{
	mpz_ptr z;
	mp_bitcnt_t res;

	z = _tompz(L, 1);
	res = mpz_popcount(z);
	z_push_bitcnt_res(L, res);
	return 1;
}

static int z_hamdist(lua_State *L)
{
	mpz_ptr a, b;
	mp_bitcnt_t res;

	a = _tompz(L, 1);
	b = _tompz(L, 2);
	res = mpz_hamdist(a, b);
	z_push_bitcnt_res(L, res);
	return 1;
}

static int z_scan(lua_State *L)
{
	mpz_ptr z;
	int what;
	mp_bitcnt_t starting_bit, res;

	z = _tompz(L, 1);
	what = luaL_checkinteger(L, 2);
	luaL_argcheck(L, what == 0 || what == 1, 2, "expect 0 or 1");
	starting_bit = _castbitcnt(L, 3);
	res = (what) ? mpz_scan1(z, starting_bit) : mpz_scan0(z, starting_bit);
	z_push_bitcnt_res(L, res);
	return 1;
}

static int z__change_bit(lua_State *L, void (*op)(mpz_ptr, mp_bitcnt_t))
{
	mpz_ptr z;
	mp_bitcnt_t bit_index;

	z = z__check(L, 1);
	bit_index = _castbitcnt(L, 2);
	(*op)(z, bit_index);
	return 0;
}

static int z_setbit(lua_State *L) { return z__change_bit(L, mpz_setbit); }
static int z_clrbit(lua_State *L) { return z__change_bit(L, mpz_clrbit); }
static int z_combit(lua_State *L) { return z__change_bit(L, mpz_combit); }

static int z_tstbit(lua_State *L)
{
	mpz_ptr z;
	mp_bitcnt_t bit_index;
	int res;

	z = _tompz(L, 1);
	bit_index = _castbitcnt(L, 2);
	res = mpz_tstbit(z, bit_index);
	lua_pushboolean(L, res);
	return 1;
}

OP_UI(divexact)
OP_UI(lcm)

OP_UI(addmul)
OP_UI(submul)

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

static int z_cmpabs(lua_State *L)
{
	mpz_ptr a, b;
	int ret, isint;
	lua_Integer ui;

	a = $__check(L, 1);
	ui = lua_tointegerx(L, 2, &isint);
	if (isint && ui >= 0 && CAN_HOLD(unsigned long, ui)) {
		ret = mpz_cmpabs_ui(a, ui);
	} else if (!isint && lua_isnumber(L, 2)) {
		lua_Number val;

		val = lua_tonumber(L, 2);
		luaL_argcheck(L, val == val, 2, "NaN");
		ret = mpz_cmpabs_d(a, val);
	} else { /* general case */
		b = _tomp$(L, 2);
		ret = mp$_cmpabs(a, b);
	}
	lua_pushinteger(L, ret);
	return 1;
}

static int z_idiv(lua_State *L)
{
	int mode;
	mpz_ptr q, r, a, b;

	q = z__check(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	_check_divisor(L, b);
	if ((r = _checkmp(L, 4, 0, 'z'))) {
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
	g = z__check(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	if (lua_isnone(L, 4)) {
		mpz_gcd(g, a, b);
	} else {
		s = z__check(L, 4);
		t = lua_isnone(L, 5) ? NULL : z__check(L, 5);
		mpz_gcdext(g, s, t, a, b);
	}
	lua_rotate(L, 2, -2);
	lua_settop(L, top-2);
	return top-2;
}

static int z_jacobi(lua_State *L)
{
	mpz_ptr a, b;
	int res;

	a = _tompz(L, 1);
	b = _tompz(L, 2);
	res = mpz_jacobi(a, b);
	lua_pushinteger(L, res);
	return 1;
}

static int z_remove(lua_State *L)
{
	mpz_ptr r, a, b;
	mp_bitcnt_t res;

	r = z__check(L, 1);
	a = _tompz(L, 2);
	b = _tompz(L, 3);
	res = mpz_remove(r, a, b);
	lua_settop(L, 1);
	lua_pushinteger(L, res);
	return 2;
}

static int z_fac(lua_State *L)
{
	mpz_ptr z;
	unsigned long n, m;

	z = z__check(L, 1);
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

static int z_primorial(lua_State *L)
{
	mpz_ptr z;
	unsigned long n;

	z = z__check(L, 1);
	n = _castulong(L, 2);
	mpz_primorial_ui(z, n);
	lua_settop(L, 1);
	return 1;
}

static int z_bin(lua_State *L)
{
	mpz_ptr z, n;
	unsigned long k;

	z = z__check(L, 1);
	n = _tompz(L, 2);
	k = _castulong(L, 3);
	mpz_bin_ui(z, n, k);
	lua_settop(L, 1);
	return 1;
}

static int z__special_seq(
	lua_State *L,
	void (*fun)(mpz_ptr, unsigned long),
	void (*fun2)(mpz_ptr, mpz_ptr, unsigned long))
{
	mpz_ptr z, z1;
	unsigned long n;

	z = z__check(L, 1);
	n = _castulong(L, 2);
	if (lua_isnone(L, 3)) {
		(*fun)(z, n);
		lua_settop(L, 1);
		return 1;
	} else {
		z1 = z__check(L, 3);
		(*fun2)(z, z1, n);
		lua_rotate(L, 2, -1);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_fib(lua_State *L) { return z__special_seq(L, mpz_fib_ui, mpz_fib2_ui); }
static int z_lucnum(lua_State *L) { return z__special_seq(L, mpz_lucnum_ui, mpz_lucnum2_ui); }

static int z_invert(lua_State *L)
{
	mpz_ptr r, a, b;

	r = z__check(L, 1);
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

	r = z__check(L, 1);
	a = _tompz(L, 2);
	val = lua_tointegerx(L, 3, &isnum);
	c = _tompz(L, 4);
	_check_divisor(L, c);
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

	z = z__check(L, 1);
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

	root = z__check(L, 1);
	z = _tompz(L, 2);
	n = _castulong(L, 3);
	if (lua_isnone(L, 4)) {
		mpz_root(root, z, n);
		lua_settop(L, 1);
		return 1;
	} else {
		rem = z__check(L, 4);
		mpz_rootrem(root, rem, z, n);
		lua_rotate(L, 2, -2);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_sqrt(lua_State *L)
{
	mpz_ptr root, rem, z;

	root = z__check(L, 1);
	z = _tompz(L, 2);
	if (lua_isnone(L, 3)) {
		mpz_sqrt(root, z);
		lua_settop(L, 1);
		return 1;
	} else {
		rem = z__check(L, 3);
		mpz_sqrtrem(root, rem, z);
		lua_rotate(L, 2, -1);
		lua_settop(L, 2);
		return 2;
	}
}

static int z_urandomm(lua_State *L)
{
	mpz_ptr r, n;
	gmp_randstate_t *state;

	r = z__check(L, 1);
	state = luaL_checkudata(L, 2, "gmp_randstate_t");
	n = z__check(L, 3);
	mpz_urandomm(r, *state, n);
	lua_settop(L, 1);
	return 1;
}

static int z_rrandomb(lua_State *L)
{
	mpz_ptr r;
	gmp_randstate_t *state;
	mp_bitcnt_t n;

	r = z__check(L, 1);
	state = luaL_checkudata(L, 2, "gmp_randstate_t");
	n = _castbitcnt(L, 3);
	mpz_rrandomb(r, *state, n);
	lua_settop(L, 1);
	return 1;
}
#elif defined(MPQ) /* rational specfic functions */
OP_DCL(binop, add)
OP_DCL(binop, sub)
OP_DCL(binop, mul)

static int q_inv(lua_State *L)
{
	mpq_ptr q, r;

	q = q__check(L, 1);
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

	a = q__check(L, 1);
	zb = zc = 0;
	if (	(lua_isinteger(L, 2) || (zb = _checkmp(L, 2, 0, 'z'))) &&
		(lua_isinteger(L, 3) || (zc = _checkmp(L, 3, 0, 'z')))) {
		/* b and c are integers, just set {num,den} */
		if (zb)
			mpz_set(mpq_numref(a), zb);
		else
			z__set_int(L, mpq_numref(a), 2);
		if (zc)
			mpz_set(mpq_denref(a), zc);
		else
			z__set_int(L, mpq_denref(a), 3);
		/* canonicalize the result */
		_check_divisor(L, mpq_denref(a));
		mpq_canonicalize(a);
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

	q = q__check(L, 1);
	_check_divisor(L, mpq_denref(q));
	mpq_canonicalize(q);
	lua_settop(L, 1);
	return 1;
}

static int q__partial_ref(lua_State *L, int i, mpz_ptr z)
{
	mpz_ptr *p = lua_newuserdata(L, sizeof (mpz_ptr));
	if (luaL_getmetatable(L, "mpz_t") != LUA_TNIL) {
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
	}
	/* doesn't matter if no such metatable;
	 * we don't need a finalizer anyway */
	/* pop metatable */
	lua_pop(L, 1);

	lua_pushvalue(L, i);
	lua_setuservalue(L, -2); /* reference the object */
	return 1;
}

static int q_numref(lua_State *L)
{
	return q__partial_ref(L, 1, mpq_numref(q__check(L, 1)));
}

static int q_denref(lua_State *L)
{
	return q__partial_ref(L, 1, mpq_denref(q__check(L, 1)));
}

static int $_equal(lua_State *L)
{
	mpq_ptr a, b;
	int ret;

	a = q__check(L, 1);
	b = _tompq(L, 2);
	ret = mpq_equal(a, b);
	lua_pushboolean(L, ret);
	return 1;
}
#elif defined(MPF)
static int f_get_prec(lua_State *L)
{
	mpf_ptr z;
	mp_bitcnt_t prec;

	z = f__check(L, 1);
	prec = mpf_get_prec(z);
	lua_pushinteger(L, (lua_Integer) prec);
	return 1;
}

static int f_set_prec(lua_State *L)
{
	mpf_ptr z;
	mp_bitcnt_t prec;

	z = f__check(L, 1);
	prec = _castbitcnt(L, 2);
	mpf_set_prec(z, prec);
	return 0;
}

static int f_div(lua_State *L)
{
	mpf_ptr r, a, b;
	lua_Integer val;
	int isint;

	r = $__check(L, 1);
	val = lua_tointegerx(L, 3, &isint);
	if (isint && val >= 0 && CAN_HOLD(unsigned long, val)) {
		if (val == 0) goto divzero;
		a = _tomp$(L, 2);
		mpf_div_ui(r, a, val);
	} else {
		b = _tompf(L, 3);
		if (mpf_sgn(b) == 0) goto divzero;
		val = lua_tointegerx(L, 2, &isint);
		if (isint && val >= 0 && CAN_HOLD(unsigned, val)) {
			mpf_ui_div(r, val, b);
		} else {
			a = _tomp$(L, 2);
			mpf_div(r, a, b);
		}
	}
	lua_settop(L, 1);
	return 1;
divzero:
	return luaL_error(L, "division by zero");
}

static int f_sqrt(lua_State *L)
{
	mpf_ptr r, a;
	lua_Integer val;
	int isnum;

	r = $__check(L, 1);
	val = lua_tointegerx(L, 2, &isnum);
	if (isnum && CAN_HOLD(unsigned long, val)) {
		if (val < 0) goto domain;
		mpf_sqrt_ui(r, val);
	} else {
		a = _tompf(L, 2);
		if (mpf_sgn(a) < 0) goto domain;
		mpf_sqrt(r, a);
	}
	lua_settop(L, 1);
	return 1;
domain:
	return luaL_error(L, "domain error");
}

static int f_pow(lua_State *L)
{
	mpf_ptr r, a;
	lua_Integer b;

	r = $__check(L, 1);
	a = _tomp$(L, 2);
	b = _castulong(L, 3);
	mpf_pow_ui(r, a, b);
	lua_settop(L, 1);
	return 1;
}

OP_DCL(unop, ceil)
OP_DCL(unop, floor)
OP_DCL(unop, trunc)
#endif

static int $_inp_str(lua_State *L)
{
	mp$_ptr z;
	FILE *f;
	int base;
	size_t n;

	z = $__check(L, 1);
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
#ifdef MPF
	size_t digits;
#endif

	z = $__check(L, 1);
	f = _checkfile(L, 2);
	base = _check_outbase(L, 3);
#ifdef MPF
	digits = (size_t) luaL_optinteger(L, 4, 0);
	n = mp$_out_str(f, base, digits, z);
#else
	n = mp$_out_str(f, base, z);
#endif

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
	METHOD(sgn),
#if defined(MPZ)
	METHOD(cmpabs),

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
	METHOD(remove),
	METHOD(jacobi),
	METHOD(fac),
	METHOD(primorial),
	METHOD(bin),
	METHOD(fib),
	METHOD(lucnum),
	METHOD(invert),
	METHOD(powm),

	METHOD_ALIAS(band, and),
	METHOD_ALIAS(bor, ior),
	METHOD_ALIAS(bxor, xor),
	METHOD_ALIAS(bnot, com),
	METHOD(popcount),
	METHOD(hamdist),
	METHOD(scan),
	METHOD(setbit),
	METHOD(clrbit),
	METHOD(combit),
	METHOD(tstbit),

	METHOD(even_p),
	METHOD(odd_p),
	METHOD(perfect_power_p),
	METHOD(perfect_square_p),
	METHOD(divisible_p),

	METHOD(sizeinbase),

	METHOD(urandomb),
	METHOD(urandomm),
	METHOD(rrandomb),
#elif defined(MPQ)
	METHOD(div),
	METHOD(inv),
	METHOD(canonicalize),
	METHOD(numref),
	METHOD(denref),
	METHOD(equal),
#elif defined(MPF)
	METHOD(set_prec),
	METHOD(get_prec),
	METHOD(div),
	METHOD(pow),
	METHOD(sqrt),
	METHOD(ceil),
	METHOD(floor),
	METHOD(trunc),
	METHOD(urandomb),
#endif
	METHOD(inp_str),
	METHOD(out_str),
	{ NULL,		NULL	}
};

LUALIB_API int luaopen_mp_$(lua_State *L)
{
	lua_pushcfunction(L, $_call);
	luaL_newlib(L, $_Reg);
#ifdef MPF
	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "precision");
#endif
	luaL_newmetatable(L, "mp$_t");
	luaL_setfuncs(L, $_Meta, 0);

	return _open_common(L);
}
