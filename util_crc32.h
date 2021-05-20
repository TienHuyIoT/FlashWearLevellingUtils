/** @file util_crc32.h
 *  @brief To porting some of type, value to mismatch with platform
 *
 *  @author Luong Thanh Tho, tienhuyiot
 *  @note No Note at the moment
 *  @bug No known bugs.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver    Who        Date             Changes
 * -----  --------   ----------       -----------------------------------------------
 * 1.1    Tienhuyiot May 20, 2021     Add header C++, rename lib
 * 1.0    Thanh Tho  Oct 30, 2020     First release
 *
 *
 *</pre>
 */
/* MODULE BSP */
#ifndef __UTIL_CRC32_H
#define __UTIL_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
//  INCLUDE HEADER
/******************************************************************************/
#include <stdint.h>

/******************************************************************************/
//  MACRO. DEFINE
/******************************************************************************/
/** Enable the refercen table / Or RAW value */
#define CRC32_ENABLE_REFLECT_BIT_ORDER      (1)
/* CRC32_POLY */
#define CRC32_POLY                          (0x04C11DB7)

/******************************************************************************/
//  TYPEDEF
/******************************************************************************/

/******************************************************************************/
//  FUNCTIONS
/******************************************************************************/
/**
 * Start to calculator CRC
 */
void CRC32_Start(uint32_t seek);

/**
 * Calculator CRC
 */
uint32_t CRC32_Accumulate(const uint8_t * buffer, uint32_t length);

/**
 * Get current CRC
 */
uint32_t CRC32_Get(void);

/**
 * Calculator CRC by Buffer
 */
uint32_t Crc32_CalculateBuffer(const uint8_t * buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_CRC32_H */
