#include "model/comms_model.h"
#include "skd_ilp.h"

bool compareById(const mod_region& a, const mod_region& b) {
    return a.id < b.id;
}

std::chrono::_V2::system_clock::time_point start_time;
std::atomic<bool> solving_done{ false };




DataPacket* SKD_ILP::set_solver_things_and_solve()
{
    pr_info("Maximum TCO %ld min TCO %ld\n", get_max_TCO(), get_min_TCO());
    pr_info("Resource limit is %.2f and USD limit is %ld (min_tco + (max_tco - min_tco) * limit)\n", get_tco_percent(), resource_limit);
    pr_info("Total number of regions %d and tiers %d\n", num_regions, num_tiers);
    pr_info("Maximum perf loss %ld min perf loss: %ld \n----------\n", get_max_perf_loss(), get_min_perf_loss());

    /* ======================= */

    start_time = chrono::high_resolution_clock::now(); // get start time
    DataPacket* dp = nullptr;
    // Variables
    vector<vector<MPVariable*> > pos_ident(num_regions, vector<MPVariable*>(num_tiers));

    unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
    // unique_ptr<MPSolver> solver(MPSolver::CreateSolver("CP-SAT"));
    absl::Status ret = solver->SetNumThreads(6);  // <======================================================================
    solver->SetTimeLimit(absl::Seconds(50));

    if (!solver) {
        LOG(WARNING) << "SCIP solver unavailable.";
        pos_ident.clear();
        return dp;
    }

    /* ============================================================================ */
    /* Placement boolean */
    for (int region_idx = 0; region_idx < num_regions; region_idx++) {
        for (int tier_idx = 0; tier_idx < num_tiers; tier_idx++) {
            pos_ident[region_idx][tier_idx] = solver->MakeBoolVar(absl::StrFormat("x_%d_%d", region_idx, tier_idx));
        }
    }

    /* ============================================================================ */
        // Constraints. Every region should be only in one tier.
    for (int region_idx = 0; region_idx < num_regions; region_idx++) {
        LinearExpr sum;
        for (int tier_idx = 0; tier_idx < num_tiers; tier_idx++) {
            sum += pos_ident[region_idx][tier_idx];
        }
        solver->MakeRowConstraint(sum == 1.0);
    }


    /* formula to cost the placement of tier. */
    LinearExpr tota_usd_cost;
    for (int region_idx = 0; region_idx < num_regions; region_idx++) {
        for (int tier_idx = 0; tier_idx < num_tiers; tier_idx++) {

            tota_usd_cost += LinearExpr(pos_ident[region_idx][tier_idx]) *
            INIT_REGION_MB * MULTIPLIER_FACTOR 
            // grp_regions_size_mb[region_idx] 
            * tier_com[tier_idx] * tier_costs[tier_idx];

        }
    }
    if (resource_limit < get_min_TCO()) {
        fprintf(stderr, "WARN: Resource limit is less than min TCO\n");
        return nullptr;
    }
    solver->MakeRowConstraint(tota_usd_cost <= resource_limit);



    /* ============================================================================ */
        // Objective
    MPObjective* const objective = solver->MutableObjective();

    LinearExpr
        objective_value; /* this will be perfromance overhead */
    for (int region_idx = 0; region_idx < num_regions; region_idx++) {
        for (int tier_idx = 0; tier_idx < num_tiers; tier_idx++) {
            {
                objective_value += LinearExpr(pos_ident[region_idx][tier_idx]) *
                    grp_hotness[region_idx] * tier_lats[tier_idx];
            }
        }
    }

    /* we minimize, irrespective of USD or performance loss */
    objective->MinimizeLinearExpr(objective_value);

    /* ============================================================================
==============================SOLVING========================================
============================================================================ */

    auto elapsed_time = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start_time); // calculate elapse
    printf("***ILP Solver: Time to set things up the ILP %ld micro seconds and in seconds %ld\n", elapsed_time.count(), elapsed_time.count() / 1000000);


    // std::vector<std::thread> threads;
    // threads.emplace_back([&] {
    //     int t = 0;
    //     while (!solving_done) {
    //         fprintf(stderr, "Solving: Time elapsed: %d seconds\n", t);
    //         usleep(1000000);
    //         t += 1;
    //     }
    //     });

    std::thread progress_thread([&] {
        using namespace std::chrono;
        auto start = steady_clock::now();
        int last_reported = 0;

        while (!solving_done) {
            std::this_thread::sleep_for(seconds(1));

            auto now = steady_clock::now();
            int elapsed = duration_cast<seconds>(now - start).count();

            if (elapsed - last_reported >= 10) {
                fprintf(stderr, "Solving: Time elapsed: %d seconds\n", elapsed);
                last_reported = elapsed;
            }
        }
        });

    // Solve
    MPSolver::ResultStatus result_status = solver->Solve();
    solving_done = true;
    // for (auto& thread : threads) {
    //     thread.join();
    // }

    progress_thread.join();  // Wait for logger thread to exit

    if (result_status != MPSolver::OPTIMAL) {
        pr_err("ILP Solver: The problem does not have an optimal solution. Result status: %d\n", result_status);
        pos_ident.clear();
        return nullptr;
    }
    else {
        pr_info("ILP Solver: The problem has an optimal solution.\n");
    }

    // print time elapsed in microseconds
    elapsed_time = chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start_time); // calculate elapse
    pr_info("***ILP Solver: Time to solve the ILP %ld micro seconds and in seconds %ld\n", elapsed_time.count(), elapsed_time.count() / 1000000);


    prepare_data_packet(pos_ident, dp, objective);




    return dp;
}

