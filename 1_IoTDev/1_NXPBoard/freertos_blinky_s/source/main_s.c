/*
 * FreeRTOS Pre-Release V1.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* Board specific includes. */

/* Trustzone config. */
#include "tzm_config.h"

/* FreeRTOS includes. */
#include "secure_port_macros.h"

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_power.h"
#include "fsl_rtc.h"
#include "fsl_usart.h"
#include "fsl_debug_console.h"
#include "fsl_ctimer.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#include <stdbool.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#include "hacl-c/Hacl_Ed25519.h"
#else
#include MBEDTLS_CONFIG_FILE
#include "ksdk_mbedtls.h"
#endif
#include <stdio.h>
#include <time.h>
#include "fsl_iap.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/**
 * @brief Start address of non-secure application.
 */
#define mainNONSECURE_APP_START_ADDRESS DEMO_CODE_START_NS

/**
 * @brief LED port and pins.
 */
#define LED_PORT      BOARD_LED_BLUE_GPIO_PORT
#define GREEN_LED_PIN BOARD_LED_GREEN_GPIO_PIN
#define BLUE_LED_PIN  BOARD_LED_BLUE_GPIO_PIN

/**
 * @brief CTimer configuration.
 */
#define CTIMER          	CTIMER2         /* Timer 2 */
#define CTIMER_MAT_OUT 		kCTIMER_Match_1 /* Match output */
#define CTIMER_CLK_FREQ 	CLOCK_GetCTimerClkFreq(2U)

/**
 * @brief typedef for non-secure Reset Handler.
 */
#if defined(__IAR_SYSTEMS_ICC__)
typedef __cmse_nonsecure_call void (*NonSecureResetHandler_t)(void);
#else
typedef void (*NonSecureResetHandler_t)(void) __attribute__((cmse_nonsecure_call));
#endif

#define WIFI_USART          	USART4
#define WIFI_USART_CLK_SRC  	kCLOCK_Flexcomm4
#define WIFI_USART_CLK_FREQ 	CLOCK_GetFlexCommClkFreq(0U)

#define PACKET_SEND_TIMER	5
#define ATTESTATION_TIMER	5

#define BUF_SIZE			(256)
#define SIG_SIZE			(256)
#define NONCE_SIZE			(32)
#define TIME_SIZE			(4)
#define ID_SIZE				(4)
#define HASH_SIZE			(32)
#define ATT_SIZE			(1)
#define TIME_PREV			(1681506039)
#define ID_DEV				(19682938)
#define PRV_DEV_KEY_PEM		"-----BEGIN EC PRIVATE KEY-----\r\nMHcCAQEEIF3U39mcfT5CzujDNem0gk4x1bzPodlveTZZhKbJdtFToAoGCCqGSM49\r\nAwEHoUQDQgAEuQnbuq0OifGY0Fb9TlVw+Y8wXX28TiW+Yq38CIx5sVghlTjBmuFh\r\nm0yBJr5L88OHBd9ymb3S5idXq0EStfbv3Q==\r\n-----END EC PRIVATE KEY-----"
#define PUB_M_SRV_KEY_PEM 		"-----BEGIN PUBLIC KEY-----\r\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEp5WVs1qXLCPdYresNZkyJ192FxXA\r\nTxFzfZHwtWX+xs50yc4x4ax7sNrzWyAe3F87ZZ8MpK+e60gEJumTrp6mzA==\r\n-----END PUBLIC KEY-----"
#define M_SRV_URL				"https://bit.ly/3HnHwEu"
#define MSG_END_CHAR		"MSGEND"
#define ACK_END_CHAR		"ACKEND"

#define CONVERT_MS_TO_S		(1000)

/*-----------------------------------------------------------*/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/**
 * @brief Application-specific implementation of the SystemInitHook().
 */
void SystemInitHook(void);

/**
 * @brief Boots into the non-secure code.
 *
 * @param[in] ulNonSecureStartAddress Start address of the non-secure application.
 */
void BootNonSecure(uint32_t ulNonSecureStartAddress);

/**
 * @brief Broadcast the message with signature.
 */
void announcement();
/*-----------------------------------------------------------*/


void ctimer_match_callback(uint32_t flags);

