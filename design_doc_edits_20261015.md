# Design-doc edits, 2026-10-15

Each item: the Overleaf text to find (as in `main.tex` or the earlier suggestions), then its replacement. All are in `main_suggested.tex` and `main_suggested.diff`.

## 1. Masking of the mass fluxes (end of subsection, before the $m_i^{t_{cpl}}$ equation)

Find:

```latex
These normalized fluxes are combined in the coupler, remapped to the MALI mesh, and masked by $\phi_i$.  Since $Q_{fr}^m > 0$ when frazil forms, which adds mass to the ice shelf, the melt rate of ice seen by MALI is:
```

Replace with:

```latex
MPAS-Ocean combines these normalized fluxes into a single net flux, which the coupler remaps to the MALI mesh and MALI masks by $\phi_i$.  The normalization always uses the remapped MALI floating fraction $\phi_i^{t_{cpl}}$, even when MPAS-Ocean's own floating fraction comes from its initial condition, so that MALI receives the mass MPAS-Ocean sends.  Since $Q_{fr}^m > 0$ when frazil forms, which adds mass to the ice shelf, the melt rate of ice seen by MALI is:
```

## 2. Conductive heat flux (append two paragraphs after the Holland and Jenkins paragraph)

Find:

```latex
Holland and Jenkins (1999) assume that the ice is a perfect insulator ($\partial_z T_i = 0$) when $\theta_{kmax} - \theta_b < 0$. However, there appears to be little downside to continuing to allow a conductive heat flux in this case.
```

Replace with:

```latex
Holland and Jenkins (1999) assume that the ice is a perfect insulator ($\partial_z T_i = 0$) when $\theta_{kmax} - \theta_b < 0$. However, there appears to be little downside to continuing to allow a conductive heat flux in this case.

MALI sends $Q_i$ only from dynamically active floating ice (thicker than \texttt{config\_dynamic\_thickness}, 10~m by default).  In thinner ice, $h_{i,zbot}$ is so small that $Q_i$ reached $\sim$1000~W~m$^{-2}$ in magnitude in tests.

MALI's $Q_i$ is an average over each MALI cell.  After conservative remapping, MPAS-Ocean divides it by the larger of its own floating fraction and the remapped MALI floating fraction $\phi_i^{t_{cpl}}$, and applies it over its own floating area.  This conserves heat whether or not MPAS-Ocean's geometry comes from MALI, because the ocean's floating fraction is never smaller than $\phi_i^{t_{cpl}}$ when it does.
```

## 3. Masking of enthalpy flux (second paragraph)

Find:

```latex
For coupling to MALI, the glacier-facing interfacial heat export is passed through a
dedicated MPAS-Ocean coupling field that contains only the advective part of the
interfacial shelf heat.  The separate frazil enthalpy export is also passed to the
coupler.  These two remapped heat terms are then combined into the single
glacier-facing heat field seen by MALI.
```

Replace with:

```latex
No heat flux is passed from MPAS-Ocean to MALI.  MALI uses $\theta_b^{t_{cpl}}$ as the
basal temperature of floating ice and computes $Q_i$ itself, and the advective enthalpy
flux does not change MALI's basal ice temperature, so MALI never needs $Q_b^T$.
```

## 4. Implementation, introduction

Find:

```latex
Note that in this implementation, mass and heat fluxes are passed from MPAS-O to MALI. Thus, only masking and no computations related to these fluxes occurs on the MALI mesh.
```

Replace with:

```latex
In this implementation, only the net mass flux and the interface temperature are passed from MPAS-Ocean to MALI; no heat flux is.  Normalization happens in the components, so the coupler only remaps, merges and time-averages these fields, and MALI only masks them.
```

## 5a. Implementation table: mass and heat rows

Find:

```latex
    $Q_{fr}^{m,n}=\rho_i m_{fr}^n$ & $\rightarrow\; Q_{fr}^{m}$ &\\
    $Q_b^{m,n}=\rho_i m_i^n$ & $\rightarrow\; Q_b^{m}$ &\\
    & $\frac{Q_b^{m} - Q_{fr}^{m}}{\rho_i} \rightarrow$ & $m_i^{t_{cpl}}$\\
    $Q^{T,n}_{b,adv} = f_2(m_i^n)$ & $\rightarrow\; Q^{T,t_{cpl}}_{b,adv}$ &\\
    $Q_{fr}^{T,n}$ & $\rightarrow\; Q_{fr}^{T}$ &\\
    & $Q_{b,adv}^{T}+Q_{fr}^T \rightarrow$ & $Q_b^{T,t_{cpl}}$\\
```

Replace with:

```latex
    $\frac{Q_b^{m,n} - Q_{fr}^{m,n}}{\phi_i^{t_{cpl}}}$ & $\rightarrow$ time average $\rightarrow$ & $m_i^{t_{cpl}} = \frac{\phi_i}{\rho_i}\left(\frac{Q_b^{m} - Q_{fr}^{m}}{\phi_i^{t_{cpl}}}\right)^{t_{cpl}}$\\
```

## 5b. Implementation table: MALI thermal row

Find:

```latex
    && $d_t T_{i,zbot}^{n-1} = f_1(Q_{b,cond}^{T,t_{cpl}})$ (not $f(Q_b^{T,t_{cpl}})$)\\
```

