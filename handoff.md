# Handoff: test Omega PR #481 on the supported machines

Temporary. This file is the only commit on top of the PR's test merge and will be dropped before anything is merged. Do not commit to `pr481` or push it.

## The job

For your machine's rows in the testing checklist of https://github.com/E3SM-Project/Omega/pull/481, run:

1. **CTests** on the PR build.
2. **Polaris `omega_pr`** on the PR build, compared against a baseline built from `develop`.

| Machine | Polaris compiler | MPI |
|---|---|---|
| aurora | `oneapi-ifx` | mpich |
| chrysalis | `intel` | openmpi (done by the Chrysalis agent) |
| frontier | `craygnu` | mpich |
| frontier | `craygnu-mphipcc` | mpich |
| pm-cpu | `gnu` | mpich |
| pm-gpu | `gnugpu` | mpich |

Only Xylar submits jobs. Set everything up, list the submit commands, and ask; a yes covers that request only. Do not build Omega on a login node: build inside the job, as below.

## Code to test

- **PR:** branch `pr481` on `git@github.com:xylar/E3SM.git`. Its parent, `c444c6e400`, is GitHub's test merge of the PR head `b3d8b9731f` into develop.
- **Baseline:** `c1aacdc2d7`, the first parent of that merge (Omega `develop` on 2026-09-28).
- **Polaris:** your machine's existing `main` checkout (`abe178ee8` or newer) and its load script. Do not run `./deploy.py`.

Make two Omega trees in a scratch test directory, then init submodules in each on the login node (the job would otherwise clone them):

```bash
git fetch git@github.com:E3SM-Project/Omega.git develop
git worktree add --detach $TEST/omega-develop c1aacdc2d7
git fetch git@github.com:xylar/E3SM.git pr481
git worktree add --detach $TEST/omega-pr481 FETCH_HEAD
cd $TEST/omega-<each>
git submodule update --init --recursive externals/ekat externals/scorpio components/omega/external cime
```

## Two things that will bite

**CTests need the new sphere mesh.** `develop` includes Omega#547, so the sphere CTests expect `cosine_bell_icos480.omega_vars.260911.nc`. Polaris `main`'s CTest utility still links `...260807.nc`, and `HORZOPERATORS_SPHERE_TEST`, `AUXVARS_SPHERE_TEST`, `TEND_SPHERE_TEST` and `FORCING_SPHERE_TEST` then fail. The fix is Polaris #779, still open. Use a scratch copy of the utility with that one-line change; it downloads the mesh if your database lacks it:

```bash
# $POLARIS is your Polaris main checkout
mkdir -p $TEST/ctest_utility
cp $POLARIS/utils/omega/ctest/{omega_ctest.py,run_command.template} $TEST/ctest_utility/
sed -i 's/icos480.omega_vars.260807/icos480.omega_vars.260911/' $TEST/ctest_utility/omega_ctest.py
```

**The standalone build may fail at CMake configure** with `Could NOT find OpenMP (missing: OpenMP_Fortran_FOUND Fortran)`. That is Omega#572, seen on Chrysalis with both intel and gnu. If you hit it, merge the fix from open PR #574 (`edefec8790`) into *both* trees and say so in your results. Chrysalis needed it.

```bash
git fetch git@github.com:E3SM-Project/Omega.git pull/574/head
git -C $TEST/omega-<each> merge --no-edit -m "Test merge of Omega#574" edefec8790
```

## The three jobs

Run them in this order, each depending on the previous with `afterany` (PBS on Aurora: `qsub -W depend=afterany:<id>`). Two Omega builds running at once race in CIME's configure and one dies with `FileNotFoundError` on `CASEROOT` or `obj`; that is not an Omega problem.

1. **PR build + CTests** (1 node): source the load script, `cd $TEST/run-pr481`, `python $TEST/ctest_utility/omega_ctest.py -c -o $TEST/omega-pr481`. Inside a job the utility builds and runs CTests in one go; the build lands in `build_omega/build_<machine>_<compiler>`.
2. **Baseline** (as many nodes as `omega_pr`'s generated job script asks for): the same in `$TEST/run-develop` with `-o $TEST/omega-develop`, then
   `polaris suite -c ocean -t omega_pr --model omega -w $TEST/omega_pr_develop -p $TEST/run-develop/build_omega/build_<machine>_<compiler>`,
   then `cd $TEST/omega_pr_develop; source load_polaris_env.sh; polaris serial omega_pr`.
3. **PR suite**: `polaris suite ... -w $TEST/omega_pr_pr481 -p $TEST/run-pr481/build_omega/build_<machine>_<compiler> -b $TEST/omega_pr_develop`, then run it the same way.

The Chrysalis scripts are templates: `/lcrc/group/e3sm/ac.xylar/polaris_1.1/chrysalis/test_20260928/omega481/job_*.sh`. Take the scheduler header (account, partition or QOS, constraint, GPU options) from a job script Polaris has already generated on your machine. Give the output file an absolute path.

## What to report back

Per row: Polaris hash; Omega hashes for baseline and PR, and whether #574 was merged in; both `-p` build paths; CTest pass count; `omega_pr` result for each task, including any baseline differences. Known and not caused by this PR: property checks can fail silently while the task still reports PASS, so read the log for `FAIL` lines inside passing tasks rather than trusting the summary alone.
