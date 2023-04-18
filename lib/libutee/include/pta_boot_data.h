#ifndef __PTA_BOOT_DATA_H
#define __PTA_BOOT_DATA_H


#define PTA_BOOT_DATA_UUID \
        { 0x60276949, 0x7FF3, 0x4920, \
                { 0x9B, 0xCE, 0x84, 0x0C, 0x9D, 0xCF, 0x30, 0x98 } }

#define PTA_BOOT_DATA_NAME                   "pta.bootdata"

/*
 * Get device id
 *
 * [out]    memref[0]:   Id of the device
 */

#define PTA_BOOT_DATA_DEVICE_ID	1

/*
 * Get kernel hash
 *
 * [out]    memref[0]:   kernel hash
 */

#define PTA_BOOT_DATA_KERNEL_HASH 2

#define PTA_IMX_TEST_CALL 3

#endif /* __PTA_BOOT_LOG_H */