Replace with:

```latex
    && $d_t T_i^{n-1}$ with $T_i(z_b) = \theta_b^{t_{cpl}}$ (not $f(Q_b^{T,t_{cpl}})$)\\
```

## 6. Implementation steps: replace the whole todolist and the status paragraph after it

Find:

```latex
\begin{todolist}
    ...  (the whole list)
\end{todolist}

This status reflects the current MCT implementation path only.  ...
...
used in MPAS-Ocean internal energy accounting.
```

Replace with:

```latex
\begin{todolist}
    \item[\done] Remove the coupler-side melt calculation (\texttt{compute\_melt\_fluxes})
    and its coupler fields.
    \item[\done] MALI sends \texttt{Sg\_lithop}, \texttt{Sg\_icemask\_floating} and
    \texttt{Sg\_icemask\_grounded}, each masked by the new \texttt{Sg\_below\_sea\_level\_mask},
    and MPAS-Ocean can import them as \texttt{landIcePressure},
    \texttt{landIceFloatingFraction} and \texttt{landIceFraction}.  The import is off by
    default (\texttt{config\_\allowbreak glc\_\allowbreak ice\_\allowbreak shelf\_\allowbreak coupling\_\allowbreak import\_\allowbreak geometry = .false.}): no
    existing ocean initial condition matches MALI's geometry closely enough, and the
    abrupt changes in pressure and draft at grounding lines make the ocean unstable.
    Turn it on only for ocean initial conditions built from MALI's geometry.
    \item[\done] MPAS-Ocean exports the net mass flux per unit floating area,
    \texttt{Foxo\_ismf} $= (Q_b^m - Q_{fr}^m)/\phi_i^{t_{cpl}}$, with $\phi_i^{t_{cpl}}$
    the remapped MALI floating fraction.  It is zero where $\phi_i^{t_{cpl}}$ is at or
    below \texttt{config\_\allowbreak glc\_\allowbreak ice\_\allowbreak shelf\_\allowbreak coupling\_\allowbreak min\_\allowbreak floating\_\allowbreak fraction} ($10^{-3}$).
    The shelf fields are remapped conservatively and without normalization in both
    directions, and MALI sets \texttt{floatingBasalMassBal} $= -$\texttt{Foxo\_ismf}.
    \item[\done] Pass the time-averaged interface temperature from MPAS-Ocean to MALI as
    \texttt{So\_tfrz\_isf} (in Kelvin, weighted by the floating fraction) with the weight
    \texttt{So\_liflfrac}.  MALI renormalizes by the remapped weight into
    \texttt{iceOceanInterfaceTemperature}.  Where the remapped weight is at or below the
    MALI option \texttt{config\_min\_floating\_frac} (0.01), the value is set to
    \texttt{config\_invalid\_value\_ocn\_intr\_temp}.
    \item[\done] MALI uses \texttt{iceOceanInterfaceTemperature} as the
    \texttt{basalTemperature} of floating ice, falling back to its depth-based freezing
    temperature where the imported value is invalid.
    \item[\done] MALI sends its basal conductive flux through a dedicated glc-to-ocean
    field, \texttt{Fogg\_hcond} (\texttt{Flgg\_hflx} is the glacier heat flux sent to the
    land model and is not reused), only from dynamically active floating ice and only
    when its thermal solver is on.  MPAS-Ocean imports it as
    \texttt{landIceConductiveHeatFlux}, divided by the larger of its own and MALI's
    floating fractions.
    \item[\done] Remove the sensible heat flux term from the 3-equation parameterization in MPAS-Ocean (i.e., meltwater enters at $\theta_b$).
    \item[\done] Add the conductive heat flux term to the 3-equation parameterization in MPAS-Ocean.
    \item[\done] With ice-shelf coupling, MALI's solid runoff (calving and face melt) is
    remapped with the liquid-runoff map, which excludes ice-shelf cavities.
    \item[\done] In G-cases with MALI, runoff is coupled as often as the atmosphere
    (every 30 minutes with JRA forcing), as it is without MALI.
    \item[\done] Without a land component, the coupler reads glc's \texttt{aream} from the
    glc-to-ocean ESMF map, so its area corrections make glc fluxes conservative even
    though MALI's cell areas are planar.  The runoff maps from MALI must then use
    spherical source areas too (MALI-Dev/E3SM\#204).
    \item[\done] The coupler budgets include MALI's side of the ice-shelf mass and heat
    exchange.
    \item MOAB: the MOAB driver aborts if ice-shelf coupling is requested.  A MOAB port
    should keep the MCT choices: unnormalized conservative shelf maps, solid runoff on
    the liquid-runoff map, and glc areas from the mapping files.
    \item Ocean initial conditions built from MALI's geometry, so the geometry import can
    be turned on.
\end{todolist}

Passing heat from MPAS-Ocean to MALI (\texttt{landIceCouplingHeatFlux},
\texttt{Fogx\_qicehi}, \texttt{basalOceanHeatFlux}) was dropped, since MALI never needs
$Q_b^T$.  \texttt{Foxo\_q\_li} remains the frazil latent heat, used only in the coupler
budgets.
```
