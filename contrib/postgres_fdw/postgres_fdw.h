/*-------------------------------------------------------------------------
 *
 * postgres_fdw.h
 *		  Foreign-data wrapper for remote PostgreSQL servers
 *
 * Portions Copyright (c) 2012-2017, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  contrib/postgres_fdw/postgres_fdw.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef POSTGRES_FDW_H
#define POSTGRES_FDW_H

#include "foreign/foreign.h"
#include "commands/copy.h"
#include "lib/stringinfo.h"
#include "nodes/relation.h"
#include "storage/latch.h"
#include "utils/relcache.h"

#include "libpq-fe.h"

/*
 * FDW-specific planner information kept in RelOptInfo.fdw_private for a
 * postgres_fdw foreign table.  For a baserel, this struct is created by
 * postgresGetForeignRelSize, although some fields are not filled till later.
 * postgresGetForeignJoinPaths creates it for a joinrel, and
 * postgresGetForeignUpperPaths creates it for an upperrel.
 */
typedef struct PgFdwRelationInfo
{
	/*
	 * True means that the relation can be pushed down. Always true for simple
	 * foreign scan.
	 */
	bool		pushdown_safe;

	/*
	 * Restriction clauses, divided into safe and unsafe to pushdown subsets.
	 * All entries in these lists should have RestrictInfo wrappers; that
	 * improves efficiency of selectivity and cost estimation.
	 */
	List	   *remote_conds;
	List	   *local_conds;

	/* Actual remote restriction clauses for scan (sans RestrictInfos) */
	List	   *final_remote_exprs;

	/* Bitmap of attr numbers we need to fetch from the remote server. */
	Bitmapset  *attrs_used;

	/* Cost and selectivity of local_conds. */
	QualCost	local_conds_cost;
	Selectivity local_conds_sel;

	/* Selectivity of join conditions */
	Selectivity joinclause_sel;

	/* Estimated size and cost for a scan or join. */
	double		rows;
	int			width;
	Cost		startup_cost;
	Cost		total_cost;
	/* Costs excluding costs for transferring data from the foreign server */
	Cost		rel_startup_cost;
	Cost		rel_total_cost;

	/* Options extracted from catalogs. */
	bool		use_remote_estimate;
	Cost		fdw_startup_cost;
	Cost		fdw_tuple_cost;
	List	   *shippable_extensions;	/* OIDs of whitelisted extensions */

	/* Cached catalog information. */
	ForeignTable *table;
	ForeignServer *server;
	UserMapping *user;			/* only set in use_remote_estimate mode */

	int			fetch_size;		/* fetch size for this remote table */

	/*
	 * Name of the relation while EXPLAINing ForeignScan. It is used for join
	 * relations but is set for all relations. For join relation, the name
	 * indicates which foreign tables are being joined and the join type used.
	 */
	StringInfo	relation_name;

	/* Join information */
	RelOptInfo *outerrel;
	RelOptInfo *innerrel;
	JoinType	jointype;
	/* joinclauses contains only JOIN/ON conditions for an outer join */
	List	   *joinclauses;	/* List of RestrictInfo */

	/* Grouping information */
	List	   *grouped_tlist;

	/* Subquery information */
	bool		make_outerrel_subquery; /* do we deparse outerrel as a
										 * subquery? */
	bool		make_innerrel_subquery; /* do we deparse innerrel as a
										 * subquery? */
	Relids		lower_subquery_rels;	/* all relids appearing in lower
										 * subqueries */

	/*
	 * Index of the relation.  It is used to create an alias to a subquery
	 * representing the relation.
	 */
	int			relation_index;
} PgFdwRelationInfo;

/*
 * Connection cache hash table entry
 *
 * The lookup key in this hash table is the user mapping OID. We use just one
 * connection per user mapping ID, which ensures that all the scans use the
 * same snapshot during a query.  Using the user mapping OID rather than
 * the foreign server OID + user OID avoids creating multiple connections when
 * the public user mapping applies to all user OIDs.
 *
 * The "conn" pointer can be NULL if we don't currently have a live connection.
 * When we do have a connection, xact_depth tracks the current depth of
 * transactions and subtransactions open on the remote side.  We need to issue
 * commands at the same nesting depth on the remote as we're executing at
 * ourselves, so that rolling back a subtransaction will kill the right
 * queries and not the wrong ones.
 */
typedef Oid ConnCacheKey;

