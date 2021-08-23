extern int getRandom(int lower, int upper);
extern double standard_alltoallv(double*sendBuf, int* sendCounts, int*sendDisplacement, double* recvBuf, int* recvCounts, int* recvDisplacement, int iter );
extern double optimized_alltoallv(double*sendBuf, int* sendCounts, int*sendDisplacement, double* recvBuf, int* recvCounts, int* recvDisplacement, int iter);
