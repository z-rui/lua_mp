static mpq_ptr Qnew(lua_State *L)
{
	mpq_ptr q = lua_newuserdata(L, sizeof (mpq_t));
	if (luaL_getmetatable(L, "mpq_t") == LUA_TNIL)
		luaL_error(L, "mp.q not loaded properly");
	mpq_init(q);
	lua_setmetatable(L, -2);
	return q;
}

static mpq_ptr Q_fromstring(lua_State *L, const char *s, int base)
{
	mpq_ptr q;

	q = Qnew(L);
	if (mpq_set_str(q, s, base) != 0) {
		if (base)
			luaL_error(L, "cannot convert string to rational in base %d", base);
		else
			luaL_error(L, "cannot convert string to rational");
	}
	return q;
}

static mpq_ptr Qget(lua_State *L, int i)
{
	mpz_ptr z;
	mpq_ptr q;
	lua_Number val;

	switch (lua_type(L, i))
	{
		case LUA_TNUMBER:
			val = lua_tonumber(L, i);
			q = Qnew(L);
			mpq_set_d(q, val);
			lua_replace(L, i);
			return q;
		case LUA_TSTRING:
			q = Q_fromstring(L, lua_tostring(L, i), 0);
			lua_replace(L, i);
			return q;
		default:
			/* mpz_t promotion */
			if ((z = luaL_testudata(L, i, "mpz_t")) != 0) {
				q = Qnew(L);
				mpq_set_z(q, z);
				return q;
			}
			q = luaL_checkudata(L, i, "mpq_t");
			/* do some sanity check ...
			 * in order to prevent exception */
			luaL_argcheck(L, mpz_sgn(mpq_denref(q)) > 0, i, "non-canonicalized rational");
			return q;
	}
	return 0;
}

static int Qcall(lua_State *L)
{
	lua_Integer base;

	lua_remove(L, 1); /* self */
	switch (lua_gettop(L)) {
	case 0:
		Qnew(L);
		break;
	case 1:
		Qget(L, 1);
		break;
	case 2:
		base = luaL_checkinteger(L, 2);
		luaL_argcheck(L, base == 0 || (2 <= base && base <= 62), 2, "base neither 0 nor in range [2,62]");
		Q_fromstring(L, luaL_checkstring(L, 1), (int) base);
		break;
	default:
		return luaL_error(L, "too many arguments");
	}
	return 1;
}

static int Qgc(lua_State *L)
{
	mpq_ptr q = Qget(L, 1);

	mpq_clear(q);
	lua_pushnil(L);
	lua_setmetatable(L, 1);
	return 0;
}

static int Qtostring(lua_State *L)	/** tostring(x) */
{
	mpq_ptr q;
	lua_Integer base;
	luaL_Buffer B;
	size_t sz;
	char *s;

	q = luaL_checkudata(L, 1, "mpq_t");
	base = luaL_optinteger(L, 2, 10);
	luaL_argcheck(L, (-36 <= base && base <= -2) || (2 <= base && base <= 62), 2, "base not in range [-36,-2] and [2,62]");

	sz = mpz_sizeinbase(mpq_numref(q), abs(base))
	   + mpz_sizeinbase(mpq_denref(q), abs(base))
	   + 3;
	s = mpq_get_str(luaL_buffinitsize(L, &B, sz), base, q);

	luaL_pushresultsize(&B, strlen(s));
	return 1;
}

static int Q_cmp(lua_State *L)
{
	mpq_srcptr x = Qget(L, 1);
	mpq_srcptr y = Qget(L, 2);

	return mpq_cmp(x, y);
}

static int Qeq(lua_State *L)
{
	lua_pushboolean(L, Q_cmp(L) == 0);
	return 1;
}

static int Qlt(lua_State *L)
{
	lua_pushboolean(L, Q_cmp(L) < 0);
	return 1;
}

static int Qswap(lua_State *L)
{
	mpq_ptr a, b;

	a = luaL_checkudata(L, 1, "mpq_t");
	b = luaL_checkudata(L, 2, "mpq_t");
	mpq_swap(a, b);
	return 0;
}

static void Q_checkops(lua_State *L, int ops)
{
	int top;

	top = lua_gettop(L);
	switch (top - ops) {
		case 0:
			Qnew(L);
			lua_insert(L, 1);
		case 1:
			break;
		default:
			luaL_error(L, "expect %d or %d arguments, got %d", ops, ops+1, top);
	}
}

static int Qunop(lua_State *L, void (*op)(mpq_ptr, mpq_srcptr))
{
	Q_checkops(L, 1);
	(*op)(luaL_checkudata(L, 1, "mpq_t"), Qget(L, 2));
	lua_settop(L, 1);
	return 1;
}

