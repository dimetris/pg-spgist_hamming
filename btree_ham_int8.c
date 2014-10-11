/*
 * contrib/btree_gist/btree_int8.c
 */
#include "postgres.h"

#include "btree_ham_gist.h"
#include "btree_ham_utils_num.h"

typedef struct int64key
{
	int64		lower;
	int64		upper;
} int64KEY;

/*
** int64 ops
*/
PG_FUNCTION_INFO_V1(gbt_int8_compress);
PG_FUNCTION_INFO_V1(gbt_int8_union);
PG_FUNCTION_INFO_V1(gbt_int8_picksplit);
PG_FUNCTION_INFO_V1(gbt_int8_consistent);
PG_FUNCTION_INFO_V1(gbt_int8_distance);
PG_FUNCTION_INFO_V1(gbt_int8_penalty);
PG_FUNCTION_INFO_V1(gbt_int8_same);


static bool
gbt_int8gt(const void *a, const void *b)
{
	return (*((const int64 *) a) > *((const int64 *) b));
}
static bool
gbt_int8ge(const void *a, const void *b)
{
	return (*((const int64 *) a) >= *((const int64 *) b));
}
static bool
gbt_int8eq(const void *a, const void *b)
{
	return (*((const int64 *) a) == *((const int64 *) b));
}
static bool
gbt_int8le(const void *a, const void *b)
{
	return (*((const int64 *) a) <= *((const int64 *) b));
}
static bool
gbt_int8lt(const void *a, const void *b)
{
	return (*((const int64 *) a) < *((const int64 *) b));
}

static int
gbt_int8key_cmp(const void *a, const void *b)
{
	int64KEY   *ia = (int64KEY *) (((const Nsrt *) a)->t);
	int64KEY   *ib = (int64KEY *) (((const Nsrt *) b)->t);

	// elog(NOTICE, "gbt_int8key_cmp = %f, %f, %f, %f", ia->lower, ib->lower, ia->upper, ib->upper);


	if (ia->lower == ib->lower)
	{
		if (ia->upper == ib->upper)
		{
			return 0;
		}

		return (ia->upper > ib->upper) ? 1 : -1;
	}


	return (ia->lower > ib->lower) ? 1 : -1;
}



int32 hamdist(int64 x, int64 y)
{
	int32 dist = 0;
	int64 val = x ^ y; // XOR

	// Count the number of set bits
	while(val)
	{
		++dist;
		val &= val - 1;
	}

	return dist;
}

static int32
gbt_int8_dist(const void *a, const void *b)
{

	return hamdist(*((const int64 *) (a)), *((const int64 *) (b)));


}


static const gbtree_ninfo tinfo =
{
	gbt_t_int8,
	sizeof(int64),
	16,							/* sizeof(gbtreekey16) */
	gbt_int8gt,
	gbt_int8ge,
	gbt_int8eq,
	gbt_int8le,
	gbt_int8lt,
	gbt_int8key_cmp,
	gbt_int8_dist
};


PG_FUNCTION_INFO_V1(int8_dist);
Datum
int8_dist(PG_FUNCTION_ARGS)
{
	int64		a = PG_GETARG_INT64(0);
	int64		b = PG_GETARG_INT64(1);

	int32		ra;

	elog(NOTICE, "int8_dist");
	ra =  hamdist(a, b);

	PG_RETURN_INT32(ra);
}


/**************************************************
 * int64 ops
 **************************************************/


Datum
gbt_int8_compress(PG_FUNCTION_ARGS)
{
	GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	GISTENTRY  *retval = NULL;

	PG_RETURN_POINTER(gbt_num_compress(retval, entry, &tinfo));
}


