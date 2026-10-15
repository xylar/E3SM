# MPAS-Ocean <-> MALI ice-shelf melt coupling: plan

Written 2026-10-13. Current prototype: `ocn-glc/modify-ocn-glc-melt-coupling`
(E3SM-Ocean-Discussion#139, tip `442dc3c90e`). Design: `main.tex` (Overleaf);
suggested fixes in `main_suggested.tex` and `main_suggested.diff` next to this file.

## Decisions (Xylar, 2026-10-13)

- **Unpushed work.** The features checked off in `main.tex` were implemented but
  probably lost with a deleted local branch. Re-implement them.
- **Heat to MALI.** Drop the ocean-to-MALI heat fields (`landIceCouplingHeatFlux`,
  `Fogx_qicehi`, `basalOceanHeatFlux`).
- **Conductive flux.** Use a dedicated glc-to-ocean field, not `Flgg_hflx`.
- **#8047 fixes.** Put them on a new branch with fixes for #8047. Test them as part
  of this work, then comment on #8047 pointing to commits that can be
  cherry-picked. #8047 is stuck because master is not accepting non-BFB changes;
  BlueTip SORRM has been abandoned, so it is no longer a factor.
- **#8661.** Build the MALI-driver work on top of it.
- **MOAB.** MCT only until MOAB support is finalized elsewhere, while keeping the
  eventual MOAB move as painless as possible.
- **Feature test.** A G-case is fine. Use existing inputs only; nothing new should
  be created.
- **Branch and PR.** A new branch and a new Discussion PR. #139 stays as the
  prototype.

## Code to reuse (found 2026-10-13)

- **`recovered/ocn-glc-melt-coupling-20260421`** (local branch, tip `12c6e19fa2`). This
  is the lost work, recovered from unreachable objects; a local branch now keeps it
  from being garbage-collected.
  - Base: `52edc135a9`, master (2026-04-17, after #8101) merged into #8047.
  - Then the 10 prototype commits, rebased.
  - Then 11 commits from 2026-04-20/21. Use as reference or cherry-pick sources:
    - Worth keeping:
      - `258c545733` index and Kelvin-averaging fixes
      - `deb744025b` MCT shelf-flux mapping and merge, with an explicit
        `seq_flds_x2g_shelf_fluxes_from_ocn` list
      - `987315a72c` MALI sign; still needs the frazil sign fixed to
        `Foxo_frazil_li - Foxo_ismw`
      - `62dd620efe` conductive flux; switch it from `Flgg_hflx` to a dedicated
        field
      - `c5bbf7287a` MALI thermal BC from `theta_b`
      - `c4604167d8` budgets; partly reusable
    - Drop, because they implement the heat-to-MALI path: `e75cfe1c36`,
      `6e143704b5`, `dadf7a5b7b`, `c5c72a7980`, `12c6e19fa2`.
  - Still missing even there:
    - Turning on `ocn_c2_glcshelf`, which #8047 hard-wires to false, so nothing is
      mapped.
    - Restoring `prep_glc_accum_ocn`.
    - The MALI-to-ocean pressure and fraction import.
  - Probably never run end to end: there are no cases after 2026-01-06.
- **E3SM-Ocean-Discussion#128** (closed; fetch with `pull/128/head`, tip `8ec175b9d1`).
  This was the coupler-side melt design.
  - `7bd599181d` covers MALI-to-ocean `landIcePressure`, floating fraction and ice
    fraction. MALI exports `Sg_below_sea_level_mask`, masks `Sg_lithop` and the ice
    masks with it, and the ocean renormalizes by the remapped mask. That answers
    the `p_i` normalization question.
  - `a68c13aaa3` makes `Sg_lithop` use MALI's `config_ice_density` instead of the
    sea-ice constant.
  - `8ec175b9d1` is the coupler-budget work.
- **Previous runs** (`~/e3sm_cases`). Every MALI-coupled case used `MALI%SIASTATIC`
  (`GMPAS-JRA1p5-DIB-PISMF-SIS`) with MCT:
  - `TL319_SOwISC12to30E3r4_ais4to20`, built from the #128 branch with
    `config_land_ice_flux_mode = 'coupled'`:
    - 1-year runs: `coupled_ismf`, `mali_topo_and_fracs`,
      `mali_topo_and_diag_land_ice_draft`, 2025-04 to 2025-07
    - 3-month runs: `coupler_budget`, 2025-08
  - Control: `20250125.GMPAS-JRA1p5-DIB-PISMF.TL319_SOwISC12to30E3r4`, a 1-year SGLC run.

## Where things stand

- **Prototype branch.** 21 commits on a master from 2025-10-27 (`effaf31abb`),
  3250 commits behind `origin/master` (`aee833b4f1`).
  - The first 11 commits are an older copy of E3SM#8047. #8047 has since been
    rebased onto `0b94f9a855` (2026-02-06), three of those commits changed, and it
    gained `e118f76f27` "Land ice pressure off by default".
  - The remaining 10 commits are the coupling prototype. They remove the
    coupler-side melt solve and add `So_tfrz_isf`, `So_liflfrac`, `Foxo_ismw` and
    `Foxo_frazil_li` for glc, plus `iceOceanInterfaceTemperature` in MALI.
- **Bugs in the prototype as pushed:**
  - `glc_cpl_indices.F` looks up `index_x2g_So_tfrz_isf` and `index_x2g_So_liflfrac`
    in `g2x`, so both come back 0.
  - `mpas_ocn_time_average_coupled.F` adds `T0_Kelvin` to the running average on
    every accumulation step. It also indexes the interface-tracer array with the
    surface-tracer `index_temperature`.
  - `floatingBasalMassBal = Foxo_ismw + Foxo_frazil_li` has the wrong sign. It
    should be `Foxo_frazil_li - Foxo_ismw`.
  - The new fields never reach MALI:
    - `cime_run_ocnglc_coupling` only maps when `ocn_c2_glctf` is set.
    - `prep_glc_merge_ocn_forcing` copies only thermal-forcing states.
    - `mapper_Fo2g_shelf` is never applied.
    - The `prep_glc_accum_ocn` call was removed.
  - `ocn_export_moab` writes `o2x_o % rAttr`, so the MOAB build should fail. That
    routine no longer exists on master.
- **Not implemented, although `main.tex` checks it off:**
  - Import of `Sg_lithop`, `Sg_icemask_floating` and the conductive flux into
    MPAS-Ocean.
  - `landIceCouplingHeatFlux`.
  - The `Fogx_qicehi` combination and `basalOceanHeatFlux`.
  - The `Sg_icemask` index typo fix.
  - MALI's use of the interface temperature.
- **E3SM#8047 on current master.**
  - It merges with a single conflict, in `ocn_comp_mct.F`. The resolution in
    `9a92c9c941` (thin-film branch) can be reused verbatim, because master has not
    touched that file since `6dc4280a2d`.
  - It still needs the fixes `f1ca83aa24` (XML `<=` in Registry) and `9047419a0d`
    (`landIceDraftForSsh` segfault).
  - It removes `config_land_ice_flux_mode = 'coupled'` and already changes the
    `mpaso/ocn_glcshelf` testmod to `'active'` (`c2b58e7414`, `c0e73517af`), so
    the three `*.mpaso-ocn_glcshelf` tests keep running. Only the testmod's README
    was stale. (An earlier claim that these tests would abort was wrong: a git
    pathspec with `*` missed the testmod.)
- **MOAB.**
  - E3SM#8101 made one MCT/MOAB import/export for MPAS-Ocean, using 2D arrays.
  - E3SM#8661 (Rob Jacob; approved, not merged) does the same for MALI and ports
    lnd<->glc to MOAB.
  - All ocn<->glc MOAB coupling (thermal forcing, ice shelf, glc runoff, x2gacc
    restart) is deferred to a follow-up PR that does not exist yet.
  - #8661 still calls the coupler-side melt solve in driver-moab.
  - MOAB has been the default driver since #8431, so every MALI test in
    e3sm_cryo_developer and e3sm_developer is pinned to `_Vmct`.
    `e3sm_landice_developer` is not pinned.

## Recommendation

Re-implement on a fresh base. Use the prototype, the recovered branch and #128 as
references, cherry-picking pieces where they are correct (see "Code to reuse").
Do not rebase the prototype. Reasons:

- The prototype is roughly a third of the design, it carries the bugs above, and
  it mixes cleanup with new behavior. That makes a BFB argument hard.
- Four of its 10 commits conflict when replayed onto master + #8047.
- Its MALI-driver commits will conflict with #8661, which rewrites the same import
  and export routines.

## Plan

### 1. #8047 fixes branch, then base

1. **Fixes branch** `ocn/modify-land-ice-melt-equations-fixes` (done 2026-10-13).
   It starts at #8047's head (`e118f76f27`), so its commits cherry-pick cleanly
   onto Carolyn's branch:
   - `ce65b3add8` and `060c29810b`: cherry-picks of `f1ca83aa24` and `9047419a0d`.
   - `b26e1a585b`: corrects the stale `mpaso/ocn_glcshelf` README (optional for
     #8047).
2. **Test** master merged with the fixes branch: `e3sm_cryo_developer` plus
   `ERS_Vmct_Ld5.TL319_oQU240wLI_ais8to30.MPAS_LISIO_JRA1p5.chrysalis_intel.mpaso-ocn_glcshelf`
   (test-id `ismc-base`).
   - These runs also generate the baselines for section 2: name `base-84f70c4093`,
     root `/lcrc/group/e3sm/ac.xylar/baselines/ice-shelf-melt-coupling`.
   - On the command line, a test with a testmod needs the
     `.chrysalis_intel.<testmod>` form, because CIME splits test names on dots.
3. **Comment on #8047.** Push the fixes branch and post a short comment pointing to
   the commits to cherry-pick, with the test results. Follow the GitHub-writing
   rules.
4. **Feature-branch base** (done 2026-10-13): `ocn-glc/ice-shelf-melt-coupling` in
   worktree `~/e3sm_work/e3sm/ocn-glc/ice-shelf-melt-coupling`.
   - Merge commit `84f70c4093` = master `aee833b4f1` + the fixes branch.
   - The `ocn_comp_mct.F` conflict was resolved with `9a92c9c941`'s version; all
     three merge inputs were byte-identical.
   - Submodules needed for G-cases with MALI were initialized with `--dissociate`.
     `fates` is needed too: `Depends.chrysalis.intel.cmake` names a FATES source.
   - Result (2026-10-13): all 10 tests pass (`ismc-base`, `ismc-base2`); baselines
     generated. The #8047 comment is posted (issuecomment-6147598204).

```
git worktree add -b ocn-glc/ice-shelf-melt-coupling <path> origin/master
git merge --no-ff <fixes-branch>      # #8047 + fixes; conflicts only in ocn_comp_mct.F
git checkout 9a92c9c941 -- components/mpas-ocean/driver/ocn_comp_mct.F
git commit                             # same inputs, so same resolution
```

The BFB reference ("merged base") is `84f70c4093`. If #8661 has merged by then,
include it too: it is non-BFB for prognostic-MALI cases. Once Carolyn
cherry-picks the fixes and #8047 merges, rebasing onto master drops the duplicate
commits.

### 2. BFB cleanup commits

Each commit should be BFB except for `FIELDLIST` in coupler history. The
ice-shelf fields are defined unconditionally in `seq_flds_mod.F90`, so every
coupled test's `cpl.hi` field list changes.

1. Remove the coupler-side melt solve from both drivers. That covers
   `prep_glc_calculate_subshelf_boundary_fluxes`, `compute_melt_fluxes`, and their
   fields:
   - `So_blt`, `So_bls`, `So_htv`, `So_stv`, `So_rhoeff`
   - `Sg_blit`, `Sg_blis`
   - `Fogx_qicelo`, `Fogx_qiceho`, `Fogx_qiceli`, `Fogx_qicehi`
   - `Sg_tbot`, `Sg_dztbot`

   Also remove the matching component code. The path is dead after #8047, because
   `ocn_c2_glcshelf` is always false. Done: `c5efe050de`. MALI still zeroes
   `floatingBasalMassBal` on import (BFB); section 4 makes the import coupled-only.
2. Fix `index_x2o_Sg_icemask`, which is assigned three times in
   `mpaso_cpl_indices.F`. Done: `2a1e28b909`.
3. Moved to section 3: give `prep_ocn_shelf_calc_g2x_ox` explicit state and flux
   field lists. Today it maps every glc field with `Sg2o` and then overwrites all of
   them with `Fg2o`. The lists name section 3's new fields.

Result (test-id `ismc-s2`, compared with `base-84f70c4093`): all 10 tests are BFB
except `FIELDLIST`, which appears only in `cpl.hi`. One PEM test shows `TPUTCOMP`
noise (6.2%). The branch is pushed to the xylar fork at `2a1e28b909`.

### 3. Coupled mode, MCT

Turn this on only when `OCN_GLC_ISMF_COUPLING = 'coupler'` (PISMF with MALI). It
should then be BFB for every existing test except the `ocn_glcshelf` ones.

- **MPAS-Ocean namelist.** Add a logical, name TBD, set by build-namelist for
  `coupler`. It requires `config_land_ice_flux_mode = 'active'` and
  `config_use_land_ice_pressure = .true.`, and it sets `ocn_c2_glcshelf`.
- **Ocean export.** Put normalization, signs and units in the component, not the
  coupler.
  - A net mass flux per unit floating area, `(Q_b^m - Q_fr^m) / phi_i`, set to zero
    below a `phi_i` threshold. Either one combined field, or `ismw` and `frazil`
    separately with the same o2x and x2g names.
  - `phi_i * theta_b` (in K, converting each sample before averaging) and `phi_i`.
  - Keep `Foxo_ismw`, `Foxo_frazil_li` and `Foxo_q_li` unchanged for the coupler
    budgets.
- **Ocean import.** Read `Sg_lithop`, `Sg_icemask_floating` and a new conductive-flux
  field (e.g. `Fogg_hcond`, positive into the ocean) into `landIcePressure`,
  `landIceFloatingFraction` and `landIceConductiveHeatFlux`.
  - Decide how `p_i` is normalized in partially floating cells so that it matches
    how the SOwISC12to30E3r4 IC was built (possibly remap `phi*p` and divide by
    `phi`).
- **Coupler.** Map, merge and accumulate only; no arithmetic between fields.
  - ocn->glc: states via `mapper_So2g_shelf`, fluxes via `mapper_Fo2g_shelf`, each
    with an explicit field list.
  - Merge both lists into `x2g`.
  - Accumulate and average over the glc coupling interval (`prep_glc_accum_ocn`);
    check that `x2gacc` is in the MCT restart.
  - glc->ocn: use the conservative map for the floating fraction, pressure and
    conductive flux, both at init and after each glc run.
- **Field definitions.** Add them to both `seq_flds_mod.F90` copies. Use the same
  name on the o2x/x2g sides and on the g2x/x2o sides, so MOAB mappers need no merge
  step.
- **Dropped from the design (decided):** ocean-to-MALI heat fields (`Foxo_ismh` for
  MALI, `Fogx_qicehi`, `landIceCouplingHeatFlux`, `basalOceanHeatFlux`). Under the
  preferred option, MALI takes `theta_b` as its basal boundary condition and
  computes its own conductive flux, so it never uses `Q_b^T`. The conductive flux
  uses a dedicated glc-to-ocean field, not `Flgg_hflx` (decided).

**Done 2026-10-13**: `77dad79587`, `684a7dec3b`, `dc496e7c1a`, `0088de5052`, pushed.
Tests (`ismc-s3c`, compared with `base-84f70c4093`):
- All 10 tests pass restart, decomposition and threading checks.
- Every test fails `NLCOMP`, because of the new namelist options.
- Non-MALI tests show only `FIELDLIST`.
- In the three MALI tests, only the coupler-history glc geometry fields differ;
  ocean and sea-ice fields are BFB.
- From the DIS/SIS coupler history, exported melt is conserved to 2e-14, and MALI
  recovers interface temperatures of 270.96-271.56 K.

- **Fields:**
  - ocn->glc, only with `flds_polar`:
    - `Foxo_ismf`: net melt per floating area
    - `So_tfrz_isf`: interface temperature times weight
    - `So_liflfrac`: the weight
  - glc->ocn:
    - `Sg_lithop`, `Sg_icemask_grounded` and `Sg_icemask_floating`, now masked by
      `Sg_below_sea_level_mask`
    - `Sg_below_sea_level_mask` (new)
    - `Fogg_hcond` (new)
  - The grounded mask (masked) stands in for #128's `Sg_icemask_below_sea_level`.
- **MPAS-Ocean options:**
  - `config_glc_ice_shelf_coupling` is set for `coupler`.
  - `config_glc_ice_shelf_coupling_import_geometry` defaults to `.true.`. It was
    added because importing MALI's geometry into the oQU240wLI tests blew up the
    ocean: the pressure differed from the IC by up to 3.2 MPa. The `ocn_glcshelf`
    testmod turns it off.
  - `config_glc_ice_shelf_coupling_min_floating_fraction` defaults to 1e-3.
- **Normalization:** the export divides by glc's raw floating fraction (not the one
  renormalized by the below-sea-level fraction), and the shelf fields are mapped
  with `norm=.false.` in both directions. Offline, this conserves the exported melt
  to 2e-14; with `norm=.true.` it was off by 0.9%. Xylar approved this choice
  (2026-10-13): conservation is required.
- **Compatible pairing (Xylar, 2026-10-13):** SOwISC12to30E3r4 + ais4to20 is the
  only ocean/MALI initial-condition pairing designed to match. Coupled runs with
  geometry import on any other pairing (e.g. oQU240wLI + ais8to30) are expected to
  crash. A coarser ocean IC built to match a MALI mesh would let the cheap tests
  exercise geometry import, but none exists yet.
- **Mismatch check:** for SOwISC12to30E3r4 + ais4to20, MALI's pressure differs from
  the IC by 0.006 MPa on average, but by up to ~1 MPa in a few edge cells. Watch the
  start of the section 5 run.
- **Follow-ups:**
  - ~~`basalConductiveFlux` may not be in MALI's restart~~: not needed (section 4).
  - The oQU240wLI and IcoswISC30E3r5 grids use `config_land_ice_draft_mode =
    'data'`, so the draft does not follow the imported pressure.
  - Coupler budgets for the new fields.
  - `prep_ocn_init` still initializes the now-unused `mapper_Sg2o`.

### 4. MALI side

Build this on #8661 (array import/export) once it merges, or merge its branch
temporarily.

- **Test merge (2026-10-13).** #8661 (head `fb06dff1ab`) is approved but still
  open. Merging it into the feature branch conflicts in three files, all
  mechanically:
  - `glc_comp_mct.F` (4 hunks): `x2g_g % rAttr(i, n)` became `x2g_g(i, n)`.
    #8661 still imports `Fogx_qiceli` and exports `Sg_tbot`, `Sg_dztbot` and an
    unmasked `Sg_lithop`.
  - driver-moab `cime_comp_mod.F90` (1 hunk) and `prep_glc_mod.F90` (3 hunks):
    #8661 keeps the coupler melt solve that section 2 removed. Keep its new
    lnd<->glc MOAB code and drop the melt solve again.

- **Import.**
  - `floatingBasalMassBal` comes from the net mass flux, positive for accretion.
  - `iceOceanInterfaceTemperature` = `remap(phi*theta_b) / remap(phi)`, set to the
    invalid value below `config_min_floating_frac`.
- **Thermal solver.** For floating ice, set `basalTemperature` from
  `iceOceanInterfaceTemperature` where it is valid. Otherwise fall back to the
  current depth-based freezing temperature.
- **Export.** Send the conductive flux (`basalConductiveFlux`, positive down) for
  floating ice. Send 0 when the thermal solver is `'none'` (STATIC and DATA modes)
  or the ice is grounded.
- **Ownership.** MALI `src/` changes may need to go through MALI-Dev; coordinate
  with Matt Hoffman.
- **Done (2026-10-13)** on `ocn-glc/ice-shelf-melt-coupling-dev` (worktree
  `ice-shelf-melt-coupling-dev`, no submodules; fast-forward the feature branch to
  test):
  - `8f37298bfb` makes geometry import opt-in: `.true.` only for
    `ocn_grid=SOwISC12to30E3r4` with `glc_grid=mpas.ais4to20km` (MPAS-Ocean's
    build-namelist now receives `GLC_GRID`).
  - `fb8724b28a` merges #8661 as described above.
  - `eedccc90d3` imports `floatingBasalMassBal = -Foxo_ismf` (zero without coupling,
    as before) and `iceOceanInterfaceTemperature`, with new MALI options
    `config_min_floating_frac` (0.01) and `config_invalid_value_ocn_intr_temp`
    (1e36). `li_thermal_init` sets the field to the invalid value until the first
    import.
  - `49060ae3c3` uses it as the floating-ice basal temperature where valid.
  - Restart: `basalConductiveFlux` needs no MALI restart. The MCT coupler restarts
    `g2x_gx` before the init glc->ocn shelf mapping, and MALI recomputes the flux
    every timestep before its next export.
  - No existing test runs this path: DIS/SIS turn off MALI's thermal solver and
    floating melt, and no compset pairs PISMF with prognostic MALI. Smoke-test it
    on `TL319_oQU240wLI_ais8to30` (code paths and restart only; the geometries do
    not match). A long compset name in a test name breaks the build (make treats
    `%` in the case path as a pattern), so the smoke tests used an uncommitted alias
    `GMPAS-JRA1p5-DIB-PISMF-PIS` = `2000_DATM%JRA-1p5_SLND_MPASSI%DIB_MPASO%
    IBPISMFDATMFORCED_DROF%JRA-1p5_MALI%SIA_SWAV`.
  - **Results (2026-10-13).** Feature branch pushed at `49060ae3c3`.
    - `ismc-s4` vs `s3-8f37298bfb`: all 10 `BASELINE` PASS; `NLCOMP` fails only
      for the 3 MALI tests (the two new MALI options); one `TPUTCOMP` (-6.5%) on
      `MPAS_LISIO`. MPAS-O and MPAS-SI history are `exclude_testing` in
      `config_archive.xml`; manual cprnc of them against the baselines: all
      identical. The 7 MOAB-driver tests build with the merged #8661.
    - `ismc-s4smoke2`: `ERS_Vmct_Ld5` PASS including `COMPARE_base_rest` (cpl.hi;
      MALI wrote no history in both legs) and `SMS_D_Vmct_Ld1` PASS. Coupled
      fields are live both ways: interface T 270.95-271.56 K on MALI cells;
      `g2x_Fogg_hcond` -19 to +0.85 W m-2 (positive where the basal ice is warmer
      than the seawater freezing point, which is physically possible).

### 5. Tests and validation

- **Staging (Xylar, 2026-10-13):** the full suites below are overkill for now.
  Start small and expand only as things look good:
  1. 5-day control, sanity (coupled, geometry import off) and coupled runs in
     `~/e3sm_cases/20261013.*.ismc_{control,sanity,coupled}.chrysalis` (MCT,
     8 nodes, 512x2, MALI 256 tasks, debug queue). Sanity must be BFB with the
     control (ocean restart + cpl.hi at day 5).
  2. Then year-long control and coupled runs.
  3. Later, if needed: the MALI-with-land MCT tests (`IGELM_MLI`, `MALISIA`).
- **5-day results (2026-10-13/14):**
  - Sanity vs control: first run NOT BFB, because `ROF_NCPL` defaults differ: 24
    for `_DATM.*_SLND.*MPASO.*_MALI`, 48 (= `ATM_NCPL`) for the control. The
    runoff update at 12:00 then reaches the ocean a step apart. With
    `ROF_NCPL=48` in the sanity case: ocean and sea-ice restarts at day 5
    IDENTICAL; cpl.hi differs only in the MALI fields and the new ocean exports.
    Plumbing verified. Always match `ROF_NCPL` in coupled-vs-control comparisons.
  - Conservation on SOwISC12to30E3r4/ais4to20 (sanity cpl.hi, day 5): ocean-side
    and MALI-side integrals of the exported net melt agree to 6e-15.
  - Coupled (geometry import): NaN under the western Ross Ice Shelf within the first
    coupling interval, with draft mode 'pressure-dependent' and also 'data'.
  - MALI ICs 20241224, 20250411 (used in 2025) and 20250625 give identical geometry;
    so the 2025 runs survived the same mismatch, and what changed is the ocean.
  - Imported (below-sea-level weighted) pressure vs the IC: mean |d draft| 0.57 m,
    but 33 cells get a draft below the bottom and 53 a water column < 10 m (the IC's
    minimum is 10.1 m), mostly partially grounded cells at grounding lines, e.g.
    western Ross. 134 cells lose ice, 4 gain it. Floating-weighted pressure is much
    worse (mean 14 m; 228 negative columns), so the IC was built with the
    below-sea-level weighting.
  - Scripts: `$SP/scripts/{import_vs_ic,pressure_options,first_diff,s5_compare}.py`
    (session scratchpad).
- **Decisions (Xylar, 2026-10-14):**
  - Runoff couples every 30 min in MALI G-cases too: `1a0bfd1cf1` drops the
    `ROF_NCPL=24` override (changes answers for DIS/SIS/LISIO/FOLISIO).
  - Option C: geometry import off by default for all grids (`f1436c4b2b`, also
    drops the GLC_GRID plumbing). A matching ocean IC (option B) waits on Polaris
    work under review; then provide matching MALI/MPAS-Ocean ICs at 3 resolutions.
    Option A (clamp/ramp in the import) rejected: not a proper fix.
  - Section 5 coupled test: prognostic MALI with Albany
    (`GGMPAS-JRA1p5-DIB-PISMF-FO`, chrysalis_gnu), import off, vs a gnu control.
    Stage 1: 5-day runs `~/e3sm_cases/20261014.*.ismc_{control_gnu,fo}.chrysalis`
    (daily MALI output via SourceMods streams.landice).
  - Caveat: MALI's SMB comes from the IC file (`config_smb_source='file'`), with a
    hugely negative SMB in ice-free cells that strips ice advected past the front.
    That frontal loss leaves MALI as liquid runoff (`Fogg_rofl`), on top of the
    data icebergs (DIB), so a coupled-vs-control comparison includes extra
    Antarctic freshwater unrelated to melt coupling.
  - The merge waits on upstream PRs (#8047 is stuck behind the code freeze).
- **5-day Albany (FO) results (2026-10-14):** stable; 1.18 SYPD (gnu) vs 1.22
  control, so a year is ~20 h. MALI floating BMB -1200 to -1700 Gt/yr (melt
  1450-1770, refreezing 70-460). Thermal BC applied: basal T equals the imported
  interface T on the final record (daily output lags the import by a day); 99.9%
  of floating cells have a valid interface T. Conductive flux under floating ice
  >= 10 m thick: mean -0.2, p1 -0.86, min -9.6, max +1.8 W m-2.
  Problems:
  - 70 floating cells < 10 m thick (margins) give -1249 to +76 W m-2 (steep
    gradient over the bottom half-layer of 1-m ice). One such cell near 69.6S
    37E drove extreme ocean refreezing and a 251 K interface T, which MALI imported.
  - The ocean divides the cell-average conductive flux by MALI's floating fraction
    but applies it over its own floating area. With import off these differ: in
    that cell 0.0185 vs 0.70, so the ocean took ~38x MALI's heat. The same cell
    causes a -32% snapshot conservation error on day 2; elsewhere ~1e-4 (evolving
    mask).
  - MALI calving from `config_restore_calving_front` (~1900 Gt/yr) + face melt
    (~44) go to the ocean as solid runoff on top of DIB data icebergs (double
    counting); MPAS_LISIO/FOLISIO avoid this by having no DIB.
- **Decisions (Xylar, 2026-10-14, continued):**
  - Fix 1 (`5036c24fee`): MALI sends conductive flux only from dynamically active
    floating ice (thickness > `config_dynamic_thickness`, 10 m).
  - Fix 2 (`30925e0baf`): the ocean divides glc's cell-average conductive flux by
    max(ocean, glc floating fraction). With imported geometry the ocean's fraction
    (glc / below-sea-level fraction) is never smaller, so heat is conserved exactly
    and nothing needs to change once geometry is consistent.
  - Calving: keep DIB and accept the double counting for now. MALI's solid runoff
    is mapped nearest-neighbour onto ~2,500 Antarctic coastal ocean cells (a third
    in cavities), unlike DIB. Longer term (out of scope here): route MALI's
    calving and face melt into regional iceberg melt patterns (Xylar recalls a
    recent Schmidt et al. paper with regional patterns like the Merino data now
    used) rather than scaling one total-Antarctic field. E3SM's existing
    remove-and-rescale route (`config_remove_ais_ice_runoff` +
    `config_scale_dib_by_removed_ice_runoff`) only works with data ice-shelf melt.
  - Year-long comparison: Albany coupled vs the same compset with
    `config_glc_ice_shelf_coupling = .false.` (both carry MALI's calving).
- **5-day rerun with fixes 1-2:** exported conductive flux [-5.9, +0.18] W m-2;
  ocean interface T 270.2-271.9 K (outlier gone). The day-2 conservation error
  (-32%) remains and has a different cause: MALI's solid runoff (calving + face
  melt) is mapped with the `-nomask` nearest-neighbour map
  (`GLC2OCN_ICE_RMAPNAME`), which delivers into all 11,854 cavity cells. Melting
  that ice under the shelves forms frazil under land ice (214-327 Gt/yr vs ~0 in
  the control), which goes back to MALI as accretion: a spurious calving ->
  frazil -> accretion loop. The masked map used for MALI's liquid runoff
  (`GLC2OCN_LIQ_RMAPNAME`) excludes exactly the cavity cells. `-nomask` likely
  makes sense in 'tf' mode, where MALI adds its sub-shelf melt to Fogg_rofi; in
  coupler mode Fogg_rofi is only calving + face melt and belongs at the front.
  With `GLC2OCN_ICE_RMAPNAME` = the masked map (5-day rerun): frazil under land
  ice 0-4 Gt/yr (as in the control), no solid runoff into cavities, conservation
  snapshots within +-0.2%. Permanent fix (Xylar agreed 2026-10-14): `9471db3cdf`
  builds the MCT `mapper_Rg2o_ice` from the liquid-runoff map when
  `glcshelf_c2_ocn` is on; other modes keep the ice-runoff map. Xylar notes that
  calving into cavities is iffy in 'tf' mode too, but fixing that mode is out of
  scope here. Verified in `ismc_fo_mapfix` (default map setting): ocean, sea-ice
  and MALI restarts and cpl.hi at day 5 identical to the masked-map 5-day run.
- **Year-long runs (started 2026-10-14 07:21, ~20 h each, jobs 1308431/1308432):**
  `~/e3sm_cases/20261014.GGMPAS-JRA1p5-DIB-PISMF-FO.TL319_SOwISC12to30E3r4_ais4to20
  .ismc_{fo_1yr,fo_uncoupled_1yr}.chrysalis` at `30925e0baf`, masked solid-runoff
  map in both, monthly cpl.hi, restarts every 3 months, ocean monthly output with
  the coupling fields, MALI output every 10 days (SourceMods; `create_clone
  --keepexe` symlinks SourceMods, so each case got its own copy).
- **Year-long results (finished 2026-10-15 ~03:30):** both runs complete.
  - MALI: calving (~1800-1900 Gt/yr), face melt and SMB identical in both runs.
    Volume change: coupled -453 Gt, uncoupled +374 Gt; the difference (827 Gt)
    equals the integrated floating melt (mean ~835 Gt/yr). VAF loss is the same
    in both (~500 Gt; MALI dynamics, not melt).
  - Ocean melt drifts down from ~1340 to ~710 Gt/yr in both runs (spin-up drift,
    not coupling). Conductive flux extracts 275-290 GW (= ~26 Gt/yr of latent
    heat) and the coupled run melts 18-24 Gt/yr less: consistent.
  - Monthly cpl.hi: conservation within 1.3e-3 (mostly 1e-5-1e-4); exported
    conductive flux [-6.1, +0.25] W m-2; interface T 270.17-272.36 K; frazil
    under land ice 2-56 Gt/yr, about the same in the uncoupled run (ocean
    refreezing, not a loop).
  - MALI thermal BC: 99.9% of floating cells use the ocean interface T, equal to
    basal T (p99 difference 0); uncoupled run uses the depth-based fallback.
- **Next steps (Xylar, 2026-10-15):** open a draft PR on E3SM directly (not a
  Discussion PR; Carolyn suggested it, to show the work waiting on #8047 and
  #8661). Order: (1) coupler budgets, (2) the draft PR and the design-doc edits
  together, (3) a comment on #8661 pointing to the draft PR.
  - Budgets: `c17b8f4940` adds glc's side to the MCT budgets: f_wpolar gets
    -Foxo_ismf over glc's floating mask (scaled by the new x2gacc_gx_cnt_avg,
    since x2g is only nonzero when glc runs); f_hpolar gets its latent heat and
    -Fogg_hcond from g2x. The ocean's Foxo_ismh already includes the conductive
    heat, so no x2o term. Testing in `ismc_fo_mapfix` with BUDGETS=TRUE and
    budget_daily=2.
  - glc areas (2026-10-15): the budget showed glc receiving 0.982 of the melt
    the ocean sent. Without lnd, the coupler never set glc's aream, so it
    stayed equal to MALI's planar (polar stereographic) areas and the area
    corrections were 1, while the aave maps conserve in ESMF spherical areas
    (ratio 0.94-1.05 per cell). `1a374718cf` (before the budget commit, now
    `456d451074`) reads glc aream from glc2ocn_fmapname area_a when
    glc_c2_ocn and there is no lnd. Retest (2-day `ismc_fo_mapfix`, uncommitted
    debug prints): glc/ocean-sent 1.00000 (day 1) and 0.99995 (day 2); MALI
    applies the coupler's daily average (MALI/coupler 0.998/1.002, mask timing).
  - Catch: the gen_runoff nn runoff maps (Matt, 2025-04) take area_a from the
    SCRIP grid_area, which is MALI's planar area, so they were conservative
    with the old aream and are now corrected twice (solid runoff about +0.5%
    on day 3, up to 6.65% per cell). The same nn maps are already inconsistent
    on master in configurations with lnd (aream from the lnd2glc ESMF map).
    Xylar (2026-10-15): regenerate, and Matt should regenerate all of them
    consistently: MALI-Dev/E3SM#204. Getting new files onto inputdata is slow,
    so testing uses temporary maps in
    `/lcrc/group/e3sm/ac.xylar/ice_shelf_melt_coupling/runoff_maps` (README
    there): SCRIP copy with ESMF area_a as grid_area, runoff_map step 1 only;
    the same settings with the original SCRIP reproduce Matt's 20250415 map
    bit for bit. runoff_map's OpenMP search drops thread 0 (0-based thread
    index into 1-based arrays; patch in work/, also on ESMCI/cime master).
    2-day retest with the temporary maps (case GLC2OCN_*_RMAPNAME overrides):
    runoff rows identical to the pre-aream run, melt still closes. Branch
    pushed at `456d451074`. config_grids.xml keeps the 20250415 maps until
    #204 produces new ones. Xylar is following up with Matt (and Andrew Nolan,
    on replacing runoff_map with a newer tool); no CIME issue for the OpenMP bug.
  - Draft PR opened 2026-10-15: E3SM#8887 (text from `PR_DESCRIPTION.md`,
    untracked in the `ice-shelf-melt-coupling-dev` worktree; Xylar edits it on
    GitHub). Labels MPAS-ocean, MPAS-albany-landice, Coupler, MCT, compset, NML,
    non-BFB; no reviewers. Comment on #8661 posted (issuecomment-6195721378).
    Testing comment on #8887 posted (issuecomment-6195773099): CIME suites at
    49060ae3c3, 5-day static sanity, year-long Albany pair, 2-day budget run.
    Still to do before ready: rerun CIME suites at the tip.
    The Overleaf design doc is private, so the PR doesn't link it (publishing it
    would mean converting it to another format). Design-doc edits for the
    decisions since 2026-10-13 are in `main_suggested.tex`, with find/replace
    blocks for Overleaf in `design_doc_edits_20261015.md`.