static int Qbinop(lua_State *L, void (*op)(mpq_ptr, mpq_srcptr, mpq_srcptr))
{
	Q_checkops(L, 2);
	(*op)(luaL_checkudata(L, 1, "mpq_t"), Qget(L, 2), Qget(L, 3));
	lua_settop(L, 1);
	return 1;
}

#define Q_UNOP_DCL(op) \
static int Q##op(lua_State *L) { return Qunop(L, mpq_##op); }
#define Q_BINOP_DCL(op) \
static int Q##op(lua_State *L) { return Qbinop(L, mpq_##op); }

Q_UNOP_DCL(abs)
Q_UNOP_DCL(neg)
/* Pitfall here! */
/* Lua's __unm takes an additional argument,
 * behaving like a binary op */
static int Qneg_wrap(lua_State *L)
{
	lua_settop(L, 1);
	return Qneg(L);
}
Q_UNOP_DCL(set)
static int Qinv(lua_State *L)
{
	mpq_ptr q, r;

	Q_checkops(L, 1);
	q = luaL_checkudata(L, 1, "mpq_t");
	r = Qget(L, 2);
	luaL_argcheck(L, mpq_sgn(q) != 0, 2, "division by zero");
	mpq_inv(q, r);
	lua_settop(L, 1);
	return 1;
}

Q_BINOP_DCL(add)
Q_BINOP_DCL(sub)
Q_BINOP_DCL(mul)

static int Qdiv(lua_State *L)
{
	mpq_ptr a, b, c;

	Q_checkops(L, 2);
	a = luaL_checkudata(L, 1, "mpq_t");
	b = Qget(L, 2);
	c = Qget(L, 3);
	luaL_argcheck(L, mpq_sgn(c) != 0, 3, "division by zero");
	mpq_div(a, b, c);
	lua_settop(L, 1);
	return 1;
}

static int Qcanonicalize(lua_State *L)
{
	mpq_ptr q;
	int dsgn;

	q = luaL_checkudata(L, 1, "mpq_t");
	dsgn = mpz_sgn(mpq_denref(q));
	luaL_argcheck(L, dsgn != 0, 1, "division by zero");
	if (lua_type(L, 2) == LUA_TBOOLEAN && !lua_toboolean(L, 2)) {
		/* only fix the sign of denominator, */
		if (dsgn < 0) {
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

static int Q_get_part(lua_State *L, void (*op)(mpz_ptr, mpq_srcptr))
{
	mpq_ptr q;
	mpz_ptr z;

	q = luaL_checkudata(L, 1, "mpq_t");
	if (lua_type(L, 2) == LUA_TNONE) {
		z = Znew(L);
	} else {
		z = luaL_checkudata(L, 2, "mpq_t");
	}
	(*op)(z, q);
	return 1;
}

static int Qget_num(lua_State *L) { return Q_get_part(L, mpq_get_num); }
static int Qget_den(lua_State *L) { return Q_get_part(L, mpq_get_den); }

static int Q_set_part(lua_State *L, void (*op)(mpq_ptr, mpz_srcptr))
{
	mpq_ptr q;
	mpz_ptr z;

	q = luaL_checkudata(L, 1, "mpq_t");
	z = Zget(L, 2);
	(*op)(q, z);
	return 0;
}

static int Qset_num(lua_State *L) { return Q_set_part(L, mpq_set_num); }
static int Qset_den(lua_State *L) { return Q_set_part(L, mpq_set_den); }

#define Q_OP_REG(op) {#op,Q##op}

static const luaL_Reg QReg[] =
{
	{ "__gc",	Qgc	},
	{ "__tostring",	Qtostring	},
	{ "__unm",	Qneg_wrap	},
	{ "__add",	Qadd	},
	{ "__sub",	Qsub	},
	{ "__mul",	Qmul	},
	{ "__div",	Qdiv	},
	{ "__eq",	Qeq	},
	{ "__lt",	Qlt	},
	Q_OP_REG(abs),
	Q_OP_REG(neg),
	Q_OP_REG(set),
	Q_OP_REG(inv),
	Q_OP_REG(add),
	Q_OP_REG(sub),
	Q_OP_REG(mul),
	Q_OP_REG(div),
	Q_OP_REG(canonicalize),
	Q_OP_REG(swap),

	Q_OP_REG(get_num),
	Q_OP_REG(get_den),
	Q_OP_REG(set_num),
	Q_OP_REG(set_den),
	{ NULL,		NULL	}
};

LUALIB_API int luaopen_mp_q(lua_State *L)
{
	luaL_newmetatable(L, "mpq_t");
	luaL_setfuncs(L, QReg, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	lua_newtable(L);
	lua_newtable(L);
	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, Qcall);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);

	return 1;
}
