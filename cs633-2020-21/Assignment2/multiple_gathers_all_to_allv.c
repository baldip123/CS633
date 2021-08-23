#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"
#include "time.h"

int nodeToGroupMap[93] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0, 3, 0, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

int getGroupNumberFromHostname(char* hostname){
  int csewsNumber = atoi(hostname+5);
  return nodeToGroupMap[csewsNumber];
}

int getRandom(int lower, int upper){
    int num = (rand() %(upper - lower + 1)) + lower;
    return num;
}

double standard_alltoallv(double*sendBuf, int* sendCounts, int*sendDisplacement, double* recvBuf, int* recvCounts, int* recvDisplacement, int iter ){
    double total_time = 0;
    for(int i = 0; i< iter; i++){
        double sTime = MPI_Wtime();
        MPI_Alltoallv(sendBuf, sendCounts, sendDisplacement, MPI_DOUBLE, recvBuf,
                        recvCounts, recvDisplacement, MPI_DOUBLE, MPI_COMM_WORLD);
        double eTime = MPI_Wtime();

        double time = eTime-sTime;
        double maxtime;
        MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        total_time += maxtime;
    }
    return (total_time/(double)iter);
}

double optimized_alltoallv(double*sendBuf, int* sendCounts, int*sendDisplacement, double* recvBuf, int* recvCounts, int* recvDisplacement, int iter){
    
    // start time for making communicator
    double total_time = 0;
    double sTime = MPI_Wtime();

    char hostname[MPI_MAX_PROCESSOR_NAME];
    int numProc, rank, processorNameLen;
    // get number of processes
    MPI_Comm_size (MPI_COMM_WORLD, &numProc);
    // printf("numproc here %d\n", numProc);
    // get my rank
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);

    // get the name of the processor
    MPI_Get_processor_name (hostname, &processorNameLen);

    //getting the group number and the host name
    int groupNumber =  getGroupNumberFromHostname(hostname);
    int csewsNumber = atoi(hostname+5);
    // printf("csews number here %d", csewsNumber);
    


    //make a communicator with all the processes but the ranks such the 
    //all process in the same nodes are together and all process in the
    //same group are together, example : lets say there are 4 process,
    //0 on node 2, 1 on node3, 2 on node2 and 3 on node4, then in the 
    //new communicator, 0 is on 2, 1 is on 2 , 2 is on 3 and 3 is on 3.
    //This is to help us keep the data to be sent to a node contiguous 

    MPI_Comm CommWorldNew;
    int key = 1000000*groupNumber + 1000*csewsNumber + rank;
    MPI_Comm_split (MPI_COMM_WORLD, 0, key, &CommWorldNew);

    //get the new rank of each process with respect to the CommWorldNew
    int CommWorldNewRank;
    MPI_Comm_rank (CommWorldNew, &CommWorldNewRank);

    //make intranode communicators
    MPI_Comm intraNodeComm;
    MPI_Comm_split (CommWorldNew, csewsNumber, CommWorldNewRank, &intraNodeComm);

    //get the rank in the intranode communicator
    int intraNodeRank;
    MPI_Comm_rank (intraNodeComm, &intraNodeRank);
    //get the size of the intranode communicator;
    int intraNodeSize;
    MPI_Comm_size (intraNodeComm, &intraNodeSize);
    

    // make internode leader communicators
    MPI_Comm interNodeLeaderComm;
    if(intraNodeRank == 0){
        //giving the colour 0 as the colour does not really matter here
        MPI_Comm_split (CommWorldNew, 0, CommWorldNewRank, &interNodeLeaderComm);
    }
    else{
        // the other process also need to call it but since they dont need
        // to be part of this communicator we keep the color as 
        // MPI_UNDEFINED.
        MPI_Comm_split (CommWorldNew, MPI_UNDEFINED, CommWorldNewRank, &interNodeLeaderComm);
    }


    //make a map from the old ranks in the MPI_COMM_WORLD to the new 
    //ranks in the CommWorldNew
    int mapFromOldToNewRanksTemp[numProc];
    for(int i = 0; i < numProc; i++){
        mapFromOldToNewRanksTemp[i] = 0;
    }
    mapFromOldToNewRanksTemp[rank] = CommWorldNewRank;
    int mapFromOldToNewRanks[numProc];
    MPI_Allreduce(mapFromOldToNewRanksTemp, mapFromOldToNewRanks, numProc, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    int mapFromNewToOldRanks[numProc];
    for(int i = 0; i < numProc; i++){
        mapFromNewToOldRanks[mapFromOldToNewRanks[i]] = i;
    }

    //now the communicators and other bookeeping data has been made
    double eTime = MPI_Wtime();
    double time = eTime-sTime;
    double maxtime;
    MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    total_time += maxtime;
    //adding time used for making communicators only once as this communicator can be used for multiple sends




    //adding this in a loop
    for(int t=0;t < iter; t++){

        //starting the time
        sTime = MPI_Wtime();
        //getting the new send counts and the new receive counts with respect to the new ranks
        int newSendCounts[numProc];
        for(int i = 0; i < numProc; i++){
            newSendCounts[i] = sendCounts[mapFromNewToOldRanks[i]];
        }

        int newRecvCounts[numProc];
        for(int i = 0; i < numProc; i++){
            newRecvCounts[i] = recvCounts[mapFromNewToOldRanks[i]];
        }

        int totalRecvCount = 0;
        for(int j =0 ; j < numProc ; j++){
            totalRecvCount += recvCounts[j];
        }
        
        double* newRecvBuf = (double *)malloc(sizeof(double)*totalRecvCount);

        //get the number of processes at every node in the nodeleaders
        if (intraNodeRank == 0)
        {
            int numNodes;
            MPI_Comm_size( interNodeLeaderComm, &numNodes);
            int numProcAtNode[numNodes];
            MPI_Allgather(&intraNodeSize, 1, MPI_INT, numProcAtNode, 1, MPI_INT, interNodeLeaderComm);
            int interNodeLeaderRank;
            MPI_Comm_rank( interNodeLeaderComm, &interNodeLeaderRank);

            //make a map from the rank of the process to the node to which it belongs
            int mapFromNewRankToNode[numProc];
            int cummNumProc = 0;
            for(int i =0; i < numNodes; i++){
                for(int j = cummNumProc; j < cummNumProc + numProcAtNode[i] ; j++){
                    mapFromNewRankToNode[j] = i;
                }
                cummNumProc += numProcAtNode[i];
            }

            // printf("num nodes here %d", numNodes);
            
            //now gather the data to be sent to the ith process, but before that you need to gather the counts
            //to be sent
            //sendCountssendCountsForProcess[i][j], has how much data is to be sent to process i(with respect to the new global comm)
            // for j(w.r.t to the intranode comm)
            int sendCountsForProcess[numProc][intraNodeSize];
            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newSendCounts[i], 1, MPI_INT, &sendCountsForProcess[i][0], 1, MPI_INT, 0, intraNodeComm);
            }

            //recvCountsForProcess[i][j], has how much data is to be recv from process i(with respect to the new global comm)
            // to j(w.r.t to the intranode comm)
            int recvCountsForProcess[numProc][intraNodeSize];
            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newRecvCounts[i], 1, MPI_INT, &recvCountsForProcess[i][0], 1, MPI_INT, 0, intraNodeComm);
            }
            
            //now gather the data to be sent to the ith process according to the new rank and gather it at the node leader of the 
            //ith rank and the send it to process ith process and reorder it there
            for(int i = 0; i < numProc; i++){
                int intraNodeGatherDisplacements[intraNodeSize];
                int displ = 0;
                for(int j = 0; j < intraNodeSize ; j++){
                    intraNodeGatherDisplacements[j] = displ;
                    displ += sendCountsForProcess[i][j];
                }
                //now displ is the size of the total data to be received in the node leader
                // printf("reached here\n");
                double * intraNodeLeaderSendBuffer = (double*)malloc(sizeof(double)*displ);
                MPI_Gatherv(sendBuf+sendDisplacement[mapFromNewToOldRanks[i]], newSendCounts[i], MPI_DOUBLE,
                intraNodeLeaderSendBuffer, &sendCountsForProcess[i][0], intraNodeGatherDisplacements,
                MPI_DOUBLE, 0, intraNodeComm);

                //now gather the data in the node leader of the process i
                if(interNodeLeaderRank == mapFromNewRankToNode[i]){

                    int dataSizeFromThisNode = 0;
                    for(int j = 0; j < intraNodeSize; j++){
                        dataSizeFromThisNode += sendCountsForProcess[i][j];
                    }

                    //find the counts coming form each node
                    int countsComingFromEachNode[numNodes];

                    //gather the counts coming from every node
                    MPI_Gather(&dataSizeFromThisNode, 1, MPI_INT, countsComingFromEachNode, 1, MPI_INT, mapFromNewRankToNode[i], interNodeLeaderComm);

                    //calculate the displacements
                    int interNodeGatherDisplacements[numNodes];
                    int displ = 0;
                    for(int j = 0; j < numNodes ; j++){
                        interNodeGatherDisplacements[j] = displ;
                        displ += countsComingFromEachNode[j];
                    }

                    //make a buffer to gather the data coming from each node
                    double * interNodeLeaderGatherBuffer = (double*)malloc(sizeof(double)*displ);


                    //get the count getting sent from this node
                    

                    // printf("reached here 2222222\n");
                    MPI_Gatherv(intraNodeLeaderSendBuffer, dataSizeFromThisNode, MPI_DOUBLE,
                    interNodeLeaderGatherBuffer, countsComingFromEachNode, interNodeGatherDisplacements,
                    MPI_DOUBLE, mapFromNewRankToNode[i], interNodeLeaderComm);

                    //Note that this code is prome to deadlock as you are sending to and receiving from a node leader
                    // put the check here if this is a node leader and other wise dont free the buffer
                    //now send this buffer to the relevant process
                    // printf("reached here start of the send ");
                    // MPI_Send(interNodeLeaderGatherBuffer, displ, MPI_DOUBLE, i, 0, CommWorldNew);
                    // printf("reached here end of the send ");
                    if(i != CommWorldNewRank){
                        //recv the data from the node leader here
                        MPI_Send(interNodeLeaderGatherBuffer, displ, MPI_DOUBLE, i, 0, CommWorldNew);
                        free(interNodeLeaderGatherBuffer);
                    }
                    else{
                        //copy the internodeleadergather buffer in the new recv buff and then free it
                        for(int j = 0; j < totalRecvCount;j++){
                            newRecvBuf[j] = interNodeLeaderGatherBuffer[j];
                        }
                        free(interNodeLeaderGatherBuffer);
                    }                    
                }
                else{
                    //send the data to the node leader of the process i
                    //get the count getting sent from this node
                    int dataSizeFromThisNode = 0;
                    for(int j = 0; j < intraNodeSize; j++){
                        dataSizeFromThisNode += sendCountsForProcess[i][j];
                    }

                    MPI_Gather(&dataSizeFromThisNode, 1, MPI_INT, NULL, 0, MPI_INT, mapFromNewRankToNode[i], interNodeLeaderComm);

                    //gather the data at the node leader of the process i
                    MPI_Gatherv(intraNodeLeaderSendBuffer, dataSizeFromThisNode, MPI_DOUBLE,
                    NULL, 0, NULL,
                    MPI_DOUBLE, mapFromNewRankToNode[i], interNodeLeaderComm);
                }

                //dont forget to free the memory
                free(intraNodeLeaderSendBuffer);
            }
        }
        else{
            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newSendCounts[i], 1, MPI_INT, NULL, 0, MPI_INT, 0, intraNodeComm);
            }

            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newRecvCounts[i], 1, MPI_INT, NULL, 0, MPI_INT, 0, intraNodeComm);
            }


            //now we start gathering the data
            

            for(int i = 0; i < numProc; i++){
                MPI_Gatherv(sendBuf+sendDisplacement[mapFromNewToOldRanks[i]], newSendCounts[i], MPI_DOUBLE, NULL, 0, 0, MPI_DOUBLE, 0, intraNodeComm);

                if(i == CommWorldNewRank){
                    //recv the data from the node leader here
                    MPI_Status stat;
                    MPI_Recv(newRecvBuf, totalRecvCount, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, CommWorldNew, &stat);
                }
            }

        

            
        }

        
        //now rearrange the data
        // int newRecvDisplacement[numProc];
        // int displ = 0;
        // for(int i = 0; i < numProc; i++){
        //     newRecvDisplacement[i] = displ;
        //     displ += newRecvCounts[i];
        // }

        // for(int i = 0; i < numProc; i++){
        //     int oldRank = mapFromNewToOldRanks[i];
        //     for(int j = 0; j < recvCounts[oldRank]; j++){
        //         recvBuf[recvDisplacement[oldRank]+j] = newRecvBuf[newRecvDisplacement[i]+j];
        //     }
        // }

        //dont forget to free
        free(newRecvBuf);  

        //ending the time
        eTime = MPI_Wtime();
        time = eTime-sTime;
        maxtime;
        MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        total_time += maxtime;
    }
    return (total_time/(double)iter);

}



