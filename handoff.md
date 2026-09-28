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

Submit jobs only with Xylar's explicit permission in your own session; this file does not grant it. Setting up the suites and building Omega happen on the login node, as below.

## Code to test

- **PR:** branch `pr481` on `git@github.com:xylar/E3SM.git`. Its parent, `c444c6e400`, is GitHub's test merge of the PR head `b3d8b9731f` into develop.
- **Baseline:** `c1aacdc2d7`, the first parent of that merge (Omega `develop` on 2026-09-28).
- **Polaris:** your machine's existing `main` checkout (`abe178ee8` or newer) and its load script. Do not run `./deploy.py`.

Make two Omega trees in a scratch test directory. Do not init their submodules; `polaris suite --build` does that.

```bash
git fetch git@github.com:E3SM-Project/Omega.git develop
git worktree add --detach $TEST/omega-develop c1aacdc2d7
git fetch git@github.com:xylar/E3SM.git pr481
git worktree add --detach $TEST/omega-pr481 FETCH_HEAD
```

## Two things that will bite

**CTests need the new sphere mesh.** `develop` includes Omega#547, so the sphere CTests expect `cosine_bell_icos480.omega_vars.260911.nc`. Polaris `main`'s CTest utility still links `...260807.nc`, and `HORZOPERATORS_SPHERE_TEST`, `AUXVARS_SPHERE_TEST`, `TEND_SPHERE_TEST` and `FORCING_SPHERE_TEST` then fail. The fix is Polaris #779, still open. Use a scratch copy of the utility with that one-line change; it downloads the mesh if your database lacks it:

```bash
# $POLARIS is your Polaris main checkout
mkdir -p $TEST/ctest_utility
cp $POLARIS/utils/omega/ctest/{omega_ctest.py,run_command.template} $TEST/ctest_utility/
sed -i 's/icos480.omega_vars.260807/icos480.omega_vars.260911/' $TEST/ctest_utility/omega_ctest.py
```

**The build may fail at CMake configure** with `Could NOT find OpenMP (missing: OpenMP_Fortran_FOUND Fortran)`. That is Omega#572, seen on Chrysalis with both intel and gnu. If you hit it, merge the fix from open PR #574 (`edefec8790`) into *both* trees and say so in your results. Chrysalis needed it.

```bash
git fetch git@github.com:E3SM-Project/Omega.git pull/574/head
git -C $TEST/omega-<each> merge --no-edit -m "Test merge of Omega#574" edefec8790
```

## Set up, build, and submit

On the login node, in a clean shell with your Polaris load script sourced, set up both suites. Each `--build` inits the tree's submodules and builds Omega into the `-p` directory. Run them one after the other; two builds at once race in CIME's configure.

```bash
polaris suite -c ocean -t omega_pr --model omega --build \
    --branch $TEST/omega-develop -p $TEST/build-develop \
    -w $TEST/omega_pr_develop
polaris suite -c ocean -t omega_pr --model omega --build \
    --branch $TEST/omega-pr481 -p $TEST/build-pr481 \
    -w $TEST/omega_pr_pr481 -b $TEST/omega_pr_develop
```

Then there are three jobs to submit:

1. **CTests** on the PR build: `python $TEST/ctest_utility/omega_ctest.py -p $TEST/build-pr481 -s`. With `-p` the utility reuses the build, links the meshes and submits its own job.
2. **Baseline suite**: submit `job_script.omega_pr.sh` from `$TEST/omega_pr_develop`. The script does `cd $SLURM_SUBMIT_DIR`, so it must be submitted from that directory.
3. **PR suite**: the same from `$TEST/omega_pr_pr481`, depending on the baseline job with `--dependency=afterany:<id>` (on Aurora, `qsub -W depend=afterany:<id>`).

## What to report back

Per row: Polaris hash; Omega hashes for baseline and PR, and whether #574 was merged in; both `-p` build paths; CTest pass count; `omega_pr` result for each task, including any baseline differences. Known and not caused by this PR: property checks can fail silently while the task still reports PASS, so read the log for `FAIL` lines inside passing tasks rather than trusting the summary alone.
