#ifndef __FF_GEN_DRV_H
#define __FF_GEN_DRV_H

#include <stdint.h>

typedef uint8_t  BYTE;
typedef uint32_t DWORD;
typedef uint32_t UINT;

typedef BYTE DSTATUS;
typedef enum { RES_OK = 0, RES_ERROR, RES_WRPRT, RES_NOTRDY, RES_PARERR } DRESULT;

#define STA_NOINIT  0x01

#define CTRL_SYNC        0
#define GET_SECTOR_COUNT 1
#define GET_BLOCK_SIZE   3
#define MMC_GET_TYPE     10
#define MMC_GET_CSD      11
#define MMC_GET_CID      12
#define MMC_GET_OCR      13

#endif /* __FF_GEN_DRV_H */
