// Generated by cgo from fdw.go. Included here so our functions are
// defined and available.
#include "steampipe_postgres_fdw.h"

extern PGDLLEXPORT void _PG_init(void);

static void fdwGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void fdwGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void exitHook(int code, Datum arg);
static ForeignScan *fdwGetForeignPlan(
    PlannerInfo *root,
    RelOptInfo *baserel,
    Oid foreigntableid,
    ForeignPath *best_path,
    List *tlist,
    List *scan_clauses,
    Plan *outer_plan
);

static void
fdwGetForeignJoinPaths(PlannerInfo *root,
    RelOptInfo *joinrel,
    RelOptInfo *outerrel,
    RelOptInfo *innerrel,
    JoinType jointype,
    JoinPathExtraData *extra);

void *serializePlanState(FdwPlanState *state);
FdwExecState *initializeExecState(void *internalstate);
//int dumpStruct(const char *fmt, ...);
// Required by postgres, doing basic checks to ensure compatibility,
// such as being compiled against the correct major version.
PG_MODULE_MAGIC;

// Define our handling functions with Postgres, following the V1 protocol.
PG_FUNCTION_INFO_V1(fdw_handler);
PG_FUNCTION_INFO_V1(fdw_validator);


/*
 * _PG_init
 * 		Library load-time initalization.
 * 		Sets exitHook() callback for backend shutdown.
 */
void
_PG_init(void)
{
	/* register an exit hook */
	on_proc_exit(&exitHook, PointerGetDatum(NULL));
}

/*
 * exitHook
 * 		Close all Oracle connections on process exit.
 */

void
exitHook(int code, Datum arg)
{
	goFdwShutdown();
}

Datum fdw_handler(PG_FUNCTION_ARGS) {
  FdwRoutine *fdw_routine = makeNode(FdwRoutine);
  fdw_routine->GetForeignRelSize = fdwGetForeignRelSize;
  fdw_routine->GetForeignPaths = fdwGetForeignPaths;
  fdw_routine->GetForeignPlan = fdwGetForeignPlan;
  fdw_routine->ExplainForeignScan = goFdwExplainForeignScan;
  fdw_routine->BeginForeignScan = goFdwBeginForeignScan;
  fdw_routine->IterateForeignScan = goFdwIterateForeignScan;
  fdw_routine->ReScanForeignScan = goFdwReScanForeignScan;
  fdw_routine->EndForeignScan = goFdwEndForeignScan;
  fdw_routine->ImportForeignSchema = goFdwImportForeignSchema;
  /* Support functions for join push-down */
  fdw_routine->GetForeignJoinPaths = fdwGetForeignJoinPaths;

PG_RETURN_POINTER(fdw_routine);
}

// TODO - Use this to validate the arguments passed to the FDW
// https://github.com/laurenz/oracle_fdw/blob/9d7b5c331b0c8851c71f410f77b41c1a83c89ece/oracle_fdw.c#L420
Datum fdw_validator(PG_FUNCTION_ARGS) {
  Oid catalog = PG_GETARG_OID(1);
  List *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
  goFdwValidate(catalog, options_list);
  PG_RETURN_VOID();
}

