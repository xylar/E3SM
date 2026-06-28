#ifndef OMEGA_PGRAD_H
#define OMEGA_PGRAD_H
//===-- ocn/PGrad.h - Pressure Gradient -----------------*- C++ -*-===//
///
/// Implements the PressureGrad class which provides a centered and
/// high-order pressure gradient option and dispatches computations to
/// functor objects. This follows the patterns used in Eos.h/Eos.cpp.
//
//===----------------------------------------------------------------------===//

#include "Config.h"
#include "Eos.h"
#include "GlobalConstants.h"
#include "HorzMesh.h"
#include "OceanState.h"
#include "OmegaKokkos.h"
#include "VertCoord.h"
#include <memory>

namespace OMEGA {

enum class PressureGradType {
   Centered,    // existing 2nd-order Montgomery scheme
   FiniteVolume // high-order finite-volume analytic-integration scheme
   // , <FutureVariant>   // e.g. a 6th-order option, added when implemented
};

// Centered pressure gradient functor
class PressureGradCentered {
 public:
   bool Enabled;

   // constructor declaration
   PressureGradCentered(const HorzMesh *Mesh,   ///< [in] Horizontal mesh
                        const VertCoord *VCoord ///< [in] Vertical coordinate
   );

   // Compute centered pressure gradient contribution for given edge and
   // vertical chunk. This appends results into the Tend array (in-place).
   KOKKOS_FUNCTION void operator()(const Array2DReal &Tend, I4 IEdge, I4 KChunk,
                                   const Array2DReal &PressureMid,
                                   const Array2DReal &PressureInterface,
                                   const Array2DReal &GeomZInterface,
                                   const Array1DReal &TidalPotential,
                                   const Array1DReal &SelfAttractionLoading,
                                   const Array2DReal &SpecVol) const {

      const I4 KStart = chunkStart(KChunk, MinLayerEdgeBot(IEdge));
      const I4 KLen   = chunkLength(KChunk, KStart, MaxLayerEdgeTop(IEdge));

      const I4 ICell0      = CellsOnEdge(IEdge, 0);
      const I4 ICell1      = CellsOnEdge(IEdge, 1);
      const Real InvDcEdge = 1.0_Real / DcEdge(IEdge);

      Real GradGeoPot =
          (TidalPotential(ICell1) - TidalPotential(ICell0)) * InvDcEdge +
          (SelfAttractionLoading(ICell1) - SelfAttractionLoading(ICell0)) *
              InvDcEdge;

      for (int KVec = 0; KVec < KLen; ++KVec) {
         const I4 K = KStart + KVec;
         Real MontPotCell0K =
             PressureInterface(ICell0, K) * SpecVol(ICell0, K) +
             Gravity * GeomZInterface(ICell0, K);
         Real MontPotCell1K =
             PressureInterface(ICell1, K) * SpecVol(ICell1, K) +
             Gravity * GeomZInterface(ICell1, K);
         Real GradMontPotK = (MontPotCell1K - MontPotCell0K) * InvDcEdge;

         Real MontPotCell0Kp1 =
             PressureInterface(ICell0, K + 1) * SpecVol(ICell0, K) +
             Gravity * GeomZInterface(ICell0, K + 1);
         Real MontPotCell1Kp1 =
             PressureInterface(ICell1, K + 1) * SpecVol(ICell1, K) +
             Gravity * GeomZInterface(ICell1, K + 1);
         Real GradMontPotKp1 = (MontPotCell1Kp1 - MontPotCell0Kp1) * InvDcEdge;
         Real GradMontPot    = 0.5_Real * (GradMontPotK + GradMontPotKp1);

         Real PGradAlpha =
             0.5_Real * (PressureMid(ICell1, K) + PressureMid(ICell0, K)) *
             (SpecVol(ICell1, K) - SpecVol(ICell0, K)) * InvDcEdge;
         Tend(IEdge, K) +=
             EdgeMask(IEdge, K) * (-GradMontPot + PGradAlpha - GradGeoPot);
      }
   }

