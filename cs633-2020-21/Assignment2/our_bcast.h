extern double MPI_Bcast_def(int count, int root, int iter);
extern double MPI_Bcast_opt_node_group(int count, int root, int iter);
extern double MPI_Bcast_opt_node_only(int count, int root, int iter);
extern double MPI_Bcast_opt_group_only(int count, int root, int iter);
extern double MPI_Bcast_opt_rank_only(int count, int root, int iter);