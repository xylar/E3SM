(omega-user-horz-mesh)=

## Horizontal Mesh

Omega uses the {ref}`MPAS mesh specification <mpas-mesh-specification>`. The
names of the mesh variables have been retained, with the caveat that they now
begin with a capital letter (e.g. `angleEdge` becomes `AngleEdge`).

The Mesh class is meant to be a container for all mesh variables local to a
decomposed sub-domain that can be easily passed among the dycore routines. It
depends on a given [Decomp](#omega-user-decomp) and reproduces the
cell/edge/vertex totals and connectivity information from that class. The Mesh
class also creates parallel I/O decompositions that are used to read in the
additional mesh variables, which are not required for Decomp.

Currently, the Mesh class reads in every field required by the
{ref}`MPAS mesh specification <mpas-mesh-specification>` except those read by
Decomp. The input contents are defined by the
`HorzMeshIn` FieldGroup. Refer to the specification for the
definition, units, and dimensions of each field.

These are all read using the input [IOStream](#omega-user-iostreams) HorzMeshIn
```yaml
  IOStreams:
    HorzMeshIn:
      UsePointerFile: false
      Filename: OmegaMesh.nc
      Mode: read
      Precision: double
      Freq: 1
      FreqUnits: OnStartup
      UseStartEnd: false
      Contents:
        - HorzMeshIn
```
Only the Filename should be changed by the user to point to the relevant input
mesh file. The mesh Filename is sometimes overridden by the driver routine in
the case of unit tests using an optional argument to the
[decomposition](#omega-dev-decomp).

In addition, some mesh metadata are read from the file and stored as mesh
variables. These include ``OnSphere`` (or ``on_a_sphere`` for backcompatibility),
``IsPeriodic`` (``is_periodic``), ``SphereRadius`` (``sphere_radius``),
``XPeriod`` (``x_period``), and ``YPeriod`` (``y_period``). The two flags
OnSphere and IsPeriodic are stored as YES/NO strings in the metadata but as
boolean flags in the code. For a spherical mesh, ``SphereRadius`` must match
the Earth radius from the Physical Constants Dictionary (``REarth`` in
``GlobalConstants.h``) to a relative tolerance of $10^{-8}$; the model aborts
otherwise. See the {ref}`MPAS mesh specification <mpas-mesh-specification>` for how to
bring older meshes into compliance.

In the future, the Mesh class will optionally compute internally those fields
the specification marks as *could be computed internally* — the areas, lengths,
angles, and weights needed for the TRiSK discretization.

### Horizontal mixing mesh scaling

Omega scales horizontal viscosity and diffusivity with the local mesh size.
This scaling is enabled by default and is controlled by the following
configuration option:

```yaml
  HorzMesh:
    MeshScalingEnable: true
```

The reference width is the global median cell width, computed during mesh
initialization. Each cell's width is defined as the diameter of a circle
with the same area as the cell.

At each edge, an effective cell width is computed from the mean area of the
two adjacent cells. The Laplacian mixing coefficient is multiplied by the
ratio of this width to the reference width, and the biharmonic mixing
coefficient by the cube of that ratio. The parameters `ViscDel2`, `ViscDel4`,
`EddyDiff2` and `EddyDiff4` therefore specify the mixing coefficients at the
median resolution. Their values may need adjustment when changing to a mesh
with a different median cell width.

On a mesh with equal cell areas, both scaling factors are one. Setting
`MeshScalingEnable` to false also sets both factors to one, giving spatially
uniform mixing coefficients.

The initialization log reports whether mesh scaling is enabled, the median
cell width and the range of both scaling factors over interior edges.

In the future, the Mesh class will optionally compute the mesh variables that
are dependent on the Cartesian mesh coordinates internally.
This includes the various areas, lengths, angles, and weights needed for the
TRiSK discretization (e.g. rows 5-11 in the table above).