/* Array of function pointers for callback for each channel */
ctimer_callback_t ctimer_callback_table[] = {
    NULL, ctimer_match_callback, NULL, NULL, NULL, NULL, NULL, NULL};

/*******************************************************************************
 * Global Variables
 ******************************************************************************/

mbedtls_pk_context private_key = {0, };
mbedtls_ctr_drbg_context ctr_drbg = {0, };

/*-----------------------------------------------------------*/


/*******************************************************************************
 * Code
 ******************************************************************************/

void SystemInitHook(void)
{
    /* The TrustZone should be configured as early as possible after RESET.
     * Therefore it is called from SystemInit() during startup. The
     * SystemInitHook() weak function overloading is used for this purpose.
     */
    BOARD_InitTrustZone();
}
/*-----------------------------------------------------------*/

void BootNonSecure(uint32_t ulNonSecureStartAddress)
{
    NonSecureResetHandler_t pxNonSecureResetHandler;

    /* Main Stack Pointer value for the non-secure side is the first entry in
     * the non-secure vector table. Read the first entry and assign the same to
     * the non-secure main stack pointer(MSP_NS). */
    secureportSET_MSP_NS(*((uint32_t *)(ulNonSecureStartAddress)));

    /* Reset handler for the non-secure side is the second entry in the
     * non-secure vector table. */
    pxNonSecureResetHandler = (NonSecureResetHandler_t)(*((uint32_t *)((ulNonSecureStartAddress) + 4U)));

    /* Start non-secure state software application by jumping to the non-secure
     * reset handler. */
    pxNonSecureResetHandler();
}


/*-----------------------------------------------------------*/
static volatile uint32_t s_MsCount = 0U;
static volatile uint32_t adjustedSyncTime = 0U;
/*!
 * @brief Milliseconds counter since last POR/reset.
 */
void SysTick_Handler(void)
{
    s_MsCount++;
}

void ctimer_match_callback(uint32_t flags)
{
    DWT->CYCCNT = 0;
    announcement();
	uint32_t cycles = DWT->CYCCNT;

	// times will be adjusted as much as the time it takes to finish the broadcast
	s_MsCount += (double)cycles/(CLOCK_GetCoreSysClkFreq()/CONVERT_MS_TO_S);

	// To prevent overflow of 's_MsCount'
	adjustedSyncTime += (s_MsCount/CONVERT_MS_TO_S);
	s_MsCount -= (s_MsCount/CONVERT_MS_TO_S)*CONVERT_MS_TO_S;
}

int getEntropyItfFunction(void* userData,uint8_t* buffer,size_t bytes)
{
	int i;
	for(i = 0; i < bytes ; i++)
	{
		buffer[i] = i;
	}

	return 0;
}

void __sha256(const char *msg, size_t msg_len, char *digest)
{
	mbedtls_sha256_context sha256 = {0, };

	mbedtls_sha256_init(&sha256);
	mbedtls_sha256_starts(&sha256, 0);
	mbedtls_sha256_update(&sha256, msg, msg_len);
	mbedtls_sha256_finish(&sha256, digest);
}

