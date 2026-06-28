//===-- Test driver for OMEGA Pressure Gradient (PGrad) --------------*-
// C++-*-===/
//
/// \file
/// \brief Test driver for PressureGrad module
//
//===----------------------------------------------------------------------===/

#include "PGrad.h"

#include "DataTypes.h"
#include "Decomp.h"
#include "Dimension.h"
#include "Eos.h"
#include "Error.h"
#include "Field.h"
#include "GlobalConstants.h"
#include "Halo.h"
#include "HorzMesh.h"
#include "IO.h"
#include "IOStream.h"
#include "Logging.h"
#include "MachEnv.h"
#include "OceanState.h"
#include "OmegaKokkos.h"
#include "PGrad.h"
#include "Pacer.h"
#include "TimeStepper.h"
#include "Tracers.h"
#include "VertCoord.h"
#include "mpi.h"
#include <limits>

using namespace OMEGA;

void initPGradTest() {

   Error Err;
   int Err1;

   MachEnv::init(MPI_COMM_WORLD);
   MachEnv *DefEnv  = MachEnv::getDefault();
   MPI_Comm DefComm = DefEnv->getComm();

   // Initialize the Logging system
   initLogging(DefEnv);

   // Read default config if present
   Config("Omega");
   Config::readAll("omega.yml");

   // First step of time stepper initialization needed for IOstream
   TimeStepper::init1();
   TimeStepper *DefStepper = TimeStepper::getDefault();
   Clock *ModelClock       = DefStepper->getClock();

   // Initialize the IO system
   IO::init(DefComm);

   // Create the default decomposition (initializes the decomposition)
   Decomp::init();

   // Initialize Field infrastructure
   Field::init(ModelClock);

   // Initialize IOStreams - this does not yet validate the contents
   // of each file, only creates streams from Config
   IOStream::init(ModelClock);

   // Initialize the default halo
   Err1 = Halo::init();
   if (Err1 != 0) {
      LOG_ERROR("PGrad: error initializing default halo");
      Err += Error(ErrorCode::Fail, "PGrad: error initializing default halo");
   }

   // Initialize the default mesh
   HorzMesh::init(ModelClock);

   // Initialize the default vertical coordinate
   VertCoord::init();

   // Initialize the equation of state
   Eos::init();

   // Initialize ocean state
   OceanState::init();

   // Initialize tracers
   Tracers::init();

   CHECK_ERROR_ABORT(Err, "PGrad: error during initialization");
}

