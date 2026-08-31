(omega-design-teos10-fits)=

# TEOS-10 Fitted Function Library

**Table of Contents**
1. [Overview](#1-overview)
2. [Requirements](#2-requirements)
3. [Algorithmic Formulation](#3-algorithmic-formulation)
4. [Design](#4-design)
5. [Verification and Testing](#5-verification-and-testing)

## 1 Overview

Omega needs a small number of TEOS-10 thermodynamic quantities that it does not
yet have, principally to support frazil ice formation and ice/seawater melting.
The reference implementation of these quantities is the Gibbs SeaWater (GSW)
Oceanographic Toolbox, which Omega already carries as an unmodified submodule
in `components/omega/external/GSW-C`.

GSW-C cannot be used directly for these quantities, for two reasons:

1. **Licence.** The GSW licence permits "redistribution and use, in source and
   binary forms, *without modification*". A Kokkos port of GSW-C source would be
   a modified redistribution and is therefore not permitted. Using the
   unmodified toolbox on the host — for verification, or as a source of
   reference values — is permitted and is what Omega already does.
2. **Portability.** GSW-C is plain host C. It cannot be called from device code.
   The frazil implementation currently calls `gsw_frazil_properties_potential_poly`,
   `gsw_melting_ice_into_seawater`, `gsw_pt_from_pot_enthalpy_ice_poly`,
   `gsw_pot_enthalpy_from_pt_ice_poly` and `gsw_t_from_pt0_ice` from inside a
   `KOKKOS_FUNCTION`, which will not compile for CUDA or HIP.

This document describes a library of Kokkos-callable approximations to the
required quantities, together with the offline tooling that produces them. The
approximations are **fit by Omega, over Omega's own valid range, to Omega's own
declared tolerances, against reference values produced by running the unmodified
GSW-C toolbox**. No GSW-C source is transcribed, adapted or redistributed in
modified form.

Fitting is not, however, treated as the default. A fit costs developer time and
carries an explanation burden of its own — in review, and in the model
description Omega must eventually publish — so this design admits a fit only
where there is no other licence-compatible route to the algorithm, or where a
fit is *measurably* cheaper than implementing the published algorithm directly.
Section 3.1 reduces the problem to the smallest set of quantities to which that
test has to be applied, and section 3.1.1 applies it. The outcome is that most
of the benefit here comes from removing Newton iterations whose trip count
varies per thread, which is worth a great deal on GPU, and not from saving
arithmetic.

Two things this document deliberately does **not** propose:

- It does not replace the Roquet et al. (2015) 75-term specific volume
  polynomial already implemented in `Eos.h`. That polynomial is itself a
  published least-squares fit to the full TEOS-10 Gibbs function, it is
  implemented from the published literature rather than from GSW-C source, and
  it is very unlikely that refitting it would improve either its accuracy or its
  cost. The same applies to `calcCtFromPt`, `calcPtFromCt` and `calcCtFreezing`.
- It does not use machine learning. A neural network was considered and
  rejected: the functions of interest are smooth functions of one to three
  variables, a regime where tensor-product polynomial approximation is close to
  optimal; even a single small hidden layer costs more arithmetic than the
  polynomials it would replace, and its transcendental activations are both
  expensive on GPU and not bit-reproducible across vendor math libraries.

## 2 Requirements

### 2.1 Requirement: device-callable evaluation

All fitted functions must be callable from within a `KOKKOS_FUNCTION` and must
compile and run correctly on CPU, CUDA and HIP backends. This is the immediate
blocker for the frazil implementation.

### 2.2 Requirement: abide by the GSW Oceanographic Toolbox licence

No GSW-C source may be modified or redistributed in modified form. The
unmodified toolbox may be used on the host to generate reference values and to
verify the fits. Fitted coefficients are derived from numerical output of the
unmodified toolbox, not from its source.

### 2.3 Requirement: fit only where necessary or demonstrably faster

A fit is admitted for one of exactly two reasons, and the reason must be
recorded for each fitted function:

1. **Necessity.** There is no way to implement the quantity other than by
   reading GSW-C source. Fitting is then the only licence-compatible route and
   needs no further justification.
2. **Efficiency.** An independently-implementable algorithm exists, but a fit is
   measurably cheaper by enough to be worth the developer time and the extra
   explanation a fit requires in review and in the model description.

Anything that meets neither test is hand-implemented from its published source,
or expressed as exact algebra, and is not fitted. In particular the design must
first reduce the problem to the smallest irreducible set of quantities, so that
this test is applied to as few functions as possible.

### 2.4 Requirement: declared, per-function accuracy targets

Each fitted function declares its own absolute and relative error target, chosen
against what the science requires and against the accuracy of the GSW reference
itself. A uniform "machine precision" target is neither achievable nor useful:
`gsw_ct_freezing_poly` already carries an error of order `6e-4` K by
construction, and TEOS-10's own uncertainty relative to real seawater is of
order `1e-6` in relative density.

### 2.5 Requirement: bounded, branch-free evaluation

Evaluation must have a fixed instruction count with no data-dependent iteration.
The GSW functions being replaced contain Newton–Raphson loops nested up to three
deep, which cause warp divergence and unbounded per-thread work on GPU. Removing
the iteration is a primary benefit of the approach, not merely a side effect.

### 2.6 Requirement: fits valid over Omega's configured range

Fits are constructed over the range of absolute salinity, conservative
temperature and pressure that Omega already declares valid, not over the full
oceanographic funnel that GSW must support. Restricting the domain is what makes
a low-order fit possible. Inputs outside the fitted domain are clamped to the
domain boundary so that evaluation remains bounded and branch-free, and the
excursion is reported through Omega's existing state validation machinery
rather than by returning an invalid value from the fit itself.

### 2.7 Requirement: reproducibility

Coefficients are generated offline and checked into the repository as
`constexpr` values in generated headers. No fitting is performed at build time
or at run time, and no coefficient file is read at initialization. Evaluation
uses only `+`, `-`, `*`, `/` and, where unavoidable, `sqrt`, so that results are
reproducible across compilers and backends to the extent that the underlying
floating-point arithmetic is.

### 2.8 Requirement: consistent derivatives

Where a derivative of a fitted function is needed — for example in a Newton
solve or in a tendency linearization — it must be obtained by analytically
differentiating the fit itself, not by fitting the derivative separately. This
guarantees that the value and its derivative are mutually consistent.

### 2.9 Requirement: verification against unmodified GSW-C

Every fitted function must be verified against the unmodified GSW-C toolbox over
a dense sample of its declared domain, and the test must fail if the declared
error target is exceeded.

### 2.10 Requirement: extensibility

Adding a further TEOS-10 quantity must be a matter of adding an entry to the
fitting configuration and regenerating, not of writing new infrastructure. The
set of quantities Omega needs will grow — sea ice coupling, geothermal fluxes
and ice-shelf thermodynamics are all likely to need more.

### 2.11 Desired: single implementation path

Once a fitted function is verified, it should be the only implementation of that
quantity in Omega. Carrying a fitted version and a hand-ported version of the
same quantity doubles the maintenance and verification burden for no benefit.

## 3 Algorithmic Formulation

### 3.1 Reducing the problem to an irreducible set

The single most important step is to recognize how little actually has to be
fitted. The functions Omega calls are mostly thin wrappers of exact algebra
around a small number of genuinely irreducible thermodynamic relations.

Consider `gsw_frazil_properties_potential_poly`, the largest and most expensive
of them. It maps bulk absolute salinity, bulk potential enthalpy and pressure,
$(S_A^{bulk}, h^{bulk}, p)$, to the equilibrium state
$(S_A, \Theta, w_{Ih})$ of a seawater/frazil mixture. Its structure is:

Define the regime indicator

$$
f_0 = h^{bulk} - c_p^0\,\Theta_f(S_A^{bulk}, p)
$$

where $\Theta_f$ is the freezing conservative temperature, which Omega already
implements as `Teos10Eos::calcCtFreezing`. Then:

- If $f_0 \ge 0$ the mixture is warmer than freezing and contains no ice. The
  answer is written down exactly, with no approximation of any kind:

$$
S_A = S_A^{bulk}, \qquad \Theta = h^{bulk}/c_p^0, \qquad w_{Ih} = 0
$$

- If $f_0 < 0$ ice is present, and $w_{Ih}$ is the root of

$$
F(w) = h^{bulk} - (1-w)\,c_p^0\,\Theta_f\!\left(\tfrac{S_A^{bulk}}{1-w}, p\right)
       - w\,h_{Ih}^{f}\!\left(\tfrac{S_A^{bulk}}{1-w}, p\right) = 0
$$

  where $h_{Ih}^{f}(S_A,p)$ is the potential enthalpy of ice at the freezing
  point. Once $w_{Ih}$ is known, the remaining two outputs follow by exact
  algebra:

$$
S_A = \frac{S_A^{bulk}}{1 - w_{Ih}}, \qquad \Theta = \Theta_f(S_A, p)
$$

So a function that appears to be a three-input, three-output map with an
iterative solver reduces to **one scalar unknown, $w_{Ih}$, on one branch**,
plus a regime test and three lines of algebra that Omega writes and documents
itself. GSW's own implementation parameterizes its initial guess for $w_{Ih}$ in
terms of $(S_A^{bulk}, f_0, p)$, which is a useful hint that these are
well-conditioned fitting coordinates.

`gsw_melting_ice_into_seawater` decomposes the same way. It is a validity guard,
two mixing rules

$$
S_A^{bulk} = (1-w_{Ih})S_A, \qquad
h^{bulk} = (1-w_{Ih})\,h(S_A,\Theta,p) + w_{Ih}\,h_{Ih}(t_{Ih},p)
$$

and then a call to the frazil equilibrium solve above. Only the enthalpies are
irreducible.

Applying this reduction across everything the frazil implementation needs leaves
the following set of quantities to approximate:

| Quantity | Inputs | Dim | Cost of the reference evaluation |
|---|---|---|---|
| $w_{Ih}(S_A^{bulk}, f_0, p)$ | bulk salinity, regime indicator, pressure | 3 | root of $F(w)$; GSW uses a ~50-term polynomial guess plus up to 3 Newton steps |
| $h_{Ih}^{f}(S_A, p)$ | absolute salinity, pressure | 2 | potential enthalpy of ice at the freezing point |
| $h_{Ih}(t, p)$ | in-situ temperature, pressure | 2 | one evaluation of the ice Gibbs function |
| $\theta_{Ih}(h_{Ih})$ | potential enthalpy of ice | 1 | inverse of the ice potential enthalpy relation |
| $h_{Ih}(\theta_{Ih})$ | potential temperature of ice | 1 | ice potential enthalpy at reference pressure |
| $t(\theta_{Ih}^0, p)$ | ice potential temperature, pressure | 2 | GSW uses up to 6 Newton iterations on the ice Gibbs function |
| $\Theta_f(S_A, p)$ | absolute salinity, pressure | 2 | already implemented in `Eos.h`; not revisited here |
| $h(S_A, \Theta, p)$ | seawater enthalpy | 3 | pressure integral of specific volume; see below |

Everything else — the mixing rules, the regime test, the recovery of $S_A$ and
$\Theta$ from $w_{Ih}$, the invalid-state guards — is exact algebra with no
approximation and no licence exposure. It is written and documented by Omega.

### 3.1.1 Which of these actually have to be fitted

Requirement 2.3 admits a fit only for necessity or for measured efficiency.
Applying that test to the table above produces a result worth stating plainly:

**Almost nothing in this table is fitted out of necessity.** TEOS-10 is built on
publicly released standards — IAPWS R13-08 for the seawater Gibbs function and
IAPWS R10-06 (Feistel & Wagner, 2006) for the Gibbs function of ice Ih — whose
full coefficient tables are published and freely available. Every quantity in
the table can in principle be computed from those two potentials by
differentiation and root-finding, with no reference to GSW-C source at all. GSW's
`*_poly` functions are simply GSW's own cheap approximations to the same
standards; they are not the standards, and Omega is free to construct its own.

So the honest position is that for this function set the *necessity* test is
mostly not met, and **the efficiency test is the one that has to be passed.**
That test is nonetheless easy to pass here, and for a specific reason: the
expensive part of these quantities is not the Gibbs function evaluation, it is
the Newton iteration wrapped around it. $t(\theta_{Ih}^0, p)$ costs up to six
Newton steps, each evaluating derivatives of the ice Gibbs function;
$w_{Ih}$ costs up to three more, each of which itself needs the freezing
temperature and the ice freezing enthalpy. Replacing an iteration whose trip
count varies per thread with a fixed-length polynomial is a large and
straightforward win on GPU (requirement 2.5), quite apart from the arithmetic
saved. It also avoids writing and maintaining an implementation of the ice
Gibbs function, which nothing else in Omega needs.

Where a quantity is a *single* evaluation of a published potential with no
iteration — $h_{Ih}(t,p)$ is the clearest case — the efficiency argument is much
weaker, and a direct implementation from IAPWS R10-06 may well be the better
choice. That decision is deferred to implementation and must be settled by
measurement, not assumed. The same applies to $h_{Ih}(\theta_{Ih})$ and
$\theta_{Ih}(h_{Ih})$, which are low-order one-dimensional relations where the
gap between a fit and a direct implementation is small either way.

Each fitted function therefore records its justification alongside its
coefficients, in the form required by 2.3, and any function admitted on
efficiency grounds records the measurement that justified it (section 3.6).

Two entries need further comment. Seawater enthalpy $h(S_A,\Theta,p)$ should not
be fitted at all: it is the pressure integral of specific volume, so it follows
analytically from the 75-term polynomial Omega already has. Deriving it that way
costs little, needs no new approximation, and guarantees that Omega's enthalpy
and its specific volume are thermodynamically consistent with each other — a
property an independent fit would not have. And $\theta_{Ih}(h_{Ih})$ and
$h_{Ih}(\theta_{Ih})$ are mutual inverses; fitting them independently will not
make them exact inverses, so the design must decide which is authoritative and
either fit the other to the inverse of the first or accept and document the
closure error.

### 3.2 Form of the approximations

All fits are tensor-product polynomials in normalized coordinates. For a
function of variables $x_1 \ldots x_d$ with declared domain
$[a_i, b_i]$, define

$$
\hat{x}_i = \frac{2 x_i - (a_i + b_i)}{b_i - a_i} \in [-1, 1]
$$

and approximate

$$
\tilde{f}(\mathbf{x}) = \sum_{|\boldsymbol{\alpha}| \le N} c_{\boldsymbol{\alpha}}
\prod_{i=1}^{d} T_{\alpha_i}(\hat{x}_i)
$$

Fits are constructed in the Chebyshev basis, where the coefficients decay
predictably and the truncation order can be chosen by inspecting the coefficient
spectrum rather than by trial and error. They are then converted to a monomial
basis in $\hat{x}$ for evaluation, so that the generated device code is a Horner
or Estrin evaluation with no basis recurrence at run time.

Where a variable is naturally better represented in a transformed coordinate —
GSW's own polynomials use $\sqrt{(S_A + \Delta S)/S_A^{norm}}$ for salinity, for
instance — the transform is applied before normalization and is recorded as part
of the function's declaration.

### 3.3 The regime boundary

The frazil equilibrium map is piecewise: it is exactly linear on the $f_0 \ge 0$
branch and smooth but distinct on the $f_0 < 0$ branch, with a kink at
$w_{Ih} = 0$. Fitting across that kink with a single global polynomial would be
poor everywhere. The design therefore fits only the ice-present branch, and
selects between branches using the exact analytic test $f_0 \ge 0$. Because the
test is exact and the ice-free branch is exact, the fit only has to be good
where ice is actually present, and continuity at $w_{Ih} = 0$ can be imposed as
a constraint on the fit.

This is the general pattern: **regime boundaries are located analytically and
handled by exact branch selection, never absorbed into a fit.**

### 3.4 Derivatives

Derivatives are obtained by differentiating the monomial form analytically, and
are generated into the same header as the value. Where Omega needs
$\partial \Theta_f/\partial S_A$ or
$\partial h_{Ih}^{f}/\partial S_A$ — GSW supplies these as separate
`*_first_derivatives_poly` functions — Omega instead differentiates its own fit,
so the derivative is exactly the derivative of the value it will be used with.

### 3.5 Fitting procedure and error measure

For each declared function:

1. Sample the declared domain. Reference values are produced by calling the
   unmodified GSW-C toolbox on the host, on a tensor grid of Chebyshev–Gauss
   points for fitting and on an independent, much denser quasi-random sample for
   verification. Sample points that GSW reports as invalid are excluded from the
   fit and recorded.
2. Fit in the Chebyshev basis by least squares, optionally weighted to convert
   an absolute target into a relative one, and optionally constrained to enforce
   continuity or an exact value at a boundary.
3. Choose the truncation order as the lowest order meeting the declared target
   on the independent verification sample. Report the achieved maximum absolute
   and relative error, the operation count, and the coefficient spectrum.
4. Emit a generated C++ header containing the coefficients as `constexpr`
   values, the domain, the achieved error, and the provenance of the fit.

The error measure is the maximum over the independent verification sample, not
the RMS. An RMS target can hide a large localized error near a regime boundary,
which is precisely where frazil is active.

### 3.6 Justifying a fit on efficiency grounds

A function admitted under the efficiency half of requirement 2.3 must be
justified by measurement before its fit is committed. The comparison is between
the fit and a direct implementation from the published standard, on the target
architecture, for the same declared accuracy:

1. Implement the quantity directly from its published source (IAPWS R10-06 for
   ice, IAPWS R13-08 for seawater), including whatever iteration it requires.
   This is throwaway reference code; it does not have to be efficient, but it
   does have to be correct, and it is verified against GSW-C like everything
   else.
2. Measure both, on CPU and on GPU, over a realistic distribution of inputs —
   in particular one that mixes ice-present and ice-free states, so that
   iteration divergence is represented rather than averaged away.
3. Record the measured speed-up, the accuracy of both, and the line count of
   both, in the fit declaration.

If the fit is not meaningfully faster, the direct implementation is kept and the
fit is discarded. Recording the measurement, rather than the intent, is what
makes the choice defensible in review and describable in a model description
paper. The expected outcome, for the reasons given in section 3.1.1, is that the
iterative quantities pass comfortably and the single-evaluation ones are
marginal.

## 4 Design

### 4.1 Data types and parameters

#### 4.1.1 Parameters

The fitted functions have no run-time configuration: their coefficients and
domains are compile-time constants. The fitting tool is driven by a checked-in
declaration file, one entry per function, which is the single place a new
quantity is added:

```yaml
Teos10Fits:
  - Name: PotEnthalpyIceFreezing
    GswReference: gsw_pot_enthalpy_ice_freezing_poly
    Inputs:
      - {Name: AbsSalinity, Min: 0.0,     Max: 42.0,  Transform: sqrt}
      - {Name: Pressure,    Min: 0.0,     Max: 1.1e8, Transform: none}
    Output: {Name: PotEnthalpyIceFreezing, Units: "J kg-1"}
    Tolerance: {Absolute: 1.0e-2, Relative: 1.0e-8}
    Derivatives: [AbsSalinity]
    # Why this function is fitted at all -- see requirement 2.3.
    # "Necessity" needs no further fields; "Efficiency" requires the
    # measurement recorded by section 3.6.
    Justification:
      Reason: Efficiency
      Alternative: "Direct evaluation of ice Gibbs function, IAPWS R10-06,
                    at the freezing temperature (itself a Newton solve)"
      MeasuredSpeedup: {Cpu: 4.1, Gpu: 11.6}
```

The domain bounds are required to be consistent with, and are checked against,
the valid ranges Omega already declares for its state variables.

#### 4.1.2 Class/structs/data types

Each fitted function is a small functor in a generated header, following the
pattern already established by the EOS functors in `Eos.h`:

```c++
namespace OMEGA::Teos10 {

/// Potential enthalpy of ice at the freezing point.
/// Generated by tools/teos10_fit from Teos10Fits.yml -- do not edit.
/// Fit: tensor-product polynomial, order (6,4) in (sqrt(SA), P)
/// Domain: SA in [0, 42] g/kg, P in [0, 1.1e8] Pa
/// Verified max abs error 3.1e-3 J/kg, max rel error 7.4e-9 vs GSW-C 3.06.
/// Fitted for: efficiency. 11.6x faster on GPU than direct evaluation of the
/// IAPWS R10-06 ice Gibbs function at the freezing point. See Teos10Fits.yml.
class PotEnthalpyIceFreezing {
 public:
   /// Value of the fitted function.
   KOKKOS_FUNCTION Real operator()(Real AbsSalinity, Real Pressure) const;

   /// Analytic derivative of the fit with respect to absolute salinity.
   KOKKOS_FUNCTION Real derivAbsSalinity(Real AbsSalinity,
                                         Real Pressure) const;

   /// Declared domain of validity, in the units of the inputs.
   static constexpr Real MinAbsSalinity = 0.0;
   static constexpr Real MaxAbsSalinity = 42.0;
   static constexpr Real MinPressure    = 0.0;
   static constexpr Real MaxPressure    = 1.1e8;

   /// Verified error bounds over the declared domain.
   static constexpr Real MaxAbsError = 3.1e-3;
   static constexpr Real MaxRelError = 7.4e-9;
};

} // namespace OMEGA::Teos10
```

The composite quantities the frazil code actually calls are hand-written
functors that combine the fits with exact algebra. These are short enough to
read and to document:

```c++
namespace OMEGA::Teos10 {

/// Equilibrium state of a seawater/frazil mixture, from bulk absolute
/// salinity, bulk potential enthalpy and pressure.
class FrazilEquilibrium {
 public:
   KOKKOS_FUNCTION void operator()(Real SaBulk, Real PotEnthalpyBulk, Real P,
                                   Real &SaFinal, Real &CtFinal,
                                   Real &IceFrac) const;

 private:
   Teos10Eos::CtFreezing CtFreez;   ///< already implemented in Eos.h
   IceFraction           IceFrac;   ///< fitted, ice-present branch only
};

} // namespace OMEGA::Teos10
```

### 4.2 Methods

The public interface is one functor per quantity. Value and derivative calls are
`KOKKOS_FUNCTION` and take and return `Real` scalars, so they compose freely
inside existing Omega kernels; no array-level or team-level interface is
provided, because these quantities are consumed pointwise inside kernels that
already own their loop structure.

The typical use is inside an existing frazil kernel:

```c++
const Teos10::FrazilEquilibrium Frazil;

parallelFor({NCellsAll, NVertLevels}, KOKKOS_LAMBDA(int ICell, int K) {
   Real SaFinal, CtFinal, IceFrac;
   Frazil(SaBulk(ICell, K), PotEnthalpyBulk(ICell, K), Pressure(ICell, K),
          SaFinal, CtFinal, IceFrac);
   ...
});
```

Input clamping is applied inside each fitted functor, at the top of
`operator()`, using `Kokkos::min` and `Kokkos::max` against the declared domain
bounds. This keeps evaluation branch-free and guarantees a bounded result.
Detecting and reporting that a state has left the valid range is the job of
Omega's existing state validation, not of the fit.

### 4.3 Offline fitting tool

The fitting tool lives under `components/omega/tools/teos10_fit` and is plain
Python depending only on NumPy and SciPy. It is not part of the Omega build and
is not required to build or run Omega; it is run by a developer when a function
is added or a fit is revised, and its output — the generated headers — is
reviewed and committed like any other source.

The tool links against the unmodified GSW-C submodule through a thin `ctypes`
or `cffi` binding to generate reference values. That binding calls the toolbox;
it does not modify it.

Each generated header records, in comments, the tool version, the GSW-C commit
used, the declaration that produced it, the fit order, the achieved error, and
the justification required by requirement 2.3, so that any coefficient in the
repository can be traced to a reproducible run and to the reason it exists. The
tool refuses to emit a header for a declaration that carries no `Justification`,
and refuses to emit one justified by efficiency that carries no measurement.

### 4.4 Relationship to existing code

`Eos.h` is unchanged by this design except that `calcCtFreezing` and its
derivative are made available to the new functors, since the frazil equilibrium
solve needs them. The Roquet et al. (2015) specific volume polynomial and the
conservative/potential temperature conversions stay exactly as they are.

`Frazil.h` drops its `#include <gswteos-10.h>` and its direct `gsw_*` calls, and
uses the functors above instead. GSW-C remains a submodule and remains linked
into the test build, where it continues to serve as the verification reference.

## 5 Verification and Testing

### 5.1 Test: each fit meets its declared tolerance

For every declared function, a host-side unit test evaluates both the fit and
the unmodified GSW-C reference over a dense quasi-random sample of the declared
domain — independent of the sample used to construct the fit — and fails if the
maximum absolute or relative error exceeds the value recorded in the generated
header. Tests requirements 2.4, 2.6, 2.9.

### 5.2 Test: device and host results agree

Every fitted function is evaluated on host and on device over the same sample
and the results are required to agree bitwise. This catches accidental use of a
host-only construct and any backend-dependent evaluation order. Tests
requirement 2.1, and the reproducibility half of 2.7.

### 5.3 Test: derivatives match the fit

Analytic derivatives are compared against a high-order finite difference of the
fitted value itself, not of the GSW reference, and must agree to the accuracy of
the difference stencil. This verifies internal consistency rather than physical
accuracy, which is the property requirement 2.8 asks for. A second, looser check
compares the derivative against the corresponding GSW `*_first_derivatives_poly`
function to confirm it is also physically right.

### 5.4 Test: composite quantities against GSW-C

The composite functors — frazil equilibrium and melting of ice into seawater —
are compared against `gsw_frazil_properties_potential_poly` and
`gsw_melting_ice_into_seawater` over a sample spanning both the ice-present and
ice-free regimes, with dense sampling within a narrow band either side of
$f_0 = 0$. Pass requires the declared tolerance on all three outputs, and exact
agreement on which regime was selected. Tests requirements 2.4, 2.9, and the
regime handling in section 3.3.

### 5.5 Test: exact algebraic identities hold

The recovered outputs must satisfy $S_A(1 - w_{Ih}) = S_A^{bulk}$ and
$\Theta = \Theta_f(S_A, p)$ to round-off, independently of how accurate the
$w_{Ih}$ fit is, since those relations are imposed exactly rather than fitted.
Failure indicates the decomposition of section 3.1 has been broken. Tests the
reduction that requirement 2.3 depends on.

### 5.6 Test: out-of-range inputs remain bounded

Evaluation at inputs well outside the declared domain, including at extreme
values and at zero salinity, must return finite values and must equal the value
at the clamped domain boundary. Tests requirement 2.6.

### 5.7 Test: no data-dependent iteration

Verified by inspection and by a timing test showing that evaluation cost is
independent of the input state, in particular that ice-present and ice-free
inputs cost the same within noise on GPU. Tests requirement 2.5.

### 5.8 Test: frazil regression

The existing frazil test in `test/ocn/FrazilTest.cpp` continues to pass with the
fitted functions in place of the direct GSW-C calls, within a tolerance widened
to the composite tolerance declared in 5.4. This is the end-to-end check that
the replacement is fit for its purpose.