static void fdwGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid) {
    // initialise logging`
    // to set the log level for fdw logging from C code, set log_min_messages in postgresql.conf
    goInit();
    elog(INFO, "fdwGetForeignRelSize");

    FdwPlanState *planstate = palloc0(sizeof(FdwPlanState));
	ForeignTable *ftable = GetForeignTable(foreigntableid);

	ListCell *lc;
	bool needWholeRow = false;
	TupleDesc	desc;

  // Save plan state information
	baserel->fdw_private = planstate;
	//planstate->fdw_instance = getInstance(foreigntableid);
	planstate->foreigntableid = foreigntableid;

	// Initialize the conversion info array
	{
		Relation	rel = RelationIdGetRelation(ftable->relid);
		AttInMetadata *attinmeta;
		desc = RelationGetDescr(rel);
		attinmeta = TupleDescGetAttInMetadata(desc);
		planstate->numattrs = RelationGetNumberOfAttributes(rel);
		planstate->cinfos = palloc0(sizeof(ConversionInfo *) *planstate->numattrs);
		initConversioninfo(planstate->cinfos, attinmeta);
		needWholeRow = rel->trigdesc && rel->trigdesc->trig_insert_after_row;
		RelationClose(rel);
	}

    // Gather the target_list of columns for this query as Value objects.
	if (needWholeRow) {
		int	i;
		for (i = 0; i < desc->natts; i++) {
			Form_pg_attribute att = TupleDescAttr(desc, i);
			if (!att->attisdropped) {
				planstate->target_list = lappend(planstate->target_list, makeString(NameStr(att->attname)));
			}
		}
	}
	else {
		foreach(lc, extractColumns(baserel->reltarget->exprs, baserel->baserestrictinfo)) {
			Var	  *var = (Var *) lfirst(lc);
			Value	*colname;
			// Store only a Value node containing the string name of the column.
			colname = colnameFromVar(var, root, planstate);
			if (colname != NULL && strVal(colname) != NULL) {
				planstate->target_list = lappend(planstate->target_list, colname);
			}
		}
	}

	// Extract the restrictions from the plan.
    elog(INFO, "**************** baserestrictinfo, %d restrictions", list_length(baserel->baserestrictinfo));
	if (list_length(baserel->baserestrictinfo) > 0) {

        foreach(lc, baserel->baserestrictinfo) {
//            displayRestriction(root, baserel->relids, ((RestrictInfo *) lfirst(lc)));
            extractRestrictions(root, baserel->relids, ((RestrictInfo *) lfirst(lc))->clause, &planstate->qual_list);
        }
	}

    elog(INFO, "**************** joininfo, %d restrictions", list_length(baserel->joininfo));
    if (list_length(baserel->joininfo) > 0) {
        elog(INFO, "joininfo");
        foreach(lc, baserel->joininfo) {
            displayRestriction(root, baserel->relids, ((RestrictInfo *) lfirst(lc)));
	}

	// Inject the "rows" and "width" attribute into the baserel
	goFdwGetRelSize(planstate, root, &baserel->rows, &baserel->reltarget->width, baserel);
	planstate->width = baserel->reltarget->width;
	elog(INFO, "fdwGetForeignRelSize finished");
}
}

/*
 * fdwGetForeignPaths
 *		Create possible access paths for a scan on the foreign table.
 *		This is done by calling the "get_path_keys method on the python side,
 *		and parsing its result to build parameterized paths according to the
 *		equivalence classes found in the plan.
 */
static void fdwGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid) {
	List *paths; /* List of ForeignPath */
	FdwPlanState *planstate = baserel->fdw_private;
	ListCell *lc;
    List* ppi_list;
	/* These lists are used to handle sort pushdown */
	List *apply_pathkeys = NULL;
	List *deparsed_pathkeys = NULL;

	/* Extract a friendly version of the pathkeys. */
	List *possiblePaths = goFdwGetPathKeys(planstate);

	/* Try to find parameterized paths */
	paths = findPaths(root, baserel, possiblePaths, planstate->startupCost, planstate, apply_pathkeys, deparsed_pathkeys);


	/* Add a simple default path */
	paths = lappend(paths, create_foreignscan_path(
    root,
    baserel,
		NULL,  /* default pathtarget */
		baserel->rows,
		planstate->startupCost,
		baserel->rows * baserel->reltarget->width * 100000, // table scan is very expensive
		NIL,		/* no pathkeys */
		NULL,
		NULL,
		NULL)
  );

	/* Handle sort pushdown */
	if (root->query_pathkeys) {
		List *deparsed = deparse_sortgroup(root, foreigntableid, baserel);
		if (deparsed) {
			/* Update the sort_*_pathkeys lists if needed */
			computeDeparsedSortGroup(deparsed, planstate, &apply_pathkeys, &deparsed_pathkeys);
		}
	}


    elog(INFO, "**************** joininfo, %d restrictions", list_length(baserel->joininfo));
    ppi_list = NIL;
	foreach(lc, baserel->joininfo)
	{
        elog(INFO, "joinInfo");
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
		Relids		required_outer;
		ParamPathInfo *param_info;

		/* Check if clause can be moved to this rel */
		if (!join_clause_is_movable_to(rinfo, baserel))
			continue;

//		/* See if it is safe to send to remote */
//		if (!is_foreign_expr(root, baserel, rinfo->clause))
//			continue;

		/* Calculate required outer rels for the resulting path */
		required_outer = bms_union(rinfo->clause_relids,
								   baserel->lateral_relids);
		/* We do not want the foreign rel itself listed in required_outer */
		required_outer = bms_del_member(required_outer, baserel->relid);

		/*
		 * required_outer probably can't be empty here, but if it were, we
		 * couldn't make a parameterized path.
		 */
		if (bms_is_empty(required_outer))
			continue;

		/* Get the ParamPathInfo */
		param_info = get_baserel_parampathinfo(root, baserel,
											   required_outer);
		Assert(param_info != NULL);

		/*
		 * Add it to list unless we already have it.  Testing pointer equality
		 * is OK since get_baserel_parampathinfo won't make duplicates.
		 */
		ppi_list = list_append_unique_ptr(ppi_list, param_info);
	}
//    elog(INFO, "after joinInfo");
	/* Add each ForeignPath previously found */
	foreach(lc, paths) {
		ForeignPath *path = (ForeignPath *) lfirst(lc);
		/* Add the path without modification */
		add_path(baserel, (Path *) path);
		/* Add the path with sort pushdown if possible */
		if (apply_pathkeys && deparsed_pathkeys) {
			ForeignPath *newpath;
			newpath = create_foreignscan_path(
                root,
                baserel,
                NULL,  /* default pathtarget */
                path->path.rows,
                path->path.startup_cost, path->path.total_cost,
                apply_pathkeys, NULL,
                NULL,
                (void *) deparsed_pathkeys
             );
			newpath->path.param_info = path->path.param_info;
			add_path(baserel, (Path *) newpath);
		}
	}
	// TODO - errorCheck();
}

