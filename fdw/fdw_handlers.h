// Generated by cgo from fdw.go. Included here so our functions are
// defined and available.
#include "fmgr.h"

static bool fdwIsForeignScanParallelSafe(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void fdwGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void fdwGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static ForeignScan *fdwGetForeignPlan(
    PlannerInfo *root,
    RelOptInfo *baserel,
    Oid foreigntableid,
    ForeignPath *best_path,
    List *tlist,
    List *scan_clauses,
    Plan *outer_plan
);

// Define our handling functions with Postgres, following the V1 protocol.
PG_FUNCTION_INFO_V1(fdw_handler);
PG_FUNCTION_INFO_V1(fdw_validator);


Datum fdw_handler(PG_FUNCTION_ARGS) {
  FdwRoutine *fdw_routine = makeNode(FdwRoutine);
  fdw_routine->IsForeignScanParallelSafe = fdwIsForeignScanParallelSafe;
  fdw_routine->GetForeignRelSize = fdwGetForeignRelSize;
  fdw_routine->GetForeignPaths = fdwGetForeignPaths;
  fdw_routine->GetForeignPlan = fdwGetForeignPlan;
  fdw_routine->ExplainForeignScan = goFdwExplainForeignScan;
  fdw_routine->BeginForeignScan = goFdwBeginForeignScan;
  fdw_routine->IterateForeignScan = goFdwIterateForeignScan;
  fdw_routine->ReScanForeignScan = goFdwReScanForeignScan;
  fdw_routine->EndForeignScan = goFdwEndForeignScan;
  fdw_routine->ImportForeignSchema = goFdwImportForeignSchema;
  fdw_routine->ExecForeignInsert = goFdwExecForeignInsert;

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