- Compsets `GGMPAS-JRA1p5-DIB-PISMF-SIA` and `-FO` (Albany; chrysalis_gnu or
  intel-classic only) added in `1b3424c4e3`.

- **BFB checks against the merged base** (MCT):
  - `e3sm_developer`, which includes `e3sm_cryo_developer`.
  - `e3sm_landice_developer --driver mct`.
  - `e3sm_diff_dev`, for the MOAB field-list impact.
  - Expect BFB plus `FIELDLIST` everywhere except the `ocn_glcshelf` tests, which
    change once coupling is on.
- **Keep the `_Vmct` pins** on the `ocn_glcshelf` tests. #8047 already updates
  that testmod.
- **Feature test**, both runs with `--driver mct`:
  - Control: `GMPAS-JRA1p5-DIB-PISMF` on `TL319_SOwISC12to30E3r4` (SGLC; IC geometry).
  - Coupled: `GMPAS-JRA1p5-DIB-PISMF-SIS` on `TL319_SOwISC12to30E3r4_ais4to20` (MALI
    STATIC mode). This matches the earlier runs in `~/e3sm_cases`.
    - No ais4to20km datamode file exists in `data/inputdata` or
      `public_html/inputdata`. MALI `buildnml` has never had a `datamode_date` for
      this mesh, and DATA mode was only ever set up for ais8to30.
    - The default MALI IC, `ais_4to20km.20250625.nc`, has thickness identical to
      `ais_4to20km.20241224.nc`. Its history says it came from
      `relaxed_10yrs_4km.nc`, the 10-year relaxation. That is presumably the same
      state as `AIS_4to20km_20241118_relaxation_0TGmelt_10yr.20241224.nc`; not
      confirmed by name.
    - STATIC holds the IC geometry, and DATA with a constant-thickness file would
      be identical: both turn off the velocity solver, advection, thermal solver,
      calving and floating basal melt. So STATIC needs nothing new.
  - `NTASKS_GLC` must be 128, 256 or 640: those are the only partition files in
    `data/inputdata` (the public mirror has more).
  - Aside: the existing ais8to30 datamode file has only `xtime`, so today's DIS
    test behaves like STATIC. Worth telling Matt Hoffman.
  - **Sanity run first:** the coupled run with the ocean ignoring MALI's
    pressure, fraction and conductive flux should be BFB with the control, given
    the same ocn/ice/atm PE layout. This isolates the plumbing.
  - **Then the full coupled run**, compared with the control. Look at:
    - melt-rate patterns and `landIcePressure`
    - `landIceFloatingFraction` against the IC
    - interface T and S, and SSH under the shelves
    - integrated melt on the ocean grid against the x2g net mass flux times MALI's
      floating mask, integrated on the MALI grid (from coupler history)
