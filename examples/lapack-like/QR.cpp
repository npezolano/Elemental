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
#include "elemental.hpp"
using namespace std;
using namespace elem;

typedef double Real;
typedef Complex<Real> C;

void Usage()
{
    cout << "QR <m> <n>\n"
         << "  <m>: height of random matrix to test QR on\n"
         << "  <n>: width of random matrix to test QR on\n"
         << endl;
}

int
main( int argc, char* argv[] )
{
    Initialize( argc, argv );

    mpi::Comm comm = mpi::COMM_WORLD;
    const int commRank = mpi::CommRank( comm );

    if( argc < 3 )
    {
        if( commRank == 0 )
            Usage();
        Finalize();
        return 0;
    }
    const int m = atoi( argv[1] );
    const int n = atoi( argv[2] );

    try 
    {
        const Grid g( comm );
        DistMatrix<C> A(g);
        Uniform( m, n, A );
        const Real frobA = Norm( A, FROBENIUS_NORM );

        // Compute the QR decomposition of A, but do not overwrite A
        DistMatrix<C> Q( A ), R(g);
        ExplicitQR( Q, R );

        // Check the error in the QR factorization, || A - Q R ||_F / || A ||_F
        DistMatrix<C> E( A );
        Gemm( NORMAL, NORMAL, C(-1), Q, R, C(1), E );
        const Real frobQR = Norm( E, FROBENIUS_NORM );

        // Check the numerical orthogonality of Q, || I - Q^H Q ||_F / || A ||_F
        const int k = std::min(m,n);
        Identity( k, k, E );
        Herk( LOWER, ADJOINT, C(-1), Q, C(1), E );
        const Real frobOrthog = HermitianNorm( LOWER, E, FROBENIUS_NORM ); 

        if( g.Rank() == 0 )
        {
            std::cout << "|| A ||_F = " << frobA << "\n"
                      << "|| A - Q R ||_F / || A ||_F   = " 
                      << frobQR/frobA << "\n"
                      << "|| I - Q^H Q ||_F / || A ||_F = "
                      << frobOrthog/frobA << "\n"
                      << std::endl;
        }
    }
    catch( exception& e )
    {
        cerr << "Process " << commRank << " caught exception with message: "
             << e.what() << endl;
#ifndef RELEASE
        DumpCallStack();
#endif
    }

    Finalize();
    return 0;
}
