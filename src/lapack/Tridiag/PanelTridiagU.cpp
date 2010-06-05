/*
   This file is part of elemental, a library for distributed-memory dense 
   linear algebra.

   Copyright (C) 2009-2010 Jack Poulson <jack.poulson@gmail.com>

   This program is released under the terms of the license contained in the 
   file LICENSE.
*/
#include "elemental/blas_internal.hpp"
#include "elemental/lapack_internal.hpp"
using namespace std;
using namespace elemental;
using namespace elemental::wrappers::mpi;

template<typename R>
void
elemental::lapack::internal::PanelTridiagU
( DistMatrix<R,MC,  MR  >& A,
  DistMatrix<R,MC,  MR  >& W,
  DistMatrix<R,MD,  Star>& e,
  DistMatrix<R,MD,  Star>& t )
{
#ifndef RELEASE
    PushCallStack("lapack::internal::PanelTridiagU");
    if( A.GetGrid() != W.GetGrid() ||
        W.GetGrid() != e.GetGrid() ||
        e.GetGrid() != t.GetGrid() )
        throw "A, W, e, and t must be distributed over the same grid.";
    if( A.Height() != A.Width() )
        throw "A must be square.";
    if( A.Width() != W.Width() )
        throw "A and W must be the same width.";
    if( W.Height() >= W.Width() )
        throw "W must be a row panel.";
    if( W.ColAlignment() != A.ColAlignment() || 
        W.RowAlignment() != A.RowAlignment()   )
        throw "W and A must be aligned.";
    if( e.Height() != W.Height() || e.Width() != 1 )
        throw "e must be a column vector of the same length as W's height.";
    if( t.Height() != W.Height() || t.Width() != 1 )
        throw "t must be a column vector of the same length as W's height.";
    if( e.ColAlignment() != A.ColAlignment() + 
        ((A.RowAlignment()+1) % e.GetGrid().Width())*e.GetGrid().Height() )
        throw "e ist not aligned with A.";
    if( t.ColAlignment() != (A.ColAlignment()+
                             A.RowAlignment()*t.GetGrid().Height()) )
        throw "t is not aligned with A.";
#endif
    const Grid& grid = A.GetGrid();
    const int myRow  = grid.MCRank();
    const int myRank = grid.VCRank();

    // Matrix views 
    DistMatrix<R,MC,MR> 
        ATL(grid), ATR(grid),  A00(grid), a01(grid),     A02(grid),  ARow(grid),
        ABL(grid), ABR(grid),  a10(grid), alpha11(grid), a12(grid),
                               A20(grid), a21(grid),     A22(grid);
    DistMatrix<R,MC,MR> 
        WTL(grid), WTR(grid),  W00(grid), w01(grid),     W02(grid),  WRow(grid),
        WBL(grid), WBR(grid),  w10(grid), omega11(grid), w12(grid),
                               W20(grid), w21(grid),     W22(grid);
    DistMatrix<R,MD,Star> eT(grid),  e0(grid),
                          eB(grid),  epsilon1(grid),
                                     e2(grid);
    DistMatrix<R,MD,Star> tT(grid),  t0(grid),
                          tB(grid),  tau1(grid),
                                     t2(grid);

    // Temporary distributions
    DistMatrix<R,Star,MC  > a12_Star_MC(grid);
    DistMatrix<R,Star,MR  > a12_Star_MR(grid);
    DistMatrix<R,MC,  Star> z01_MC_Star(grid);
    DistMatrix<R,MC,  MR  > z12(grid);
    DistMatrix<R,Star,MC  > z12_Star_MC(grid);
    DistMatrix<R,Star,MR  > z12_Star_MR(grid);
    DistMatrix<R,MR,  MC  > z12_MR_MC(grid);

    // Push to the blocksize of 1, then pop at the end of the routine
    PushBlocksizeStack( 1 );

    PartitionDownDiagonal
    ( A, ATL, ATR,
         ABL, ABR );
    PartitionDownDiagonal
    ( W, WTL, WTR,
         WBL, WBR );
    PartitionDown
    ( e, eT,
         eB );
    PartitionDown
    ( t, tT,
         tB );
    while( WTL.Height() < W.Height() )
    {
        RepartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, /**/ a01,     A02,
         /*************/ /**********************/
               /**/       a10, /**/ alpha11, a12, 
          ABL, /**/ ABR,  A20, /**/ a21,     A22 );
        
        RepartitionDownDiagonal
        ( WTL, /**/ WTR,  W00, /**/ w01,     W02,
         /*************/ /**********************/
               /**/       w10, /**/ omega11, w12,
          WBL, /**/ WBR,  W20, /**/ w21,     W22 );

        RepartitionDown
        ( eT,  e0,
         /**/ /********/
               epsilon1,
          eB,  e2 );

        RepartitionDown
        ( tT,  t0,
         /**/ /****/
               tau1,
          tB,  t2 );

        ARow.View1x2( alpha11, a12 );

        WRow.View1x2( omega11, w12 );

        a12_Star_MC.AlignWith( A22 );
        a12_Star_MR.AlignWith( A22 );
        z01_MC_Star.AlignWith( W02 );
        z12.AlignWith( w12 );
        z12_Star_MC.AlignWith( A22 );
        z12_Star_MR.AlignWith( A22 );
        z12_MR_MC.AlignRowsWith( A22 );
        z12_Star_MC.ResizeTo( 1, w12.Width() );
        z12_Star_MR.ResizeTo( 1, w12.Width() );
        z01_MC_Star.ResizeTo( w01.Height(), 1 );
        z12_Star_MC.SetToZero();
        z12_Star_MR.SetToZero();
        //--------------------------------------------------------------------//
        blas::Gemv( ConjugateTranspose, (R)-1, ATR, w01, (R)1, ARow );
        blas::Gemv( ConjugateTranspose, (R)-1, WTR, a01, (R)1, ARow );

        R tau = 0; // Initializing avoids false compiler warnings
        const bool thisIsMyRow = ( myRow == a12.ColAlignment() );
        if( thisIsMyRow )
        {
            tau = lapack::internal::LocalRowReflector( a12 );
            if( myRank == tau1.ColAlignment() )
                tau1.LocalEntry(0,0) = tau;
        }
            
        a12.GetDiagonal( epsilon1 );
        a12.Set( 0, 0, (R)1 ); 

        a12_Star_MC = a12_Star_MR = a12;

        PopBlocksizeStack();
        blas::internal::HemvRowAccumulate
        ( Upper, (R)1, A22, a12_Star_MC, a12_Star_MR, 
                            z12_Star_MC, z12_Star_MR );
        PushBlocksizeStack( 1 );

        blas::Gemv
        ( Normal, 
          (R)1, W02.LockedLocalMatrix(),
                a12_Star_MR.LockedLocalMatrix(),
          (R)0, z01_MC_Star.LocalMatrix() );
        z01_MC_Star.SumOverRow();

        blas::Gemv
        ( ConjugateTranspose, 
          (R)-1, A02.LockedLocalMatrix(),
                 z01_MC_Star.LockedLocalMatrix(),
          (R)+1, z12_Star_MR.LocalMatrix() );

        blas::Gemv
        ( Normal, 
          (R)1, A02.LockedLocalMatrix(),
                a12_Star_MR.LockedLocalMatrix(),
          (R)0, z01_MC_Star.LocalMatrix() );
        z01_MC_Star.SumOverRow();

        blas::Gemv
        ( ConjugateTranspose, 
          (R)-1, W02.LockedLocalMatrix(),
                 z01_MC_Star.LockedLocalMatrix(),
          (R)+1, z12_Star_MR.LocalMatrix() );

        w12.SumScatterFrom( z12_Star_MR );
        z12_MR_MC.SumScatterFrom( z12_Star_MC );
        z12 = z12_MR_MC;

        if( thisIsMyRow )
        {
            blas::Axpy( (R)1, z12, w12 );
            blas::Scal( tau, w12 );

            R alpha;
            R myAlpha = -static_cast<R>(0.5)*tau*
                        blas::Dot( w12.LockedLocalMatrix(),
                                   a12.LockedLocalMatrix() );
            AllReduce( &myAlpha, &alpha, 1, MPI_SUM, grid.MRComm() );
            blas::Axpy( alpha, a12, w12 );
        }
        //--------------------------------------------------------------------//
        a12_Star_MC.FreeAlignments();
        a12_Star_MR.FreeAlignments();
        z01_MC_Star.FreeAlignments();
        z12.FreeAlignments();
        z12_Star_MC.FreeAlignments();
        z12_Star_MR.FreeAlignments();
        z12_MR_MC.FreeAlignments();

        SlidePartitionDown
        ( eT,  e0,
               epsilon1,
         /**/ /********/
          eB,  e2 );

        SlidePartitionDown
        ( tT,  t0,
               tau1,
         /**/ /****/
          tB,  t2 );

        SlidePartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, a01,     /**/ A02,
               /**/       a10, alpha11, /**/ a12,
         /*************/ /**********************/
          ABL, /**/ ABR,  A20, a21,     /**/ A22 );

        SlidePartitionDownDiagonal
        ( WTL, /**/ WTR,  W00, w01,     /**/ W02,
               /**/       w10, omega11, /**/ w12,
         /*************/ /**********************/
          WBL, /**/ WBR,  W20, w21,     /**/ W22 );
    }

    PopBlocksizeStack();