typedef struct ConnCacheEntry ConnCacheEntry;
struct ConnCacheEntry
{
	ConnCacheKey key;			/* hash key (must be first) */
	PGconn	   *conn;			/* connection to foreign server, or NULL */
	WaitEventSet *wait_set;		/* for data from server ready notifications */
	/* Remaining fields are invalid when conn is NULL: */
	int			xact_depth;		/* 0 = no xact open, 1 = main xact open, 2 =
								 * one level of subxact open, etc */
	bool		have_prep_stmt; /* have we prepared any stmts in this xact? */
	bool		have_error;		/* have any subxacts aborted in this xact? */
	bool		changing_xact_state;	/* xact state change in process */
	bool		invalidated;	/* true if reconnect is pending */
	uint32		server_hashvalue;	/* hash value of foreign server OID */
	uint32		mapping_hashvalue;	/* hash value of user mapping OID */
	bool		copy_from_started;	/* COPY FROM in progress on this conn */
	HTAB		*dm_prepared; /* prepared statements for DirectModify */
};

/* sql -> prepared statement hashtable */
typedef struct DirectModifyPrepStmtHashEnt
{
	char  *sql; /* SQL of the statement, the key; arbitrary size */
	char  prep_name[NAMEDATALEN]; /* name of prepared statement */
} DirectModifyPrepStmtHashEnt;

/* in postgres_fdw.c */
extern int	set_transmission_modes(void);
extern void reset_transmission_modes(int nestlevel);

/* in connection.c */
extern ConnCacheEntry *GetConnection(UserMapping *user, bool will_prep_stmt);
extern PGconn *ConnectionEntryGetConn(ConnCacheEntry *entry);
extern ConnCacheEntry *GetConnectionCopyFrom(UserMapping *user, bool will_prep_stmt,
									 bool **copy_from_started);
extern void ReleaseConnection(ConnCacheEntry *entry);
extern unsigned int GetCursorNumber(ConnCacheEntry *entry);
extern unsigned int GetPrepStmtNumber(ConnCacheEntry *entry);
extern PGresult *pgfdw_get_result(ConnCacheEntry *entry, const char *query);
extern PGresult *pgfdw_exec_query(ConnCacheEntry *entry, const char *query);
extern void pgfdw_report_error(int elevel, PGresult *res, PGconn *conn,
				   bool clear, const char *sql);

/* in option.c */
extern int ExtractConnectionOptions(List *defelems,
						 const char **keywords,
						 const char **values);
extern List *ExtractExtensionList(const char *extensionsString,
					 bool warnOnMissing);

/* in deparse.c */
extern void classifyConditions(PlannerInfo *root,
				   RelOptInfo *baserel,
				   List *input_conds,
				   List **remote_conds,
				   List **local_conds);
extern bool is_foreign_expr(PlannerInfo *root,
				RelOptInfo *baserel,
				Expr *expr);
extern void deparseInsertSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *targetAttrs, bool doNothing, List *returningList,
				 List **retrieved_attrs);
extern void deparseUpdateSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *targetAttrs, List *returningList,
				 List **retrieved_attrs);
extern void deparseDirectUpdateSql(StringInfo buf, PlannerInfo *root,
					   Index rtindex, Relation rel,
					   List *targetlist,
					   List *targetAttrs,
					   List *remote_conds,
					   List **params_list,
					   List *returningList,
					   List **retrieved_attrs);
extern void deparseDeleteSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *returningList,
				 List **retrieved_attrs);
extern void deparseDirectDeleteSql(StringInfo buf, PlannerInfo *root,
					   Index rtindex, Relation rel,
					   List *remote_conds,
					   List **params_list,
					   List *returningList,
					   List **retrieved_attrs);
extern void deparseAnalyzeSizeSql(StringInfo buf, Relation rel);
extern void deparseAnalyzeSql(StringInfo buf, Relation rel,
				  List **retrieved_attrs);
extern void deparseStringLiteral(StringInfo buf, const char *val);
extern Expr *find_em_expr_for_rel(EquivalenceClass *ec, RelOptInfo *rel);
extern List *build_tlist_to_deparse(RelOptInfo *foreignrel);
extern void deparseSelectStmtForRel(StringInfo buf, PlannerInfo *root,
						RelOptInfo *foreignrel, List *tlist,
						List *remote_conds, List *pathkeys, bool is_subquery,
						List **retrieved_attrs, List **params_list);
extern const char *get_jointype_name(JoinType jointype);
extern void deparseCopyFromSql(StringInfo buf, Relation rel, CopyState cstate,
							   const char *dest_relname);

/* in shippable.c */
extern bool is_builtin(Oid objectId);
extern bool is_shippable(Oid objectId, Oid classId, PgFdwRelationInfo *fpinfo);

extern bool UseTsDtmTransactions;
extern bool Use2PC;
extern bool UseRepeatableRead;

#endif							/* POSTGRES_FDW_H */
