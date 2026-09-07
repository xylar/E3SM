(omega-design-teos10-functions)=

# TEOS-10 Functions

**Table of Contents**
1. [Overview](#1-overview)
2. [Requirements](#2-requirements)
3. [Algorithmic Formulation](#3-algorithmic-formulation)
4. [Design](#4-design)
5. [Verification and Testing](#5-verification-and-testing)
6. [References](#6-references)

## 1 Overview

Omega needs a set of TEOS-10 thermodynamic quantities that it can evaluate on
GPU and redistribute. The reference implementation of these quantities is the
Gibbs SeaWater (GSW) Oceanographic Toolbox, which Omega carries as an unmodified
submodule in `components/omega/external/GSW-C`. That toolbox cannot supply them
directly, for two reasons.

**Licence.** The GSW licence permits "redistribution and use, in source and
binary forms, *without modification*". A Kokkos port of GSW-C source is a
modified redistribution and is not permitted. Using the unmodified toolbox on
the host, as a source of reference values for testing, is permitted and is what
Omega already does.

**Portability.** GSW-C is host C and cannot be called from device code. The
frazil implementation currently calls five `gsw_*` functions from inside a
`KOKKOS_FUNCTION`, which will not compile for CUDA, HIP or SYCL.

This document describes how Omega derives these quantities itself, from
published sources, and verifies them against the unmodified toolbox. It covers
both remediation of code already in `Eos.h` that was transcribed from GSW-C —
Omega issue #499 — and the additional functions the frazil work needs.

The immediate consumer is PR #462, which adds a TEOS-10 frazil formation and
melt scheme and states plainly that "the EOS functions rely on the module, which
require CPU; later port of EOS features will allow portability". This design is
that port. Section 4.5 sets out the interface contract between the two so they
can proceed in parallel.

### 1.1 Why this replaces an earlier fitting-based design

An earlier draft of this design proposed a general polynomial-fitting framework:
sample GSW-C over Omega's valid range, fit our own approximations offline,
generate coefficient headers from a declaration file. The motivation was partly
licensing and partly a belief that fitted approximations would be cheaper than
GSW's algorithms on GPU, principally by removing Newton iterations whose trip
count varies per work item.

That performance claim was measured on Aurora before anything was built. It did
not survive:

- `Teos10Eos` is **memory-bandwidth-bound**, running at 1.06–1.26× a pure
  streaming kernel with the same memory traffic once the working set leaves
  cache. Its arithmetic is almost entirely hidden. Making the specific volume
  polynomial cheaper would buy close to nothing.
- The frazil equilibrium solve — the function the fitting design was built
  around — gains only about **5–7%** from a fit in a realistic, sparse ice
  field, and the fitted kernel is itself bandwidth-bound.
- Divergence does serialize on the hardware, so the mechanism is real, but GSW's
  frazil solve varies its trip count only between one and three, and the common
  ocean case is one. The mechanism has little to act on.

Only one function showed a large gain, and not for the expected reason. See
section 3.5. One worthwhile fit does not justify a fitting framework, so the
framework is gone: this design implements functions directly from published
sources and treats fitting as an occasional tactic rather than the method.

**Licensing is therefore the driver, and correctness rather than speed is the
requirement.** That changes what is built. The cheapest licence-clean route to
most of these quantities is direct implementation from the published standards,
not fitting, and there is now no performance argument to pay for the extra work
a fit costs.

## 2 Requirements

### 2.1 Requirement: abide by the GSW Oceanographic Toolbox licence

No GSW-C source may be modified or redistributed in modified form. No
coefficient values may be transcribed from GSW-C into Omega source, tests or
reference tables. The unmodified toolbox may be linked and called on the host to
generate reference values for verification.

### 2.2 Requirement: remediate the functions identified in issue #499

`calcGibbsDerivPt0Pt0`, `calcPtFromCt`, `calcCtFromPt` and
`calcCtFreezingTeos10` in `Eos.h` are direct ports of GSW-C code. This is
established for `calcCtFreezingTeos10`, whose constants `C0`–`C22`, `A` and `B`
are the values of `GSW_FREEZING_POLY_COEFFICIENTS` in GSW-C's
`gsw_internal_const.h` under renamed symbols. Each must be replaced by an
implementation Omega derives itself.

### 2.3 Requirement: device-callable evaluation

Every function must be callable from within a `KOKKOS_FUNCTION` and must compile
and run correctly on CPU, CUDA, HIP and SYCL backends. This is the immediate
blocker for the frazil implementation.

### 2.4 Requirement: derive from published sources, and record which

Each function is implemented from a citable published source, and records that
source in the code. "Published" means a document anyone can obtain independently
of the GSW-C code base — see section 3.1, which distinguishes the TEOS-10
standards from GSW's own approximations to them.

### 2.5 Requirement: prefer direct implementation; fit only with justification

The default route is a direct implementation from the published standard. A
fitted approximation is admitted only where it is *simpler* than the direct
implementation, or where it is *measurably faster in the kernel it will actually
run in*. Both a fit and a hand-written port cost developer time, review effort
and space in the eventual model description; neither is free, and the choice
must be recorded per function.

### 2.6 Requirement: verification against unmodified GSW-C

Every function must be verified against the unmodified GSW-C toolbox over a
dense sample of its valid range, and the test must fail if the declared
tolerance is exceeded. Tolerances are declared per function, chosen against what
the science needs and against the accuracy of the GSW reference itself — several
of GSW's own polynomial variants carry errors of order `1e-4` K by construction.

### 2.7 Requirement: reproducibility

Coefficients are compile-time constants in the source. No fitting is performed at
build time or run time and no coefficient file is read at initialization.
Evaluation uses only arithmetic operations and, where unavoidable, `sqrt`, so
that results are reproducible across compilers and backends to the extent the
underlying floating-point arithmetic is.

### 2.8 Requirement: consistent derivatives

Where a derivative is needed — in a Newton solve, or in a tendency
linearization — it is obtained by analytically differentiating the same
expression used for the value, not by implementing a separate approximation to
the derivative. This guarantees value and derivative are mutually consistent.

### 2.9 Requirement: bounded evaluation

Evaluation must terminate in a bounded number of steps. Iteration counts must be
fixed at compile time rather than driven by a convergence test, so that cost is
independent of the input state. This is required for predictable performance and
to avoid unbounded per-work-item work on GPU; it is no longer claimed as a major
performance win in itself (section 3.5).

### 2.10 Desired: extensibility

Omega will need further TEOS-10 quantities — sea ice coupling, geothermal
fluxes and ice-shelf thermodynamics are all likely candidates. Adding one should
follow the same pattern: identify the published source, choose a route, verify
against the toolbox, record the provenance.

## 3 Algorithmic Formulation

### 3.1 What is published, and what is not

Two different things could be meant by saying TEOS-10 is published, and only one
of them is fully public. The distinction determines what Omega may implement and
how.

**The standard itself is published.** The primitives are freely available
releases, not code:

- The seawater Gibbs function is IAPWS R13-08.
- The Gibbs function of ice Ih is IAPWS R10-06(2009), also published as Feistel
  and Wagner (2006).
- Both are restated in the TEOS-10 Manual (IOC, SCOR and IAPSO, 2010).
- The thermodynamics of frazil ice and of ice melting into seawater is derived
  in McDougall et al. (2014).

Every quantity Omega needs is *defined* by these documents, with full
coefficient tables. Nothing here requires reading GSW-C.

**GSW's polynomial approximations to the standard are only partly published.**
Roquet et al. (2015) published theirs, which is why Omega's specific volume
polynomial is on sound footing. Most of the others exist only as coefficients in
the code. TEOS-10's own notes on `gsw_frazil_properties_potential_poly` are
explicit about the *form* of the approximation:

> In the GSW code we first estimate the ice mass fraction using a polynomial
> with 36 coefficients. This polynomial is in terms of (normalized values of)
> $S_A^B$, `func0` and $p$. Every term in the polynomial contains at least the
> first power of `func0` so that $w_{Ih} \to 0$ as `func0` $\to 0$.

The document derives the whole method and describes how the polynomial was
constructed, including that its fit was weighted towards the region ocean models
actually occupy — but it lists no coefficient values. The same is true of the
29-coefficient derivative polynomial described in the same document, and
`gsw_pot_enthalpy_ice_freezing_poly` cites only the TEOS-10 Manual and a
Newton-method paper for its coefficients.

**The consequence is that Omega is never blocked, only occasionally inconvenienced.**
The published layer defines the answer. Where GSW's approximation to it is also
published, Omega may implement that approximation. Where it is not, Omega
implements the underlying relation from the standard, or constructs its own
approximation and checks it against the unmodified toolbox. The publication
status of GSW's polynomials determines how much work a function costs, never
whether it can be done.

### 3.2 The functions in scope

Two groups. First, the issue #499 remediations, already in `Eos.h`:

| Function | Quantity | Published source for the relation |
|---|---|---|
| `calcGibbsDerivPt0Pt0` | $\partial^2 g/\partial\theta^2$ at reference pressure | IAPWS R13-08 |
| `calcCtFromPt` | conservative from potential temperature | TEOS-10 Manual §3.3 |
| `calcPtFromCt` | potential from conservative temperature | inverse of the above |
| `calcCtFreezingTeos10` | $\Theta_f(S_A, p)$, freezing conservative temperature | TEOS-10 Manual §3.33; McDougall et al. (2014) |

Second, the quantities the frazil implementation needs and Omega does not have.
These correspond to the five `gsw_*` calls currently in `Frazil.h`, but they are
listed here as *quantities*, because the mapping is not one to one — section 3.4
explains why.

| Quantity | Inputs | Dim | Notes |
|---|---|---|---|
| $\theta_{Ih}(h_{Ih})$ | potential enthalpy of ice | 1 | GSW's is a degree-7 polynomial |
| $h_{Ih}(\theta_{Ih})$ | potential temperature of ice | 1 | inverse of the above; see 3.5 |
| $t(\theta_{Ih}^0, p)$ | ice potential temperature, pressure | 2 | from the ice Gibbs function |
| $h_{Ih}^{f}(S_A, p)$ | absolute salinity, pressure | 2 | ice potential enthalpy at freezing |
| $w_{Ih}(S_A^{bulk}, h^{bulk}, p)$ | bulk salinity, bulk enthalpy, pressure | 3 | frazil equilibrium; see 3.4 |

Seawater enthalpy $h(S_A,\Theta,p)$ is deliberately absent. It is the pressure
integral of specific volume, so it follows analytically from the 75-term
polynomial Omega already has. Deriving it that way needs no new approximation
and guarantees that Omega's enthalpy and its specific volume are
thermodynamically consistent with each other — a property an independent
implementation would not have.

### 3.3 Choosing a route for each function

Requirement 2.5 admits three routes. In decreasing order of preference:

1. **Direct from the standard.** Implement the published relation. Preferred
   wherever the relation is a closed-form expression or a fixed, short
   iteration.
2. **Implement a published approximation.** Where GSW's approximation is itself
   published — Roquet et al. (2015) is the model case — Omega may implement it
   from the paper, taking coefficients from the publication and not from the
   code.
3. **Construct our own approximation.** Fit to values generated by the
   unmodified toolbox, or to the standard evaluated directly. Reserved for cases
   meeting requirement 2.5: simpler than the direct route, or measurably faster
   where it runs.

Route 3 is expected to be rare. Section 3.5 identifies the one function in the
current scope that clearly qualifies.

### 3.4 Structural reductions

Before choosing routes, it is worth reducing what has to be implemented at all.
The GSW functions Omega calls are mostly thin wrappers of exact algebra around a
small number of genuinely irreducible relations, and the algebra is ours to
write.

`gsw_frazil_properties_potential_poly` is the clearest case. It appears to be a
three-input, three-output map solved iteratively. Define the regime indicator

$$
f_0 = h^{bulk} - c_p^0\,\Theta_f(S_A^{bulk}, p)
$$

using the freezing temperature Omega already needs for other reasons. Then:

- If $f_0 \ge 0$ the mixture is warmer than freezing and contains no ice, and
  the answer is written down exactly, with no approximation:
  $S_A = S_A^{bulk}$, $\Theta = h^{bulk}/c_p^0$, $w_{Ih} = 0$.
- If $f_0 < 0$, ice is present and $w_{Ih}$ is the root of

$$
F(w) = h^{bulk} - (1-w)\,c_p^0\,\Theta_f\!\left(\tfrac{S_A^{bulk}}{1-w}, p\right)
       - w\,h_{Ih}^{f}\!\left(\tfrac{S_A^{bulk}}{1-w}, p\right)
$$

  and once $w_{Ih}$ is known the other two outputs follow by exact algebra:
  $S_A = S_A^{bulk}/(1 - w_{Ih})$ and $\Theta = \Theta_f(S_A, p)$.

So a function that looks like a three-output iterative solve reduces to **one
scalar unknown on one branch**, plus a regime test and three lines of algebra
Omega writes and documents itself. `gsw_melting_ice_into_seawater` decomposes
the same way: a validity guard, two mixing rules

$$
S_A^{bulk} = (1-w_{Ih})S_A, \qquad
h^{bulk} = (1-w_{Ih})\,h(S_A,\Theta,p) + w_{Ih}\,h_{Ih}(t_{Ih},p)
$$

and the same equilibrium solve. Only the enthalpies are irreducible.

**Regime boundaries are located analytically and handled by exact branch
selection, never absorbed into an approximation.** The $f_0$ test is exact and
the ice-free branch is exact, so any approximation error is confined to the
branch where ice is actually present.

### 3.5 What the profiling established

The measurements summarized in section 1.1 were made on one tile of an Intel
Data Center GPU Max 1550, in Omega's own launch geometry, in fp64. Three results
bear on this design.

**The equation of state is bandwidth-bound.** Measured against a streaming
kernel with identical memory traffic, `Teos10Eos` costs 1.06–1.26× once the
working set exceeds L2, with its arithmetic alone at about a third of its memory
time. The exemption of the Roquet et al. polynomial from remediation is on solid
ground, and the reasoning generalizes: any approximation landing in a kernel
with this memory profile will be hidden behind bandwidth.

**Bounded evaluation is a correctness property here, not a performance one.**
Divergence does serialize — a sub-group pays for its slowest work item — but
GSW's frazil solve varies its trip count only between one and three, and sets it
by classifying the state rather than by a convergence test, with the ordinary
ocean falling in the one-iteration case. In a realistic sparse ice field the
measured gain from replacing it was 5–7%. Requirement 2.9 is kept because
predictable cost is worth having, not because it pays for itself.

*Caveat.* That 5–7% was measured with lanes mapped to vertical levels, following
`Eos.cpp`. The frazil kernel in PR #462 instead maps lanes to whole columns — it
must, because its accumulators carry state down the column — so its divergence
pattern is driven by horizontal patchiness of supercooling rather than vertical.
The penalty there could be larger. This should be measured before the frazil
equilibrium solve is finally ruled out as a candidate for route 3, and it should
be measured against whatever launch geometry PR #462 settles on, since that PR
is still a draft and its kernel structure may yet change.

**One function is worth approximating, for a reason unrelated to divergence.**
`gsw_pot_enthalpy_from_pt_ice_poly` maps ice potential temperature to potential
enthalpy — one input, one output. GSW computes it by Newton-inverting
`gsw_pt_from_pot_enthalpy_ice_poly`, itself a degree-7 polynomial, through five
unconditional iterations, each evaluating that polynomial plus two derivative
polynomials: roughly sixteen polynomial evaluations to invert a one-dimensional
function. A single polynomial fitted directly to the inverse measured **4.4×**
faster, with no divergence involved at all — the iteration count is fixed, so
every work item does identical work. This is a pure arithmetic win.

It is also the case where all three considerations point the same way: a direct
fit to the inverse is *less* code than transcribing a Newton loop, it is
licence-clean because the inverse relation is defined by the published ice Gibbs
function, and it is faster. This is the model for route 3.

## 4 Design

### 4.1 Data types and parameters

#### 4.1.1 Parameters

None at run time. All coefficients are `constexpr` and all iteration counts are
compile-time constants. There is no configuration file section for this
functionality, and nothing is read at initialization.

#### 4.1.2 Class/structs/data types

Functions follow the pattern already established in `Eos.h`: `KOKKOS_FUNCTION`
scalar methods, static where they need no state, grouped into a class per
coherent set of quantities.

The new ice and frazil quantities are grouped in a single header,
`components/omega/src/ocn/Teos10Ice.h`:

```c++
namespace OMEGA {

/// Thermodynamic properties of ice Ih and of seawater/frazil mixtures,
/// derived from IAPWS R10-06(2009) and McDougall et al. (2014).
/// See doc/design/Teos10Functions.md for provenance of each relation.
class Teos10Ice {
 public:
   /// Potential temperature of ice from its potential enthalpy.
   static KOKKOS_FUNCTION Real calcPtFromPotEnthalpyIce(Real PotEnthalpyIce);

   /// Potential enthalpy of ice from its potential temperature.
   /// Inverse of calcPtFromPotEnthalpyIce.
   static KOKKOS_FUNCTION Real calcPotEnthalpyFromPtIce(Real PtIce);

   /// In situ temperature of ice from potential temperature and pressure.
   static KOKKOS_FUNCTION Real calcTFromPt0Ice(Real Pt0Ice, Real P);

   /// Potential enthalpy of ice at the freezing point.
   static KOKKOS_FUNCTION Real calcPotEnthalpyIceFreezing(Real Sa, Real P);

   /// Derivative of the above with respect to absolute salinity.
   static KOKKOS_FUNCTION Real calcPotEnthalpyIceFreezingDSa(Real Sa, Real P);
};

} // namespace OMEGA
```

The composite quantities that the frazil code calls are separate, because they
are exact algebra over the above rather than thermodynamic relations in their
own right:

```c++
namespace OMEGA {

/// Equilibrium state of a seawater/frazil mixture. Implements the
/// decomposition in Teos10Functions.md section 3.4: an exact regime test,
/// an exact ice-free branch, and a bounded solve for the ice mass fraction.
class FrazilEquilibrium {
 public:
   KOKKOS_FUNCTION void operator()(Real SaBulk, Real PotEnthalpyBulk, Real P,
                                   Real &SaFinal, Real &CtFinal,
                                   Real &IceFrac) const;

   /// Number of solve iterations. Fixed at compile time (requirement 2.9).
   static constexpr int NumIterations = 3;
};

} // namespace OMEGA
```

### 4.2 Methods

All methods are scalar and `KOKKOS_FUNCTION`, taking and returning `Real`, so
they compose inside existing Omega kernels. No array-level or team-level
interface is provided: these are consumed pointwise inside kernels that already
own their loop structure, as `Frazil.cpp` does today.

Typical use, replacing the current direct `gsw_*` calls:

```c++
const FrazilEquilibrium Frazil;

parallelFor({NCellsAll}, KOKKOS_LAMBDA(I4 ICell) {
   for (I4 K = KMax; K >= KMin; --K) {
      const Real Tfrz = Eos::calcCtFreezing(EosChoice, SA(ICell, K), PDb, 0.0);
      if (CT(ICell, K) < Tfrz) {
         Real SaFinal, CtFinal, IceFrac;
         Frazil(SaBulk, PotEnthalpyBulk, PDb, SaFinal, CtFinal, IceFrac);
         ...
      }
   }
});
```

### 4.3 Recording provenance

Requirements 2.4 and 2.5 are satisfied by a comment block on each function
giving the source of the relation and the route chosen. This replaces the
declaration file and code generator of the earlier design; with a handful of
functions, a convention is enough and a tool is not warranted.

```c++
/// Potential enthalpy of ice from potential temperature.
///
/// Source:  IAPWS R10-06(2009), ice Ih Gibbs function, via the potential
///          enthalpy relation of McDougall et al. (2014) section 2.
/// Route:   fitted approximation (Teos10Functions.md section 3.3, route 3).
/// Reason:  the direct route inverts a degree-7 polynomial by iteration;
///          a fit to the inverse is both shorter and 4.4x faster on PVC.
///          Fit constructed against unmodified GSW-C 3.06 as oracle.
/// Domain:  Pt in [-100, 2] degC.
/// Verified: max abs error <TBD> J/kg over the domain, see Teos10IceTest.
static KOKKOS_FUNCTION Real calcPotEnthalpyFromPtIce(Real PtIce);
```

Every numeric claim in such a block is filled in from a measurement or a test,
never from an estimate. Where a value is not yet established it is written
`<TBD>` rather than guessed.

### 4.4 Relationship to existing code, and ordering

The Roquet et al. (2015) specific volume polynomial in `Eos.h` is unchanged. Its
coefficients come from the paper's appendix, it is bandwidth-bound in practice,
and it is not in scope.

The four functions of requirement 2.2 are replaced in place, keeping their
current signatures so that callers are unaffected.

**$\Theta_f$ is remediated first.** `calcCtFreezingTeos10` is on the critical
path twice over: the frazil equilibrium regime test $f_0$ depends on it, and
`Frazil.cpp` already calls it unconditionally for every level of every cell. Any
frazil work built on top of the current implementation inherits the licence
problem, so it is the first thing to be re-derived and the ordering is not
negotiable.

GSW-C remains a submodule and remains linked into the test build, where it
continues to serve as the verification reference.

### 4.5 Relationship to the frazil work in PR #462

PR #462 adds the TEOS-10 frazil formation and melt scheme that consumes these
functions. It is an open draft, and it currently calls
`gsw_pt_from_pot_enthalpy_ice_poly`, `gsw_t_from_pt0_ice`,
`gsw_pot_enthalpy_from_pt_ice_poly`, `gsw_melting_ice_into_seawater` and
`gsw_frazil_properties_potential_poly` directly from inside a `KOKKOS_FUNCTION`.
That is the code this design exists to unblock, and the changes to it belong to
that PR rather than to this one.

**Interface contract.** The two efforts can proceed in parallel provided they
agree on the following, which is all PR #462 needs from this design:

| PR #462 currently calls | Replaced by |
|---|---|
| `gsw_pt_from_pot_enthalpy_ice_poly` | `Teos10Ice::calcPtFromPotEnthalpyIce` |
| `gsw_pot_enthalpy_from_pt_ice_poly` | `Teos10Ice::calcPotEnthalpyFromPtIce` |
| `gsw_t_from_pt0_ice` | `Teos10Ice::calcTFromPt0Ice` |
| `gsw_frazil_properties_potential_poly` | `FrazilEquilibrium::operator()` |
| `gsw_melting_ice_into_seawater` | composite over the above, section 3.4 |

All are scalar `KOKKOS_FUNCTION`s taking and returning `Real`, so the
substitution is local to the call sites and does not disturb PR #462's kernel
structure, its accumulators, or its conservation logic.

**Sequencing.** Two things are worth settling early with that PR's author:

1. PR #462 already calls `Eos::calcCtFreezing` unconditionally for every level
   of every cell. That is one of the issue #499 functions, so remediating
   $\Theta_f$ first (section 4.4) benefits both efforts and creates no
   additional coupling.
2. Whether PR #462 merges with its current CPU-only path and is ported
   afterwards, or waits for these functions, is a decision for that PR and the
   Omega team, not for this design. Either order works. What does not work is
   merging the direct `gsw_*` calls to `develop` and leaving them there, since
   they cannot be built for GPU and, being ports of GSW-C code, carry the same
   licence problem as issue #499.

**Testing overlap.** PR #462 notes that its current tests depend on verbose
logging that only works on CPU. The device-side verification in section 5.2 is
independent of that and does not inherit the limitation.

## 5 Verification and Testing

### 5.1 Test: each function matches unmodified GSW-C

For every function in scope, a host-side unit test evaluates both the Omega
implementation and the unmodified GSW-C reference over a dense sample of the
valid range, and fails if the declared tolerance is exceeded. Tests requirements
2.4, 2.5, 2.6.

### 5.2 Test: device and host results agree

Every function is evaluated on host and device over the same sample, and results
must agree bitwise. This catches accidental use of host-only constructs and
backend-dependent evaluation order. Tests requirements 2.3 and 2.7.

### 5.3 Test: derivatives are consistent with values

Analytic derivatives are compared against a high-order finite difference of the
Omega value, and must agree to the accuracy of the stencil. A second, looser
check against the corresponding GSW `*_first_derivatives*` function confirms the
derivative is also physically right. Tests requirement 2.8.

### 5.4 Test: composite quantities against GSW-C

`FrazilEquilibrium` and the melting composite are compared against
`gsw_frazil_properties_potential_poly` and `gsw_melting_ice_into_seawater` over a
sample spanning both regimes, with dense sampling in a narrow band either side of
$f_0 = 0$. Pass requires the declared tolerance on all three outputs and exact
agreement on which regime was selected. Tests requirements 2.6 and section 3.4.

### 5.5 Test: exact algebraic identities hold

The recovered outputs must satisfy $S_A(1 - w_{Ih}) = S_A^{bulk}$ and
$\Theta = \Theta_f(S_A, p)$ to round-off, independently of the accuracy of the
$w_{Ih}$ solve, because those relations are imposed exactly rather than
approximated. Failure indicates the decomposition of section 3.4 has been
broken.

### 5.6 Test: evaluation is bounded

Verified by inspection that no loop bound depends on input data, and by a timing
test showing evaluation cost is independent of input state — in particular that
ice-present and ice-free inputs cost the same within noise. Tests requirement
2.9.

### 5.7 Test: no GSW-C coefficients appear in Omega source

An automated check that no numeric literal in `components/omega/src` matches a
coefficient in the GSW-C headers. This is a blunt instrument and will need an
allowlist for genuinely shared physical constants, but it is the only test that
directly guards requirement 2.1, which is the reason this work exists. Tests
requirement 2.1.

### 5.8 Test: frazil regression

The frazil unit test added by PR #462, including its column conservation check
on ice mass, brine mass and brine salt, continues to pass with these
implementations in place of the direct GSW-C calls, within the composite
tolerance of 5.4. Because that check tests conservation rather than agreement
with GSW, it is the strongest end-to-end evidence that the replacement is fit
for purpose. Tests requirements 2.3 and 2.6.

## 6 References

Primary standards, all freely available and sufficient to define every quantity
in this document without reference to GSW-C source:

- IAPWS, 2008: *Release on the IAPWS Formulation 2008 for the Thermodynamic
  Properties of Seawater* (R13-08).
  <https://iapws.org/documents/release/Seawater> ·
  <https://www.teos-10.org/pubs/IAPWS-08.pdf>
- IAPWS, 2009: *Revised Release on the Equation of State 2006 for H2O Ice Ih*
  (R10-06(2009)). <https://iapws.org/documents/release/Ice-2009>
- Feistel, R. and W. Wagner, 2006: A new equation of state for H2O ice Ih.
  *J. Phys. Chem. Ref. Data*, **35**, 1021–1047.
  <https://www.teos-10.org/pubs/Feistel_and_Wagner_2006.pdf>
- IOC, SCOR and IAPSO, 2010: *The international thermodynamic equation of
  seawater – 2010: Calculation and use of thermodynamic properties.*
  Intergovernmental Oceanographic Commission, Manuals and Guides No. 56, UNESCO.
  <https://www.teos-10.org/pubs/TEOS-10_Manual.pdf>
- McDougall, T. J., P. M. Barker, R. Feistel and B. K. Galton-Fenzi, 2014:
  Melting of ice and sea ice into seawater and frazil ice formation.
  *J. Phys. Oceanogr.*, **44**, 1751–1775.
  <https://doi.org/10.1175/JPO-D-13-0253.1>
- McDougall, T. J. and S. J. Wotherspoon, 2014: A simple modification of
  Newton's method to achieve convergence of order $1 + \sqrt{2}$.
  *Appl. Math. Lett.*, **29**, 20–25.
  <https://doi.org/10.1016/j.aml.2013.10.008>

Published polynomial approximation Omega already relies on:

- Roquet, F., G. Madec, T. J. McDougall and P. M. Barker, 2015: Accurate
  polynomial expressions for the density and specific volume of seawater using
  the TEOS-10 standard. *Ocean Modelling*, **90**, 29–43.
  <https://doi.org/10.1016/j.ocemod.2015.04.002>

Method description without coefficient values, cited in section 3.1:

- TEOS-10: *Notes on gsw_frazil_properties_potential_poly*.
  <https://www.teos-10.org/pubs/gsw/pdf/frazil_properties_potential_poly.pdf>

Licence:

- *Licence for the use of the Gibbs SeaWater (GSW) Oceanographic Toolbox*,
  SCOR/IAPSO WG127. <https://github.com/TEOS-10/GSW-C/blob/main/LICENSE>
