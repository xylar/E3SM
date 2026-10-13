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

### 4. MALI side

Build this on #8661 (array import/export) once it merges, or merge its branch
temporarily.

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

### 5. Tests and validation

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