Datum
gbt_int8_consistent(PG_FUNCTION_ARGS)
{
	GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	int64		query = PG_GETARG_INT64(1);
	StrategyNumber strategy = (StrategyNumber) PG_GETARG_UINT16(2);

	/* Oid		subtype = PG_GETARG_OID(3); */
	bool	   *recheck = (bool *) PG_GETARG_POINTER(4);
	int64KEY   *kkk = (int64KEY *) DatumGetPointer(entry->key);
	GBT_NUMKEY_R key;


	elog(NOTICE, "gbt_int8_consistent");

	/* All cases served by this function are exact */
	*recheck = false;

	key.lower = (GBT_NUMKEY *) &kkk->lower;
	key.upper = (GBT_NUMKEY *) &kkk->upper;

	PG_RETURN_BOOL(
				   gbt_num_consistent(&key, (void *) &query, &strategy, GIST_LEAF(entry), &tinfo)
		);
}


Datum
gbt_int8_distance(PG_FUNCTION_ARGS)
{
	GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	int64		query = PG_GETARG_INT64(1);

	/* Oid		subtype = PG_GETARG_OID(3); */
	int64KEY   *kkk = (int64KEY *) DatumGetPointer(entry->key);
	GBT_NUMKEY_R key;

	elog(NOTICE, "gbt_int8_distance");

	key.lower = (GBT_NUMKEY *) &kkk->lower;
	key.upper = (GBT_NUMKEY *) &kkk->upper;

	PG_RETURN_FLOAT8(
			gbt_num_distance(&key, (void *) &query, GIST_LEAF(entry), &tinfo)
		);
}


Datum
gbt_int8_union(PG_FUNCTION_ARGS)
{
	GistEntryVector *entryvec = (GistEntryVector *) PG_GETARG_POINTER(0);
	void	   *out = palloc(sizeof(int64KEY));

	*(int *) PG_GETARG_POINTER(1) = sizeof(int64KEY);
	PG_RETURN_POINTER(gbt_num_union((void *) out, entryvec, &tinfo));
}


Datum
gbt_int8_penalty(PG_FUNCTION_ARGS)
{
	int64KEY   *origentry = (int64KEY *) DatumGetPointer(((GISTENTRY *) PG_GETARG_POINTER(0))->key);
	int64KEY   *newentry = (int64KEY *) DatumGetPointer(((GISTENTRY *) PG_GETARG_POINTER(1))->key);
	float	   *result = (float *) PG_GETARG_POINTER(2);


	/*
	 * Note: The factor 0.49 in following macro avoids floating point overflows
	 */

	double	tmp = 0.0F;
	(*(result))	= 0.0F;
	if ( (newentry->upper) > (origentry->upper) )
	{
		tmp += ( ((double)newentry->upper)*0.49F - ((double)origentry->upper)*0.49F );
	}
	if (	(origentry->lower) > (newentry->lower) )
	{
		tmp += ( ((double)origentry->lower)*0.49F - ((double)newentry->lower)*0.49F );
	}
	if (tmp > 0.0F)
	{
		(*(result)) += FLT_MIN;
		(*(result)) += (float) ( ((double)(tmp)) / ( (double)(tmp) + ( ((double)(origentry->upper))*0.49F - ((double)(origentry->lower))*0.49F ) ) );
		(*(result)) *= (FLT_MAX / (((GISTENTRY *) PG_GETARG_POINTER(0))->rel->rd_att->natts + 1));
	}


	// elog(NOTICE, "gbt_int8_penalty = %f", *result);

	PG_RETURN_POINTER(result);
}

Datum
gbt_int8_picksplit(PG_FUNCTION_ARGS)
{

	PG_RETURN_POINTER(gbt_num_picksplit(
									(GistEntryVector *) PG_GETARG_POINTER(0),
									  (GIST_SPLITVEC *) PG_GETARG_POINTER(1),
										&tinfo
										));
}

Datum
gbt_int8_same(PG_FUNCTION_ARGS)
{

	int64KEY   *b1 = (int64KEY *) PG_GETARG_POINTER(0);
	int64KEY   *b2 = (int64KEY *) PG_GETARG_POINTER(1);
	bool	   *result = (bool *) PG_GETARG_POINTER(2);

	*result = gbt_num_same((void *) b1, (void *) b2, &tinfo);
	PG_RETURN_POINTER(result);
}
