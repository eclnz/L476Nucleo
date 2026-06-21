#ifndef __USER_DISKIO_H
#define __USER_DISKIO_H

#include "ff_gen_drv.h"

DSTATUS USER_status(BYTE pdrv);
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff);
DRESULT USER_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT USER_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);

#endif /* __USER_DISKIO_H */