void syncReq(uint8_t *req_buffer)
{
	uint32_t time_prev_int = TIME_PREV + 1;
	const uint32_t id_dev = ID_DEV;

	uint8_t n1_dev[NONCE_SIZE]= {0, };
	uint8_t digest[HASH_SIZE] = {0, };
	uint8_t signature[SIG_SIZE] = {0, };

	size_t msg_len = 0;
	int ret = 0;

	// TODO: read previous timestamp (time_prev) and device ID (id_dev) from secure flash memory.

	uint8_t time_prev[TIME_SIZE] = {0, };
	memcpy(time_prev, &time_prev_int, TIME_SIZE);

	// old syncReq msg: [n1_dev(32) || time_prev(4) || id_dev(4) || signature(variable) || "MSGEND"(6)]
	// syncReq msg: [id_dev(4) || n1_dev(32) || time_prev(4) || signature(variable) || "MSGEND"(6)]
	// The last 6 bytes, "MSGEND" is for the network module to differentiate this from syncAck.

	ret = mbedtls_ctr_drbg_random(&ctr_drbg, n1_dev, sizeof(n1_dev));

	memcpy(req_buffer+msg_len, &id_dev, ID_SIZE);
	msg_len += ID_SIZE;
	memcpy(req_buffer+msg_len, n1_dev, NONCE_SIZE);
	msg_len += NONCE_SIZE;
	memcpy(req_buffer+msg_len, time_prev, TIME_SIZE);
	msg_len += TIME_SIZE;

	__sha256(req_buffer, msg_len, digest);

	size_t sig_len = 0;

	ret = mbedtls_pk_sign (&private_key, MBEDTLS_MD_SHA256, digest, sizeof(digest), signature,
			&sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
	if(ret != 0){while(1);}
	memcpy(req_buffer+msg_len, signature, sig_len);
	msg_len += sig_len;

	memcpy(req_buffer+msg_len, MSG_END_CHAR, strlen(MSG_END_CHAR));
	msg_len += strlen(MSG_END_CHAR);
	USART_WriteBlocking(WIFI_USART, req_buffer, msg_len);
}

void cmp_ts_and_save(const uint8_t *time_prev, const uint8_t *time_cur)
{
	uint32_t time_prev_int = time_prev[0] + (time_prev[1] << 8) + (time_prev[2] << 16) + (time_prev[3] << 24);
	uint32_t time_prev_cur = time_cur[0] + (time_cur[1] << 8) + (time_cur[2] << 16) + (time_cur[3] << 24);

	if (time_prev_int > time_prev_cur) {while(1);}

	// TODO: save it to the flash memory
	s_MsCount = 0;
	adjustedSyncTime = time_prev_cur;
}

void syncResp(uint8_t *req_buffer, uint8_t *resp_buffer)
{
	uint32_t msg_len = 0;
	uint32_t sig_len = 0;
	mbedtls_pk_context public_key = {0, };
	uint8_t digest[HASH_SIZE] = {0, };
	int ret = 0;

	// syncReq msg: [id_dev(4) || n1_dev(32) || time_prev(4) || signature(variable) || "MSGEND"(6)]
	// resp_buffer: [n1_dev(32) || n1_m_srv(32) || time_cur(4) from m_srv || signature(variable)]
	// syncResp msg: [id_dev(4) || n1_dev(32) || n1_m_srv(32) || time_cur(4) from m_srv || signature(variable)]


	USART_ReadBlocking(WIFI_USART, (uint8_t*)&msg_len, sizeof(msg_len));

	USART_ReadBlocking(WIFI_USART, resp_buffer, msg_len);

	// If the received id_dev and n1_dev are not the same as the ones sent to the server
	ret = memcmp(req_buffer, resp_buffer, ID_SIZE+NONCE_SIZE);
	if(ret != 0){while(1);}

	// Compare current time from m_srv with time_prev and save it to the flash memory
	cmp_ts_and_save(req_buffer+ID_SIZE+NONCE_SIZE, resp_buffer+ID_SIZE+NONCE_SIZE*2);

	__sha256(resp_buffer, ID_SIZE + NONCE_SIZE*2 + TIME_SIZE, digest);

	sig_len = msg_len - (ID_SIZE + NONCE_SIZE*2 + TIME_SIZE);
	mbedtls_pk_init(&public_key);

	ret = mbedtls_pk_parse_public_key(&public_key, PUB_M_SRV_KEY_PEM, strlen(PUB_M_SRV_KEY_PEM)+1);
	if(ret != 0){while(1);}

	ret = mbedtls_pk_verify(&public_key, MBEDTLS_MD_SHA256, digest, sizeof(digest), resp_buffer+msg_len-sig_len, sig_len);
	if(ret != 0){while(1);}
}

void syncAck(const uint8_t *resp_buffer)
{
	const uint32_t id_dev = ID_DEV;
	size_t msg_len = 0;
	uint8_t digest[HASH_SIZE] = {0, };
	uint8_t signature[SIG_SIZE] = {0, };
	uint8_t n2_dev[NONCE_SIZE] = {0, };
	uint8_t ack_buffer[BUF_SIZE] = {0, };
	int ret = 0;

	// old ack_buffer: [n2_dev(32) || time_cur(4) from m_srv || signature(variable) || "ACKEND"(6)]
	// ack_buffer: [id_dev(4) || n2_dev(32) || n1_m_srv(32) || time_cur(4) from m_srv || signature(variable) || "ACKEND"(6)]
	// The last 6 bytes, "ACKEND" is for the network module to differentiate this from syncReq.


	ret = mbedtls_ctr_drbg_random(&ctr_drbg, n2_dev, sizeof(n2_dev));

	memcpy(ack_buffer+msg_len, &id_dev, ID_SIZE);
	msg_len += ID_SIZE;
	memcpy(ack_buffer+msg_len, n2_dev, NONCE_SIZE);
	msg_len += NONCE_SIZE;
	memcpy(ack_buffer+msg_len, resp_buffer + ID_SIZE + NONCE_SIZE, NONCE_SIZE);
	msg_len += NONCE_SIZE;
	memcpy(ack_buffer+msg_len, resp_buffer + ID_SIZE + NONCE_SIZE*2, TIME_SIZE);
	msg_len += TIME_SIZE;

	__sha256(ack_buffer, msg_len, digest);

	size_t sig_len = 0;
	ret = mbedtls_pk_sign (&private_key, MBEDTLS_MD_SHA256, digest, sizeof(digest), signature,
			&sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
	if(ret != 0){while(1);}

	memcpy(ack_buffer+msg_len, signature, sig_len);
	msg_len += sig_len;

	memcpy(ack_buffer+msg_len, ACK_END_CHAR, strlen(ACK_END_CHAR));
	msg_len += strlen(ACK_END_CHAR);
	USART_WriteBlocking(WIFI_USART, ack_buffer, msg_len);

}

void delay(uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++)
    {
        __NOP();
    }
}

uint8_t* expand_msg(uint8_t* newMsg, uint8_t* msg, size_t msg_len){

    for (int i=0; i<msg_len; i++)
    {
        sprintf(&newMsg[i*2], "%X%X", msg[i]/16, msg[i]%16);
     }
     return newMsg;
}

/* Memory location to attest. */
#define MEM_SIZE 		0x10000				/* Length of FLASH 1 with NS-User privilege */
static unsigned long saddr = 0x40000;	/* Start of FLASH 1 with NS-User privilege */

// Due to the limitation of mbedTLS SHA256 library implemented on NXP,
// it couldn't hash data larger than 64KB in Secure World even with 192MB secure RAM,
// so it's inevitable to divide data into 4KB chucks.
#define MEM_SIZE_4K		0x1000



uint8_t attest()
{
	// It is assumed that the hash of expected result would be stored in the secure flash memory
	// Given digest is hashed value of address 0x40000 with the size of 0x10000
	uint8_t expected_digest[HASH_SIZE] = { 0x4B, 0x95, 0x99, 0x39, 0xC0, 0xD7, 0xF5, 0x0A,
			0x34, 0xF2, 0xA5, 0xDB, 0x50, 0x66, 0x24, 0x22,
			0x75, 0x74, 0x60, 0x5C, 0x09, 0xB8, 0xE1, 0x3E,
			0x37, 0x5D, 0xBE, 0x73, 0xBF, 0xCB, 0xE1, 0xFD,
	};
	uint8_t digest[HASH_SIZE] = {0, };
	uint8_t digest_hash_chain[HASH_SIZE*MEM_SIZE/MEM_SIZE_4K];

	for (int i=0; i<MEM_SIZE/MEM_SIZE_4K; i++) {
		__sha256((uint8_t *)saddr+i*MEM_SIZE_4K, MEM_SIZE_4K, digest_hash_chain+i*HASH_SIZE);
	}
	__sha256(digest_hash_chain, sizeof(digest_hash_chain), digest);


//	if (memcmp(expected_digest, digest, HASH_SIZE) != 0)
//		return 1;

	return 0;
}

void announcement()
{
	uint8_t msg[BUF_SIZE] = {0, };
	size_t msg_len = 0;
	uint8_t digest[HASH_SIZE] = {0, };
	uint8_t signature[SIG_SIZE] = {0, };
	uint8_t n_dev[NONCE_SIZE] = {0, };
	mbedtls_sha256_context sha256 = {0, };
	const uint32_t id_dev = 19682938;
	int ret = 0;
	uint8_t m_srv_url_len = strlen(M_SRV_URL);

	uint32_t curTs = adjustedSyncTime + s_MsCount/CONVERT_MS_TO_S;
	static uint8_t attest_result[ATT_SIZE] = {0};
	static uint32_t time_attest = 0;
	static uint8_t attest_count = 0;
	uint32_t cycles_before, cycles_after = 0;

	// run attestation in every PACKET_SEND_TIMER*ATTESTATION_TIMER seconds
	if ( (++attest_count % ATTESTATION_TIMER == 0) || !time_attest ) {
		attest_count = 0;
		time_attest = curTs;

#ifdef PERFORMANCE_EVALUATION
		cycles_before = DWT->CYCCNT;
#endif
		attest_result[0] = attest();


#ifdef PERFORMANCE_EVALUATION
		cycles_after = DWT->CYCCNT;

		PRINTF("[Attestation] Cycle consumed: %u cycles\n\r", cycles_after - cycles_before);
#endif
	}

	// Announce procedure

	// msg: [n_dev(32) || curTS(4) || signature(variable) || M_SRV_URL(22) || m_srv_url_len(1) || attest_result(1) || time_attest(4)]
	// signature: [n_dev(32) || time_cur (4) from Dev || id_dev(4) || H(M_SRV_URL)(32) || attest_result(1) || time_attest(4)]

	ret = mbedtls_ctr_drbg_random(&ctr_drbg, n_dev, NONCE_SIZE);

	memcpy(msg+msg_len, n_dev, NONCE_SIZE);
	msg_len += NONCE_SIZE;
	memcpy(msg+msg_len, &curTs, TIME_SIZE);
	msg_len += TIME_SIZE;

	size_t sig_body_len = msg_len;

	memcpy(msg+sig_body_len, &id_dev, sizeof(id_dev));
	sig_body_len += sizeof(id_dev);

	__sha256(M_SRV_URL, strlen(M_SRV_URL), msg+sig_body_len);
	sig_body_len += HASH_SIZE;

	memcpy(msg+sig_body_len, attest_result, ATT_SIZE);
	sig_body_len += ATT_SIZE;

	memcpy(msg+sig_body_len, &time_attest, TIME_SIZE);
	sig_body_len += TIME_SIZE;

	__sha256(msg, sig_body_len, digest);

	size_t sig_len = 0;
#ifdef PERFORMANCE_EVALUATION
	cycles_before = DWT->CYCCNT;
#endif
	ret = mbedtls_pk_sign (&private_key, MBEDTLS_MD_SHA256, digest, HASH_SIZE, signature,
			&sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef PERFORMANCE_EVALUATION
	cycles_after = DWT->CYCCNT;
	PRINTF("[Signing] Cycle consumed: %u cycles\n\r", cycles_after - cycles_before);
#endif
	if(ret != 0){while(1);}

	memset(msg+msg_len, 0, BUF_SIZE-msg_len);
	memcpy(msg+msg_len, signature, sig_len);
	msg_len += sig_len;

	memcpy(msg+msg_len, M_SRV_URL, m_srv_url_len);
	msg_len += m_srv_url_len;

	memcpy(msg+msg_len, &m_srv_url_len, sizeof(m_srv_url_len));
	msg_len += sizeof(m_srv_url_len);

	memcpy(msg+msg_len, attest_result, ATT_SIZE);
	msg_len += ATT_SIZE;

	memcpy(msg+msg_len, &time_attest, TIME_SIZE);
	msg_len += TIME_SIZE;

	USART_WriteBlocking(WIFI_USART, msg, msg_len);
}


/* Secure main(). */
/*!
 * @brief Main function
 */

// TODO: Currently, if failure takes place, then just stops working. We might need to keep working normally, but without broadcasting
#define RX_RING_BUFFER_SIZE 100U

uint8_t g_rxRingBuffer[RX_RING_BUFFER_SIZE] = {0}; /* RX ring buffer. */
uint8_t g_resp_bufferfer[RX_RING_BUFFER_SIZE] = {0}; /* Buffer for receive data to echo. */


uint8_t ringBuffer[RX_RING_BUFFER_SIZE];
volatile uint16_t rxIndex = 0; /* Index of the memory to save new arrived data. */

void ctimer_init()
{
	ctimer_config_t config;
	ctimer_match_config_t matchConfig;


	CTIMER_GetDefaultConfig(&config);
	CTIMER_Init(CTIMER, &config);

	/* Configuration 0 */
	matchConfig.enableCounterReset = true;
	matchConfig.enableCounterStop  = false;
	matchConfig.matchValue         = CTIMER_CLK_FREQ * PACKET_SEND_TIMER;
	matchConfig.outControl         = kCTIMER_Output_Toggle;
	matchConfig.outPinInitState    = false;
	matchConfig.enableInterrupt    = true;

	CTIMER_RegisterCallBack(CTIMER, ctimer_callback_table, kCTIMER_MultipleCallback);
	CTIMER_SetupMatch(CTIMER, CTIMER_MAT_OUT, &matchConfig);
	CTIMER_StartTimer(CTIMER);

}

int main(void)
{
	usart_config_t config;
	uint8_t req_buffer[BUF_SIZE] = {0, };
	uint8_t resp_buffer[BUF_SIZE] = {0, };
	int ret = 0;
	uint32_t cycle_records[5] = {0, };
	mbedtls_entropy_context entropy = {0, };


	/* Init DWT at the beginning of main function*/
	DWT->CTRL |= (1 << DWT_CTRL_CYCCNTENA_Pos);
	DWT->CYCCNT = 0;

    /* Init board hardware. */
    /* set BOD VBAT level to 1.65V */
    POWER_SetBodVbatLevel(kPOWER_BodVbatLevel1650mv, kPOWER_BodHystLevel50mv, false);
    gpio_pin_config_t xLedConfig = {.pinDirection = kGPIO_DigitalOutput, .outputLogic = 1};

    /* Initialize GPIO for LEDs. */
    GPIO_PortInit(GPIO, LED_PORT);
    GPIO_PinInit(GPIO, LED_PORT, GREEN_LED_PIN, &(xLedConfig));
    GPIO_PinInit(GPIO, LED_PORT, BLUE_LED_PIN, &(xLedConfig));

    /* Set non-secure vector table */
    SCB_NS->VTOR = mainNONSECURE_APP_START_ADDRESS;

    /* attach main clock divide to FLEXCOMM0 (debug console) */
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    /* Use FRO_HF clock as input clock source. */
    CLOCK_AttachClk(kFRO_HF_to_CTIMER2);

    /* enable clock for GPIO*/
	CLOCK_EnableClock(kCLOCK_Gpio0);
	CLOCK_EnableClock(kCLOCK_Gpio1);

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

	if( CRYPTO_InitHardware() != kStatus_Success )
	{
		PRINTF( "Initialization of crypto HW failed\n\r" );
		while(1);
	}

	/* Init SysTick module */
	/* call CMSIS SysTick function. It enables the SysTick interrupt at low priority */
	SysTick_Config(CLOCK_GetCoreSysClkFreq() / CONVERT_MS_TO_S); /* 1 ms period */

    USART_GetDefaultConfig(&config);
	config.baudRate_Bps = BOARD_DEBUG_UART_BAUDRATE;
	config.enableTx     = true;
	config.enableRx     = true;

	USART_Init(WIFI_USART, &config, WIFI_USART_CLK_FREQ);

    /* Init RTC */
   	RTC_Init(RTC);

   	mbedtls_pk_init(&private_key);

	ret = mbedtls_pk_parse_key(&private_key, PRV_DEV_KEY_PEM, strlen(PRV_DEV_KEY_PEM)+1, NULL, 0);
	if(ret != 0){while(1);}
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init( &ctr_drbg );
	ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
								mbedtls_entropy_func,
								&entropy,
								(const unsigned char *) "RANDOM_GEN",
								10);
	if(ret != 0){while(1);}

#ifdef PERFORMANCE_EVALUATION
	cycle_records[0] = DWT->CYCCNT;
#endif
	syncReq(req_buffer);
#ifdef PERFORMANCE_EVALUATION
	cycle_records[1] = DWT->CYCCNT;
#endif
	syncResp(req_buffer, resp_buffer);
#ifdef PERFORMANCE_EVALUATION
	cycle_records[2] = DWT->CYCCNT;
#endif
	syncAck(resp_buffer);

   	/* init CTimer */
   	ctimer_init();
#ifdef PERFORMANCE_EVALUATION
   	cycle_records[3] = DWT->CYCCNT;
#endif

   	announcement();

   	PRINTF("Finish booting process with time sync \n\r");

#ifdef PERFORMANCE_EVALUATION
   	cycle_records[4] = DWT->CYCCNT;

   	PRINTF("Pure boot/Send/Read/ACK/Boot/Announcement/Total\n\r");
   	PRINTF("%u %u %u %u %u %u %u\n\r", cycle_records[0], cycle_records[1] - cycle_records[0], cycle_records[2] - cycle_records[1],
   			cycle_records[3] - cycle_records[2], cycle_records[3] - cycle_records[0], cycle_records[4] - cycle_records[3], cycle_records[4]);
#endif

   	/* Boot the non-secure code. */
	BootNonSecure(mainNONSECURE_APP_START_ADDRESS);

    /* Non-secure software does not return, this code is not executed. */
    for (;;)
    {
    }

    // clean up
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
}
/*-----------------------------------------------------------*/

void vGetRegistersFromStack(uint32_t *pulFaultStackAddress)
{
    /* These are volatile to try and prevent the compiler/linker optimising them
     * away as the variables never actually get used.  If the debugger won't show the
     * values of the variables, make them global my moving their declaration outside
     * of this function. */
    volatile uint32_t r0;
    volatile uint32_t r1;
    volatile uint32_t r2;
    volatile uint32_t r3;
    volatile uint32_t r12;
    volatile uint32_t lr;  /* Link register. */
    volatile uint32_t pc;  /* Program counter. */
    volatile uint32_t psr; /* Program status register. */
    volatile uint32_t _CFSR;
    volatile uint32_t _HFSR;
    volatile uint32_t _DFSR;
    volatile uint32_t _AFSR;
    volatile uint32_t _SFSR;
    volatile uint32_t _BFAR;
    volatile uint32_t _MMAR;
    volatile uint32_t _SFAR;

    r0 = pulFaultStackAddress[0];
    r1 = pulFaultStackAddress[1];
    r2 = pulFaultStackAddress[2];
    r3 = pulFaultStackAddress[3];

    r12 = pulFaultStackAddress[4];
    lr  = pulFaultStackAddress[5];
    pc  = pulFaultStackAddress[6];
    psr = pulFaultStackAddress[7];

    /* Configurable Fault Status Register. Consists of MMSR, BFSR and UFSR. */
    _CFSR = (*((volatile unsigned long *)(0xE000ED28)));

    /* Hard Fault Status Register. */
    _HFSR = (*((volatile unsigned long *)(0xE000ED2C)));

    /* Debug Fault Status Register. */
    _DFSR = (*((volatile unsigned long *)(0xE000ED30)));

    /* Auxiliary Fault Status Register. */
    _AFSR = (*((volatile unsigned long *)(0xE000ED3C)));

    /* Secure Fault Status Register. */
    _SFSR = (*((volatile unsigned long *)(0xE000EDE4)));

    /* Read the Fault Address Registers. Note that these may not contain valid
     * values. Check BFARVALID/MMARVALID to see if they are valid values. */
    /* MemManage Fault Address Register. */
    _MMAR = (*((volatile unsigned long *)(0xE000ED34)));

    /* Bus Fault Address Register. */
    _BFAR = (*((volatile unsigned long *)(0xE000ED38)));

    /* Secure Fault Address Register. */
    _SFAR = (*((volatile unsigned long *)(0xE000EDE8)));

    /* Remove compiler warnings about the variables not being used. */
    (void)r0;
    (void)r1;
    (void)r2;
    (void)r3;
    (void)r12;
    (void)lr;  /* Link register. */
    (void)pc;  /* Program counter. */
    (void)psr; /* Program status register. */
    (void)_CFSR;
    (void)_HFSR;
    (void)_DFSR;
    (void)_AFSR;
    (void)_SFSR;
    (void)_MMAR;
    (void)_BFAR;
    (void)_SFAR;

    /* When the following line is hit, the variables contain the register values. */
    for (;;)
    {
    }
}
/*-----------------------------------------------------------*/
