// Generated by cgo from fdw.go. Included here so our functions are
// defined and available.
#include "steampipe_postgres_fdw.h"
#include "fdw_handlers.h"
#include "nodes/plannodes.h"
#include "access/xact.h"

extern PGDLLEXPORT void _PG_init(void);

// static int deparseLimit(PlannerInfo *root);
// static char *datumToString(Datum datum, Oid type);
// static char *convertUUID(char *uuid);

static void pgfdw_xact_callback(XactEvent event, void *arg);
static void exitHook(int code, Datum arg);

// void *serializePlanState(FdwPlanState *state);
// FdwExecState *initializeExecState(void *internalstate);
// Required by postgres, doing basic checks to ensure compatibility,
// such as being compiled against the correct major version.
PG_MODULE_MAGIC;

/*
 * _PG_init
 * 		Library load-time initalization.
 * 		Sets exitHook() callback for backend shutdown.
 */
void _PG_init(void)
{
  /* register an exit hook */
  on_proc_exit(&exitHook, PointerGetDatum(NULL));
  RegisterXactCallback(pgfdw_xact_callback, NULL);
}

/*
 * pgfdw_xact_callback gets called when a running
 * query is cancelled
 */
static void
pgfdw_xact_callback(XactEvent event, void *arg)
{
  if (event == XACT_EVENT_ABORT)
  {
    goFdwAbortCallback();
  }
}

/*
 * exitHook
 * 		Close all Oracle connections on process exit.
 */

void exitHook(int code, Datum arg)
{
  goFdwShutdown();
}

static void fdwGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
  // initialise logging`
  // to set the log level for fdw logging from C code, set log_min_messages in postgresql.conf
//   goInit();

//   FdwPlanState *planstate = palloc0(sizeof(FdwPlanState));
//   ForeignTable *ftable = GetForeignTable(foreigntableid);

//   ListCell *lc;
//   bool needWholeRow = false;
//   TupleDesc desc;

//   // Save plan state information
//   baserel->fdw_private = planstate;
//   planstate->foreigntableid = foreigntableid;

//   // Initialize the conversion info array
//   {
//     Relation rel = RelationIdGetRelation(ftable->relid);
//     AttInMetadata *attinmeta;
//     desc = RelationGetDescr(rel);
//     attinmeta = TupleDescGetAttInMetadata(desc);
//     planstate->numattrs = RelationGetNumberOfAttributes(rel);
//     planstate->cinfos = palloc0(sizeof(ConversionInfo *) * planstate->numattrs);
//     initConversioninfo(planstate->cinfos, attinmeta);
//     needWholeRow = rel->trigdesc && rel->trigdesc->trig_insert_after_row;
//     RelationClose(rel);
//   }

//   // Gather the target_list of columns for this query as Value objects.
//   if (needWholeRow)
//   {
//     int i;
//     for (i = 0; i < desc->natts; i++)
//     {
//       Form_pg_attribute att = TupleDescAttr(desc, i);
//       if (!att->attisdropped)
//       {
//         planstate->target_list = lappend(planstate->target_list, makeString(NameStr(att->attname)));
//       }
//     }
//   }
//   else
//   {
//     foreach (lc, extractColumns(baserel->reltarget->exprs, baserel->baserestrictinfo))
//     {
//       Var *var = (Var *)lfirst(lc);
// #if PG_VERSION_NUM >= 150000
//       String *colname;
// #else
//       Value *colname;
// #endif
//       // Store only a Value node containing the string name of the column.
//       colname = colnameFromVar(var, root, planstate);
//       if (colname != NULL && strVal(colname) != NULL)
//       {
//         planstate->target_list = lappend(planstate->target_list, colname);
//       }
//     }
//   }

//   // Deduce the limit, if one was specified
//   planstate->limit = deparseLimit(root);

//   // Inject the "rows" and "width" attribute into the baserel
//   goFdwGetRelSize(planstate, root, &baserel->rows, &baserel->reltarget->width, baserel);

//   planstate->width = baserel->reltarget->width;
}

/*
 * fdwGetForeignPaths
 *		Create possible access paths for a scan on the foreign table.
 *		This is done by calling the "get_path_keys method on the python side,
 *		and parsing its result to build parameterized paths according to the
 *		equivalence classes found in the plan.
 */