- **Optional:** a short run with prognostic MALI (SIA) to check the MALI-side sign,
  magnitude and thermal boundary condition.

### 6. MOAB

- Implement the coupler routing in MCT only (decided), keeping the design
  MOAB-friendly:
  - component-side arithmetic
  - explicit state and flux lists
  - same-named fields
  - no coupler-side combinations
  - field definitions in both drivers
- Tell Rob Jacob early that the coupler-side melt solve is going away, so the
  ocn<->glc MOAB follow-up to #8661 does not port `compute_melt_fluxes`.
- E3SM#8775 (draft) would require `share/field_limits.yaml` entries for new fields.
- When MOAB gets ice-shelf coupling, carry over the MCT choices: conservative,
  unnormalized shelf mapping both ways, and glc solid runoff mapped with the
  liquid-runoff map (no cavity cells) when ice-shelf coupling is on.

## Open design items

None of these blocks sections 1 and 2. Xylar accepted all four defaults on
2026-10-13; section 3 implements them.

The next project after this one is wetting-and-drying simulations. That is where
`landIceFraction` and `landIceMask` get revisited.

- **Normalization of `p_i` in partially covered ocean cells.** Proposed: adopt
  #128's approach. MALI masks `Sg_lithop` and its ice masks by
  `Sg_below_sea_level_mask`, and the ocean divides by the remapped mask. This is
  what the 2025 STATIC runs used. Check it against the IC fields at the start of
  the STATIC test.
- **Frazil in cells where `phi_li = 1` but `phi_i` is below the threshold.** That
  mass has no MALI destination. Proposed: do not send it to MALI, but save the
  discarded mass as an ocean diagnostic so the STATIC test can quantify it.
  Revisit only if it is significant.
- **`landIceFraction` and `landIceMask` following MALI.** Proposed: import
  `landIceFraction` along with the pressure and floating fraction, as #128 did.
  Keep `landIceMask` and `landIceFloatingMask` from the IC, because the cavity
  geometry is static.
- **`theta_b` is a potential temperature; MALI expects in-situ.** The error is about
  0.1 C per km of depth. Proposed: accept it and say so in the design doc.
