#ifndef SKD_ERROR_H
#define SKD_ERROR_H

#define SKD_MK_ERROR(x)              (0x00000000|(x))

#define ALREADY_IN_DRAM -20
#define ALREADY_IN_OPTANE -21
#define ALREADY_IN_ZSWAP -22
#define ALREADY_SOMEWHERE -23

#define LEN_IS_ZERO -30


typedef enum _status_t
{
    SKD_SUCCESS                  = SKD_MK_ERROR(0x0000),

    SKD_ERROR_UNEXPECTED         = SKD_MK_ERROR(0x0001),      /* Unexpected error */
    SKD_ERROR_INVALID_PARAMETER  = SKD_MK_ERROR(0x0002),      /* The parameter is incorrect */
    SKD_ERROR_PROCESS_DIED      = SKD_MK_ERROR(0x0003),      /* Not enough memory is available to complete this operation */
    
    SKD_ERROR_NO_EVENTS         = SKD_MK_ERROR(0x2005),      /* Not enough EPC is available to load the enclave */
    

} SKD_status_t;

#endif