/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/

namespace elem {

//
// Based on Yuji Nakatsukasa's implementation of a QR-based dynamically 
// weighted Halley iteration for the polar decomposition. In particular, this
// implementation mirrors the routine 'qdwh', which is part of the zip-file
// available here:
//     http://www.mathworks.com/matlabcentral/fileexchange/36830
//
// No support for column-pivoting or row-sorting yet.
//
// The careful calculation of the coefficients is due to a suggestion from
// Gregorio Quintana Orti.
//

template<typename F>
int QDWH
( Matrix<F>& A, 
  typename Base<F>::type lowerBound,
  typename Base<F>::type upperBound )
{
#ifndef RELEASE
    PushCallStack("QDWH");
#endif
    typedef typename Base<F>::type R;
    const int height = A.Height();
    const int width = A.Width();
    const R oneHalf = R(1)/R(2);
    const R oneThird = R(1)/R(3);

    if( height < width )
        throw std::logic_error("Height cannot be less than width");

    const R epsilon = lapack::MachineEpsilon<R>();
    const R tol = 5*epsilon;
    const R cubeRootTol = Pow(tol,oneThird);

    // Form the first iterate
    Scale( 1/upperBound, A );

    int numIts=0;
    R frobNormADiff;
    Matrix<F> ALast;
    Matrix<F> Q( height+width, width );
    Matrix<F> QT, QB;
    PartitionDown( Q, QT,
                      QB, height );
    Matrix<F> C;
    Matrix<F> ATemp;
    do
    {
        ++numIts;
        ALast = A;

        R L2;
        Complex<R> dd, sqd;
        if( Abs(1-lowerBound) < tol )
        {
            L2 = 1;
            dd = 0;
            sqd = 1;
        }
        else
        {
            L2 = lowerBound*lowerBound;
            dd = Pow( 4*(1-L2)/(L2*L2), oneThird );
            sqd = Sqrt( 1+dd );
        }
        const Complex<R> arg = 8 - 4*dd + 8*(2-L2)/(L2*sqd);
        const R a = (sqd + Sqrt( arg )/2).real;
        const R b = (a-1)*(a-1)/4;
        const R c = a+b-1;
        const Complex<R> alpha = a-b/c;
        const Complex<R> beta = b/c;

        lowerBound = lowerBound*(a+b*L2)/(1+c*L2);

        if( c > 100 )
        {
            //
            // The standard QR-based algorithm
            //
            QT = A;
            Scale( Sqrt(c), QT );
            MakeIdentity( QB );
            ExplicitQR( Q );
            Gemm( NORMAL, ADJOINT, alpha/Sqrt(c), QT, QB, beta, A );
        }
        else
        {
            //
            // Use faster Cholesky-based algorithm since A is well-conditioned
            //
            Identity( width, width, C );
            Herk( LOWER, ADJOINT, F(c), A, F(1), C );
            Cholesky( LOWER, C );
            ATemp = A;
            Trsm( RIGHT, LOWER, ADJOINT, NON_UNIT, F(1), C, ATemp );
            Trsm( RIGHT, LOWER, NORMAL, NON_UNIT, F(1), C, ATemp );
            Scale( beta, A );
            Axpy( alpha, ATemp, A );
        }

        Axpy( F(-1), A, ALast );
        frobNormADiff = Norm( ALast, FROBENIUS_NORM );
    }
    while( frobNormADiff > cubeRootTol || Abs(1-lowerBound) > tol );
#ifndef RELEASE
    PopCallStack();
#endif
    return numIts;
}

template<typename F>
int QDWH
( DistMatrix<F>& A, 
  typename Base<F>::type lowerBound,
  typename Base<F>::type upperBound )
{
#ifndef RELEASE
    PushCallStack("QDWH");
#endif
    typedef typename Base<F>::type R;
    const Grid& g = A.Grid();
    const int height = A.Height();
    const int width = A.Width();
    const R oneHalf = R(1)/R(2);
    const R oneThird = R(1)/R(3);

    if( height < width )
        throw std::logic_error("Height cannot be less than width");

    const R epsilon = lapack::MachineEpsilon<R>();
    const R tol = 5*epsilon;
    const R cubeRootTol = Pow(tol,oneThird);

    // Form the first iterate
    Scale( 1/upperBound, A );

    int numIts=0;
    R frobNormADiff;
    DistMatrix<F> ALast( g );
    DistMatrix<F> Q( height+width, width, g );
    DistMatrix<F> QT(g), QB(g);
    PartitionDown( Q, QT,
                      QB, height );
    DistMatrix<F> C( g );
    DistMatrix<F> ATemp( g );
    do
    {
        ++numIts;
        ALast = A;

        R L2;
        Complex<R> dd, sqd;
        if( Abs(1-lowerBound) < tol )
        {
            L2 = 1;
            dd = 0;
            sqd = 1;
        }
        else
        {
            L2 = lowerBound*lowerBound;
            dd = Pow( 4*(1-L2)/(L2*L2), oneThird );
            sqd = Sqrt( 1+dd );
        }
        const Complex<R> arg = 8 - 4*dd + 8*(2-L2)/(L2*sqd);
        const R a = (sqd + Sqrt( arg )/2).real;
        const R b = (a-1)*(a-1)/4;
        const R c = a+b-1;
        const Complex<R> alpha = a-b/c;
        const Complex<R> beta = b/c;

        lowerBound = lowerBound*(a+b*L2)/(1+c*L2);

        if( c > 100 )
        {
            //
            // The standard QR-based algorithm
            //
            QT = A;
            Scale( Sqrt(c), QT );
            MakeIdentity( QB );
            ExplicitQR( Q );
            Gemm( NORMAL, ADJOINT, alpha/Sqrt(c), QT, QB, beta, A );
        }
        else
        {
            //
            // Use faster Cholesky-based algorithm since A is well-conditioned
            //
            Identity( width, width, C );
            Herk( LOWER, ADJOINT, F(c), A, F(1), C );
            Cholesky( LOWER, C );
            ATemp = A;
            Trsm( RIGHT, LOWER, ADJOINT, NON_UNIT, F(1), C, ATemp );
            Trsm( RIGHT, LOWER, NORMAL, NON_UNIT, F(1), C, ATemp );
            Scale( beta, A );
            Axpy( alpha, ATemp, A );
        }

        Axpy( F(-1), A, ALast );
        frobNormADiff = Norm( ALast, FROBENIUS_NORM );
    }
    while( frobNormADiff > cubeRootTol || Abs(1-lowerBound) > tol );
#ifndef RELEASE
    PopCallStack();
#endif
    return numIts;
}

} // namespace elem