static void fdwGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
  // List *paths; /* List of ForeignPath */
  // FdwPlanState *planstate = baserel->fdw_private;
  // ListCell *lc;
  // /* These lists are used to handle sort pushdown */
  // List *apply_pathkeys = NULL;
  // List *deparsed_pathkeys = NULL;

  // /* Extract a friendly version of the pathkeys. */
  // List *possiblePaths = goFdwGetPathKeys(planstate);

  // /* Try to find parameterized paths */
  // paths = findPaths(root, baserel, possiblePaths, planstate->startupCost, planstate, apply_pathkeys, deparsed_pathkeys);

  // /* Add a simple default path */
  // paths = lappend(paths, create_foreignscan_path(
  //                            root,
  //                            baserel,
  //                            NULL, /* default pathtarget */
  //                            baserel->rows,
  //                            planstate->startupCost,
  //                            baserel->rows * baserel->reltarget->width * 100000, // table scan is very expensive
  //                            NIL,                                                /* no pathkeys */
  //                            NULL,
  //                            NULL,
  //                            NULL));

  // /* Handle sort pushdown */
  // if (root->query_pathkeys)
  // {
  //   List *deparsed = deparse_sortgroup(root, foreigntableid, baserel);
  //   if (deparsed)
  //   {
  //     /* Update the sort_*_pathkeys lists if needed */
  //     computeDeparsedSortGroup(deparsed, planstate, &apply_pathkeys, &deparsed_pathkeys);
  //   }
  // }

  // /* Add each ForeignPath previously found */
  // foreach (lc, paths)
  // {
  //   ForeignPath *path = (ForeignPath *)lfirst(lc);
  //   /* Add the path without modification */
  //   add_path(baserel, (Path *)path);
  //   /* Add the path with sort pushdown if possible */
  //   if (apply_pathkeys && deparsed_pathkeys)
  //   {
  //     ForeignPath *newpath;
  //     newpath = create_foreignscan_path(
  //         root,
  //         baserel,
  //         NULL, /* default pathtarget */
  //         path->path.rows,
  //         path->path.startup_cost, path->path.total_cost,
  //         apply_pathkeys, NULL,
  //         NULL,
  //         (void *)deparsed_pathkeys);
  //     newpath->path.param_info = path->path.param_info;
  //     add_path(baserel, (Path *)newpath);
  //   }
  // }
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
    Plan *outer_plan)
{
  // Index scan_relid = baserel->relid;
  // FdwPlanState *planstate = (FdwPlanState *)baserel->fdw_private;
  // best_path->path.pathtarget->width = planstate->width;
  // scan_clauses = extract_actual_clauses(scan_clauses, false);

  // planstate->pathkeys = (List *)best_path->fdw_private;
  // ForeignScan *s = make_foreignscan(
  //     tlist,
  //     scan_clauses,
  //     scan_relid,
  //     scan_clauses, /* no expressions to evaluate */
  //     serializePlanState(planstate),
  //     NULL,
  //     NULL, /* All quals are meant to be rechecked */
  //     NULL);
  ForeignScan *s = NULL;
  return s;
}

/*
 *	"Deserialize" an internal state and inject it in an
 *	FdwExecState
 */
FdwExecState *initializeExecState(void *internalstate)
{
  // FdwExecState *execstate = palloc0(sizeof(FdwExecState));
  // // internalstate is actually a list generated by serializePlanState consisting of:
  // //  numattrs, target_list, target_list, pathkeys
  // List *values = (List *)internalstate;
  // AttrNumber numattrs = ((Const *)linitial(values))->constvalue;
  // List *pathkeys;
  // int limit;
  // /* Those list must be copied, because their memory context can become */
  // /* invalid during the execution (in particular with the cursor interface) */
  // execstate->target_list = copyObject(lsecond(values));
  // pathkeys = lthird(values);
  // limit = ((Const *)lfourth(values))->constvalue;

  // execstate->pathkeys = deserializeDeparsedSortGroup(pathkeys);
  // execstate->buffer = makeStringInfo();
  // execstate->cinfos = palloc0(sizeof(ConversionInfo *) * numattrs);
  // execstate->numattrs = numattrs;
  // execstate->values = palloc(numattrs * sizeof(Datum));
  // execstate->nulls = palloc(numattrs * sizeof(bool));
  // execstate->limit = limit;
  // return execstate;
}

