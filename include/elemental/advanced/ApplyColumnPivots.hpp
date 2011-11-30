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

template<typename F>
inline void
elemental::advanced::ApplyColumnPivots
( DistMatrix<F,MC,MR>& A, const DistMatrix<int,VC,STAR>& p )
{
#ifndef RELEASE
    PushCallStack("advanced::ApplyColumnPivots");
#endif
    DistMatrix<int,STAR,STAR> p_STAR_STAR( p );
    advanced::ApplyColumnPivots( A, p_STAR_STAR );
#ifndef RELEASE
    PopCallStack();
#endif
}

template<typename F>
inline void
elemental::advanced::ApplyColumnPivots
( DistMatrix<F,MC,MR>& A, const DistMatrix<int,STAR,STAR>& p )
{
#ifndef RELEASE
    PushCallStack("advanced::ApplyColumnPivots");
#endif
    std::vector<int> image, preimage;
    advanced::ComposePivots( p, image, preimage );
    advanced::ApplyColumnPivots( A, image, preimage );
#ifndef RELEASE
    PopCallStack();
#endif
}

template<typename F> // represents a real or complex number
inline void
elemental::advanced::ApplyColumnPivots
( DistMatrix<F,MC,MR>& A, 
  const std::vector<int>& image,
  const std::vector<int>& preimage )
{
    const int b = image.size();
#ifndef RELEASE
    PushCallStack("advanced::ApplyColumnPivots");
    if( A.Width() < b || b != preimage.size() )
        throw std::logic_error
        ("image and preimage must be vectors of equal length that are not "
         "wider than A.");
#endif
    const int localHeight = A.LocalHeight();
    if( A.Height() == 0 || A.Width() == 0 )
    {
#ifndef RELEASE
        PopCallStack();
#endif
        return;
    }

    // Extract the relevant process grid information
    const Grid& g = A.Grid();
    const int c = g.Width();
    const int rowAlignment = A.RowAlignment();
    const int rowShift = A.RowShift();
    const int myCol = g.MRRank();

    // Extract the send and recv counts from the image and preimage.
    // This process's sends may be logically partitioned into two sets:
    //   (a) sends from rows [0,...,b-1]
    //   (b) sends from rows [b,...]
    // The latter is analyzed with image, the former deduced with preimage.
    std::vector<int> sendCounts(c,0), recvCounts(c,0);
    for( int j=rowShift; j<b; j+=c )
    {
        const int sendCol = preimage[j];         
        const int sendTo = (rowAlignment+sendCol) % c; 
        sendCounts[sendTo] += localHeight;

        const int recvCol = image[j];
        const int recvFrom = (rowAlignment+recvCol) % c;
        recvCounts[recvFrom] += localHeight;
    }
    for( int j=0; j<b; ++j )
    {
        const int sendCol = preimage[j];
        if( sendCol >= b )
        {
            const int sendTo = (rowAlignment+sendCol) % c;
            if( sendTo == myCol )
            {
                const int sendFrom = (rowAlignment+j) % c;
                recvCounts[sendFrom] += localHeight;
            }
        }

        const int recvCol = image[j];
        if( recvCol >= b )
        {
            const int recvFrom = (rowAlignment+recvCol) % c;
            if( recvFrom == myCol )
            {
                const int recvTo = (rowAlignment+j) % c;
                sendCounts[recvTo] += localHeight;
            }
        }
    }

    // Construct the send and recv displacements from the counts
    std::vector<int> sendDispls(c), recvDispls(c);
    int totalSend=0, totalRecv=0;
    for( int i=0; i<c; ++i )
    {
        sendDispls[i] = totalSend;
        recvDispls[i] = totalRecv;
        totalSend += sendCounts[i];
        totalRecv += recvCounts[i];
    }
#ifndef RELEASE
    if( totalSend != totalRecv )
    {
        std::ostringstream msg;
        msg << "Send and recv counts do not match: (send,recv)=" 
             << totalSend << "," << totalRecv;
        throw std::logic_error( msg.str().c_str() );
    }
#endif

    // Fill vectors with the send data
    const int ALDim = A.LocalLDim();
    std::vector<F> sendData(std::max(1,totalSend));
    std::vector<int> offsets(c,0);
    const int localWidth = LocalLength( b, rowShift, c );
    for( int jLocal=0; jLocal<localWidth; ++jLocal )
    {
        const int sendCol = preimage[rowShift+jLocal*c];
        const int sendTo = (rowAlignment+sendCol) % c;
        const int offset = sendDispls[sendTo]+offsets[sendTo];
        std::memcpy
        ( &sendData[offset], A.LocalBuffer(0,jLocal), localHeight*sizeof(F) );
        offsets[sendTo] += localHeight;
    }
    for( int j=0; j<b; ++j )
    {
        const int recvCol = image[j];
        if( recvCol >= b )
        {
            const int recvFrom = (rowAlignment+recvCol) % c; 
            if( recvFrom == myCol )
            {
                const int recvTo = (rowAlignment+j) % c;
                const int jLocal = (recvCol-rowShift) / c;
                const int offset = sendDispls[recvTo]+offsets[recvTo];
                std::memcpy
                ( &sendData[offset], A.LocalBuffer(0,jLocal), 
                  localHeight*sizeof(F) );
                offsets[recvTo] += localHeight;
            }
        }
    }

    // Communicate all pivot rows
    std::vector<F> recvData(std::max(1,totalRecv));
    mpi::AllToAll
    ( &sendData[0], &sendCounts[0], &sendDispls[0],
      &recvData[0], &recvCounts[0], &recvDispls[0], g.MRComm() );

    // Unpack the recv data
    for( int k=0; k<c; ++k )
    {
        offsets[k] = 0;
        int thisRowShift = Shift( k, rowAlignment, c );
        for( int j=thisRowShift; j<b; j+=c )
        {
            const int sendCol = preimage[j];
            const int sendTo = (rowAlignment+sendCol) % c;
            if( sendTo == myCol )
            {
                const int offset = recvDispls[k]+offsets[k];
                const int jLocal = (sendCol-rowShift) / c;
                std::memcpy
                ( A.LocalBuffer(0,jLocal), &recvData[offset], 
                  localHeight*sizeof(F) );
                offsets[k] += localHeight;
            }
        }
    }
    for( int j=0; j<b; ++j )
    {
        const int recvCol = image[j];
        if( recvCol >= b )
        {
            const int recvTo = (rowAlignment+j) % c;
            if( recvTo == myCol )
            {
                const int recvFrom = (rowAlignment+recvCol) % c; 
                const int jLocal = (j-rowShift) / c;
                const int offset = recvDispls[recvFrom]+offsets[recvFrom];
                std::memcpy
                ( A.LocalBuffer(0,jLocal), &recvData[offset], 
                  localHeight*sizeof(F) );
                offsets[recvFrom] += localWidth;
            }
        }
    }
#ifndef RELEASE
    PopCallStack();
#endif
}