 private:
   Array2DI4 CellsOnEdge;
   Array1DReal DcEdge;
   Array2DReal EdgeMask;
   Array1DI4 MinLayerEdgeBot;
   Array1DI4 MaxLayerEdgeTop;
};

// High-order pressure gradient functor (placeholder)
class PressureGradHighOrder {
 public:
   bool Enabled;

   // constructor declaration
   PressureGradHighOrder(const HorzMesh *Mesh,   ///< [in] Horizontal mesh
                         const VertCoord *VCoord ///< [in] Vertical coordinate
   );

   KOKKOS_FUNCTION void operator()(
       const Array2DReal &Tend, I4 IEdge, I4 KChunk,
       const Array2DReal &PressureMid, const Array2DReal &PressureInterface,
       const Array2DReal &GeomZInterface, const Array1DReal &TidalPotential,
       const Array1DReal &SelfAttractionLoading, const Array2DReal &SpecVol,
       const Array2DReal &ConservTemp, const Array2DReal &AbsSalinity,
       const Array2DReal &SpecVolDThetaCons, const Array2DReal &SpecVolDSalt,
       const Array2DReal &SpecVolDPressure) const {

      // Finite-volume horizontal pressure gradient, constant-in-layer limit.
      //
      // The layer-mean edge-normal acceleration is -alpha*grad(p) - grad(Phi),
      // evaluated at the layer's two interfaces and averaged. Specific volume
      // at each interface comes from a reference-state Taylor expansion about
      // the mid-layer pressure,
      //    alpha(p) = alpha0 + alpha_p * (p - p_mid),
      // with alpha0 = SpecVol and alpha_p = SpecVolDPressure. Including the
      // compressibility term alpha_p makes the discrete pressure force and the
      // geopotential (built hydrostatically from the same alpha0) cancel to
      // machine precision for a column whose alpha is horizontally uniform as a
      // function of pressure (discrete hydrostatic consistency). Dropping
      // alpha_p reduces this operator exactly to PressureGradCentered.
      //
      // ConservTemp, AbsSalinity, and the temperature/salinity derivatives are
      // not used in the constant-in-layer limit; they enter once the
      // mean-preserving within-layer reconstruction is added.
      (void)ConservTemp;
      (void)AbsSalinity;
      (void)SpecVolDThetaCons;
      (void)SpecVolDSalt;

      const I4 KStart = chunkStart(KChunk, MinLayerEdgeBot(IEdge));
      const I4 KLen   = chunkLength(KChunk, KStart, MaxLayerEdgeTop(IEdge));

      const I4 ICell0      = CellsOnEdge(IEdge, 0);
      const I4 ICell1      = CellsOnEdge(IEdge, 1);
      const Real InvDcEdge = 1.0_Real / DcEdge(IEdge);

      const Real GradGeoPot =
          (TidalPotential(ICell1) - TidalPotential(ICell0)) * InvDcEdge +
          (SelfAttractionLoading(ICell1) - SelfAttractionLoading(ICell0)) *
              InvDcEdge;

      for (int KVec = 0; KVec < KLen; ++KVec) {
         const I4 K = KStart + KVec;

         const Real Alpha0Cell0 = SpecVol(ICell0, K);
         const Real Alpha0Cell1 = SpecVol(ICell1, K);
         const Real AlphaPCell0 = SpecVolDPressure(ICell0, K);
         const Real AlphaPCell1 = SpecVolDPressure(ICell1, K);
         const Real PMidCell0   = PressureMid(ICell0, K);
         const Real PMidCell1   = PressureMid(ICell1, K);

         // -alpha*grad(p) - g*grad(z) at the top (J=K) and bottom (J=K+1)
         // interfaces, then averaged over the layer.
         Real Acc = 0.0_Real;
         for (int J = K; J <= K + 1; ++J) {
            const Real PCell0 = PressureInterface(ICell0, J);
            const Real PCell1 = PressureInterface(ICell1, J);
            const Real AlphaCell0 =
                Alpha0Cell0 + AlphaPCell0 * (PCell0 - PMidCell0);
            const Real AlphaCell1 =
                Alpha0Cell1 + AlphaPCell1 * (PCell1 - PMidCell1);
            const Real AlphaEdge = 0.5_Real * (AlphaCell0 + AlphaCell1);
            const Real GradP     = (PCell1 - PCell0) * InvDcEdge;
            const Real GradZ =
                (GeomZInterface(ICell1, J) - GeomZInterface(ICell0, J)) *
                InvDcEdge;
            Acc += -AlphaEdge * GradP - Gravity * GradZ;
         }
         Acc *= 0.5_Real;

         Tend(IEdge, K) += EdgeMask(IEdge, K) * (Acc - GradGeoPot);
      }
   }

