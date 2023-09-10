/***********************************************************************************************************************
 * This file was generated by the MCUXpresso Config Tools. Any manual edits made to this file
 * will be overwritten if the respective MCUXpresso Config Tools is used to update this file.
 **********************************************************************************************************************/

#ifndef _TZM_CONFIG_H_
#define _TZM_CONFIG_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/***********************************************************************************************************************
 * BOARD_InitTEE Configuration
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Initialize TrustZone
 **********************************************************************************************************************/
void BOARD_InitTrustZone(void);

/***********************************************************************************************************************
 * Initialize Trusted Execution Environment
 **********************************************************************************************************************/
void BOARD_InitBootTEE(void);

#if defined(__cplusplus)
}
#endif

#endif /* _TZM_CONFIG_H_ */