/*
 * fdwGetForeignPlan
 *		Create a ForeignScan plan node for scanning the foreign table
 */
static ForeignScan *fdwGetForeignPlan(
  PlannerInfo *root,
  RelOptInfo *baserel,
	Oid foreigntableid,
	ForeignPath *best_path,
	List *tlist,
	List *scan_clauses,
	Plan *outer_plan
) {
    elog(INFO, "************** fdwGetForeignPlan");
	Index scan_relid = baserel->relid;
	FdwPlanState *planstate = (FdwPlanState *) baserel->fdw_private;
	ListCell *lc;
	best_path->path.pathtarget->width = planstate->width;
	elog(INFO, "**************** fdwGetForeignPlan %d scan_clauses", list_length(scan_clauses));
	scan_clauses = extract_actual_clauses(scan_clauses, false);
    	/* Extract the quals coming from a parameterized path, if any */
	if (best_path->path.param_info) {
		foreach(lc, scan_clauses) {
		    elog(INFO, "**************** fdwGetForeignPlan extractRestrictions");
			extractRestrictions(root, baserel->relids, (Expr *) lfirst(lc), &planstate->qual_list);
		}
	}
	elog(INFO, "**************** fdwGetForeignPlan %d planstate->qual_list", list_length(planstate->qual_list));
//	foreach(lc, planstate->qual_list) {
//        elog(INFO, "%s", nodeToString((Expr *) lfirst(lc)));
//	}
	planstate->pathkeys = (List *) best_path->fdw_private;
	ForeignScan * s = make_foreignscan(
        tlist,
        scan_clauses,
        scan_relid,
        scan_clauses,		/* no expressions to evaluate */
        serializePlanState(planstate),
        NULL,
        NULL, /* All quals are meant to be rechecked */
        NULL
    );
//	elog(INFO, "************** ret %d, %d, %d", list_length(s->fdw_exprs) , list_length(s->fdw_scan_tlist) , list_length(s->fdw_recheck_quals) );

//    if (list_length(s->fdw_exprs) > 0) {
//        elog(INFO, "**************** fdwGetForeignPlan fdw expr");
//
//        foreach(lc, s->fdw_exprs) {
//            elog(INFO, "**************** fdwGetForeignPlan fdw expr: %s", nodeToString( (Expr *) lfirst(lc)));
//        }
//    }
//    if (list_length(s->fdw_scan_tlist) > 0) {
//        elog(INFO, "**************** fdwGetForeignPlan fdw_scan_tlist");
//
//        foreach(lc, s->fdw_scan_tlist) {
//            elog(INFO, "**************** fdwGetForeignPlan fdw_scan_tlist: %s", nodeToString( (Expr *) lfirst(lc)));
//        }
//    }
//    if (list_length(s->fdw_recheck_quals) > 0) {
//        elog(INFO, "**************** fdwGetForeignPlan fdw_recheck_quals");
//
//        foreach(lc, s->fdw_recheck_quals) {
//            elog(INFO, "**************** fdwGetForeignPlan fdw_recheck_quals: %s", nodeToString( (Expr *) lfirst(lc)));
//        }
//    }
	return s;
}


