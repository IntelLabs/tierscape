// Server code

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

#include "skd_ilp.h"
#include "model/model.h"
#include "utils/utils_server.h"
#include "utils/utils_ilp.h"


std::mutex mtx;
std::condition_variable cv;
bool solvingCompleted = false;

void solveWithTimeout(SKD_ILP* skd_ilp, int timeoutSeconds, DataPacket** dp)
{
	*dp = skd_ilp->set_solver_things_and_solve();

	// Notify the main thread that solving is completed
	{
		std::lock_guard<std::mutex> lock(mtx);
		solvingCompleted = true;
	}
	cv.notify_one();
	return;
}




int main(int argc, char* argv[]) {

	float tco_percent;
	int ret;
	if (argc < 2) {
		std::cout << "USING DEFAULT val of .5 Please provide an integer argument between (0 and 1)." << std::endl;
		tco_percent = .5;
	}
	else {
		tco_percent = std::atof(argv[1]);
	}

	if (tco_percent <= 0 || tco_percent >= 1) {
		fprintf(stderr, "tco_percent should be between 0 and 1 (exclusive): %f\n", tco_percent);
		exit(1);
	}
	
	pr_info("init tco_percent is %f\n", tco_percent);
	pr_info("multipier factor is %d\n", MULTIPLIER_FACTOR);


	try {

		signal(SIGINT, sigintHandler);
		ret = init_connection();
		if (ret != 0) {
			fprintf(stderr, "init connection failed. Ret is %d\n", ret);
			return -1;
		}

		int iter=1;
		while (iter)
		{
			read_data();
			// read_fake_data(&iter);

			SKD_ILP* skd_ilp = new SKD_ILP();
			skd_ilp->init_data_from_comms(regions, tiers);
			skd_ilp->set_resource_limit(tco_percent); // 50% of the TCO


			int timeoutSeconds = 100;  // Set a timeout in seconds
			// int timeoutSeconds = 20;  // Set a timeout in seconds
			DataPacket* dp = nullptr;

			// solveWithTimeout(skd_ilp, timeoutSeconds, &dp);

			std::future<void>* hThread = new std::future<void>(std::async(std::launch::async, solveWithTimeout, skd_ilp, timeoutSeconds, &dp));
			if (hThread->wait_for(std::chrono::seconds(timeoutSeconds)) == std::future_status::timeout) {
				std::cout << "FUTURE Solver timeout reached. Stopping the solving process." << std::endl;
			}
			else {
				std::cout << "FUTURE Solver completed." << std::endl;
				delete hThread;
				hThread = nullptr;
			}


			send_results(dp, skd_ilp);
			cleanup(skd_ilp);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}

	printf("Closing socket.. Done with EXP\n");
	// getchar();

	return 0;
}
