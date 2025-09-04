#ifndef REGION_BASE_H
#define REGION_BASE_H

#include "model.h"
#include "utils.h"

class REGION_BASE
{
    int process_pid;    
    uint8_t is_initalized;
public:
    vector<REGION_SKD *> regions;

    REGION_BASE(){
        regions.clear();
        is_initalized=0;
    };

    int get_process_pid(){return process_pid;}
    void set_process_pid(int _pid){process_pid = _pid;}

    virtual int init_initial_regions(int )=0;
    virtual int assign_events_to_regions(vector<AddrEntry *> )=0;
    virtual int fix_regions()=0;

     int is_init(){return is_initalized;}
     void set_is_init(uint8_t _is_initalized){is_initalized = _is_initalized;}
};
  
#endif