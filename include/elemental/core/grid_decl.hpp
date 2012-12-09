/*
   Copyright (c) 2009-2012, Jack Poulson
                      2012, Jed Brown 
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

class Grid
{
public:
    Grid( mpi::Comm comm=mpi::COMM_WORLD );
    Grid( mpi::Comm comm, int height, int width );
    ~Grid();

    // Simple interface (simpler version of distributed-based interface)
    int Row() const;           // same as MCRank()
    int Col() const;           // same as MRRank()
    int Rank() const;          // same as VCRank()
    int Height() const;        // same as MCSize()
    int Width() const;         // same as MRSize()
    int Size() const;          // same as VCSize() and VRSize()
    mpi::Comm ColComm() const; // same as MCComm()
    mpi::Comm RowComm() const; // same as MRComm()
    mpi::Comm Comm() const;    // same as VCComm()

    // Distribution-based interface
    int MCRank() const;
    int MRRank() const;
    int VCRank() const;
    int VRRank() const;
    int MCSize() const;
    int MRSize() const;
    int VCSize() const;
    int VRSize() const;
    mpi::Comm MCComm() const;
    mpi::Comm MRComm() const;
    mpi::Comm VCComm() const;
    mpi::Comm VRComm() const;

    // Advanced routines
    Grid( mpi::Comm viewers, mpi::Group owners );
    Grid( mpi::Comm viewers, mpi::Group owners, int height, int width ); 
    int GCD() const; // greatest common denominator of grid height and width
    int LCM() const; // lowest common multiple of grid height and width
    bool InGrid() const;
    int OwningRank() const;
    int ViewingRank() const;
    int VCToViewingMap( int VCRank ) const;
    mpi::Group OwningGroup() const;
    mpi::Comm OwningComm() const;
    mpi::Comm ViewingComm() const;
    int DiagPath() const;
    int DiagPath( int vectorColRank ) const;
    int DiagPathRank() const;
    int DiagPathRank( int vectorColRank ) const;

    static int FindFactor( int p );

private:
    int height_, width_, size_, gcd_;
    int matrixColRank_;
    int matrixRowRank_;
    int vectorColRank_;
    int vectorRowRank_;
    std::vector<int> diagPathsAndRanks_;

    mpi::Comm viewingComm_; // all processes that create the grid
    mpi::Group viewingGroup_;
    int viewingRank_; // our rank in the viewing communicator

    mpi::Group owningGroup_; // the processes that can own data
    mpi::Group notOwningGroup_; // contains the remaining processes

    std::vector<int> vectorColToViewingMap_;

    // Keep track of whether or not our process is in the grid. This is 
    // necessary to avoid calls like MPI_Comm_size when we're not in the
    // communicator's group. Note that we can__ call MPI_Group_rank when not 
    // in the group and that the result is MPI_UNDEFINED.
    bool inGrid_;

    // Create a communicator for the processes that are in the process grid
    mpi::Comm owningComm_;
    mpi::Comm notOwningComm_; // necessary complimentary communicator
    int owningRank_;

    // These will only be valid if we are in the grid
    mpi::Comm cartComm_;  // the processes that are in the grid
    mpi::Comm matrixColComm_;
    mpi::Comm matrixRowComm_;
    mpi::Comm vectorColComm_;
    mpi::Comm vectorRowComm_;

    void SetUpGrid();

    // Disable copying this class due to MPI_Comm/MPI_Group ownership issues
    // and potential performance loss from duplicating MPI communicators, e.g.,
    // on Blue Gene/P there is supposedly a performance loss
    const Grid& operator=( Grid& );
    Grid( const Grid& );
};

bool operator== ( const Grid& A, const Grid& B );
bool operator!= ( const Grid& A, const Grid& B );

// Return a grid constructed using mpi::COMM_WORLD.
const Grid& DefaultGrid();

} // namespace elem