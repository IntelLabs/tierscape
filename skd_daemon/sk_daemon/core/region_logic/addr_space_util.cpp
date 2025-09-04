
// #include <vector>
#include "addr_space_util.h"

#include "mapping_base.h"

using namespace std;


bool compare_AddrEntry_ptrs_by_addr(const AddrEntry* a, const AddrEntry* b) {
    return a->addr < b->addr;
}

int sort_count_unique_process_mod(vector<AddrEntry*> page_addrs, uint64_t min_val, uint64_t max_val, vector<AddrCountEntry*>* scup) {
    // vector<AddrCountEntry *> ret_scup;
    AddrCountEntry* ac;

    sort(page_addrs.begin(), page_addrs.end(), compare_AddrEntry_ptrs_by_addr);

    size_t l_page_addrs = page_addrs.size();

    uint64_t last_page_addr = 0ul;
    uint64_t curr_page_addr = 0ul;
    float last_page_time = 0.0;
    float curr_page_time = 0.0;

    uint64_t nr_discarded_address = 0, nr_min_discard = 0, nr_max_discard = 0, nr_small_range = 0;
    uint64_t cnt = 1;
    for (uint64_t i = 0; i < l_page_addrs; i++) {

        curr_page_addr = page_addrs[i]->addr;
        curr_page_time = page_addrs[i]->addr_time;

        if (curr_page_addr < min_val || curr_page_addr > max_val || curr_page_addr == 0) {
            nr_discarded_address++;
            if (curr_page_addr <= 202686720lu && curr_page_addr >= 6072384lu) {
                nr_small_range++;
            }
            if (curr_page_addr < min_val)
                nr_min_discard++;
            if (curr_page_addr > max_val)
                nr_max_discard++;
            continue;
        }

        if (i == 0 || last_page_addr == 0) {
            last_page_addr = curr_page_addr;
            last_page_time = curr_page_time;
            continue;
        }

        if (curr_page_addr == last_page_addr) {
            // Keep counting
            cnt++;
            continue;
        }
        if (last_page_addr == 0ul) {
            throw runtime_error("last_page_addr is 0");
        }

        /* Save the last entry */
        ac = new AddrCountEntry();
        ac->page_addr = last_page_addr;
        ac->addr_time = last_page_time;
        ac->count = cnt;
        scup->push_back(ac);

        cnt = 1;
        last_page_addr = curr_page_addr;
        last_page_time = curr_page_time;
    }

    // handle the last element
    ac = new AddrCountEntry();
    ac->page_addr = last_page_addr;
    ac->addr_time = last_page_time;
    ac->count = cnt;
    scup->push_back(ac);

    pr_debug("Total discarded addresses: %lu, min: %lu, max: %lu, small range: %lu\n", nr_discarded_address, nr_min_discard, nr_max_discard, nr_small_range);

    return 0;

    // return a;
}

vector<AddrCountEntry*> sort_count_unique_process(vector<AddrEntry*> page_addrs) {
    vector<AddrCountEntry*> page_addrs_counts;
    AddrCountEntry* ac;

    sort(page_addrs.begin(), page_addrs.end(), compare_AddrEntry_ptrs_by_addr);

    size_t l_page_addrs = page_addrs.size();

    uint64_t last_page_addr = 0ul;
    uint64_t curr_page_addr = 0ul;
    // float last_page_time=0.0;
    float curr_page_time = 0.0;

    uint64_t cnt = 1;
    for (uint64_t i = 0; i < l_page_addrs; i++) {
        curr_page_addr = page_addrs[i]->addr;
        curr_page_time = page_addrs[i]->addr_time;

        if (curr_page_addr == last_page_addr) {
            cnt++;
            // Keep counting
        }
        else {
            if (i != 0) {
                // Need to account and prepare for the next address. if this is not the first address.
                ac = new AddrCountEntry();
                ac->page_addr = curr_page_addr;
                ac->count = cnt;
                ac->addr_time = curr_page_time;

                page_addrs_counts.push_back(ac);
            }

            cnt = 1;
            last_page_addr = curr_page_addr;
            // last_page_time = curr_page_time;
        }
    }

    // handle the last element
    ac = new AddrCountEntry();
    ac->page_addr = curr_page_addr;
    ac->count = cnt;
    ac->addr_time = curr_page_time;

    page_addrs_counts.push_back(ac);

    return page_addrs_counts;

    // return a;
}

