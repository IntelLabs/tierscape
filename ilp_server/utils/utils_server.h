#ifndef UTILS_SERVER_H
#define UTILS_SERVER_H

void read_data();
void read_fake_data(int *);
void send_results(DataPacket* dp, SKD_ILP* skd_ilp);

extern int nr_tiers, nr_regions;
extern  vector<TierInfo_Comm*> tiers;
extern vector<uint64_t> regions;

extern int server_fd, new_socket, valread;
extern struct sockaddr_in address;

int init_connection();
void sigintHandler(int signal);
// void printRecvInfo(vector<uint64_t > regions, vector<TierInfo_Comm*> tiers);
void printHistogram(std::vector<uint64_t> numbers, uint64_t numBuckets);

// extern int MULTIPLIER_FACTOR;
#endif