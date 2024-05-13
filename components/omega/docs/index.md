
# Omega

The Ocean Model for E3SM Global Applications (Omega) initially is an eddy-resolving,
global ocean model in the early stages of development by the
[E3SM](https://e3sm.org/) ocean team.  The first release is planned for
Summer 2026.  A non-eddying configuration will be released in early 2027.

The model is written in c++ using the [Kokkos](https://github.com/kokkos)
framework for performance portability.  Omega is based on the
[TRSK formulation](https://doi.org/10.1016/j.jcp.2009.08.006) for geophysical
models on unstructured meshes. The first version of Omega will primarily be a direct port
of the [MPAS-Ocean](https://e3sm.org/model/e3sm-model-description/v1-description/v1-ocean-sea-ice-land-ice/)
component of E3SM for comparison purposes.

Development is taking place at
[https://github.com/E3SM-Project/Omega](https://github.com/E3SM-Project/Omega).


* [User Guide](userGuide/index.md): how to configure Omega in standalone or E3SM runs
* [Developer Guide](devGuide/index.md): how to contribute to the code
* Omega Technical Guide: (coming soon) the algorithms and science behind used in Omega
* [Design Documents](design/index.md): detail the thinking behing features
  during the process of adding them to Omega
