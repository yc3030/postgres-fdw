// Generated by cgo from fdw.go. Included here so our functions are
// defined and available.
#include "steampipe_postgres_fdw.h"
#include "nodes/plannodes.h"

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

void *serializePlanState(FdwPlanState *state);
FdwExecState *initializeExecState(void *internalstate);
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
	planstate->foreigntableid = foreigntableid;

	// Initialize the conversion info array
	{
		Relation	rel = RelationIdGetRelation(ftable->relid);
		AttInMetadata *attinmeta;
		desc = RelationGetDescr(rel);
		attinmeta = TupleDescGetAttInMetadata(desc);
		planstate->numattrs = RelationGetNumberOfAttributes(rel);
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

	// Inject the "rows" and "width" attribute into the baserel
	goFdwGetRelSize(planstate, root, &baserel->rows, &baserel->reltarget->width, baserel);

	planstate->width = baserel->reltarget->width;
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


    ppi_list = NIL;
	foreach(lc, baserel->joininfo)
	{
        elog(INFO, " joininfo, %d restrictions", list_length(baserel->joininfo));
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
    Index scan_relid = baserel->relid;
	FdwPlanState *planstate = (FdwPlanState *) baserel->fdw_private;
	best_path->path.pathtarget->width = planstate->width;
	elog(INFO, "fdwGetForeignPlan %d scan_clauses", list_length(scan_clauses));
	scan_clauses = extract_actual_clauses(scan_clauses, false);

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
	elog(INFO, "************** ret %d, %d, %d", list_length(s->fdw_exprs) , list_length(s->fdw_scan_tlist) , list_length(s->fdw_recheck_quals) );

	return s;
}

/*
 *	"Serialize" a FdwPlanState, so that it is safe to be carried
 *	between the plan and the execution safe.
 */
 void *serializePlanState(FdwPlanState * state) {
 	List *result = NULL;
 	result = lappend(result, makeConst(INT4OID, -1, InvalidOid, 4, Int32GetDatum(state->numattrs), false, true));
 	result = lappend(result, state->target_list);
 	result = lappend(result, serializeDeparsedSortGroup(state->pathkeys));

 	return result;
 }

/*
 *	"Deserialize" an internal state and inject it in an
 *	FdwExecState
 */
FdwExecState *initializeExecState(void *internalstate) {
    FdwExecState *execstate = palloc0(sizeof(FdwExecState));
	// internalstate is actually a list generated by serializePlanState consisting of:

	//  numattrs, target_list, target_list, pathkeys
	List	   *values = (List *) internalstate;
	AttrNumber	numattrs = ((Const *) linitial(values))->constvalue;
	List		*pathkeys;
	/* Those list must be copied, because their memory context can become */
	/* invalid during the execution (in particular with the cursor interface) */
	execstate->target_list = copyObject(lsecond(values));
	pathkeys = lthird(values);

	execstate->pathkeys = deserializeDeparsedSortGroup(pathkeys);
	execstate->buffer = makeStringInfo();
	execstate->cinfos = palloc0(sizeof(ConversionInfo *) * numattrs);
	execstate->values = palloc(numattrs * sizeof(Datum));
	execstate->nulls = palloc(numattrs * sizeof(bool));
	return execstate;
}
