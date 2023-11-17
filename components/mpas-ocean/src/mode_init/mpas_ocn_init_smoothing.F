! Copyright (c) 2013,  Los Alamos National Security, LLC (LANS)
! and the University Corporation for Atmospheric Research (UCAR).
!
! Unless noted otherwise source code is licensed under the BSD license.
! Additional copyright and license information can be found in the LICENSE file
! distributed with this code, or at http://mpas-dev.github.io/license.html
!
!|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
!
!  ocn_init_smoothing
!
!> \brief MPAS ocean horizontal smoothing
!> \author Xylar Asay-Davis
!> \date   02/02/2020
!> \details
!>  This module contains the routines for horizontally smoothing 2D fields.
!
!-----------------------------------------------------------------------
module ocn_init_smoothing

   use mpas_kind_types
   use mpas_derived_types
   use mpas_pool_routines
   use mpas_dmpar
   use mpas_field_routines

   implicit none
   private
   save

   !--------------------------------------------------------------------
   !
   ! Public parameters
   !
   !--------------------------------------------------------------------

   !--------------------------------------------------------------------
   !
   ! Public member functions
   !
   !--------------------------------------------------------------------

   public :: ocn_init_smooth_field

   !--------------------------------------------------------------------
   !
   ! Private module variables
   !
   !--------------------------------------------------------------------

!***********************************************************************

contains


   !***********************************************************************
   !
   !  routine ocn_init_set_smoothing_weights
   !
   !> \brief  Set the weights used for smoothing based on a depth threshold
   !> \author Xylar Asay-Davis
   !> \date   11/17/2023
   !> \details
   !>  This routine set the weights for each cell to be used during smoothing
   !>  based on the weight itself and a threshold for the depth of the
   !>  topography above which no smoothing is performed
   !
   !-----------------------------------------------------------------------
   subroutine ocn_init_set_smoothing_weights(domain, weight, min_depth)!{{{

       type (domain_type), intent(inout) :: domain
       real (kind=RKIND), intent(in) :: weight, min_depth

       type (block_type), pointer :: block_ptr

       type (mpas_pool_type), pointer :: meshPool

       real (kind=RKIND), dimension(:), pointer :: initSmoothingWeights, bottomDepthObserved

       integer, pointer :: nCells

       integer :: iCell

      block_ptr => domain % blocklist
      do while(associated(block_ptr))
         call mpas_pool_get_subpool(block_ptr % structs, 'mesh', meshPool)

         call mpas_pool_get_dimension(meshPool, 'nCells', nCells)

         call mpas_pool_get_array(meshPool, 'initSmoothingWeights', initSmoothingWeights)
         call mpas_pool_get_array(meshPool, 'bottomDepthObserved', bottomDepthObserved)

         do iCell = 1, nCells
            if (bottomDepthObserved(iCell) < minDepth) then
               initSmoothingWeights(iCell) = weight
            else
               initSmoothingWeights(iCell) = 1.0_RKIND
            end if
         end do
      end do

   end subroutine ocn_init_set_smoothing_weights!}}}


   !***********************************************************************
   !
   !  routine ocn_init_smooth_field
   !
   !> \brief  Horizontally smooth a field
   !> \author Xylar Asay-Davis
   !> \date   11/17/2023
   !> \details
   !>  This routine smooths the given field on cells from the given pool by
   !>  iteratively perform a nearest-neighbor weighted average.
   !
   !-----------------------------------------------------------------------
   subroutine ocn_init_smooth_field(domain, fieldName, fieldPoolName, iterations)!{{{

       type (domain_type), intent(inout) :: domain
       character (len=*), intent(in) :: fieldName, fieldPoolName
       integer, intent(in) :: iterations

       type (block_type), pointer :: block_ptr

       type (mpas_pool_type), pointer :: meshPool, scratchPool, fieldPool

       real (kind=RKIND), dimension(:), pointer :: array, smoothedArray, initSmoothingWeights

       integer, pointer :: nCells, nCellsSolve

       type (field1DReal), pointer :: field, smoothedField
       integer, dimension(:), pointer :: nEdgesOnCell
       integer, dimension(:, :), pointer :: cellsOnCell

       integer :: iCell, coc, j, iIter, n

       real (kind=RKIND) :: weight, coc_weight

       do iIter = 1, iterations
          block_ptr => domain % blocklist
          do while(associated(block_ptr))
             call mpas_pool_get_subpool(block_ptr % structs, 'mesh', meshPool)
             call mpas_pool_get_subpool(block_ptr % structs, 'scratch', scratchPool)
             call mpas_pool_get_subpool(block_ptr % structs, fieldPoolName, fieldPool)

             call mpas_pool_get_dimension(meshPool, 'nCells', nCells)
             call mpas_pool_get_dimension(meshPool, 'nCellsSolve', nCellsSolve)
             call mpas_pool_get_array(meshPool, 'nEdgesOnCell', nEdgesOnCell)
             call mpas_pool_get_array(meshPool, 'cellsOnCell', cellsOnCell)

             call mpas_pool_get_field(scratchPool, 'smoothedField', smoothedField)
             call mpas_allocate_scratch_field(smoothedField, .true.)

             call mpas_pool_get_array(meshPool, 'initSmoothingWeights', initSmoothingWeights)
             call mpas_pool_get_array(fieldPool, fieldName, array)
             call mpas_pool_get_array(scratchPool, 'smoothedField', smoothedArray)

             do iCell = 1, nCellsSolve
                smoothedArray(iCell) = 0.0_RKIND
                n = 0
                do j = 1, nEdgesOnCell(iCell)
                   coc = cellsOnCell(j, iCell)
                   if (coc < nCells+1) then
                      smoothedArray(iCell) = smoothedArray(iCell) + array(coc)
                      n = n + 1
                   end if
                end do
                weight = initSmoothingWeights(iCell)
                coc_weight = (1.0_RKIND - weight)/n
                smoothedArray(iCell) = coc_weight * smoothedArray(iCell) + weight * array(iCell)
             end do


             do iCell = 1, nCellsSolve
                array(iCell) = smoothedArray(iCell)
             end do

             call mpas_deallocate_scratch_field(smoothedField, .true.)
             block_ptr => block_ptr % next
          end do

          ! do a halo exchange
          call mpas_pool_get_subpool(domain % blocklist % structs, fieldPoolName, fieldPool)
          call mpas_pool_get_field(fieldPool, fieldName, field)
          call mpas_dmpar_exch_halo_field(field)

      end do

   end subroutine ocn_init_smooth_field!}}}

!***********************************************************************

end module ocn_init_smoothing


!|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
! vim: foldmethod=marker et ts=3 tw=132
