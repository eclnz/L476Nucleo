#ifndef DISKIO_TEST_H
#define DISKIO_TEST_H

#ifdef RUN_DISKIO_TESTS

#include "ff_gen_drv.h"
#include "user_diskio.h"

/* Halts execution at the point of failure — caught cleanly by the debugger */
#define TEST_ASSERT(x) do { if (!(x)) { Error_Handler(); } } while(0)

typedef struct {
    /* USER_status */
    DSTATUS status;

    /* USER_ioctl */
    DRESULT ioctl_sync;
    DRESULT ioctl_sector_count;  DWORD sector_count;
    DRESULT ioctl_block_size;    DWORD block_size;
    DRESULT ioctl_card_type;     BYTE  card_type;
    DRESULT ioctl_csd;           BYTE  csd[16];
    DRESULT ioctl_cid;           BYTE  cid[16];
    DRESULT ioctl_ocr;           BYTE  ocr[4];

    /* USER_read — sector 0 */
    DRESULT read;
    uint8_t read_sig_ok;  /* 1 if bytes 510-511 are 0x55 0xAA (valid MBR/boot sector) */

    /* USER_write — reads last sector, writes it back, reads again and compares */
    DRESULT write;
    DRESULT readback;
    uint8_t readback_match;

    uint8_t all_passed;
} DiskioTestResult;

void diskio_test_run(DiskioTestResult *r);

#endif /* RUN_DISKIO_TESTS */
#endif /* DISKIO_TEST_H */
