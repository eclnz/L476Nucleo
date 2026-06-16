/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */
/* Implementation largely taken from http://elm-chan.org/fsw/ff/ffsample.zip*/

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "main.h" /* [STM32L4-HAL] */

/* Private functions below are adapted from ChaN's mmc_stm32f1_spi.c
 * Copyright (C) 2018, ChaN, all right reserved.
 * https://elm-chan.org/fsw/ff/ffsample.zip
 * https://elm-chan.org/docs/mmc/mmc_e.html
 * Lines changed from the original are marked [STM32L4-HAL]. */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define DISK_TICK() HAL_GetTick()

/* [STM32L4-HAL] Original selected SPI channel via SPI_CH and defined CS/clock macros
 * using direct GPIO register writes and SPIx_CR1 manipulation. Replaced with HAL:
 * CubeMX owns peripheral init; only CS and prescaler need overriding here. */
#define CS_HIGH()   HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET)
#define CS_LOW()    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET)
#define FCLK_SLOW() do { hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128; HAL_SPI_Init(&hspi2); } while(0)
#define FCLK_FAST() do { hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;   HAL_SPI_Init(&hspi2); } while(0)

/* MMC card type flags — from ChaN's diskio.h (not present in project's diskio.h) */
#define CT_MMC3		0x01	/* MMC ver 3 */
#define CT_MMC4		0x02	/* MMC ver 4+ */
#define CT_MMC		0x03	/* MMC */
#define CT_SDC1		0x02	/* SDC ver 1 */
#define CT_SDC2		0x04	/* SDC ver 2+ */
#define CT_SDC		0x0C	/* SDC */
#define CT_BLOCK	0x10	/* Block addressing */

/* MMC/SD command — verbatim from ChaN mmc_stm32f1_spi.c */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
/* [STM32L4-HAL] Timer1/Timer2 (1kHz decrement, driven by disk_timerproc ISR) removed;
 * Used DISK_TICK() macro */
static BYTE CardType;

extern SPI_HandleTypeDef hspi2; /* [STM32L4-HAL] */

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/
/* https://elm-chan.org/docs/spi_e.html */
/**
 * @brief  [STM32L4-HAL] Initialise the SPI2 interface for SD card use.
 * @note   Originally called SPIxENABLE() to configure GPIO and enable the SPI
 *         peripheral. CubeMX handles peripheral init; only CS de-assertion and
 *         a settling delay are needed here.
 */
static void init_spi (void)
{
	CS_HIGH();
	HAL_Delay(10);
}

/**
 * @brief  [STM32L4-HAL] Exchange a single byte over SPI2 (full-duplex, blocking).
 * @param  dat  Byte to transmit. Pass 0xFF when only receiving.
 * @return Byte received from the peripheral during the same clock cycle.
 * @note   Originally accessed SPIx_DR/SPIx_SR directly; replaced with
 *         HAL_SPI_TransmitReceive for portability.
 */
static BYTE xchg_spi (BYTE dat)
{
	BYTE rx;
	HAL_SPI_TransmitReceive(&hspi2, &dat, &rx, 1, HAL_MAX_DELAY);
	return rx;
}

/**
 * @brief  [STM32L4-HAL] Receive multiple bytes from the card over SPI2.
 * @param  buff  Destination buffer; also serves as the TX dummy buffer (pre-filled 0xFF).
 * @param  btr   Number of bytes to receive.
 * @note   Originally switched SPI to 16-bit mode via direct CR1 manipulation for
 *         throughput. Replaced with a single HAL_SPI_TransmitReceive call. The buffer
 *         is pre-filled with 0xFF (the required dummy TX value). In polling mode HAL
 *         reads buff[i] before overwriting it with the received byte, so this is safe.
 */
static void rcvr_spi_multi (BYTE *buff, UINT btr)
{
	memset(buff, 0xFF, btr);
	HAL_SPI_TransmitReceive(&hspi2, buff, buff, btr, HAL_MAX_DELAY);
}