hotness_and_entries* get_hotness_region(int start_idx, int end_idx, vector<AddrCountEntry*> a) {
    uint64_t min = 0ul, max = 0ul;
    float mean;

    vector<uint64_t> v;
    hotness_and_entries* hae = new hotness_and_entries();

    if (start_idx == end_idx) {

        hae->count_sum = a[start_idx]->count;
        hae->std = 0;  // the std of a single number is zero (x-u)^2/n. Here x==u and n=1
        hae->mean = a[start_idx]->count;
        hae->max = a[start_idx]->count;
        hae->min = a[start_idx]->count;

        return hae;
    }

    for (int i = start_idx; i < end_idx; i++) {
        v.push_back(a[i]->count);
    }

    uint64_t sum = std::accumulate(v.begin(), v.end(), 0.0);
    mean = sum / v.size();

    std::vector<float> diff(v.size());
    std::transform(v.begin(), v.end(), diff.begin(),
        std::bind2nd(std::minus<float>(), mean));
    float sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    float stdev = std::sqrt(sq_sum / v.size());

    max = *max_element(v.begin(), v.end());
    min = *min_element(v.begin(), v.end());

    hae->count_sum = sum;
    hae->std = stdev;
    hae->mean = mean;
    hae->max = max;
    hae->min = min;

    pr_debug("Processing get_hotness_region. DONE\n");
    return hae;
}

void print_ans(vector<REGION_SKD*> ans) {
    for (size_t i = 0; i < ans.size(); i++) {
        cout << ans[i]->start_address << "->" << ans[i]->get_end_address() << " h:" << ans[i]->get_hotness() << "\n";
    }
}

bool covers_the_val(uint64_t val, REGION_SKD* rd) {
    if (rd == NULL)
        return false;
    if (val >= rd->start_address && val <= rd->get_end_address())
        return true;
    else
        return false;
}

/* The function add region with access count as 0
It iwll also take care of merging the regions.
Current approach:
-  Simple count based.
*/
// vector<REGION_SKD *> fill_gap_smartly(vector<REGION_SKD *> regions, uint64_t start_addr, uint64_t end_addr) {

// }

bool customer_sorter(REGION_SKD* const lhs, REGION_SKD* const rhs) {
    return lhs->size_4k > rhs->size_4k;
}

vector<REGION_SKD*> collect_regions_sorted_by_size(vector<REGION_SKD*> regions, uint64_t hotness_level) {
    vector<REGION_SKD*> list_d;
    REGION_SKD* curr_region;

    for (uint64_t i = 0; i < regions.size(); i++) {
        curr_region = regions[i];

        if (hotness_level == 0) {
            if (curr_region->get_hotness() == 0) {
                list_d.push_back(curr_region);
            }
        }
        else {
            if (curr_region->get_hotness() != 0) {
                list_d.push_back(curr_region);
            }
        }
    }
    // sort list based on size.
    sort(list_d.begin(), list_d.end(), &customer_sorter);
    return list_d;
}



