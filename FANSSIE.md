# FAnSSIE ocean–ice-sheet coupling branch

`fanssie-ocn-glc-coupling` on E3SM-Ocean-Discussion/E3SM is a long-lived, shared development branch for coupling MPAS-Ocean and MALI in E3SM, as part of FAnSSIE, the Framework for Antarctic System Science in E3SM. Polaris pins it as the submodule `e3sm_submodules/FAnSSIE` for its ocean–ice-sheet tasks, which are regression-tested with Polaris's `fanssie_pr` suite. Everything else in Polaris builds from E3SM-Project.

## Rules

- The branch only gains commits. Never force-push or rebase it.
- Work starts from this branch and merges back in through pull requests to it, so that each pull request shows only its own changes. Pull requests are merged with a merge commit. Upstream pull requests the work depends on are merged in as they are. Small fixes, and changes to this file, may be committed directly.
- E3SM master is merged in from time to time.
- Every change except to this file gets a row in the ledger below, so that nothing is lost. The row is added by a direct commit right after the pull request merges, not in the pull request, so that a pull request holds only code. The row gives the merge commit and names any other change on this branch that the change needs.
- Moving changes into E3SM is encouraged but not required. To move one, cherry-pick its pull request's commits (`git log <merge>^1..<merge>^2`) onto E3SM master. Bring any changes made in upstream review back to this branch before master is next merged in.
- Test each change to the code with Polaris's `fanssie_pr` suite against a baseline from the previous commit of this branch.

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
