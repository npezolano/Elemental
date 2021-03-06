Invariants, inner products, and divergences
===========================================

Condition number
----------------
Returns the two-norm condition number

.. math::

   \kappa_2(A) = \|A\|_2 \|A^{-1}\|_2.

.. cpp:function:: typename Base<F>::type ConditionNumber( const Matrix<F>& A )
.. cpp:function:: typename Base<F>::type ConditionNumber( const DistMatrix<F,U,V>& A )

Determinant
-----------
Though there are many different possible definitions of the determinant of a 
matrix :math:`A \in \mathbb{F}^{n \times n}`, the simplest one is in terms of 
the product of the eigenvalues (including multiplicity):

.. math::

   \mbox{det}(A) = \prod_{i=0}^{n-1} \lambda_i.

Since :math:`\mbox{det}(AB)=\mbox{det}(A)\mbox{det}(B)`, we can compute the 
determinant of an arbitrary matrix in :math:`\mathcal{O}(n^3)` work by 
computing its LU decomposition (with partial pivoting), :math:`PA=LU`, 
recognizing that :math:`\mbox{det}(P)=\pm 1` 
(the *signature* of the permutation), and computing

.. math::

   \mbox{det}(A) = \mbox{det}(P)\mbox{det}(L)\mbox{det}(U) 
                 = \mbox{det}(P) \prod_{i=0}^{n-1} \upsilon_{i,i}
                 = \pm \prod_{i=0}^{n-1} \upsilon_{i,i},

where :math:`\upsilon_{i,i}` is the i'th diagonal entry of :math:`U`.

.. cpp:function:: F Determinant( const Matrix<F>& A )
.. cpp:function:: F Determinant( const DistMatrix<F>& A )
.. cpp:function:: F Determinant( Matrix<F>& A, bool canOverwrite=false )
.. cpp:function:: F Determinant( DistMatrix<F>& A, bool canOverwrite=false )

   Returns the determinant of the (fully populated) square matrix `A`.
   Some of the variants allow for overwriting the input matrix in order to 
   avoid forming another temporary matrix.

.. cpp:type:: struct SafeProduct<F>

   Represents the product of `n` values as :math:`\rho \exp(\kappa n)`, 
   where :math:`|\rho| \le 1` and :math:`\kappa \in \mathbb{R}`.

   .. cpp:member:: F rho

      For nonzero values, `rho` is the modulus and lies *on* the unit 
      circle; in order to represent a value that is precisely zero, `rho` 
      is set to zero.

   .. cpp:member:: typename Base<F>::type kappa

      `kappa` can be an arbitrary real number.

   .. cpp:member:: int n

      The number of values in the product.

.. cpp:function:: SafeProduct<F> SafeDeterminant( const Matrix<F>& A )
.. cpp:function:: SafeProduct<F> SafeDeterminant( const DistMatrix<F>& A )
.. cpp:function:: SafeProduct<F> SafeDeterminant( Matrix<F>& A, bool canOverwrite=false )
.. cpp:function:: SafeProduct<F> SafeDeterminant( DistMatrix<F>& A, bool canOverwrite=false )

   Returns the determinant of the square matrix `A` in an expanded form 
   which is less likely to over/under-flow.

HPDDeterminant
--------------
A version of the above determinant specialized for Hermitian positive-definite
matrices (which will therefore have all positive eigenvalues and a positive 
determinant).

.. cpp:function:: typename Base<F>::type HPDDeterminant( UpperOrLower uplo, const Matrix<F>& A )
.. cpp:function:: typename Base<F>::type HPDDeterminant( UpperOrLower uplo, const DistMatrix<F>& A )
.. cpp:function:: typename Base<F>::type HPDDeterminant( UpperOrLower uplo, Matrix<F>& A, bool canOverwrite=false )
.. cpp:function:: typename Base<F>::type HPDDeterminant( UpperOrLower uplo, DistMatrix<F>& A, bool canOverwrite=false )

   Returns the determinant of the (fully populated) Hermitian positive-definite
   matrix `A`.
   Some of the variants allow for overwriting the input matrix in order to 
   avoid forming another temporary matrix.

.. cpp:function:: SafeProduct<F> SafeHPDDeterminant( UpperOrLower uplo, const Matrix<F>& A )
.. cpp:function:: SafeProduct<F> SafeHPDDeterminant( UpperOrLower uplo, const DistMatrix<F>& A )
.. cpp:function:: SafeProduct<F> SafeHPDDeterminant( UpperOrLower uplo, Matrix<F>& A, bool canOverwrite=false )
.. cpp:function:: SafeProduct<F> SafeHPDDeterminant( UpperOrLower uplo, DistMatrix<F>& A, bool canOverwrite=false )

   Returns the determinant of the Hermitian positive-definite matrix `A` in an 
   expanded form which is less likely to over/under-flow.

LogBarrier
----------
Uses a careful calculation of the log of the determinant in order to return
the *log barrier* of a Hermitian positive-definite matrix `A`, 
:math:`-\log(\mbox{det}(A))`.

.. cpp:function:: typename Base<F>::type LogBarrier( UpperOrLower uplo, const Matrix<F>& A )
.. cpp:function:: typename Base<F>::type LogBarrier( UpperOrLower uplo, const DistMatrix<F>& A )
.. cpp:function:: typename Base<F>::type LogBarrier( UpperOrLower uplo, Matrix<F>& A, bool canOverwrite=false )
.. cpp:function:: typename Base<F>::type LogBarrier( UpperOrLower uplo, DistMatrix<F>& A, bool canOverwrite=false )

LogDetDivergence
----------------
The *log-det divergence* of a pair of :math:`n \times n` Hermitian 
positive-definite matrices :math:`A` and :math:`B` is 