int main(int argc, char *argv[]) {
   int RetVal = 0;

   MPI_Init(&argc, &argv);
   Kokkos::initialize();
   Pacer::initialize(MPI_COMM_WORLD);
   Pacer::setPrefix("Omega:");

   {
      initPGradTest();
      // Initialize default PressureGrad
      PressureGrad::init();

      HorzMesh *DefMesh    = HorzMesh::getDefault();
      VertCoord *VCoord    = VertCoord::getDefault();
      OceanState *DefState = OceanState::getDefault();
      Eos *DefEos          = Eos::getInstance();

      // create arrays: Tend on edges, Pressure/Geopotential/SpecVol on cells
      Array2DReal Tend("Tend", DefMesh->NEdgesSize, VCoord->NVertLayers);
      Array2DReal SpecVolOld("SpecVolOld", DefMesh->NCellsSize,
                             VCoord->NVertLayers);
      Array2DReal PressureMidOld("PressureMidOld", DefMesh->NCellsSize,
                                 VCoord->NVertLayers);
      Array1DReal SurfacePressure("SurfacePressure", DefMesh->NCellsSize);

      I4 NEdgesAll = DefMesh->NEdgesAll;
      I4 NCellsAll = DefMesh->NCellsAll;

      I4 NVertLayers  = 60;
      Real DC         = 30000.0_Real;
      I4 NRefinements = 4;
      HostArray1DReal Rmse("Rmse", NRefinements);
      for (int Refinement = 0; Refinement < NRefinements; ++Refinement) {

         LOG_INFO("PGradTest: Starting refinement level {}", Refinement);
         VCoord->NVertLayers   = NVertLayers;
         VCoord->NVertLayersP1 = NVertLayers + 1;

         auto &MinLayerCell = VCoord->MinLayerCell;
         auto &MaxLayerCell = VCoord->MaxLayerCell;
         parallelFor(
             {NCellsAll}, KOKKOS_LAMBDA(int i) {
                MinLayerCell(i) = 0;
                MaxLayerCell(i) = NVertLayers - 1;
             });

         auto &MinLayerEdgeBot = VCoord->MinLayerEdgeBot;
         auto &MaxLayerEdgeTop = VCoord->MaxLayerEdgeTop;
         parallelFor(
             {NEdgesAll}, KOKKOS_LAMBDA(int i) {
                MinLayerEdgeBot(i) = 0;
                MaxLayerEdgeTop(i) = NVertLayers - 1;
             });

         auto &CellsOnEdge = DefMesh->CellsOnEdge;
         auto &DcEdge      = DefMesh->DcEdge;
         parallelFor(
             {NEdgesAll}, KOKKOS_LAMBDA(int i) {
                CellsOnEdge(i, 0) = 0;
                CellsOnEdge(i, 1) = 1;
                DcEdge(i)         = DC;
             });

         // Fetch reference desnity from Config
         Real Density0;
         Density0 = RhoSw;

         I4 TimeLevel = 0;

         // get state and tracer arrays
         Array2DReal PseudoThick = DefState->getPseudoThickness(TimeLevel);
         Array2DReal Temp        = Tracers::getByName(TimeLevel, "Temperature");
         Array2DReal Salinity    = Tracers::getByName(TimeLevel, "Salinity");

         // set Z interface and mid-point locations
         Real ZBottom          = -1000.0_Real;
         Real DZ               = 2.0_Real * (-ZBottom / NVertLayers);
         auto &BottomGeomDepth = VCoord->BottomGeomDepth;
         auto &GeomZInterface  = VCoord->GeomZInterface;
         auto &GeomZMid        = VCoord->GeomZMid;
         Real TiltFactor       = 0.495_Real;
         parallelFor(
             {NCellsAll}, KOKKOS_LAMBDA(int i) {
                GeomZInterface(i, NVertLayers) = ZBottom;
                SurfacePressure(i)             = 0.0_Real;
                BottomGeomDepth(i)             = 0.0_Real;
                for (int k = NVertLayers - 1; k >= 0; --k) {
                   Real X  = (k + i) % 2;
                   Real Dz = (2.0_Real * TiltFactor - 1.0_Real) * X * DZ +
                             (1.0_Real - TiltFactor) *
                                 DZ; // staggered pseudo-thickness
                   GeomZInterface(i, k) = GeomZInterface(i, k + 1) + Dz;
                   PseudoThick(i, k) =
                       GeomZInterface(i, k) - GeomZInterface(i, k + 1);
                   GeomZMid(i, k) = 0.5_Real * (GeomZInterface(i, k) +
                                                GeomZInterface(i, k + 1));
                   BottomGeomDepth(i) += Dz;
                }
             });

         LOG_INFO("NVertLayers = {}", NVertLayers);
         LOG_INFO("dC = {}", DC);
         DefState->copyToHost(0);
         HostArray2DReal PseudoThickH =
             DefState->getPseudoThicknessH(TimeLevel);
         for (int i = 0; i < 2; ++i) {
            for (int k = 0; k < 2; ++k) {
               LOG_INFO("PseudoThick({}, {}) = {}", i, k, PseudoThickH(i, k));
            }
         }

         // set simple temperature and salinity profiles
         auto &SpecVol = DefEos->SpecVol;
         parallelFor(
             {NCellsAll, NVertLayers}, KOKKOS_LAMBDA(int i, int k) {
                Real T0 = 30.0;
                Real TB = 5.0;
                Real S0 = 30.0;
                Real SB = 40.0;

                Real Phi0 = (GeomZMid(i, k) - ZBottom) / (-ZBottom);
                Real PhiB = 1.0_Real - Phi0;

                Temp(i, k)       = T0 * Phi0 + TB * PhiB;
                Salinity(i, k)   = S0 * Phi0 + SB * PhiB;
                SpecVol(i, k)    = 1.0_Real / Density0;
                SpecVolOld(i, k) = SpecVol(i, k);
             });

         // Iterate to converge PseudoThick, SpecVol, PressureMid
         auto &PressureMid = VCoord->PressureMid;
         VCoord->computePressure(PseudoThick, SurfacePressure);
         deepCopy(PressureMidOld, PressureMid);
         for (int Iteration = 0; Iteration < 15; ++Iteration) {

            // compute specific volume from EOS
            VCoord->computePressure(PseudoThick, SurfacePressure);
            DefEos->computeSpecVol(Temp, Salinity, PressureMid);

            // compute psuedo thickness from specific volume
            parallelFor(
                {NCellsAll, NVertLayers}, KOKKOS_LAMBDA(int i, int k) {
                   PseudoThick(i, k) =
                       1.0_Real / (SpecVol(i, k) * Density0) *
                       (GeomZInterface(i, k) - GeomZInterface(i, k + 1));
                });

            // compute difference from previous iteration
            Real MaxValue = 0.0_Real;
            parallelReduce(
                {NCellsAll, NVertLayers},
                KOKKOS_LAMBDA(int i, int k, Real &max) {
                   Real Diff = Kokkos::abs(SpecVol(i, k) - SpecVolOld(i, k));
                   if (Diff > max)
                      max = Diff;
                },
                Kokkos::Max<Real>(MaxValue));

            // check convergence
            if (MaxValue < 1e-12_Real) {
               LOG_INFO("converged: max diff = {}", MaxValue);
               break;
            } else {
               parallelFor(
                   {NCellsAll, NVertLayers}, KOKKOS_LAMBDA(int i, int k) {
                      SpecVolOld(i, k) = SpecVol(i, k);
                   });
            }
         }

         // compute pressure once more with converged PseudoThick
         VCoord->computePressure(PseudoThick, SurfacePressure);

         // compute z levels
         VCoord->computeGeomZHeight(PseudoThick, SpecVol);

         // get PressureGrad instance
         PressureGrad *DefPGrad = PressureGrad::getDefault();
         if (!DefPGrad) {
            LOG_INFO("PGrad: default instance not created by init");
         }

         // compute pressure gradient
         deepCopy(Tend, 0.0_Real);

         const auto &PressureInterface = VCoord->PressureInterface;
         const auto &SpecVolDThetaCons = DefEos->SpecVolDThetaCons;
         const auto &SpecVolDSalt      = DefEos->SpecVolDSalt;
         const auto &SpecVolDPressure  = DefEos->SpecVolDPressure;
         DefPGrad->computePressureGrad(Tend, PressureMid, PressureInterface,
                                       SpecVol, GeomZInterface, PseudoThick,
                                       Temp, Salinity, SpecVolDThetaCons,
                                       SpecVolDSalt, SpecVolDPressure);

         // compute errors
         Real MaxValue = 0.0_Real;
         parallelReduce(
             {NEdgesAll, NVertLayers - 2},
             KOKKOS_LAMBDA(int i, int k, Real &max) {
                Real Val = Kokkos::abs(Tend(i, k + 1));
                if (Val > max)
                   max = Val;
             },
             Kokkos::Max<Real>(MaxValue));
         Real SumValue = 0.0_Real;
         parallelReduce(
             {NEdgesAll, NVertLayers - 2},
             KOKKOS_LAMBDA(int i, int k, Real &LSum) {
                LSum += Tend(i, k + 1) * Tend(i, k + 1);
             },
             Kokkos::Sum<Real>(SumValue));
         Real RmseVal = std::sqrt(SumValue / (NEdgesAll * (NVertLayers - 2)));
         Rmse(Refinement) = RmseVal;

         LOG_INFO("refinement level {}: max |Tend| = {}, average Tend = {}",
                  Refinement, MaxValue, RmseVal);

         // coarsen for next iteration
         DC          = DC * 2.0_Real;
         NVertLayers = NVertLayers / 2;

      } // refinement loop

      // Test for second order convergence
      // resolution (dC) increases in refimenent loop
      if (Rmse(0) < Rmse(NRefinements - 1) / pow(4.0_Real, NRefinements - 1)) {
         RetVal = 0;
      } else {
         RetVal = 1;
      }

      // ---- Finite-volume scheme tests ----
      // (1) Discrete hydrostatic consistency: a resting column whose specific
      //     volume is the same (linear-in-pressure) function of pressure in
      //     both columns must produce zero pressure gradient to round-off, even
      //     with arbitrarily tilted layers.
      // (2) With the compressibility term alpha_p set to zero, the
      // finite-volume
      //     operator must reproduce the centered scheme to round-off.
      {
         PressureGrad *DefPGrad = PressureGrad::getDefault();
         const I4 NK            = 16;
         VCoord->NVertLayers    = NK;
         VCoord->NVertLayersP1  = NK + 1;
         I4 NEdgesSize          = DefMesh->NEdgesSize;

         auto &MinLayerCell    = VCoord->MinLayerCell;
         auto &MaxLayerCell    = VCoord->MaxLayerCell;
         auto &MinLayerEdgeBot = VCoord->MinLayerEdgeBot;
         auto &MaxLayerEdgeTop = VCoord->MaxLayerEdgeTop;
         auto &EdgeMask        = VCoord->EdgeMask;
         auto &CellsOnEdge     = DefMesh->CellsOnEdge;
         auto &DcEdge          = DefMesh->DcEdge;
         Real DCL              = 30000.0_Real;
         parallelFor(
             {NCellsAll}, KOKKOS_LAMBDA(int i) {
                MinLayerCell(i) = 0;
                MaxLayerCell(i) = NK - 1;
             });
         parallelFor(
             {NEdgesAll}, KOKKOS_LAMBDA(int i) {
                MinLayerEdgeBot(i) = 0;
                MaxLayerEdgeTop(i) = NK - 1;
                CellsOnEdge(i, 0)  = 0;
                CellsOnEdge(i, 1)  = 1;
                DcEdge(i)          = DCL;
             });
         parallelFor(
             {NEdgesAll, NK},
             KOKKOS_LAMBDA(int i, int k) { EdgeMask(i, k) = 1.0_Real; });

         // resting, linear-in-pressure specific volume (isothermal): the same
         // function of pressure in both columns; column 1 layers are tilted
         // relative to column 0.
         const Real AA0   = 9.7e-4_Real;
         const Real AA1   = -4.0e-13_Real;
         const Real APref = 1.0e7_Real;
         const Real Psurf = 1.0e5_Real;
         auto &PInterface = VCoord->PressureInterface;
         auto &PMid       = VCoord->PressureMid;
         auto &GeomZIface = VCoord->GeomZInterface;
         auto &SpecVolL   = DefEos->SpecVol;
         auto &SpecVolDP  = DefEos->SpecVolDPressure;
         auto &SpecVolDT  = DefEos->SpecVolDThetaCons;
         auto &SpecVolDS  = DefEos->SpecVolDSalt;
         parallelFor(
             {NCellsAll}, KOKKOS_LAMBDA(int i) {
                Real Pres        = Psurf;
                Real Gz          = 0.0_Real;
                PInterface(i, 0) = Pres;
                GeomZIface(i, 0) = Gz;
                for (int k = 0; k < NK; ++k) {
                   Real Tilt =
                       (i == 1) ? (40.0_Real * ((k % 2) ? 1.0_Real : -1.0_Real))
                                : 0.0_Real;
                   Real Htil       = 100.0_Real + Tilt;
                   Real Dp         = RhoSw * Gravity * Htil;
                   Real Pmid       = Pres + 0.5_Real * Dp;
                   Real Alpha0     = AA0 + AA1 * (Pmid - APref);
                   PMid(i, k)      = Pmid;
                   SpecVolL(i, k)  = Alpha0;
                   SpecVolDP(i, k) = AA1;
                   SpecVolDT(i, k) = 0.0_Real;
                   SpecVolDS(i, k) = 0.0_Real;
                   Gz -= Alpha0 * Dp / Gravity;
                   Pres += Dp;
                   PInterface(i, k + 1) = Pres;
                   GeomZIface(i, k + 1) = Gz;
                }
             });

         // build a FiniteVolume PressureGrad instance in-memory
         Config FVOpts("Omega");
         Config FVGroup("PressureGrad");
         FVGroup.add("PressureGradType", std::string("FiniteVolume"));
         FVOpts.add(FVGroup);
         PressureGrad *FVPGrad =
             PressureGrad::create("FVTest", DefMesh, VCoord, &FVOpts);

         Array2DReal PseudoThickL = DefState->getPseudoThickness(0);

         // (1) full finite-volume (with alpha_p) should be ~zero
         Array2DReal TendFV("TendFV", NEdgesSize, NK);
         deepCopy(TendFV, 0.0_Real);
         FVPGrad->computePressureGrad(
             TendFV, PMid, PInterface, SpecVolL, GeomZIface, PseudoThickL,
             SpecVolL, SpecVolL, SpecVolDT, SpecVolDS, SpecVolDP);
         Real MaxFV = 0.0_Real;
         parallelReduce(
             {NEdgesAll, NK},
             KOKKOS_LAMBDA(int i, int k, Real &m) {
                Real v = Kokkos::abs(TendFV(i, k));
                if (v > m)
                   m = v;
             },
             Kokkos::Max<Real>(MaxFV));
         LOG_INFO("PGradTest: FV hydrostatic consistency max|Tend| = {}",
                  MaxFV);
         // Threshold tracks Real's epsilon (the resting state cancels to round-
         // off in double precision; single-precision builds expose a higher
         // floor, so the check is meaningful mainly in double precision).
         const Real ConsistencyTol =
             1.0e4_Real * std::numeric_limits<Real>::epsilon();
         if (MaxFV > ConsistencyTol) {
            LOG_ERROR("PGradTest: FV hydrostatic consistency FAIL ({})", MaxFV);
            RetVal = 1;
         }

         // (2) with alpha_p = 0 the finite-volume operator must match centered
         deepCopy(SpecVolDP, 0.0_Real);
         Array2DReal TendFV0("TendFV0", NEdgesSize, NK);
         Array2DReal TendC("TendC", NEdgesSize, NK);
         deepCopy(TendFV0, 0.0_Real);
         deepCopy(TendC, 0.0_Real);
         FVPGrad->computePressureGrad(
             TendFV0, PMid, PInterface, SpecVolL, GeomZIface, PseudoThickL,
             SpecVolL, SpecVolL, SpecVolDT, SpecVolDS, SpecVolDP);
         DefPGrad->computePressureGrad(
             TendC, PMid, PInterface, SpecVolL, GeomZIface, PseudoThickL,
             SpecVolL, SpecVolL, SpecVolDT, SpecVolDS, SpecVolDP);
         Real MaxDiff = 0.0_Real;
         parallelReduce(
             {NEdgesAll, NK},
             KOKKOS_LAMBDA(int i, int k, Real &m) {
                Real v = Kokkos::abs(TendFV0(i, k) - TendC(i, k));
                if (v > m)
                   m = v;
             },
             Kokkos::Max<Real>(MaxDiff));
         LOG_INFO("PGradTest: FV(alpha_p=0) vs centered max|diff| = {}",
                  MaxDiff);
         const Real ReductionTol =
             1.0e6_Real * std::numeric_limits<Real>::epsilon();
         if (MaxDiff > ReductionTol) {
            LOG_ERROR("PGradTest: FV reduction-to-centered FAIL ({})", MaxDiff);
            RetVal = 1;
         }

         PressureGrad::erase("FVTest");
      }

      // cleanup
      PressureGrad::clear();
      IOStream::finalize();
      TimeStepper::clear();
      Tracers::clear();
      Eos::destroyInstance();
      OceanState::clear();
      VertCoord::clear();
      Field::clear();
      Dimension::clear();
      HorzMesh::clear();
      Halo::clear();
      Decomp::clear();
      MachEnv::removeAll();
   }
   Pacer::finalize();
   Kokkos::finalize();
   MPI_Finalize();

   if (RetVal >= 256)
      RetVal = 255;
   return RetVal;
}