/*
 *	"Serialize" a FdwPlanState, so that it is safe to be carried
 *	between the plan and the execution safe.
 */
 void *serializePlanState(FdwPlanState * state) {
 	List *result = NULL;
 	result = lappend(result, makeConst(INT4OID, -1, InvalidOid, 4, Int32GetDatum(state->numattrs), false, true));
// 	result = lappend(result, makeConst(INT4OID, -1, InvalidOid, 4, Int32GetDatum(state->foreigntableid), false, true));
 	result = lappend(result, state->target_list);
 	result = lappend(result, serializeDeparsedSortGroup(state->pathkeys));
 	result = lappend(result, state->qual_list);
 	result = lappend(result, state->cinfos);

 	return result;
 }


/*
 *	"Deserialize" an internal state and inject it in an
 *	FdwExecState
 */
FdwExecState *initializeExecState(void *internalstate) {
    FdwExecState *execstate = palloc0(sizeof(FdwExecState));
	// internalstate is actually a list generated by serializePlanState consisting of:

	//  numattrs, target_list, target_list, pathkeys, qual_list, cinfos
	List	   *values = (List *) internalstate;
	AttrNumber	numattrs = ((Const *) linitial(values))->constvalue;
	//Oid			foreigntableid = ((Const *) lsecond(values))->constvalue;
	List		*pathkeys;
	/* Those list must be copied, because their memory context can become */
	/* invalid during the execution (in particular with the cursor interface) */
	execstate->target_list = copyObject(lsecond(values));
	pathkeys = lthird(values);

	execstate->pathkeys = deserializeDeparsedSortGroup(pathkeys);

	List* quals = (List*)lfourth(values);
    execstate->qual_list = quals;
	//execstate->fdw_instance = getInstance(foreigntableid);
	execstate->buffer = makeStringInfo();
	// TODO define fifth() function or at least make this nicer
	execstate->cinfos =	lfirst(lnext(lnext(lnext(lnext(list_head(values))))));

	execstate->values = palloc(numattrs * sizeof(Datum));
	execstate->nulls = palloc(numattrs * sizeof(bool));
	return execstate;
}

/*
 * postgresGetForeignJoinPaths
 *		Add possible ForeignPath to joinrel, if join is safe to push down.
 */
