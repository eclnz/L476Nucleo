#include <string.h>
#include "diskio_test.h"

#ifdef RUN_DISKIO_TESTS

/* 512-byte buffers for read/write tests — static to avoid large stack allocation */
static BYTE sector_buf[512];
static BYTE readback_buf[512];

void diskio_test_run(DiskioTestResult *r)
{
    memset(r, 0, sizeof(*r));

    /* --- USER_status --------------------------------------------------------
     * Should return 0 after a successful mount (STA_NOINIT cleared). */
    r->status = USER_status(0);
    TEST_ASSERT(r->status == 0);

    /* --- USER_ioctl: CTRL_SYNC ----------------------------------------------
     * Flushes any pending writes. Should return RES_OK. */
    r->ioctl_sync = USER_ioctl(0, CTRL_SYNC, NULL);
    TEST_ASSERT(r->ioctl_sync == RES_OK);

    /* --- USER_ioctl: GET_SECTOR_COUNT ---------------------------------------
     * Returns total number of 512-byte sectors on the card. */
    r->ioctl_sector_count = USER_ioctl(0, GET_SECTOR_COUNT, &r->sector_count);
    TEST_ASSERT(r->ioctl_sector_count == RES_OK);

    /* --- USER_ioctl: GET_BLOCK_SIZE (diagnostic — f_mkfs only) --------------
     * Erase block size in sectors. Not asserted: not called in normal FatFs use. */
    r->ioctl_block_size = USER_ioctl(0, GET_BLOCK_SIZE, &r->block_size);

    /* --- USER_ioctl: MMC_GET_* (diagnostic only — never called by FatFs) ----
     * Stored in the result struct for debugger inspection. */
    r->ioctl_card_type = USER_ioctl(0, MMC_GET_TYPE, &r->card_type);
    r->ioctl_csd       = USER_ioctl(0, MMC_GET_CSD,  r->csd);
    r->ioctl_cid       = USER_ioctl(0, MMC_GET_CID,  r->cid);
    r->ioctl_ocr       = USER_ioctl(0, MMC_GET_OCR,  r->ocr);

    /* --- USER_read: sector 0 ------------------------------------------------
     * Reads the MBR/boot sector. Checks the 0x55 0xAA signature at bytes 510-511
     * which every valid FAT volume has. */
    r->read = USER_read(0, sector_buf, 0, 1);
    TEST_ASSERT(r->read == RES_OK);
    r->read_sig_ok = (sector_buf[510] == 0x55 && sector_buf[511] == 0xAA) ? 1 : 0;
    TEST_ASSERT(r->read_sig_ok == 1);

    /* --- USER_write: last sector --------------------------------------------
     * Reads the final sector on the card, writes the same data back, then reads
     * again and compares. Uses the last sector to avoid touching filesystem data. */
    DWORD test_sector = r->sector_count - 1;
    TEST_ASSERT(USER_read(0, sector_buf, test_sector, 1) == RES_OK);
    r->write = USER_write(0, sector_buf, test_sector, 1);
    TEST_ASSERT(r->write == RES_OK);
    r->readback = USER_read(0, readback_buf, test_sector, 1);
    TEST_ASSERT(r->readback == RES_OK);
    r->readback_match = (memcmp(sector_buf, readback_buf, 512) == 0) ? 1 : 0;
    TEST_ASSERT(r->readback_match == 1);

    /* --- Summary ------------------------------------------------------------
     * Only FatFs-facing functions gate all_passed.
     * GET_BLOCK_SIZE (f_mkfs only) and MMC_GET_* (diagnostic) do not. */
    r->all_passed =
        (r->status             == 0)      &&
        (r->ioctl_sync         == RES_OK) &&
        (r->ioctl_sector_count == RES_OK) &&
        (r->read               == RES_OK) &&
        (r->read_sig_ok        == 1)      &&
        (r->write              == RES_OK) &&
        (r->readback           == RES_OK) &&
        (r->readback_match     == 1)      ? 1 : 0;
}

#endif /* RUN_DISKIO_TESTS */
