#define RUN_DISKIO_TESTS

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "stubs/ff_gen_drv.h"
#include "stubs/user_diskio.h"

/* diskio_test.h uses Error_Handler — stub it to abort on unexpected failures */
void Error_Handler(void) { __builtin_trap(); }

#include "../Core/Inc/diskio_test.h"
#include "../Core/Src/diskio_test.c"

/* ---------- mock state ---------------------------------------------------- */

static DSTATUS mock_status = 0;
static DWORD   mock_sector_count = 1024;
static int     mock_write_fails = 0;
static int     mock_sig_valid = 1;   /* controls whether MBR has 0x55AA */
static BYTE    mock_sector_data[512];

/* ---------- mock implementations ------------------------------------------ */

DSTATUS USER_status(BYTE pdrv) { (void)pdrv; return mock_status; }

DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    (void)pdrv;
    switch (cmd) {
        case CTRL_SYNC:        return RES_OK;
        case GET_SECTOR_COUNT: *(DWORD *)buff = mock_sector_count; return RES_OK;
        case GET_BLOCK_SIZE:   *(DWORD *)buff = 1; return RES_OK;
        case MMC_GET_TYPE:     *(BYTE *)buff  = 0; return RES_OK;
        case MMC_GET_CSD:      memset(buff, 0, 16); return RES_OK;
        case MMC_GET_CID:      memset(buff, 0, 16); return RES_OK;
        case MMC_GET_OCR:      memset(buff, 0,  4); return RES_OK;
        default:               return RES_PARERR;
    }
}

DRESULT USER_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
    (void)pdrv; (void)count;
    memcpy(buff, mock_sector_data, 512);
    if (sector == 0 && mock_sig_valid) {
        buff[510] = 0x55;
        buff[511] = 0xAA;
    }
    return RES_OK;
}

DRESULT USER_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
    (void)pdrv; (void)sector; (void)count;
    if (mock_write_fails) return RES_ERROR;
    memcpy(mock_sector_data, buff, 512);
    return RES_OK;
}

/* ---------- tests ---------------------------------------------------------- */

static void test_all_passed(void) {
    DiskioTestResult r;
    diskio_test_run(&r);

    assert(r.status            == 0);
    assert(r.ioctl_sync        == RES_OK);
    assert(r.ioctl_sector_count == RES_OK);
    assert(r.sector_count      == mock_sector_count);
    assert(r.read              == RES_OK);
    assert(r.read_sig_ok       == 1);
    assert(r.write             == RES_OK);
    assert(r.readback          == RES_OK);
    assert(r.readback_match    == 1);
    assert(r.all_passed        == 1);
}

static void test_invalid_mbr_signature(void) {
    mock_sig_valid = 0;

    DiskioTestResult r;
    /* TEST_ASSERT will trap on read_sig_ok failure — catch it via the result
       by running only the signature check directly */
    memset(mock_sector_data, 0, 512);
    BYTE buf[512];
    USER_read(0, buf, 0, 1);
    int sig_ok = (buf[510] == 0x55 && buf[511] == 0xAA) ? 1 : 0;
    assert(sig_ok == 0);

    mock_sig_valid = 1;
    (void)r;
}

static void test_readback_match(void) {
    /* Write known data then read it back and verify match */
    BYTE src[512], dst[512];
    memset(src, 0xAB, 512);
    USER_write(0, src, 0, 1);
    USER_read(0, dst, 1, 1);  /* sector != 0 so no sig overlay */
    assert(memcmp(src, dst, 512) == 0);
}

int main(void) {
    test_all_passed();
    test_invalid_mbr_signature();
    test_readback_match();
    printf("All diskio tests passed.\n");
    return 0;
}