void SKD_ILP::prepare_data_packet(std::vector<std::vector<operations_research::MPVariable*>>& pos_ident, DataPacket*& dp, operations_research::MPObjective* const objective)
{
    dst_tiers.clear();

    if (merged && mod_regions.size() > 0) {
        std::sort(mod_regions.begin(), mod_regions.end(), compareById);
    }

    /* Print answers */
    // for (int region_idx = 0; region_idx < num_regions; region_idx++) 

    for (uint64_t k = 0; k < mod_regions.size(); k++) {
        int region_idx;
        if (merged)
            region_idx = mod_regions[k].group_id;
        else
            region_idx = k;

        int is_accounted = 0;
        for (int tier_idx = 0; tier_idx < num_tiers; tier_idx++) {
            if (pos_ident[region_idx][tier_idx]->solution_value() == 1) {
                is_accounted = 1;
                dst_tiers.push_back((tier_idx)); // virt tier
                break;
            }
        }
        if (is_accounted == 0) {
            throw std::runtime_error("BIG ERROR A Region has not been assigned a TIER.\n");
        }
    }

    pr_info("Total regions %ld mod regions %d merged %d \n", dst_tiers.size(), num_regions, merged);

    float total_other_value = 0;



    for (int tier_idx = 0; tier_idx < num_tiers; tier_idx++) {
        for (int region_idx = 0; region_idx < num_regions; region_idx++) {
            if (pos_ident[region_idx][tier_idx]->solution_value() == 1) {
                //this region is in this tier
                /* As we have a limit on the USD, we are optimizing performance.
Objetive value will give us performance. USD we have to calculate */
                total_other_value += 
                // grp_regions_size_mb[region_idx] 
                INIT_REGION_MB * MULTIPLIER_FACTOR 
                * tier_com[tier_idx] * tier_costs[tier_idx];

            }
        }
    }




    dp = new DataPacket();
    dp->final_obejctive_value = objective->Value();
    dp->resource_limit = resource_limit;
    dp->total_other_value = total_other_value;

    dp->min_tco = get_min_TCO();
    dp->max_tco = get_max_TCO();
    dp->min_perf_loss = get_min_perf_loss();
    dp->max_perf_loss = get_max_perf_loss();
}
