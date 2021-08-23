#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "mpi.h"
#include "time.h"

int getRandom(int lower, int upper){
    int num = (rand() %(upper - lower + 1)) + lower;
    return num;
}
extern int getGroupNumberFromHostname(char* hostname);

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
    // get my rank
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);

    // get the name of the processor
    MPI_Get_processor_name (hostname, &processorNameLen);

    //getting the group number and the host name
    int groupNumber =  getGroupNumberFromHostname(hostname);
    int csewsNumber = atoi(hostname+5);

    


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

        //get the number of processes at every node in the nodeleaders
        if (intraNodeRank == 0)
        {
            int numNodes;
            MPI_Comm_size( interNodeLeaderComm, &numNodes);
            int numProcAtNode[numNodes];
            MPI_Allgather(&intraNodeSize, 1, MPI_INT, numProcAtNode, 1, MPI_INT, interNodeLeaderComm);
            
            //now gather the data to be sent to the ith process, but before that you need to gather the counts
            //to be sent
            //sendCountssendCountsForProcess[i][j], has how much data is to be sent to process i(with respect to the new global comm)
            // for j(w.r.t to the intranode comm)
            int sendCountsForProcess[numProc][intraNodeSize];
            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newSendCounts[i], 1, MPI_INT, &sendCountsForProcess[i][0], 1, MPI_INT, 0, intraNodeComm);
            }

            
            int sendDisplacementsForProcess[numProc][intraNodeSize];
            int displ = 0;
            for(int i = 0; i < numProc; i++){
                for(int j = 0; j < intraNodeSize; j++){
                    sendDisplacementsForProcess[i][j] = displ;
                    displ += sendCountsForProcess[i][j];
                }
            }

            //now we will calculate the displacements and receive counts, and after that we will start receiving the data
            int displacementsOfDataOfIthProc[numProc];
            int countsOfDataOfIthProc[numProc];
            displ = 0;
            for(int i = 0; i < numProc; i++){
                displacementsOfDataOfIthProc[i] = displ;
                countsOfDataOfIthProc[i] = 0;
                for(int j = 0; j < intraNodeSize; j++){
                    countsOfDataOfIthProc[i] += sendCountsForProcess[i][j];
                }
                displ += countsOfDataOfIthProc[i];
            }


            //now displ = the total data size
            
            double * intraNodeLeaderSendBuffer = (double*)malloc(sizeof(double)*displ);
            for(int i = 0; i < numProc; i++){
                MPI_Gatherv(sendBuf+sendDisplacement[mapFromNewToOldRanks[i]], newSendCounts[i], MPI_DOUBLE,
                intraNodeLeaderSendBuffer, &sendCountsForProcess[i][0], &sendDisplacementsForProcess[i][0],
                MPI_DOUBLE, 0, intraNodeComm);
            } 

            //recvCountsForProcess[i][j], has how much data is to be recv from process i(with respect to the new global comm)
            // to j(w.r.t to the intranode comm)
            int recvCountsForProcess[numProc][intraNodeSize];
            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newRecvCounts[i], 1, MPI_INT, &recvCountsForProcess[i][0], 1, MPI_INT, 0, intraNodeComm);
            }

            //now we will calculate the displacements and receive counts, and after that we will start receiving the data
            int recvDisplacementsOfDataOfIthProc[numProc];
            int recvCountsOfDataOfIthProc[numProc];
            displ = 0;
            for(int i = 0; i < numProc; i++){
                recvDisplacementsOfDataOfIthProc[i] = displ;
                recvCountsOfDataOfIthProc[i] = 0;
                for(int j = 0; j < intraNodeSize; j++){
                    recvCountsOfDataOfIthProc[i] += recvCountsForProcess[i][j];
                }
                displ += recvCountsOfDataOfIthProc[i];
            }

            //double intraNodeLeaderRecvBuffer[displ];
            double * intraNodeLeaderRecvBuffer = (double*)malloc(sizeof(double)*displ);

            //now we will call mpialltoallv at the node leader level, but we have to  calculate the receive 
            //counts, displacements, send counts and displacements 
            int nodeLeaderSendCounts[numNodes];
            int nodeLeaderSendDisplacements[numNodes];
            int nodeLeaderRecvCounts[numNodes];
            int nodeLeaderRecvDisplacements[numNodes];

            //find the rank of the first process at every node
            //lets say there are 2 processes at node 1, 2 proc at node 2 and 2 proc at node 3, then this is 
            //[0, 2, 4]
            int rankOfFirstProcAtNode[numNodes];
            int cummRank = 0;
            for(int i =0 ; i < numNodes; i++){
                rankOfFirstProcAtNode[i] = cummRank;
                cummRank +=  numProcAtNode[i];
            }

            for(int i =0 ; i < numNodes ; i++){
                nodeLeaderSendCounts[i] = 0;
                nodeLeaderRecvCounts[i] = 0;
                for(int j = rankOfFirstProcAtNode[i]; j < rankOfFirstProcAtNode[i] + numProcAtNode[i]; j++){
                    nodeLeaderSendCounts[i] += countsOfDataOfIthProc[j];
                    nodeLeaderRecvCounts[i] += recvCountsOfDataOfIthProc[j];
                }
                nodeLeaderSendDisplacements[i] = displacementsOfDataOfIthProc[rankOfFirstProcAtNode[i]];
                nodeLeaderRecvDisplacements[i] = recvDisplacementsOfDataOfIthProc[rankOfFirstProcAtNode[i]];
            }
            
            MPI_Alltoallv(intraNodeLeaderSendBuffer, nodeLeaderSendCounts,
                        nodeLeaderSendDisplacements, MPI_DOUBLE, intraNodeLeaderRecvBuffer,
                        nodeLeaderRecvCounts, nodeLeaderRecvDisplacements, MPI_DOUBLE, interNodeLeaderComm);

            int recvDisplacementsForProcess[numProc][intraNodeSize];
            displ = 0;
            for(int i = 0; i < numNodes; i++){
                for(int k = 0; k < intraNodeSize; k++){
                    for(int j=rankOfFirstProcAtNode[i];j<rankOfFirstProcAtNode[i]+numProcAtNode[i];j++){
                        recvDisplacementsForProcess[j][k] = displ;
                        displ += recvCountsForProcess[j][k];
                    }
                }
            }

            //scatter the data to each proc
            for(int i = 0; i < numProc; i++){
                MPI_Scatterv(intraNodeLeaderRecvBuffer, &recvCountsForProcess[i][0], &recvDisplacementsForProcess[i][0], MPI_DOUBLE, 
                recvBuf + recvDisplacement[mapFromNewToOldRanks[i]], newRecvCounts[i], MPI_DOUBLE,
                0, intraNodeComm);
            }

            //free the memory
            free(intraNodeLeaderRecvBuffer);
            free(intraNodeLeaderSendBuffer);
        }
        else{
            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newSendCounts[i], 1, MPI_INT, NULL, 0, MPI_INT, 0, intraNodeComm);
            }

            //now we start gathering the data
            for(int i = 0; i < numProc; i++){
                MPI_Gatherv(sendBuf+sendDisplacement[mapFromNewToOldRanks[i]], newSendCounts[i], MPI_DOUBLE, NULL, 0, 0, MPI_DOUBLE, 0, intraNodeComm);
            }

            for(int i = 0; i < numProc; i++){
                MPI_Gather(&newRecvCounts[i], 1, MPI_INT, NULL, 0, MPI_INT, 0, intraNodeComm);
            }

            //now we scatter the data
            for(int i = 0; i < numProc; i++){
                MPI_Scatterv(NULL, NULL, NULL, MPI_DOUBLE, 
                recvBuf + recvDisplacement[mapFromNewToOldRanks[i]], newRecvCounts[i], MPI_DOUBLE,
                0, intraNodeComm);
            }

        }
        //ending the time
        eTime = MPI_Wtime();
        time = eTime-sTime;
        maxtime;
        MPI_Reduce(&time, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        total_time += maxtime;
    }
    return (total_time/(double)iter);

}