 private:
   Array2DI4 CellsOnEdge;
   Array1DReal DcEdge;
   Array2DReal EdgeMask;
   Array1DI4 MinLayerEdgeBot;
   Array1DI4 MaxLayerEdgeTop;
};

// Pressure gradient manager class
class PressureGrad {
 public:
   // Flag to indicate if pressure gradient term is enabled
   bool Enabled;

   // Initialize the default instance
   static void init();

   // Create a new pressure gradient object and add to map
   static PressureGrad *create(const std::string &Name, const HorzMesh *Mesh,
                               const VertCoord *VCoord, Config *Options);

   // Get the default instance
   static PressureGrad *getDefault();

   // Get instance by name
   static PressureGrad *
   get(const std::string &Name ///< [in] Name of PressureGrad
   );

   // Deallocates arrays and deletes instance
   static void clear();

   // Remove pressure gradient object by name
   static void erase(const std::string &Name ///< [in] Name of PressureGrad
   );

   // Destructor
   ~PressureGrad();

   // Compute pressure gradient tendencies and add into Tend array. The
   // ConservTemp, AbsSalinity, and specific-volume derivative fields are used
   // only by the high-order finite-volume option; the centered option ignores
   // them.
   void computePressureGrad(
       Array2DReal &Tend, const Array2DReal &PressureMid,
       const Array2DReal &PressureInterface, const Array2DReal &SpecVol,
       const Array2DReal &GeomZInterface, const Array2DReal &PseudoThick,
       const Array2DReal &ConservTemp, const Array2DReal &AbsSalinity,
       const Array2DReal &SpecVolDThetaCons, const Array2DReal &SpecVolDSalt,
       const Array2DReal &SpecVolDPressure) const;

 private:
   // Construct a new pressure gradient object
   PressureGrad(const HorzMesh *Mesh, const VertCoord *VCoord, Config *Options);

   // forbid copy and move construction
   PressureGrad(const PressureGrad &) = delete;
   PressureGrad(PressureGrad &&)      = delete;

   // Pointer to default pressure gradient object
   static PressureGrad *DefaultPGrad;

   // Mesh-related sizes
   I4 NEdgesAll     = 0;
   I4 NEdgesOwned   = 0;
   I4 NVertLayers   = 0;
   I4 NVertLayersP1 = 0;

   // Data required for computation (stored copies of VCoord arrays)
   Array1DI4 MinLayerEdgeBot; ///< min vertical layer on each edge
   Array1DI4 MaxLayerEdgeTop; ///< max vertical layer on each edge

   // Temporary: to be moveed to tidal forcing module in future
   Array1DReal TidalPotential; ///< Tidal potential for tidal forcing
   Array1DReal
       SelfAttractionLoading; ///< Self attraction and loading for tidal forcing

   // Instances of functors
   PressureGradCentered CenteredPGrad;
   PressureGradHighOrder HighOrderPGrad;

   // Choice from config
   PressureGradType PressureGradChoice = PressureGradType::Centered;

   // Map of all pressure gradient objects by name
   static std::map<std::string, std::unique_ptr<PressureGrad>> AllPGrads;

}; // end class PressureGrad

} // namespace OMEGA
#endif