#if _USE_WRITE == 1
/**
 * @brief  [STM32L4-HAL] Transmit multiple bytes to the card over SPI2.
 * @param  buff  Source data buffer.
 * @param  btx   Number of bytes to transmit.
 * @note   Originally used the 16-bit SPI register trick for throughput. Replaced with
 *         HAL_SPI_Transmit for the full buffer in one call. RX bytes are clocked in
 *         by hardware but discarded.
 */
static void xmit_spi_multi (const BYTE *buff, UINT btx)
{
	HAL_SPI_Transmit(&hspi2, (BYTE*)buff, btx, HAL_MAX_DELAY);
}
#endif

/**
 * @brief  Wait until the SD card signals ready (DO = 0xFF) or a timeout expires.
 * @param  wt  Timeout in milliseconds.
 * @return 1 if the card became ready within the timeout, 0 on timeout.
 */
static int wait_ready (
	UINT wt			/* Timeout [ms] */
)
{
	BYTE d;

	uint32_t t = DISK_TICK(); /* [STM32L4-HAL] was: Timer2 = wt; */
	do {
		d = xchg_spi(0xFF);
	} while (d != 0xFF && (DISK_TICK() - t < wt)); /* [STM32L4-HAL] was: && Timer2 */

	return (d == 0xFF) ? 1 : 0;
}

/**
 * @brief  De-select the SD card and release the SPI bus.
 * @note   An extra dummy clock is sent after raising CS to force DO hi-Z,
 *         required for multi-slave SPI configurations. Verbatim from ChaN.
 */
static void deselect (void)
{
	CS_HIGH();		/* Set CS# high */
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */
}

/**
 * @brief  Select the SD card and wait until it is ready.
 * @return 1 if the card is ready, 0 on timeout (card is also de-selected on timeout).
 * @note   Verbatim from ChaN.
 */
static int select (void)
{
	CS_LOW();		/* Set CS# low */
	xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */
	if (wait_ready(500)) return 1;	/* Wait for card ready */

	deselect();
	return 0;	/* Timeout */
}

/**
 * @brief  Receive a data block from the SD card.
 * @param  buff  Destination buffer (must be at least @p btr bytes).
 * @param  btr   Number of bytes to read (typically 512).
 * @return 1 on success, 0 if the data start token (0xFE) was not received within 200 ms.
 */
static int rcvr_datablock (
	BYTE *buff,
	UINT btr
)
{
	BYTE token;

	uint32_t t = DISK_TICK(); /* [STM32L4-HAL] was: Timer1 = 200; */
	do {
		token = xchg_spi(0xFF);
	} while ((token == 0xFF) && (DISK_TICK() - t < 200)); /* [STM32L4-HAL] was: && Timer1 */
	if(token != 0xFE) return 0;

	rcvr_spi_multi(buff, btr);
	xchg_spi(0xFF); xchg_spi(0xFF);			/* Discard CRC */

	return 1;
}

#if _USE_WRITE == 1
/**
 * @brief  Send a 512-byte data block to the SD card.
 * @param  buff   Pointer to the 512-byte data buffer to transmit.
 * @param  token  Data token preceding the block: 0xFE = single-block write,
 *                0xFC = multi-block write, 0xFD = stop transmission.
 * @return 1 if the card accepted the block, 0 on failure. Verbatim from ChaN.
 */
static int xmit_datablock (
	const BYTE *buff,
	BYTE token
)
{
	BYTE resp;

	if (!wait_ready(500)) return 0;
	xchg_spi(token);
	if (token != 0xFD) {
		xmit_spi_multi(buff, 512);
		xchg_spi(0xFF); xchg_spi(0xFF);	/* Dummy CRC */

		resp = xchg_spi(0xFF);
		if ((resp & 0x1F) != 0x05) return 0;
	}
	return 1;
}
#endif

/**
 * @brief  Send an MMC/SD command and return the R1 response byte.
 * @param  cmd  Command index. OR with 0x80 (ACMD flag) to send CMD55 first.
 * @param  arg  32-bit command argument.
 * @return R1 response byte. Bit 7 set indicates the command could not be sent.
 * @note   Verbatim from ChaN.
 */