int main( int argc, char *argv[])

{
  int D = atoi(argv[1]);
  char hostname[MPI_MAX_PROCESSOR_NAME];

  // initialize MPI
  MPI_Init (&argc, &argv);

  int numProc, rank, processorNameLen;

  // get number of processes
  MPI_Comm_size (MPI_COMM_WORLD, &numProc);

  // get my rank
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);

  //so that every process has unequal data being sent
  srand(time(0)+rank);
  int sendCounts[numProc];
  int totalCounts = 0;
  for(int i = 0; i < numProc; i++){
      sendCounts[i] = getRandom(0, D);
      totalCounts += sendCounts[i];
  }

  //find the displacements
  int sendDisplacement[numProc];
  int displ = 0;
  for(int i = 0; i < numProc; i++){
      sendDisplacement[i] = displ;
      displ += sendCounts[i];
  }

  //initialize the array
  double* sendBuf = (double*)malloc(sizeof(double)*totalCounts);
  for(int i = 0; i < totalCounts; i++){
      sendBuf[i] = i;
  }

  //get the receiveCounts from all the process
  int recvCounts[numProc];
  
  //get the counts of doubles being sent by each pro 
  MPI_Alltoall(sendCounts, 1, MPI_INT, recvCounts, 1, MPI_INT, MPI_COMM_WORLD);
  

  //get the total double being sent
  int totalRecvCount = 0;
  for(int i = 0; i < numProc; i++){
      totalRecvCount += recvCounts[i];
  }

  //define the receive buffer
  double* recvBuf = (double*)malloc(sizeof(double)*totalRecvCount);

  //define the receiveDisplacement
  int recvDisplacement[numProc];
  displ = 0;
  for(int i = 0; i < numProc; i++){
      recvDisplacement[i] = displ;
      displ += recvCounts[i];
  }

  //here each process has the counts
  //now we can call the functions for the standard alltoallv and our optimized alltoallv
  double standardTime =  standard_alltoallv(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement, 5);
  if(!rank){
    printf("%lf\n", standardTime);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double optimizedTime = optimized_alltoallv(sendBuf, sendCounts, sendDisplacement, recvBuf, recvCounts, recvDisplacement, 5);

  if(!rank){
    printf("%lf\n", optimizedTime);
  }

  //dont forget to free the buffers
  free(recvBuf);
  free(sendBuf);

  


  // done with MPI
  MPI_Finalize();

  
  return 0;
}

