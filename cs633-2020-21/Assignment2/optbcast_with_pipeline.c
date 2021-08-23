#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"

int nodeToGroupMap[93] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0, 3, 0, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

// takes the starting of the hostname string and returns the groups to which the nodes belongs to.
int getGroupNumberFromHostname(char* hostname){
  int csewsNumber = atoi(hostname+5);
  return nodeToGroupMap[csewsNumber];
}



int main( int argc, char *argv[])

{
  
  // 1. make subcommunicators for all the process in a single group
  // 2. designated the processes with rank 0 in each intranode group 
  //    as a leader.
  // 3. make a subcommunicator of all the leaders
  // 4. If there is only group simply broadcast as the further steps
  //    are not need
  // 5. from the root of the bcast send the buffer to the leader of 
  //    the group to which the root belongs.
  //    (if the root is the leader no need to send to itself otherwise
  //     it may deadlock).
  // 6. from the root's group leader bcast to all the leaders of the groups, 
  // 	note that if the code reaches here the that means that there
  // 	are multiple leaders.
  // 7. now from each leader if the leader is not the only member 
  //    in its group broadcast from it to the whole group.
  
  
  int root = 0;
  int D = atoi(argv[1]);
  int *buf =  (int*)(malloc(sizeof(int)*D));
  char hostname[MPI_MAX_PROCESSOR_NAME];
  int numsegments = 10;
  // initialize MPI
  MPI_Init (&argc, &argv);

  int numProc, rank, processorNameLen;

  // get number of processes
  MPI_Comm_size (MPI_COMM_WORLD, &numProc);

  //printf("numproc %d\n",numProc);

  // get my rank
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);

  //initialize the buffer the from the root
  if(rank == root){
    for(int i = 0; i<D;i++){
      buf[i] = i;
    }
  }

  double sTime = MPI_Wtime();
  // get the name of the processor
  MPI_Get_processor_name (hostname, &processorNameLen);

  //getting the groupNumber
  int groupNumber =  getGroupNumberFromHostname(hostname);

  //inverting the rank of the root process ensures that there
  //when we split comm world to make new commnicators that the rank 
  //of the root is always 0, this ensures we dont have incurs 
  //additional overhead of transferring new ranks.
  if(rank == root)
    rank = (-1)*rank;

  // nodes in the same group
  int color = groupNumber;
  int intraGroupRank, intraGroupSize;
  MPI_Comm intraGroupComm;

  // meta info of the group
  MPI_Comm_split (MPI_COMM_WORLD, color, rank, &intraGroupComm);
  MPI_Comm_rank( intraGroupComm, &intraGroupRank );
  MPI_Comm_size( intraGroupComm, &intraGroupSize );

  // group's leader subgroup for the communication
  int interGroupSize;
  MPI_Comm interGroupLeaderComm;
  if(intraGroupRank == 0)
    MPI_Comm_split (MPI_COMM_WORLD, 0, rank, &interGroupLeaderComm);
  else
    MPI_Comm_split (MPI_COMM_WORLD, MPI_UNDEFINED, rank, &interGroupLeaderComm);

  if(intraGroupRank == 0)
    MPI_Comm_size (interGroupLeaderComm, &interGroupSize);

  //broadcasting in the intergroupleadercomm
  //

  if(intraGroupSize > 1){
    MPI_Bcast(&interGroupSize, 1, MPI_INT, 0, intraGroupComm);
  }
  
  if(interGroupSize>1){
    MPI_Request request[numsegments];
    MPI_Status status[numsegments];
    MPI_Request request2[numsegments];
    MPI_Status status2[numsegments];
    int segmentsize = D/numsegments;
    for(int i = 0; i < numsegments; i++){
      if(intraGroupRank == 0){
        MPI_Ibcast(buf+i*segmentsize, segmentsize, MPI_INT,0, interGroupLeaderComm, &request[i]);
	int flag;
	MPI_Test(&request[i], &flag, &status[i]);
      }
      
       //printf("hi\n");
      
      if(intraGroupRank == 0){
        MPI_Wait(&request[i], &status[i]);
      }
      
       //printf("hi1\n");
      
      if(i >= 1){
        MPI_Wait(&request2[i-1], &status2[i-1]);
      }
      
       //printf("hi2\n");
      
      MPI_Ibcast(buf+i*segmentsize, segmentsize, MPI_INT,0, intraGroupComm, &request2[i]);
      int flag;
        MPI_Test(&request2[i], &flag, &status2[i]);
      //printf("hi3\n");
    }
    MPI_Wait(&request2[numsegments-1], &status2[numsegments-1]);
       //printf("hi4\n");
      
  }
  else if(interGroupSize == 1){
    //broadcasting withing a group
    if(intraGroupSize > 1){
      MPI_Bcast(buf, D, MPI_INT, 0, intraGroupComm);
    }
  }

  double eTime = MPI_Wtime();
  double time = eTime-sTime;
  double maxtime;
  MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, root, MPI_COMM_WORLD);

  if(rank == root){
    printf("%lf\n", maxtime);
  }

  // done with MPI
  MPI_Finalize();
  
  
  //for(int i = 0; i<D;i++){
  //  printf("%d ", buf[i]);
  //}
  //printf("\n");


  return 0;



}