static void
fdwGetForeignJoinPaths(PlannerInfo *root,
 							RelOptInfo *joinrel,
 							RelOptInfo *outerrel,
 							RelOptInfo *innerrel,
 							JoinType jointype,
 							JoinPathExtraData *extra)
 {
    elog(ERROR, "*****************fdwGetForeignJoinPaths");
    return
    goFdwGetForeignJoinPaths(root, joinrel, outerrel,innerrel,jointype, extra );

//	PgFdwRelationInfo *fpinfo;
//	ForeignPath *joinpath;
//	double		rows;
//	int			width;
//	Cost		startup_cost;
//	Cost		total_cost;
//	Path	   *epq_path;		/* Path to create plan to be executed when
//								 * EvalPlanQual gets triggered. */

	/*
	 * Skip if this join combination has been considered already.
	 */
	if (joinrel->fdw_private)
		return;

	/*
	 * This code does not work for joins with lateral references, since those
	 * must have parameterized paths, which we don't generate yet.
	 */
	if (!bms_is_empty(joinrel->lateral_relids))
		return;
//
//	/*
//	 * Create unfinished PgFdwRelationInfo entry which is used to indicate
//	 * that the join relation is already considered, so that we won't waste
//	 * time in judging safety of join pushdown and adding the same paths again
//	 * if found safe. Once we know that this join can be pushed down, we fill
//	 * the entry.
//	 */
//	fpinfo = (PgFdwRelationInfo *) palloc0(sizeof(PgFdwRelationInfo));
//	fpinfo->pushdown_safe = false;
//	joinrel->fdw_private = fpinfo;
//	/* attrs_used is only for base relations. */
//	fpinfo->attrs_used = NULL;
//
//	/*
//	 * If there is a possibility that EvalPlanQual will be executed, we need
//	 * to be able to reconstruct the row using scans of the base relations.
//	 * GetExistingLocalJoinPath will find a suitable path for this purpose in
//	 * the path list of the joinrel, if one exists.  We must be careful to
//	 * call it before adding any ForeignPath, since the ForeignPath might
//	 * dominate the only suitable local path available.  We also do it before
//	 * calling foreign_join_ok(), since that function updates fpinfo and marks
//	 * it as pushable if the join is found to be pushable.
//	 */
//	if (root->parse->commandType == CMD_DELETE ||
//		root->parse->commandType == CMD_UPDATE ||
//		root->rowMarks)
//	{
//		epq_path = GetExistingLocalJoinPath(joinrel);
//		if (!epq_path)
//		{
//			elog(DEBUG3, "could not push down foreign join because a local path suitable for EPQ checks was not found");
//			return;
//		}
//	}
//	else
//		epq_path = NULL;
//
//	if (!foreign_join_ok(root, joinrel, jointype, outerrel, innerrel, extra))
//	{
//		/* Free path required for EPQ if we copied one; we don't need it now */
//		if (epq_path)
//			pfree(epq_path);
//		return;
//	}
//
//	/*
//	 * Compute the selectivity and cost of the local_conds, so we don't have
//	 * to do it over again for each path. The best we can do for these
//	 * conditions is to estimate selectivity on the basis of local statistics.
//	 * The local conditions are applied after the join has been computed on
//	 * the remote side like quals in WHERE clause, so pass jointype as
//	 * JOIN_INNER.
//	 */
//	fpinfo->local_conds_sel = clauselist_selectivity(root,
//													 fpinfo->local_conds,
//													 0,
//													 JOIN_INNER,
//													 NULL);
//	cost_qual_eval(&fpinfo->local_conds_cost, fpinfo->local_conds, root);
//
//	/*
//	 * If we are going to estimate costs locally, estimate the join clause
//	 * selectivity here while we have special join info.
//	 */
//	if (!fpinfo->use_remote_estimate)
//		fpinfo->joinclause_sel = clauselist_selectivity(root, fpinfo->joinclauses,
//														0, fpinfo->jointype,
//														extra->sjinfo);
//
//	/* Estimate costs for bare join relation */
//	estimate_path_cost_size(root, joinrel, NIL, NIL, NULL,
//							&rows, &width, &startup_cost, &total_cost);
//	/* Now update this information in the joinrel */
//	joinrel->rows = rows;
//	joinrel->reltarget->width = width;
//	fpinfo->rows = rows;
//	fpinfo->width = width;
//	fpinfo->startup_cost = startup_cost;
//	fpinfo->total_cost = total_cost;
//
//	/*
//	 * Create a new join path and add it to the joinrel which represents a
//	 * join between foreign tables.
//	 */
//	joinpath = create_foreign_join_path(root,
//										joinrel,
//										NULL,	/* default pathtarget */
//										rows,
//										startup_cost,
//										total_cost,
//										NIL,	/* no pathkeys */
//										joinrel->lateral_relids,
//										epq_path,
//										NIL);	/* no fdw_private */
//
//	/* Add generated path into joinrel by add_path(). */
//	add_path(joinrel, (Path *) joinpath);
//
//	/* Consider pathkeys for the join relation */
//	add_paths_with_pathkeys_for_rel(root, joinrel, epq_path);

	/* XXX Consider parameterized paths for the join relation */
}


//
// int dumpStruct(const char *fmt, ...) {
// elog(LOG, "DUMP");
//    char str[800];
//  	va_list ap;
//    //int res = 0;
//	//char myString;
//	va_start(ap, fmt);
//	//res = vsprintf(&myString, fmt, ap);
//	//elog(LOG, fmt,  myString);
//	vsprintf(str, fmt, ap);
//	va_end(ap);
//	elog(LOG, "%s", str);
//	elog(LOG, "DUMP END");
//    //return res;
//	return true;
//}