static BYTE send_cmd (
	BYTE cmd,
	DWORD arg
)
{
	BYTE n, res;

	if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select()) return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);				/* Start + command index */
	xchg_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xchg_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xchg_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	xchg_spi((BYTE)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command resp */
	if (cmd == CMD12) xchg_spi(0xFF);	/* Discard following one byte when CMD12 */
	n = 10;								/* Wait for response (10 bytes max) */
	do {
		res = xchg_spi(0xFF);
	} while ((res & 0x80) && --n);

	return res;
}

/* [STM32L4-HAL] disk_timerproc() omitted — it decremented Timer1/Timer2 from a 1ms ISR
 * and updated Stat via MMC_CD/MMC_WP pins. Neither is needed: timeouts use
 * card detect/WP are hardcoded above. */

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    /* Verbatim from ChaN disk_initialize(), Timer1 replaced with HAL_GetTick() [STM32L4-HAL] */
    BYTE n, cmd, ty, ocr[4];

    if (pdrv) return STA_NOINIT;
    init_spi();

    if (Stat & STA_NODISK) return Stat;	/* [STM32L4-HAL] MMC_CD hardcoded 1, so never set */

    FCLK_SLOW();
    for (n = 10; n; n--) xchg_spi(0xFF);	/* Send 80 dummy clocks */

    ty = 0;
    uint32_t t = HAL_GetTick();					/* [STM32L4-HAL] was: Timer1 = 1000; */
    if (send_cmd(CMD0, 0) == 1) {				/* Put the card SPI/Idle state */
        if (send_cmd(CMD8, 0x1AA) == 1) {		/* SDv2? */
            for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);	/* Get 32 bit return value of R7 resp */
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {			/* Is the card supports vcc of 2.7-3.6V? */
                while ((HAL_GetTick() - t < 1000) && send_cmd(ACMD41, 1UL << 30)) ;	/* [STM32L4-HAL] was: while (Timer1 && ...) */
                if ((HAL_GetTick() - t < 1000) && send_cmd(CMD58, 0) == 0) {			/* [STM32L4-HAL] was: if (Timer1 && ...) */
                    for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
                    ty = (ocr[0] & 0x40) ? CT_SDC2 | CT_BLOCK : CT_SDC2;	/* Card id SDv2 */
                }
            }
        } else {	/* Not SDv2 card */
            if (send_cmd(ACMD41, 0) <= 1) {
                ty = CT_SDC1; cmd = ACMD41;		/* SDv1 (ACMD41(0)) */
            } else {
                ty = CT_MMC3; cmd = CMD1;		/* MMCv3 (CMD1(0)) */
            }
            while ((HAL_GetTick() - t < 1000) && send_cmd(cmd, 0)) ;				/* [STM32L4-HAL] was: while (Timer1 && ...) */
            if ((HAL_GetTick() - t >= 1000) || send_cmd(CMD16, 512) != 0) ty = 0;	/* [STM32L4-HAL] was: if (!Timer1 || ...) */
        }
    }
    CardType = ty;
    deselect();

    if (ty) {
        FCLK_FAST();
        Stat &= ~STA_NOINIT;
    } else {
        Stat = STA_NOINIT;
    }

    return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    /* Verbatim from ChaN disk_status() */
    if (pdrv) return STA_NOINIT;
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    /* Verbatim from ChaN disk_read() */
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    if (!(CardType & CT_BLOCK)) sector *= 512;	/* LBA to BA conversion (byte addressing cards) */

    if (count == 1) {	/* Single sector read */
        if ((send_cmd(CMD17, sector) == 0) && rcvr_datablock(buff, 512)) {
            count = 0;
        }
    } else {			/* Multiple sector read */
        if (send_cmd(CMD18, sector) == 0) {
            do {
                if (!rcvr_datablock(buff, 512)) break;
                buff += 512;
            } while (--count);
            send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
        }
    }
    deselect();

    return count ? RES_ERROR : RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
    /* Verbatim from ChaN disk_write() */
    if (pdrv || !count) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (Stat & STA_PROTECT) return RES_WRPRT;

    if (!(CardType & CT_BLOCK)) sector *= 512;	/* LBA to BA conversion (byte addressing cards) */

    if (count == 1) {	/* Single sector write */
        if ((send_cmd(CMD24, sector) == 0) && xmit_datablock(buff, 0xFE)) {
            count = 0;
        }
    } else {			/* Multiple sector write */
        if (CardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */
        if (send_cmd(CMD25, sector) == 0) {
            do {
                if (!xmit_datablock(buff, 0xFC)) break;
                buff += 512;
            } while (--count);
            if (!xmit_datablock(0, 0xFD)) count = 1;	/* STOP_TRAN token */
        }
    }
    deselect();

    return count ? RES_ERROR : RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    /* Verbatim from ChaN disk_ioctl().
     * Note: MMC_GET_TYPE/CSD/CID/OCR/SDSTAT command codes differ from ChaN's newer
     * diskio.h (50+) vs this project's diskio.h (10+) — [STM32L4-HAL] */
    DRESULT res;
    BYTE n, csd[16];
    DWORD csize; /* st, ed removed — only used in CTRL_TRIM which is not implemented */

    if (pdrv) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    res = RES_ERROR;

    switch (cmd) {
    case CTRL_SYNC:			/* Wait for end of internal write process of the drive */
        if (select()) { deselect(); res = RES_OK; }
        break;

    case GET_SECTOR_COUNT:	/* Get drive capacity in unit of sector (DWORD) */
        if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
            if ((csd[0] >> 6) == 1) {	/* SDC CSD ver 2 */
                csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
                *(DWORD*)buff = csize << 10;
            } else {					/* SDC CSD ver 1 or MMC */
                n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
                csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
                *(DWORD*)buff = csize << (n - 9);
            }
            res = RES_OK;
        }
        break;

    case GET_SECTOR_SIZE:	/* [STM32L4-HAL] Not in ChaN original — SD cards always use 512-byte sectors */
        *(WORD*)buff = 512;
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:	/* Get erase block size in unit of sector (DWORD) */
        if (CardType & CT_SDC2) {	/* SDC ver 2+ */
            if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
                xchg_spi(0xFF);
                if (rcvr_datablock(csd, 16)) {
                    for (n = 64 - 16; n; n--) xchg_spi(0xFF);	/* Purge trailing data */
                    *(DWORD*)buff = 16UL << (csd[10] >> 4);
                    res = RES_OK;
                }
            }
        } else {					/* SDC ver 1 or MMC */
            if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
                if (CardType & CT_SDC1) {	/* SDC ver 1.XX */
                    *(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
                } else {					/* MMC */
                    *(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
                }
                res = RES_OK;
            }
        }
        break;

    case MMC_GET_TYPE:		/* Get MMC/SDC type (BYTE) */
        *(BYTE*)buff = CardType;
        res = RES_OK;
        break;

    case MMC_GET_CSD:		/* Read CSD (16 bytes) */
        if (send_cmd(CMD9, 0) == 0 && rcvr_datablock((BYTE*)buff, 16)) {
            res = RES_OK;
        }
        break;

    case MMC_GET_CID:		/* Read CID (16 bytes) */
        if (send_cmd(CMD10, 0) == 0 && rcvr_datablock((BYTE*)buff, 16)) {
            res = RES_OK;
        }
        break;

    case MMC_GET_OCR:		/* Read OCR (4 bytes) */
        if (send_cmd(CMD58, 0) == 0) {
            for (n = 0; n < 4; n++) *(((BYTE*)buff) + n) = xchg_spi(0xFF);
            res = RES_OK;
        }
        break;

    case MMC_GET_SDSTAT:	/* Read SD status (64 bytes) */
        if (send_cmd(ACMD13, 0) == 0) {
            xchg_spi(0xFF);
            if (rcvr_datablock((BYTE*)buff, 64)) res = RES_OK;
        }
        break;

    default:
        res = RES_PARERR;
    }

    deselect();

    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