address_range get_address_range(int pid) {
    string start_addr, end_addr;
    char cmd[CHAR_FILE_LEN];

    const char* process_name = get_process_name_by_pid(pid);

    if (strstr(process_name, "memcached") != NULL) {
        pr_debug("Using Entries between heap and first mapped lib. Use for Memcached \n");
        /* Entries between heap and first mapped lib */
        sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/var/p'| sed -e '1d' -e '$d'|head -n 2|tail -n 1 |awk -F '-' '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/var/p'| sed -e '1d' -e '$d'|tail -n 1|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);
    }
    else if (strstr(process_name, "python") != NULL) {
        pr_debug("Using Entries between heap and first mapped lib. Use for PYTHON \n");
        /* Entries between heap and first mapped lib */
        sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/var/p'| sed -e '1d' -e '$d'|head -n 2|tail -n 1 |awk -F '-' '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/var/p'| sed -e '1d' -e '$d'|tail -n 1|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);
    }
    else if ((strstr(process_name, "XSBench") != NULL)) {
        pr_debug("Using Entries between heap and first mapped lib. Use for XSBench \n");
        sprintf(cmd, "cat /proc/%d/maps|grep heap -A 1 |tail -n 1|awk -F '-' '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/usr/p'| sed -e '1d' -e '$d'|tail -n 1|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);

    }
    else if ((strstr(process_name, "redis") != NULL)) {
        pr_debug("Using entries from the heap. Use for Redis \n");
        sprintf(cmd, "cat /proc/%d/maps|grep heap |tail -n 1|awk -F '-' '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| grep heap |tail -n 1|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);
    }
    else if (strstr(process_name, "PageRank") != NULL) {
        pr_debug("Using entries just the heap. Use for PageRank \n");
        sprintf(cmd, "cat /proc/%d/maps|grep heap |awk -F '-' '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| grep heap |awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);
    }
    else if (strstr(process_name, "pr") != NULL) {
        printf("Using entries just  heap. Use for PR  \n");
        sprintf(cmd, "pmap $(pidof pr)|grep anon|head -n -1|tail -n +2|head -n 1 |awk '{print $1}'\n");
        start_addr = exec(cmd);
        sprintf(cmd, "pmap $(pidof pr)|grep anon|head -n -1|tail -n +2|tail -n 1|awk '{print $1}'\n");
        end_addr = exec(cmd);
    }
    else if (strstr(process_name, "BFS") != NULL) {
        pr_debug("Using Entries between heap and first mapped lib. Use for BFS \n");
        /* Entries between heap and first mapped lib */
        sprintf(cmd, "cat /proc/%d/maps| grep heap|awk -F '-' '{print $1}'|awk '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| grep heap|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);
    }else if (strstr(process_name, "masim") != NULL) {
        pr_debug("Using entries just after the heap. Use for Masim \n");
        sprintf(cmd, "cat /proc/%d/maps|grep heap -A 1|tail -n 1|awk -F '-' '{print $1}'\n", (pid));
        start_addr = exec(cmd);
        sprintf(cmd, "cat /proc/%d/maps| grep heap -A 1 |tail -n 1|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        end_addr = exec(cmd);

        // pr_debug("Using Entries between heap and first mapped lib. Use for masim \n");
        // /* Entries between heap and first mapped lib */
        // sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/var/p'| sed -e '1d' -e '$d'|head -n 2|tail -n 1 |awk -F '-' '{print $1}'\n", (pid));
        // start_addr = exec(cmd);
        // sprintf(cmd, "cat /proc/%d/maps| sed -n '/heap/,/var/p'| sed -e '1d' -e '$d'|tail -n 1|awk -F '-' '{print $2}'|awk '{print $1}'\n", (pid));
        // end_addr = exec(cmd);
    }
    else {
        pr_err("FATAL: Unknow process. Not sure how to get the address space of this process %s\n", process_name);
        exit(1);

        // pair<string, string> largest_region = getLargestMemoryRegion(pid);
        // start_addr = largest_region.first;
        // end_addr = largest_region.second;
    }


    // cout << "start_addr " << start_addr << "end_addr " << end_addr << endl;



    string::size_type sz = 0;
    address_range ar;
    ar.start_addr = 0;
    ar.end_addr = 0;


    try {
        ar.start_addr = stoull(start_addr, &sz, 16);
        ar.end_addr = stoull(end_addr, &sz, 16);



    }
    catch (const char* e) {
        pr_err("FATAL: Error while getting address range.\n");
        exit(1);
    }




    return ar;
}