#include <linux/mtd/mtd.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/slab.h>

typedef enum {
	NVM_CPFLAG,
	NVM_HSFLAG,         // Headset & Debug flag
	NVM_SDFLAG,
	NVM_RECOVERYFLAG,
	NVM_RECOVERYFLAG_END = NVM_RECOVERYFLAG + 2,
	NVM_PSCAL,          // P-Sensor Calibration data
	NVM_WSINFO, 		  // Work station info  
	NVM_LOGO,			  // LOGO size = 40k
	NVM_LOGO_END = NVM_LOGO+80,
	NVM_MAX
} NVM_t;

extern int read_nvm(NVM_t part, char *buf, size_t len);
extern int write_nvm(NVM_t part, char *buf, size_t len);

