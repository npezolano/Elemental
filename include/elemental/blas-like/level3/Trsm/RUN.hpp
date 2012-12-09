/*
   Copyright (c) 2009-2012, Jack Poulson
   All rights reserved.

   This file is part of Elemental.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    - Neither the name of the owner nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

namespace elem {
namespace internal {

// Right Upper Normal (Non)Unit Trsm
//   X := X triu(U)^-1, and
//   X := X triuu(U)^-1
template<typename F>
inline void
TrsmRUN
( UnitOrNonUnit diag, F alpha, const DistMatrix<F>& U, DistMatrix<F>& X,
  bool checkIfSingular )
{
#ifndef RELEASE
    PushCallStack("internal::TrsmRUN");
#endif
    const Grid& g = U.Grid();

    // Matrix views
    DistMatrix<F> 
        UTL(g), UTR(g),  U00(g), U01(g), U02(g),
        UBL(g), UBR(g),  U10(g), U11(g), U12(g),
                         U20(g), U21(g), U22(g);

    DistMatrix<F> XL(g), XR(g),
                  X0(g), X1(g), X2(g);

    // Temporary distributions
    DistMatrix<F,STAR,STAR> U11_STAR_STAR(g); 
    DistMatrix<F,STAR,MR  > U12_STAR_MR(g);
    DistMatrix<F,VC,  STAR> X1_VC_STAR(g);    
    DistMatrix<F,STAR,MC  > X1Trans_STAR_MC(g);
    
    // Start the algorithm
    Scale( alpha, X );
    LockedPartitionDownDiagonal
    ( U, UTL, UTR,
         UBL, UBR, 0 );
    PartitionRight( X, XL, XR, 0 );
    while( XR.Width() > 0 )
    {
        LockedRepartitionDownDiagonal
        ( UTL, /**/ UTR,  U00, /**/ U01, U02,
         /*************/ /******************/
               /**/       U10, /**/ U11, U12, 
          UBL, /**/ UBR,  U20, /**/ U21, U22 );

        RepartitionRight
        ( XL, /**/     XR,
          X0, /**/ X1, X2 ); 

        X1_VC_STAR.AlignWith( X2 );
        X1Trans_STAR_MC.AlignWith( X2 );
        U12_STAR_MR.AlignWith( X2 );
        //--------------------------------------------------------------------//
        U11_STAR_STAR = U11; 
        X1_VC_STAR = X1;

        LocalTrsm
        ( RIGHT, UPPER, NORMAL, diag, F(1), U11_STAR_STAR, X1_VC_STAR,
          checkIfSingular );

        X1Trans_STAR_MC.TransposeFrom( X1_VC_STAR );
        X1.TransposeFrom( X1Trans_STAR_MC );
        U12_STAR_MR = U12; 

        // X2[MC,MR] -= X1[MC,* ] U12[* ,MR]
        //            = X1^T[* ,MC] U12[* ,MR]
        LocalGemm
        ( TRANSPOSE, NORMAL, F(-1), X1Trans_STAR_MC, U12_STAR_MR, F(1), X2 );
        //--------------------------------------------------------------------//
        X1_VC_STAR.FreeAlignments();
        X1Trans_STAR_MC.FreeAlignments();
        U12_STAR_MR.FreeAlignments();

        SlideLockedPartitionDownDiagonal
        ( UTL, /**/ UTR,  U00, U01, /**/ U02,
               /**/       U10, U11, /**/ U12,
         /*************/ /******************/
          UBL, /**/ UBR,  U20, U21, /**/ U22 );

        SlidePartitionRight
        ( XL,     /**/ XR,
          X0, X1, /**/ X2 );
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

} // namespace internal
} // namespace elem