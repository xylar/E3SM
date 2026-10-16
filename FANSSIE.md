# FAnSSIE ocean–ice-sheet coupling branch

`fanssie-ocn-glc-coupling` on E3SM-Ocean-Discussion/E3SM is a long-lived, shared development branch for coupling MPAS-Ocean and MALI in E3SM, as part of FAnSSIE. Polaris pins it as the submodule `e3sm_submodules/FAnSSIE` for its ocean–ice-sheet tasks, which are regression-tested with Polaris's `fanssie_pr` suite. Everything else in Polaris builds from E3SM-Project.

## Rules

- The branch only gains commits. Never force-push or rebase it.
- Changes arrive as merges of feature branches based on E3SM master, merged as they are, through pull requests to this branch. A small fix to a feature that is already merged may be committed directly.
- E3SM master is merged in from time to time.
- Every change gets a row in the ledger below, so that nothing is lost. Moving changes into E3SM is encouraged but not required.
- Test each change with Polaris's `fanssie_pr` suite against a baseline from the previous commit of this branch.

## Ledger

Base: E3SM master `6816738295` (2026-10-15).

| Change | Source | Upstream | Status (2026-10-16) |
|---|---|---|---|
| New land-ice melt equations and options | cbegeman:ocn/modify-land-ice-melt-equations | E3SM#8047 | draft |
| Fixes for E3SM#8047 | xylar:ocn/modify-land-ice-melt-equations-fixes | suggested for cherry-picking on E3SM#8047 | not yet in E3SM#8047 |
| Two-way land-ice coupling in driver-moab | E3SM-Project:rljacob/mbcpl/port-mali | E3SM#8661 | approved, open |
| Ice-shelf melt coupling between MPAS-Ocean and MALI, including the three rows above | xylar:ocn-glc/ice-shelf-melt-coupling | E3SM#8887 | draft |
| Wetting and drying with RK4 and a subglacial thin film | cbegeman:ocn/limit-grounded-land-ice-pressure | E3SM-Ocean-Discussion#119 | draft |
| Copy land-ice pressure only over nCellsAll, a fix for E3SM-Ocean-Discussion#119 | `26fb0b6bed` from xylar:ocn/thin-film-wetting-drying, cherry-picked | suggested on E3SM-Ocean-Discussion#119 | not yet in E3SM-Ocean-Discussion#119 |
| Zero land-ice fluxes where the ice is grounded | xylar:ocn/fix-land-ice-fluxes-in-grounded-cells | E3SM-Ocean-Discussion#141 | open |
| Keep the floating mask zero where there is no land ice | xylar:ocn/fix-floating-mask-in-open-ocean | E3SM-Ocean-Discussion#142 | open |