#ifndef RELEASE
    PopCallStack();
#endif
}

#ifndef WITHOUT_COMPLEX
template<typename R>
void
elemental::lapack::internal::PanelTridiagU
( DistMatrix<complex<R>,MC,  MR  >& A,
  DistMatrix<complex<R>,MC,  MR  >& W,
  DistMatrix<R,MD,Star>& e,
  DistMatrix<complex<R>,MD,  Star>& t )
{
#ifndef RELEASE
    PushCallStack("lapack::internal::PanelTridiagU");
    if( A.GetGrid() != W.GetGrid() ||
        W.GetGrid() != e.GetGrid() ||
        e.GetGrid() != t.GetGrid() )
        throw "A, W, e, and t must be distributed over the same grid.";
    if( A.Height() != A.Width() )
        throw "A must be square.";
    if( A.Width() != W.Width() )
        throw "A and W must be the same width.";
    if( W.Height() >= W.Width() )
        throw "W must be a row panel.";
    if( W.ColAlignment() != A.ColAlignment() || 
        W.RowAlignment() != A.RowAlignment()   )
        throw "W and A must be aligned.";
    if( e.Height() != W.Height() || e.Width() != 1 )
        throw "e must be a column vector of the same length as W's height.";
    if( t.Height() != W.Height() || t.Width() != 1 )
        throw "t must be a column vector of the same length as W's height.";
    if( e.ColAlignment() != A.ColAlignment() + 
        ((A.RowAlignment()+1) % e.GetGrid().Width())*e.GetGrid().Height() )
        throw "e ist not aligned with A.";
    if( t.ColAlignment() != (A.ColAlignment()+
                             A.RowAlignment()*t.GetGrid().Height()) )
        throw "t is not aligned with A.";
#endif
    typedef complex<R> C;

    const Grid& grid = A.GetGrid();
    const int myRow  = grid.MCRank();
    const int myRank = grid.VCRank();

    // Matrix views 
    DistMatrix<C,MC,MR> 
        ATL(grid), ATR(grid),  A00(grid), a01(grid),     A02(grid),  ARow(grid),
        ABL(grid), ABR(grid),  a10(grid), alpha11(grid), a12(grid),
                               A20(grid), a21(grid),     A22(grid);
    DistMatrix<C,MC,MR> 
        WTL(grid), WTR(grid),  W00(grid), w01(grid),     W02(grid),  WRow(grid),
        WBL(grid), WBR(grid),  w10(grid), omega11(grid), w12(grid),
                               W20(grid), w21(grid),     W22(grid);
    DistMatrix<R,MD,Star> eT(grid),  e0(grid),
                          eB(grid),  epsilon1(grid),
                                     e2(grid);
    DistMatrix<C,MD,Star> tT(grid),  t0(grid),
                          tB(grid),  tau1(grid),
                                     t2(grid);

    // Temporary distributions
    DistMatrix<C,Star,MC  > a12_Star_MC(grid);
    DistMatrix<C,Star,MR  > a12_Star_MR(grid);
    DistMatrix<C,MC,  Star> z01_MC_Star(grid);
    DistMatrix<C,MC,  MR  > z12(grid);
    DistMatrix<C,Star,MC  > z12_Star_MC(grid);
    DistMatrix<C,Star,MR  > z12_Star_MR(grid);
    DistMatrix<C,MR,  MC  > z12_MR_MC(grid);

    // Push to the blocksize of 1, then pop at the end of the routine
    PushBlocksizeStack( 1 );

    PartitionDownDiagonal
    ( A, ATL, ATR,
         ABL, ABR );
    PartitionDownDiagonal
    ( W, WTL, WTR,
         WBL, WBR );
    PartitionDown
    ( e, eT,
         eB );
    PartitionDown
    ( t, tT,
         tB );
    while( WTL.Height() < W.Height() )
    {
        RepartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, /**/ a01,     A02,
         /*************/ /**********************/
               /**/       a10, /**/ alpha11, a12, 
          ABL, /**/ ABR,  A20, /**/ a21,     A22 );
        
        RepartitionDownDiagonal
        ( WTL, /**/ WTR,  W00, /**/ w01,     W02,
         /*************/ /**********************/
               /**/       w10, /**/ omega11, w12,
          WBL, /**/ WBR,  W20, /**/ w21,     W22 );

        RepartitionDown
        ( eT,  e0,
         /**/ /********/
               epsilon1,
          eB,  e2 );

        RepartitionDown
        ( tT,  t0,
         /**/ /****/
               tau1,
          tB,  t2 );

        ARow.View1x2( alpha11, a12 );

        WRow.View1x2( omega11, w12 );

        a12_Star_MC.AlignWith( A22 );
        a12_Star_MR.AlignWith( A22 );
        z01_MC_Star.AlignWith( W02 );
        z12.AlignWith( w12 );
        z12_Star_MC.AlignWith( A22 );
        z12_Star_MR.AlignWith( A22 );
        z12_MR_MC.AlignRowsWith( A22 );
        z12_Star_MC.ResizeTo( 1, w12.Width() );
        z12_Star_MR.ResizeTo( 1, w12.Width() );
        z01_MC_Star.ResizeTo( w01.Height(), 1 );
        z12_Star_MC.SetToZero();
        z12_Star_MR.SetToZero();
        //--------------------------------------------------------------------//
        blas::Gemv( ConjugateTranspose, (C)-1, ATR, w01, (C)1, ARow );
        blas::Gemv( ConjugateTranspose, (C)-1, WTR, a01, (C)1, ARow );

        C tau = 0; // Initializing avoids false compiler warnings
        const bool thisIsMyRow = ( myRow == a12.ColAlignment() );
        if( thisIsMyRow )
        {
            tau = lapack::internal::LocalRowReflector( a12 );
            if( myRank == tau1.ColAlignment() )
                tau1.LocalEntry(0,0) = tau;
        }
            
        a12.GetRealDiagonal( epsilon1 );
        a12.Set( 0, 0, (C)1 ); 

        a12_Star_MC = a12_Star_MR = a12;

        PopBlocksizeStack();
        blas::internal::HemvRowAccumulate
        ( Upper, (C)1, A22, a12_Star_MC, a12_Star_MR, 
                            z12_Star_MC, z12_Star_MR );
        PushBlocksizeStack( 1 );

        blas::Gemv
        ( Normal, 
          (C)1, W02.LockedLocalMatrix(),
                a12_Star_MR.LockedLocalMatrix(),
          (C)0, z01_MC_Star.LocalMatrix() );
        z01_MC_Star.SumOverRow();

        blas::Gemv
        ( ConjugateTranspose, 
          (C)-1, A02.LockedLocalMatrix(),
                 z01_MC_Star.LockedLocalMatrix(),
          (C)+1, z12_Star_MR.LocalMatrix() );

        blas::Gemv
        ( Normal, 
          (C)1, A02.LockedLocalMatrix(),
                a12_Star_MR.LockedLocalMatrix(),
          (C)0, z01_MC_Star.LocalMatrix() );
        z01_MC_Star.SumOverRow();

        blas::Gemv
        ( ConjugateTranspose, 
          (C)-1, W02.LockedLocalMatrix(),
                 z01_MC_Star.LockedLocalMatrix(),
          (C)+1, z12_Star_MR.LocalMatrix() );

        w12.SumScatterFrom( z12_Star_MR );
        z12_MR_MC.SumScatterFrom( z12_Star_MC );
        z12 = z12_MR_MC;

        if( thisIsMyRow )
        {
            blas::Axpy( (C)1, z12, w12 );
            blas::Scal( tau, w12 );

            C alpha;
            C myAlpha = -static_cast<C>(0.5)*tau*
                        blas::Dot( w12.LockedLocalMatrix(),
                                   a12.LockedLocalMatrix() );
            AllReduce( &myAlpha, &alpha, 1, MPI_SUM, grid.MRComm() );
            blas::Axpy( alpha, a12, w12 );
        }
        //--------------------------------------------------------------------//
        a12_Star_MC.FreeAlignments();
        a12_Star_MR.FreeAlignments();
        z01_MC_Star.FreeAlignments();
        z12.FreeAlignments();
        z12_Star_MC.FreeAlignments();
        z12_Star_MR.FreeAlignments();
        z12_MR_MC.FreeAlignments();

        SlidePartitionDown
        ( eT,  e0,
               epsilon1,
         /**/ /********/
          eB,  e2 );

        SlidePartitionDown
        ( tT,  t0,
               tau1,
         /**/ /****/
          tB,  t2 );

        SlidePartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, a01,     /**/ A02,
               /**/       a10, alpha11, /**/ a12,
         /*************/ /**********************/
          ABL, /**/ ABR,  A20, a21,     /**/ A22 );

        SlidePartitionDownDiagonal
        ( WTL, /**/ WTR,  W00, w01,     /**/ W02,
               /**/       w10, omega11, /**/ w12,
         /*************/ /**********************/
          WBL, /**/ WBR,  W20, w21,     /**/ W22 );
    }

    PopBlocksizeStack();

#ifndef RELEASE
    PopCallStack();
#endif
}
#endif // WITHOUT_COMPLEX

template void elemental::lapack::internal::PanelTridiagU
( DistMatrix<float,MC,MR  >& A,
  DistMatrix<float,MC,MR  >& W,
  DistMatrix<float,MD,Star>& e,
  DistMatrix<float,MD,Star>& t );

template void elemental::lapack::internal::PanelTridiagU
( DistMatrix<double,MC,MR  >& A,
  DistMatrix<double,MC,MR  >& W,
  DistMatrix<double,MD,Star>& e,
  DistMatrix<double,MD,Star>& t );

#ifndef WITHOUT_COMPLEX
template void elemental::lapack::internal::PanelTridiagU
( DistMatrix<scomplex,MC,MR  >& A,
  DistMatrix<scomplex,MC,MR  >& W,
  DistMatrix<float,   MD,Star>& e,
  DistMatrix<scomplex,MD,Star>& t );

template void elemental::lapack::internal::PanelTridiagU
( DistMatrix<dcomplex,MC,MR  >& A,
  DistMatrix<dcomplex,MC,MR  >& W,
  DistMatrix<double,  MD,Star>& e,
  DistMatrix<dcomplex,MD,Star>& t );
#endif
