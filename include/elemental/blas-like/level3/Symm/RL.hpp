/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/

namespace elem {
namespace internal {

template<typename T>
inline void
SymmRLA
( T alpha, const DistMatrix<T>& A, const DistMatrix<T>& B,
  T beta,        DistMatrix<T>& C )
{
#ifndef RELEASE
    PushCallStack("internal::SymmRLA");
    if( A.Grid() != B.Grid() || B.Grid() != C.Grid() )
        throw std::logic_error
        ("{A,B,C} must be distributed over the same grid");
#endif
    const Grid& g = A.Grid();

    DistMatrix<T>
        BT(g),  B0(g),
        BB(g),  B1(g),
                B2(g);
    DistMatrix<T>
        CT(g),  C0(g),
        CB(g),  C1(g),
                C2(g);

    DistMatrix<T,MR,  STAR> B1Trans_MR_STAR(g);
    DistMatrix<T,VC,  STAR> B1Trans_VC_STAR(g);
    DistMatrix<T,STAR,MC  > B1_STAR_MC(g);
    DistMatrix<T,MC,  STAR> Z1Trans_MC_STAR(g);
    DistMatrix<T,MR,  STAR> Z1Trans_MR_STAR(g);
    DistMatrix<T,MC,  MR  > Z1Trans(g);
    DistMatrix<T,MR,  MC  > Z1Trans_MR_MC(g);

    B1Trans_MR_STAR.AlignWith( A );
    B1Trans_VC_STAR.AlignWith( A );
    B1_STAR_MC.AlignWith( A );
    Z1Trans_MC_STAR.AlignWith( A );
    Z1Trans_MR_STAR.AlignWith( A );

    Matrix<T> Z1Local;

    Scale( beta, C );
    LockedPartitionDown
    ( B, BT,
         BB, 0 );
    PartitionDown
    ( C, CT,
         CB, 0 );
    while( BT.Height() < B.Height() )
    {
        LockedRepartitionDown
        ( BT,  B0,
         /**/ /**/
               B1,
          BB,  B2 );

        RepartitionDown
        ( CT,  C0,
         /**/ /**/
               C1,
          CB,  C2 );

        Z1Trans_MR_MC.AlignWith( C1 );
        Zeros( C1.Width(), C1.Height(), Z1Trans_MC_STAR );
        Zeros( C1.Width(), C1.Height(), Z1Trans_MR_STAR );
        //--------------------------------------------------------------------//
        B1Trans_MR_STAR.TransposeFrom( B1 );
        B1Trans_VC_STAR = B1Trans_MR_STAR;
        B1_STAR_MC.TransposeFrom( B1Trans_VC_STAR );
        LocalSymmetricAccumulateRL
        ( TRANSPOSE, alpha, A, B1_STAR_MC, B1Trans_MR_STAR, 
          Z1Trans_MC_STAR, Z1Trans_MR_STAR );

        Z1Trans.SumScatterFrom( Z1Trans_MC_STAR );
        Z1Trans_MR_MC = Z1Trans;
        Z1Trans_MR_MC.SumScatterUpdate( T(1), Z1Trans_MR_STAR );
        Transpose( Z1Trans_MR_MC.LockedLocalMatrix(), Z1Local );
        Axpy( T(1), Z1Local, C1.LocalMatrix() );
        //--------------------------------------------------------------------//
        Z1Trans_MR_MC.FreeAlignments();

        SlideLockedPartitionDown
        ( BT,  B0,
               B1,
         /**/ /**/
          BB,  B2 );

        SlidePartitionDown
        ( CT,  C0,
               C1,
         /**/ /**/
          CB,  C2 );
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

template<typename T>
inline void
SymmRLC
( T alpha, const DistMatrix<T>& A, const DistMatrix<T>& B,
  T beta,        DistMatrix<T>& C )
{
#ifndef RELEASE
    PushCallStack("internal::SymmRLC");
    if( A.Grid() != B.Grid() || B.Grid() != C.Grid() )
        throw std::logic_error
        ("{A,B,C} must be distributed over the same grid");
#endif
    const Grid& g = A.Grid();

    // Matrix views
    DistMatrix<T> 
        ATL(g), ATR(g),  A00(g), A01(g), A02(g),  AColPan(g),
        ABL(g), ABR(g),  A10(g), A11(g), A12(g),  ARowPan(g),
                         A20(g), A21(g), A22(g);
    DistMatrix<T> BL(g), BR(g),
                  B0(g), B1(g), B2(g);
    DistMatrix<T> CL(g), CR(g),
                  C0(g), C1(g), C2(g),
                  CLeft(g), CRight(g);

    // Temporary distributions
    DistMatrix<T,MC,  STAR> B1_MC_STAR(g);
    DistMatrix<T,VR,  STAR> AColPan_VR_STAR(g);
    DistMatrix<T,STAR,MR  > AColPanTrans_STAR_MR(g);
    DistMatrix<T,MR,  STAR> ARowPanTrans_MR_STAR(g);

    B1_MC_STAR.AlignWith( C );

    // Start the algorithm
    Scale( beta, C );
    LockedPartitionDownDiagonal
    ( A, ATL, ATR,
         ABL, ABR, 0 );
    LockedPartitionRight( B, BL, BR, 0 );
    PartitionRight( C, CL, CR, 0 );
    while( CR.Width() > 0 )
    {
        LockedRepartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, /**/ A01, A02,
         /*************/ /******************/
               /**/       A10, /**/ A11, A12,
          ABL, /**/ ABR,  A20, /**/ A21, A22 );

        LockedRepartitionRight
        ( BL, /**/ BR,
          B0, /**/ B1, B2 );

        RepartitionRight
        ( CL, /**/ CR,
          C0, /**/ C1, C2 );

        LockedView1x2( ARowPan, A10, A11 );
        LockedView2x1
        ( AColPan, A11,
                   A21 );

        View1x2( CLeft, C0, C1 );
        View1x2( CRight, C1, C2 );

        AColPan_VR_STAR.AlignWith( CRight );
        AColPanTrans_STAR_MR.AlignWith( CRight );
        ARowPanTrans_MR_STAR.AlignWith( CLeft );
        //--------------------------------------------------------------------//
        B1_MC_STAR = B1;

        ARowPanTrans_MR_STAR.TransposeFrom( ARowPan );
        AColPan_VR_STAR = AColPan;
        AColPanTrans_STAR_MR.TransposeFrom( AColPan_VR_STAR );
        MakeTrapezoidal( RIGHT, UPPER, 0, ARowPanTrans_MR_STAR );
        MakeTrapezoidal( LEFT,  UPPER, 1, AColPanTrans_STAR_MR );

        LocalGemm
        ( NORMAL, TRANSPOSE, 
          alpha, B1_MC_STAR, ARowPanTrans_MR_STAR, T(1), CLeft );

        LocalGemm
        ( NORMAL, NORMAL, 
          alpha, B1_MC_STAR, AColPanTrans_STAR_MR, T(1), CRight );
        //--------------------------------------------------------------------//
        AColPan_VR_STAR.FreeAlignments();
        AColPanTrans_STAR_MR.FreeAlignments();
        ARowPanTrans_MR_STAR.FreeAlignments();

        SlideLockedPartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, A01, /**/ A02,
               /**/       A10, A11, /**/ A12,
         /*************/ /******************/
          ABL, /**/ ABR,  A20, A21, /**/ A22 );

        SlideLockedPartitionRight
        ( BL,     /**/ BR,
          B0, B1, /**/ B2 );

        SlidePartitionRight
        ( CL,     /**/ CR,
          C0, C1, /**/ C2 );
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

template<typename T>
inline void
SymmRL
( T alpha, const DistMatrix<T>& A, const DistMatrix<T>& B,
  T beta,        DistMatrix<T>& C )
{
#ifndef RELEASE
    PushCallStack("internal::SymmRL");
#endif
    // TODO: Come up with a better routing mechanism
    if( A.Height() > 5*B.Height() )
        SymmRLA( alpha, A, B, beta, C );
    else
        SymmRLC( alpha, A, B, beta, C );
#ifndef RELEASE
    PopCallStack();
#endif
}

} // namespace internal
} // namespace elem