.. math::

   D_{ld}(A,B) = \mbox{tr}(A B^{-1}) -\log(\mbox{det}(A B^{-1})) - n,

which can be greatly simplified using the Cholesky factors of :math:`A` and :math:`B`.
In particular, if we set :math:`Z = L_B^{-1} L_A`, where :math:`A=L_A L_A^H` and 
:math:`B=L_B L_B^H` are Cholesky factorizations, then 

.. math::

   D_{ld}(A,B) = \| Z \|_F^2 - 2 \log(\mbox{det}(Z)) - n.

.. cpp:function:: typename Base<F>::type LogDetDivergence( UpperOrLower uplo, const Matrix<F>& A, const Matrix<F>& B )
.. cpp:function:: typename Base<F>::type LogDetDivergence( UpperOrLower uplo, const DistMatrix<F>& A, const DistMatrix<F>& B )

Norm
----
The following routines can return either
:math:`\|A\|_1`, :math:`\|A\|_\infty`, :math:`\|A\|_F` (the Frobenius norm),
the maximum entrywise norm, :math:`\|A\|_2`, or :math:`\|A\|_*`
(the nuclear/trace norm) of fully-populated matrices.

.. cpp:function:: typename Base<F>::type Norm( const Matrix<F>& A, NormType type=FROBENIUS_NORM )
.. cpp:function:: typename Base<F>::type Norm( const DistMatrix<F,U,V>& A, NormType type=FROBENIUS_NORM )

HermitianNorm
-------------
Same as :cpp:func:`Norm`, but the (distributed) matrix is implicitly
Hermitian with the data stored in the triangle specified by
:cpp:type:`UpperOrLower`.
Also, while :cpp:func:`Norm` supports every type of distribution,
:cpp:func:`HermitianNorm` currently only supports the standard matrix
distribution.

.. cpp:function:: typename Base<F>::type HermitianNorm( UpperOrLower uplo, const Matrix<F>& A, NormType type=FROBENIUS_NORM )
.. cpp:function:: typename Base<F>::type HermitianNorm( UpperOrLower uplo, const DistMatrix<F>& A, NormType type=FROBENIUS_NORM )

SymmetricNorm
-------------
Same as :cpp:func:`Norm`, but the (distributed) matrix is implicitly
symmetric with the data stored in the triangle specified by
:cpp:type:`UpperOrLower`.
Also, while :cpp:func:`Norm` supports every type of distribution,
:cpp:func:`SymmetricNorm` currently only supports the standard matrix
distribution.

.. cpp:function:: typename Base<F>::type SymmetricNorm( UpperOrLower uplo, const Matrix<F>& A, NormType type=FROBENIUS_NORM )
.. cpp:function:: typename Base<F>::type SymmetricNorm( UpperOrLower uplo, const DistMatrix<F>& A, NormType type=FROBENIUS_NORM )

Two-norm estimates
------------------
Since the two-norm is extremely useful, but expensive to compute, it is useful
to be able to compute rough lower and upper bounds for it. The following
routines provide cheap, rough estimates. The ability to compute sharper
estimates will likely be added later.

.. cpp:function:: typename Base<F>::type TwoNormLowerBound( const Matrix<F>& A )
.. cpp:function:: typename Base<F>::type TwoNormLowerBound( const DistMatrix<F>& A )

   Return the tightest lower bound on :math:`\|A\|_2` implied by the following inequalities:

   .. math::

      \|A\|_2 \ge \|A\|_{\mathrm{max}},

   .. math::

      \|A\|_2 \ge \frac{1}{\sqrt{n}} \|A\|_{\infty},

   .. math::

      \|A\|_2 \ge \frac{1}{\sqrt{m}} \|A\|_1,\;\;\mathrm{and}

   .. math::

      \|A\|_2 \ge \frac{1}{\mathrm{min}(m,n)} \|A\|_F.

.. cpp:function:: typename Base<F>::type TwoNormUpperBound( const Matrix<F>& A )
.. cpp:function:: typename Base<F>::type TwoNormUpperBound( const DistMatrix<F>& A )

   Return the tightest upper bound on :math:`\|A\|_2` implied by the following inequalities:

   .. math::

      \|A\|_2 \le \sqrt{m n} \|A\|_{\mathrm{max}},

   .. math::

      \|A\|_2 \le \sqrt{m} \|A\|_{\infty},

   .. math::

      \|A\|_2 \le \sqrt{n} \|A\|_1,\;\;\mathrm{and}

   .. math::

      \|A\|_2 \le \sqrt{ \|A\|_1 \|A\|_{\infty} }.

Trace
-----
The two equally useful definitions of the trace of a square matrix 
:math:`A \in \mathbb{F}^{n \times n}` are

.. math::

   \mbox{tr}(A) = \sum_{i=0}^{n-1} \alpha_{i,i} = \sum_{i=0}^{n-1} \lambda_i,

where :math:`\alpha_{i,i}` is the i'th diagonal entry of :math:`A` and 
:math:`\lambda_i` is the i'th eigenvalue (counting multiplicity) of :math:`A`.

Clearly the former equation is easier to compute, but the latter is an 
important characterization.

.. cpp:function:: F Trace( const Matrix<F>& A )
.. cpp:function:: F Trace( const DistMatrix<F>& A )

   Return the trace of the square matrix `A`.

HilbertSchmidt
--------------
The Hilbert-Schmidt inner-product of two :math:`m \times n` matrices :math:`A`
and :math:`B` is :math:`\mbox{tr}(A^H B)`.

.. cpp:function:: F HilbertSchmidt( const Matrix<F>& A, const Matrix<F>& B )
.. cpp:function:: F HilbertSchmidt( const DistMatrix<F,U,V>& A, const DistMatrix<F,U,V>& B )
