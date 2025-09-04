
#include <iostream>
#include <vector>
#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>
#include <random>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include "skd_ilp.h"
#include "utils_server.h"
#include "utils.h"

#define PORT 8080


using namespace std;

int nr_tiers, nr_regions;
bool is_fake = false;
int server_fd, new_socket, valread;

vector<TierInfo_Comm*> tiers;
vector<uint64_t> regions;
struct sockaddr_in address;
int opt = 1;

void printRecvInfo()
{
	printf("printRecvInfo ILP... %lu %lu\n", regions.size(), tiers.size());



	uint64_t total_non_zero_hot_regions = 0;
	for (int i = 0; i < nr_regions; i++) {
		if (regions[i] > 0ul) {
			total_non_zero_hot_regions++;
		}
	}
	printf("**********\nTotal non zero hot regions: %lu\n*******\n", total_non_zero_hot_regions);
}


// Signal handler function
void sigintHandler(int signal)
{
	cout << "SIGINT received. Exiting..." << endl;
	// Perform any necessary cleanup or other actions
	// ...
	close(new_socket);
	close(server_fd);

	// Terminate the program
	exit(signal);
}


int init_connection()
{
	// Create server socket
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Set socket options
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
		sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	// Bind socket to a specific port
	if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// Listen for incoming connections
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	return 0;
}



void read_fake_data(int *iter) {
	printf("READING FAKE DATA\n");

	(*iter)--;

	nr_tiers = 4;
	nr_regions = 26172;
	is_fake = true;


	// TierInfo_Comm(int virt_tier_id, bool isCPU, int backing_store, 
	// float compression_ratio, int tier_cost, int tier_latency)
	TierInfo_Comm* object = new TierInfo_Comm(1, 10, 1);
	tiers.push_back(object);

	object = new TierInfo_Comm(1, 3, 3);
	tiers.push_back(object);

	object = new TierInfo_Comm( .5, 10, 5);
	tiers.push_back(object);

	object = new TierInfo_Comm(.5, 3, 8);
	tiers.push_back(object);


	std::vector<int> lastColumnValues = readLastColumnIntegers("../hotness_data");
	for (int i = 0; i < nr_regions; i++) {
		regions.push_back(lastColumnValues[i]);
	}


	
	 std::sort(regions.begin(), regions.end());
	
	// printHistogram(regions, 10);



}

void read_data() {

	printf("Waiting for a connection...\n");
	int addrlen = sizeof(address);
	// Accept incoming connection
	if ((new_socket = accept(server_fd, (struct sockaddr*)&address,
		(socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	// Receive the number of objects
	valread = read(new_socket, &nr_tiers, sizeof(nr_tiers));
	// fprintf(stderr, "valread is %d\n", valread);
	valread = read(new_socket, &nr_regions, sizeof(nr_regions));
	// fprintf(stderr, "valread is %d\n", valread);

	printf("Received tiers %d, regions %d\n", nr_tiers, nr_regions);

	/* Read tiers */
	for (int i = 0; i < nr_tiers; i++) {
		TierInfo_Comm* object = new TierInfo_Comm();
		valread = read(new_socket, object, sizeof(TierInfo_Comm));
		// fprintf(stderr, "valread is %d\n", valread);


		tiers.push_back(object);
	}



	/* Read regions */
	for (int i = 0; i < nr_regions; i++) {
		uint64_t object;
		valread = read(new_socket, &object, sizeof(uint64_t));
		// fprintf(stderr, "valread is %d\n", valread);
		regions.push_back(object);
	}



	printRecvInfo();

	fprintf(stderr, "Received %d tiers and %d regions\n", nr_tiers,
		nr_regions);
}

void send_results(DataPacket* dp, SKD_ILP* skd_ilp) {
	if (dp == nullptr) {
		fprintf(stderr,
			"WARN: ILP failed dst_tier elements %ld nr_regions %d\n",
			skd_ilp->dst_tiers.size(), nr_regions);


		skd_ilp->dst_tiers.clear();
		for (int i = 0; i < nr_regions; i++) {
			skd_ilp->dst_tiers.push_back(DST_IGNORE);
		}

		dp = new DataPacket();
		dp->failed = true;

	}
	else {
		
		dp->failed = false;
		dp->toString();
	}

	printf("Sending results back to client...\n");

	unordered_map<int, int> countMap;
	for (uint64_t i = 0; i < skd_ilp->dst_tiers.size(); i++) {
		countMap[skd_ilp->dst_tiers[i]]++;
	}
	for (int i = 0; i < nr_tiers; i++) {
		countMap[i]++;
		countMap[i]--;
	}

	std::map<int, int> sortedMap(countMap.begin(), countMap.end());
	printf("Seding recommendation\n");
	for (const auto& pair : sortedMap) {
		// printf("Region in tier %d is %d\n", pair.first, pair.second);
		printf("[ %d: %d ] ", pair.first, pair.second);
	}
	printf("\n");

	if (!is_fake) {
		valread = write(new_socket, dp, sizeof(DataPacket));
		// Send the dst_tiers to the client
		for (int i = 0; i < nr_regions; i++) {
			// printf("Sending region %d/%d to client\n", i, nr_regions);
			valread = write(new_socket, &(skd_ilp->dst_tiers[i]),
				sizeof(int));
		}
	}
	else {
		printf("NOT Sending fake data\n");
	}

}
void cleanup(SKD_ILP* skd_ilp) {
	fprintf(stderr, "Deleting stuff\n");

	try
	{
		for (auto* tierInfo : tiers) {
			delete tierInfo;
		}


		tiers.clear();
		regions.clear();

		delete skd_ilp;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}

	fprintf(stderr, "Deleted stuff\n");
}
