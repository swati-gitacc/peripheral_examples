/**************************************************************************//**
 * @main.c
 * @brief This project uses the BURTC (Backup Real Time Counter) to wake the
 * device from EM4 mode and thus trigger a reset. This project also shows
 * how to use the BURAM retention registers to have data persist between
 * resets.
 * @version 0.0.1
 ******************************************************************************
 * @section License
 * <b>Copyright 2019 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_burtc.h"
#include "em_rmu.h"
#include "bsp.h"
#include "retargetserial.h"
#include "stdio.h"

// Number of 1 KHz ULFRCO clocks between BURTC interrupts
#define BURTC_IRQ_PERIOD 	3000

/**************************************************************************//**
 * @brief  BURTC Handler
 *****************************************************************************/
void BURTC_IRQHandler(void)
{
  BURTC_IntClear(BURTC_IF_COMP); // compare match
  GPIO_PinOutToggle(BSP_GPIO_LED0_PORT, BSP_GPIO_LED0_PIN);
}

/**************************************************************************//**
 * @brief  Initialize GPIOs for push button and LED
 *****************************************************************************/
void initGPIO(void)
{
  GPIO_PinModeSet(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN, gpioModeInput, 1);
  GPIO_PinModeSet(BSP_GPIO_LED0_PORT, BSP_GPIO_LED0_PIN, gpioModePushPull, 1); // LED on
}

/**************************************************************************//**
 * @brief  Configure BURTC to interrupt every BURTC_IRQ_PERIOD and
 *         wake from EM4
 *****************************************************************************/
void initBURTC(void)
{
  CMU_ClockSelectSet(cmuClock_EM4GRPACLK, cmuSelect_ULFRCO);

  BURTC_Init_TypeDef burtcInit = BURTC_INIT_DEFAULT;
  burtcInit.compare0Top = true; // reset counter when counter reaches compare value
  burtcInit.em4comp = true;     // BURTC compare interrupt wakes from EM4 (causes reset)
  BURTC_Init(&burtcInit);

  BURTC_CounterReset();
  BURTC_CompareSet(0, BURTC_IRQ_PERIOD);

  BURTC_IntEnable(BURTC_IF_COMP); 		// compare match
  NVIC_EnableIRQ(BURTC_IRQn);
  BURTC_Enable(true);
}

/**************************************************************************//**
 * @brief	Check RSTCAUSE for EM4 wakeups (reset) and save wakeup count
 *          to BURAM
 *****************************************************************************/
void checkResetCause (void)
{
  uint32_t cause = RMU_ResetCauseGet();
  RMU_ResetCauseClear();

  // Print reset cause
  if (cause & EMU_RSTCAUSE_PIN)
  {
    printf("-- RSTCAUSE = PIN \n");
    BURAM->RET[0].REG = 0; // reset EM4 wakeup counter
  }
  else if (cause & EMU_RSTCAUSE_EM4)
  {
    printf("-- RSTCAUSE = EM4 wakeup \n");
    BURAM->RET[0].REG += 1; // increment EM4 wakeup counter
  }

  // Print # of EM4 wakeups
  printf("-- Number of EM4 wakeups = %ld \n", BURAM->RET[0].REG);
  printf("-- BURTC ISR will toggle LED every ~3 seconds \n");
}

/**************************************************************************//**
 * @brief	Main function
 *****************************************************************************/
int main(void)
{
  CHIP_Init();
  EMU_UnlatchPinRetention();

  // Init
  RETARGET_SerialInit();
  RETARGET_SerialCrLf(1);
  printf("In EM0 \n");
  initGPIO();
  initBURTC();
  EMU_EM4Init_TypeDef em4Init = EMU_EM4INIT_DEFAULT;
  EMU_EM4Init(&em4Init);

  // Check RESETCAUSE, update and print EM4 wakeup count
  checkResetCause();

  // Wait for user to press PB0, reset BURTC counter
  printf("Press PB0 to enter EM4 \n");
  while(GPIO_PinInGet(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN) == 1);
  printf("-- Button pressed \n");
  BURTC_CounterReset(); // reset BURTC counter to wait full ~3 sec before EM4 wakeup
  printf("-- BURTC counter reset \n");

  // Enter EM4
  printf("Entering EM4 and wake on BURTC compare in ~3 seconds \n\n");
  for(volatile uint32_t i=0; i<1000; i++); // delay for printf to finish
  EMU_EnterEM4();

  // This line should never be reached
  while(1);
}

