/*
   Copyright (c) 2009-2011, Jack Poulson
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
#include <ctime>
#include "elemental.hpp"
#include "elemental/lapack_internal.hpp"
using namespace std;
using namespace elemental;
using namespace elemental::imports::mpi;

void Usage()
{
    cout << "Generates SPD matrix then solves for its Cholesky factor.\n\n"
         << "  Chol <r> <c> <shape> <m> <nb> <correctness?> <print?>\n\n"
         << "  r: number of process rows\n"
         << "  c: number of process cols\n"
         << "  shape: {L,U}\n"
         << "  m: height of matrix\n"
         << "  nb: algorithmic blocksize\n"
         << "  test correctness?: false iff 0\n"
         << "  print matrices?: false iff 0\n" << endl;
}

template<typename F> // represents a real or complex field
void TestCorrectness
( bool printMatrices, Shape shape,
  const DistMatrix<F,MC,MR>& A,
  const DistMatrix<F,MC,MR>& AOrig )
{
    const Grid& g = A.Grid();
    const int m = AOrig.Height();

    DistMatrix<F,MC,MR> X(m,100,g);
    DistMatrix<F,MC,MR> Y(m,100,g);
    X.SetToRandom();
    Y = X;

    if( shape == Lower )
    {
        // Test correctness by comparing the application of AOrig against a 
        // random set of 100 vectors to the application of tril(A) tril(A)^H
        blas::Trmm( Left, Lower, ConjugateTranspose, NonUnit, (F)1, A, Y );
        blas::Trmm( Left, Lower, Normal, NonUnit, (F)1, A, Y );
        blas::Hemm( Left, Lower, (F)-1, AOrig, X, (F)1, Y );
        F oneNormOfError = lapack::OneNorm( Y );
        F infNormOfError = lapack::InfinityNorm( Y );
        F frobNormOfError = lapack::FrobeniusNorm( Y );
        F infNormOfA = lapack::HermitianInfinityNorm( shape, AOrig );
        F frobNormOfA = lapack::HermitianFrobeniusNorm( shape, AOrig );
        F oneNormOfX = lapack::OneNorm( X );
        F infNormOfX = lapack::InfinityNorm( X );
        F frobNormOfX = lapack::FrobeniusNorm( X );
        if( g.VCRank() == 0 )
        {
            cout << "||A||_1 = ||A||_oo   = " << Abs(infNormOfA) << "\n"
                 << "||A||_F              = " << Abs(frobNormOfA) << "\n"
                 << "||X||_1              = " << Abs(oneNormOfX) << "\n"
                 << "||X||_oo             = " << Abs(infNormOfX) << "\n"
                 << "||X||_F              = " << Abs(frobNormOfX) << "\n"
                 << "||A X - L L^H X||_1  = " << Abs(oneNormOfError) << "\n"
                 << "||A X - L L^H X||_oo = " << Abs(infNormOfError) << "\n"
                 << "||A X - L L^H X||_F  = " << Abs(frobNormOfError) << endl;
        }
    }
    else
    {
        // Test correctness by comparing the application of AOrig against a 
        // random set of 100 vectors to the application of triu(A)^H triu(A)
        blas::Trmm( Left, Upper, Normal, NonUnit, (F)1, A, Y );
        blas::Trmm( Left, Upper, ConjugateTranspose, NonUnit, (F)1, A, Y );
        blas::Hemm( Left, Upper, (F)-1, AOrig, X, (F)1, Y );
        F oneNormOfError = lapack::OneNorm( Y );
        F infNormOfError = lapack::InfinityNorm( Y );
        F frobNormOfError = lapack::FrobeniusNorm( Y );
        F infNormOfA = lapack::HermitianInfinityNorm( shape, AOrig );
        F frobNormOfA = lapack::HermitianFrobeniusNorm( shape, AOrig );
        F oneNormOfX = lapack::OneNorm( X );
        F infNormOfX = lapack::InfinityNorm( X );
        F frobNormOfX = lapack::FrobeniusNorm( X );
        if( g.VCRank() == 0 )
        {
            cout << "||A||_1 = ||A||_oo   = " << Abs(infNormOfA) << "\n"
                 << "||A||_F              = " << Abs(frobNormOfA) << "\n"
                 << "||X||_1              = " << Abs(oneNormOfX) << "\n"
                 << "||X||_oo             = " << Abs(infNormOfX) << "\n"
                 << "||X||_F              = " << Abs(frobNormOfX) << "\n"
                 << "||A X - U^H U X||_1  = " << Abs(oneNormOfError) << "\n"
                 << "||A X - U^H U X||_oo = " << Abs(infNormOfError) << "\n"
                 << "||A X - U^H U X||_F  = " << Abs(frobNormOfError) << endl;
        }
    }
}

template<typename F> // represents a real or complex field
void TestChol
( bool testCorrectness, bool printMatrices, 
  Shape shape, int m, const Grid& g )
{
    double startTime, endTime, runTime, gFlops;
    DistMatrix<F,MC,MR> A(g);
    DistMatrix<F,MC,MR> AOrig(g);

    A.ResizeTo( m, m );

    A.SetToRandomHPD();
    if( testCorrectness )
    {
        if( g.VCRank() == 0 )
        {
            cout << "  Making copy of original matrix...";
            cout.flush();
        }
        AOrig = A;
        if( g.VCRank() == 0 )
            cout << "DONE" << endl;
    }
    if( printMatrices )
        A.Print("A");

    if( g.VCRank() == 0 )
    {
        cout << "  Starting Cholesky factorization...";
        cout.flush();
    }
    Barrier( MPI_COMM_WORLD );
    startTime = Time();
    lapack::Chol( shape, A );
    Barrier( MPI_COMM_WORLD );
    endTime = Time();
    runTime = endTime - startTime;
    gFlops = lapack::internal::CholGFlops<F>( m, runTime );
    if( g.VCRank() == 0 )
    {
        cout << "DONE.\n"
             << "  Time = " << runTime << " seconds. GFlops = " 
             << gFlops << endl;
    }
    if( printMatrices )
        A.Print("A after factorization");
    if( testCorrectness )
        TestCorrectness( printMatrices, shape, A, AOrig );
}

int main( int argc, char* argv[] )
{
    int rank;
    Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    if( argc != 8 )
    {
        if( rank == 0 )
            Usage();
        Finalize();
        return 0;
    }
    try
    {
        int argNum = 0;
        const int r = atoi(argv[++argNum]);
        const int c = atoi(argv[++argNum]);
        const Shape shape = CharToShape(*argv[++argNum]);
        const int m = atoi(argv[++argNum]);
        const int nb = atoi(argv[++argNum]);
        const bool testCorrectness = atoi(argv[++argNum]);
        const bool printMatrices = atoi(argv[++argNum]);
#ifndef RELEASE
        if( rank == 0 )
        {
            cout << "==========================================\n"
                 << " In debug mode! Performance will be poor! \n"
                 << "==========================================" << endl;
        }
#endif
        const Grid g( MPI_COMM_WORLD, r, c );
        SetBlocksize( nb );

        if( rank == 0 )
            cout << "Will test Chol" << ShapeToChar(shape) << endl;

        if( rank == 0 )
        {
            cout << "---------------------\n"
                 << "Testing with doubles:\n"
                 << "---------------------" << endl;
        }
        TestChol<double>
        ( testCorrectness, printMatrices, shape, m, g );

#ifndef WITHOUT_COMPLEX
        if( rank == 0 )
        {
            cout << "--------------------------------------\n"
                 << "Testing with double-precision complex:\n"
                 << "--------------------------------------" << endl;
        }
        TestChol<dcomplex>
        ( testCorrectness, printMatrices, shape, m, g );
#endif
    }
    catch( exception& e )
    {
#ifndef RELEASE
        DumpCallStack();
#endif
        cerr << "Process " << rank << " caught error message:\n"
             << e.what() << endl;
    }   

    Finalize();
    return 0;
}
