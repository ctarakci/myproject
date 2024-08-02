/*! *********************************************************************************
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* Copyright 2016-2019 NXP
* All rights reserved.
*
* \file
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

#include "EmbeddedTypes.h"
#include "DCDC.h"
#include "TimersManager.h"
#include "fsl_os_abstraction.h"
#include "board.h"
/*****************************************************************************
******************************************************************************
* Private type definitions
******************************************************************************
*****************************************************************************/

typedef struct dcdcInputs_tag
{
  dcdcMcuVOutputTargetVal_t outputTarget_McuV;
  dcdc1P8OutputTargetVal_t  outputTarget_1P8;
  uint16_t vbatVal_mV;
  bool_t outputTargetsToUpdate;
}dcdcInputs_t;

/*****************************************************************************
******************************************************************************
* Private macros
******************************************************************************
*****************************************************************************/
#ifdef DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BUCK_MASK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK  DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BUCK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK_MASK  DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BUCK_MASK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK_SHIFT  DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BUCK_SHIFT
#else
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK  DCDC_REG3_DCDC_VDD1P45CTRL_TRG_BUCK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK_MASK  DCDC_REG3_DCDC_VDD1P45CTRL_TRG_BUCK_MASK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK_SHIFT  DCDC_REG3_DCDC_VDD1P45CTRL_TRG_BUCK_SHIFT
#endif

#ifdef DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BOOST_MASK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BOOST  DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BOOST
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BOOST_MASK  DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BOOST_MASK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BOOST_SHIFT  DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BOOST_SHIFT
#else
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BOOST  DCDC_REG3_DCDC_VDD1P45CTRL_TRG_BOOST
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BOOST_MASK  DCDC_REG3_DCDC_VDD1P45CTRL_TRG_BOOST_MASK
#define DCDC_REG3_VDD_MCU_CTRL_TRG_BOOST_SHIFT  DCDC_REG3_DCDC_VDD1P45CTRL_TRG_BOOST_SHIFT
#endif
#ifdef DCDC_REG3_DCDC_VDD1P5CTRL_DISABLE_STEP_MASK
#define DCDC_REG3_DCDC_VDD_MCU_CTRL_DISABLE_STEP_MASK  DCDC_REG3_DCDC_VDD1P5CTRL_DISABLE_STEP_MASK
#else
#define DCDC_REG3_DCDC_VDD_MCU_CTRL_DISABLE_STEP_MASK  DCDC_REG3_DCDC_VDD1P45CTRL_DISABLE_STEP_MASK
#endif


#define REG_READ_FIELD(reg, fieldName)                (((reg) & (fieldName##_MASK)) >> (fieldName##_SHIFT)) 

#define REG_READ_MODIFY_FIELD(reg, fieldName, value)  (((reg) & (~fieldName##_MASK)) | (((uint32_t)(((uint32_t)(value)) << fieldName##_SHIFT)) & fieldName##_MASK))

#define REG_WRITE_FIELD(reg, fieldName, value) {reg = (((reg) & (~fieldName##_MASK)) | fieldName(value));}


#define mDCDC_VBatMinBuck_c  1800
#define mDCDC_VBatMaxBuck_c  3600
#define mDCDC_BuckVBatToVOutMin_c  0
#define mDCDC_BGAPVal_mV_c           999
#define mDCDC_DutyCycleMax_c         127
#ifndef gADC16_ReferenceVoltageSource_d
#define gADC16_ReferenceVoltageSource_d kADC16_ReferenceVoltageSourceVref
#endif
#define gDCDC_McuV_OutputTargetVal_MIN_c gDCDC_McuV_OutputTargetVal_1_450_c
#define gDCDC_McuV_OutputTargetVal_MAX_c gDCDC_McuV_OutputTargetVal_1_800_c 
#define gDCDC_1P8OutputTargetVal_MIN_c   gDCDC_1P8OutputTargetVal_1_800_c
#define gDCDC_1P8OutputTargetVal_MAX_c   gDCDC_1P8OutputTargetVal_3_500_c
/*****************************************************************************
 *****************************************************************************
 * Private prototypes
 *****************************************************************************
 *****************************************************************************/

/*****************************************************************************
 *****************************************************************************
 * Private memory definitions
 *****************************************************************************
 *****************************************************************************/

#if gDCDC_Enabled_d
static tmrTimerID_t mVBatMonitorTimerId;
static const dcdcConfig_t * mpDCDCConfig;
static dcdcCallbackParam_t mDCDCCallbackParam;
static dcdcInputs_t mDCDCInputs;
static pfDCDCPSwitchCallback_t mpfDCDCPSwitchCallback;
#endif /*gDCDC_Enabled_d*/





/*****************************************************************************
******************************************************************************
* Private functions
******************************************************************************
*****************************************************************************/

#if gDCDC_Enabled_d
///*---------------------------------------------------------------------------
//* NAME: DCDC_AdjustVbatDiv
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
static void DCDC_AdjustVbatDiv()
{
  uint32_t batVal;
  uint8_t vBatDiv = 3;
  do
  {
    DCDC->REG0 = REG_READ_MODIFY_FIELD(DCDC->REG0, DCDC_REG0_DCDC_VBAT_DIV_CTRL, vBatDiv);
    batVal = BOARD_GetAdcBatLvl();
    if(batVal > 0x7FF)
    {
      break;
    }
  }
  while(vBatDiv-- != 1);
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_Get1P8VOutputTargetAndVBatInmV
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
static void DCDC_Get1P8VOutputTargetAndVBatInmV(uint16_t* p1P8VOutputInMv, uint16_t* pVBatInMv )
{
  uint32_t batVal;
  uint32_t bgapVal;
  uint32_t mvVal;
  uint8_t vBatDiv;
  batVal = BOARD_GetAdcBatLvl();
  bgapVal = BOARD_GetAdcBgLvl();
  mvVal = 4095;
  mvVal *= mDCDC_BGAPVal_mV_c;
  mvVal += (bgapVal>>1);
  mvVal /= bgapVal;
  *p1P8VOutputInMv = mvVal;
  vBatDiv = REG_READ_FIELD(DCDC->REG0, DCDC_REG0_DCDC_VBAT_DIV_CTRL);
  if(vBatDiv)
  {
    vBatDiv--;
  }
  mvVal = (batVal<<vBatDiv);
  mvVal *= mDCDC_BGAPVal_mV_c;
  mvVal += (bgapVal>>1);
  mvVal /= bgapVal;  
  *pVBatInMv = mvVal;
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_mVTo1P8OutputTargetBuck
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
static dcdc1P8OutputTargetVal_t DCDC_mVTo1P8OutputTargetBuck(uint16_t mV)
{
    
    if(mV < 1800)
    {
        return gDCDC_1P8OutputTargetVal_1_800_c;
    }
    if(mV <= 2100)
    {
        mV -= 1650;
        mV /= 25;
        return (dcdc1P8OutputTargetVal_t)mV;
    }
    if (mV <= 2800)
    {
        mV -= 2100;
        mV /= 50;
        mV += gDCDC_1P8OutputTargetVal_2_100_c;
        return (dcdc1P8OutputTargetVal_t)mV;
    }
    mV -= 2800;
    mV /= 25;
    mV += gDCDC_1P8OutputTargetVal_2_800_c;
    if(mV > gDCDC_1P8OutputTargetVal_MAX_c)
    {
        mV = gDCDC_1P8OutputTargetVal_MAX_c;
    }  
    return (dcdc1P8OutputTargetVal_t)mV;
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_mVToMcuVOutputTargetBuck
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
dcdcMcuVOutputTargetVal_t DCDC_mVToMcuVOutputTargetBuck(uint16_t mV)
{
  if (mV <= 1450)
  {
    return gDCDC_McuV_OutputTargetVal_1_450_c;
  }
  mV -= 1275;
  mV /= 25;
  
  if(mV > gDCDC_McuV_OutputTargetVal_MAX_c)
  {
    mV = gDCDC_McuV_OutputTargetVal_MAX_c;
  }
  return (dcdcMcuVOutputTargetVal_t)mV;
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_GetOutputTargetsBuck
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
static void DCDC_GetOutputTargetsBuck(dcdcMcuVOutputTargetVal_t* pMcuVOutputTarget, dcdc1P8OutputTargetVal_t* p1P8OutputTarget )
{
  dcdcMcuVOutputTargetVal_t dcdcMcuVWanted;
  dcdc1P8OutputTargetVal_t dcdc1P8MaxOutputTarget,dcdc1P8OutputTarget,dcdc1P8Wanted;
  uint16_t vBATmV;
  OSA_InterruptDisable(); 
  {
    dcdcMcuVWanted = mDCDCInputs.outputTarget_McuV;
    dcdc1P8Wanted = mDCDCInputs.outputTarget_1P8;
    vBATmV = mDCDCInputs.vbatVal_mV;
  }
  OSA_InterruptEnable();
  dcdc1P8MaxOutputTarget = DCDC_mVTo1P8OutputTargetBuck(vBATmV - mDCDC_BuckVBatToVOutMin_c);
  if(dcdc1P8Wanted <= dcdc1P8MaxOutputTarget)
  {
    dcdc1P8OutputTarget = dcdc1P8Wanted;
  }
  else
  {
    dcdc1P8OutputTarget = dcdc1P8MaxOutputTarget;
  }
  *pMcuVOutputTarget = dcdcMcuVWanted;
  *p1P8OutputTarget = dcdc1P8OutputTarget;
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_VBatMonitorBuck
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
static void DCDC_VBatMonitorBuck(void* param)
{
  uint16_t mV_VBat;
  uint16_t mV_1P8V;
  dcdcMcuVOutputTargetVal_t dcdcMcuVOutputTarget;
  dcdc1P8OutputTargetVal_t dcdc1P8OutputTarget; 
  mDCDCCallbackParam.dcdcEvent = gDCDC_Event_NoEvent_c;
  DCDC_AdjustVbatDiv();
  DCDC_Get1P8VOutputTargetAndVBatInmV(&mV_1P8V ,&mV_VBat);
  OSA_InterruptDisable();  
  {
    mDCDCInputs.vbatVal_mV  = mV_VBat;
    mDCDCInputs.outputTargetsToUpdate = 1;
  }
  OSA_InterruptEnable();
  DCDC->REG2 &= ~DCDC_REG2_DCDC_BATTMONITOR_EN_BATADJ_MASK;
  DCDC->REG2 = REG_READ_MODIFY_FIELD(DCDC->REG2, DCDC_REG2_DCDC_BATTMONITOR_BATT_VAL, mV_VBat >> 3);
  DCDC->REG2 |= DCDC_REG2_DCDC_BATTMONITOR_EN_BATADJ_MASK;
  mDCDCCallbackParam.dcdcVbatMeasuredVal = mV_VBat;
  mDCDCCallbackParam.dcdc1P8OutputMeasuredVal = mV_1P8V;
  DCDC_GetOutputTargetsBuck(&dcdcMcuVOutputTarget, &dcdc1P8OutputTarget );
  
  OSA_InterruptDisable();  
  {
    if(mDCDCInputs.outputTargetsToUpdate == 1)
    {
      DCDC->REG3 = REG_READ_MODIFY_FIELD(DCDC->REG3, DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK, dcdcMcuVOutputTarget);
      DCDC->REG3 = REG_READ_MODIFY_FIELD(DCDC->REG3, DCDC_REG3_DCDC_VDD1P8CTRL_TRG, dcdc1P8OutputTarget);
      mDCDCInputs.outputTargetsToUpdate = 0;
    }
  }
  OSA_InterruptEnable();
  
  if(param == NULL)
  {
    if(mpDCDCConfig->pfDCDCAppCallback)
    {
      if((mDCDCInputs.vbatVal_mV < mpDCDCConfig->vbatMin) || (mDCDCInputs.vbatVal_mV > mpDCDCConfig->vbatMax))
      {
        mDCDCCallbackParam.dcdcEvent |= gDCDC_Event_VBatOutOfRange_c;
      }
      OSA_InterruptDisable();
if(mDCDCCallbackParam.dcdcMcuVOutputTargetVal != (dcdcMcuVOutputTargetVal_t)REG_READ_FIELD(DCDC->REG3, DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK))      
      {
        mDCDCCallbackParam.dcdcMcuVOutputTargetVal = (dcdcMcuVOutputTargetVal_t)REG_READ_FIELD(DCDC->REG3, DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK);        
        mDCDCCallbackParam.dcdcEvent |= gDCDC_Event_McuV_OutputTargetChange_c;
      }
      if(mDCDCCallbackParam.dcdc1P8OutputTargetVal != (dcdc1P8OutputTargetVal_t)REG_READ_FIELD(DCDC->REG3, DCDC_REG3_DCDC_VDD1P8CTRL_TRG))
      {
        mDCDCCallbackParam.dcdc1P8OutputTargetVal = (dcdc1P8OutputTargetVal_t)REG_READ_FIELD(DCDC->REG3, DCDC_REG3_DCDC_VDD1P8CTRL_TRG);  
        mDCDCCallbackParam.dcdcEvent |= gDCDC_Event_1P8OutputTargetChange_c;
      } 
      OSA_InterruptEnable();
      mpDCDCConfig->pfDCDCAppCallback(&mDCDCCallbackParam);
    }    
  }
}

///*---------------------------------------------------------------------------
//* NAME: DCDC_PSwitchIsr
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
static void DCDC_PSwitchIsr(void)
{
  dcdcPSwStatus_t dcdcPSwStatus;
  DCDC->REG6 |= DCDC_REG6_PSWITCH_INT_CLEAR_MASK;
  DCDC->REG6 &= ~DCDC_REG6_PSWITCH_INT_CLEAR_MASK;
  if( REG_READ_FIELD(DCDC->REG0, DCDC_REG0_PSWITCH_STATUS) == 0) 
  {
    dcdcPSwStatus = gDCDC_PSwStatus_Low_c;
  }
  else
  {
    dcdcPSwStatus = gDCDC_PSwStatus_High_c;
  }
  mpfDCDCPSwitchCallback(dcdcPSwStatus);
}
#endif /*gDCDC_Enabled_d*/


/*****************************************************************************
******************************************************************************
* Public functions
******************************************************************************
*****************************************************************************/


/*---------------------------------------------------------------------------
 * NAME: DCDC_Init
 * DESCRIPTION: initialize 
 * PARAMETERS: -
 * RETURN: -
 *---------------------------------------------------------------------------*/
bool_t DCDC_Init
(
const dcdcConfig_t * pDCDCConfig
)
{
#if gDCDC_Enabled_d == 0
    (void) pDCDCConfig;
    return TRUE;
#else  
    static bool_t DCDC_Initialized = FALSE;
    if(DCDC_Initialized == TRUE)
    {
        return FALSE;
    } 
    DCDC_Initialized = TRUE;
    if(pDCDCConfig == NULL)
    {
        return FALSE;
    }
    CLOCK_EnableClock(kCLOCK_Dcdc0);
    if(pDCDCConfig->dcdcMode == gDCDC_Mode_Bypass_c)
    {
        mpDCDCConfig = pDCDCConfig;
        return TRUE;
    }
    if(pDCDCConfig->vbatMin > pDCDCConfig->vbatMax)
    {
        return FALSE;
    }  
    if(pDCDCConfig->dcdcMode == gDCDC_Mode_Buck_c)
    {
        if((pDCDCConfig->vbatMin < mDCDC_VBatMinBuck_c)||(pDCDCConfig->vbatMax > mDCDC_VBatMaxBuck_c))
        {
            return FALSE;
        }
        if((gDCDC_McuV_OutputTargetVal_MAX_c < pDCDCConfig->dcdcMcuVOutputTargetVal) || (pDCDCConfig->dcdcMcuVOutputTargetVal < gDCDC_McuV_OutputTargetVal_MIN_c))
        {
            return FALSE;
        }
        if((gDCDC_1P8OutputTargetVal_MAX_c < pDCDCConfig->dcdc1P8OutputTargetVal) || (pDCDCConfig->dcdc1P8OutputTargetVal < gDCDC_1P8OutputTargetVal_MIN_c))
        {
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    TMR_Init();
    mVBatMonitorTimerId = TMR_AllocateTimer();
    if(gTmrInvalidTimerID_c == mVBatMonitorTimerId)
    {
        return FALSE;
    }
    
    BOARD_InitAdc();
    
    mDCDCInputs.outputTarget_McuV = pDCDCConfig->dcdcMcuVOutputTargetVal;
    mDCDCInputs.outputTarget_1P8 = pDCDCConfig->dcdc1P8OutputTargetVal; 
    mpDCDCConfig = pDCDCConfig;
    DCDC->REG1 |= DCDC_REG1_DCDC_LOOPCTRL_EN_DF_HYST_MASK | DCDC_REG1_DCDC_LOOPCTRL_EN_CM_HYST_MASK;
    DCDC->REG2 |= DCDC_REG2_DCDC_LOOPCTRL_HYST_SIGN_MASK;
    DCDC->REG3 &= ~(DCDC_REG3_DCDC_VDD_MCU_CTRL_DISABLE_STEP_MASK | DCDC_REG3_DCDC_VDD1P8CTRL_DISABLE_STEP_MASK );
    PMC->REGSC |=PMC_REGSC_BGBE_MASK;
    mDCDCCallbackParam.dcdc1P8OutputTargetVal = gDCDC_1P8OutputTargetVal_1_800_c;
#ifdef DCDC_REG3_DCDC_VDD1P5CTRL_TRG_BUCK_MASK
    mDCDCCallbackParam.dcdcMcuVOutputTargetVal = gDCDC_McuV_OutputTargetVal_1_500_c;
#else
    mDCDCCallbackParam.dcdcMcuVOutputTargetVal = gDCDC_McuV_OutputTargetVal_1_450_c;
#endif    
    DCDC_AdjustVbatDiv();
    DCDC_VBatMonitorBuck((void*)1);
    TMR_StartLowPowerTimer(mVBatMonitorTimerId, gTmrIntervalTimer_c ,pDCDCConfig->vBatMonitorIntervalMs, DCDC_VBatMonitorBuck, NULL);    
    return TRUE; 
#endif  
}

/*---------------------------------------------------------------------------
 * NAME: DCDC_SetOutputVoltageTargets
 * DESCRIPTION: initialize the timer module
 * PARAMETERS: -
 * RETURN: -
 *---------------------------------------------------------------------------*/
bool_t DCDC_SetOutputVoltageTargets
(
dcdcMcuVOutputTargetVal_t dcdcMcuVOutputTargetVal,
dcdc1P8OutputTargetVal_t  dcdc1P8OutputTargetVal
)
{
#if gDCDC_Enabled_d == 0
    (void)dcdcMcuVOutputTargetVal;
    (void)dcdc1P8OutputTargetVal;
    return TRUE;
#else  
    dcdcMcuVOutputTargetVal_t dcdcMcuVOutputTarget;
    dcdc1P8OutputTargetVal_t dcdc1P8OutputTarget; 
    
    if(mpDCDCConfig == NULL)
    {
        // DCDC_Init has to be called prior to this function
        return FALSE;
    }
    if(mpDCDCConfig->dcdcMode == gDCDC_Mode_Bypass_c)
    {
        return TRUE;
    }
    if((gDCDC_McuV_OutputTargetVal_MAX_c < dcdcMcuVOutputTargetVal) || (dcdcMcuVOutputTargetVal < gDCDC_McuV_OutputTargetVal_MIN_c))
    {
        return FALSE;
    }
    if((gDCDC_1P8OutputTargetVal_MAX_c < dcdc1P8OutputTargetVal) || (dcdc1P8OutputTargetVal < gDCDC_1P8OutputTargetVal_MIN_c))
    {
        return FALSE;
    }
    
    OSA_InterruptDisable();  
    {
        mDCDCInputs.outputTarget_McuV = dcdcMcuVOutputTargetVal;
        mDCDCInputs.outputTarget_1P8 = dcdc1P8OutputTargetVal; 
        mDCDCInputs.outputTargetsToUpdate = 1;
    }
    OSA_InterruptEnable();
    DCDC_GetOutputTargetsBuck(&dcdcMcuVOutputTarget, &dcdc1P8OutputTarget );
    OSA_InterruptDisable();  
    {
        if(mDCDCInputs.outputTargetsToUpdate == 1)
        {
            DCDC->REG3 = REG_READ_MODIFY_FIELD(DCDC->REG3, DCDC_REG3_VDD_MCU_CTRL_TRG_BUCK, dcdcMcuVOutputTarget);
            DCDC->REG3 = REG_READ_MODIFY_FIELD(DCDC->REG3, DCDC_REG3_DCDC_VDD1P8CTRL_TRG, dcdc1P8OutputTarget);
            mDCDCInputs.outputTargetsToUpdate = 0;
        }
    }
    OSA_InterruptEnable();
    return TRUE;
#endif
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_McuVOutputTargetTomV
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
uint16_t DCDC_McuVOutputTargetTomV(dcdcMcuVOutputTargetVal_t dcdcMcuVOutputTarget)
{
  uint16_t mV;
  mV = (uint16_t)(dcdcMcuVOutputTarget) * 25 + 1275;
  return mV;
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_1P8OutputTargetTomV
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
uint16_t DCDC_1P8OutputTargetTomV(dcdc1P8OutputTargetVal_t dcdc1P8OutputTarget)
{
  uint16_t mV;
  if( dcdc1P8OutputTarget < gDCDC_1P8OutputTargetVal_2_100_c)
  {
    mV = (uint16_t)(dcdc1P8OutputTarget)*25 + 1650;
  }
  else if(dcdc1P8OutputTarget < gDCDC_1P8OutputTargetVal_2_800_c )
  {
    mV = (uint16_t)(dcdc1P8OutputTarget - gDCDC_1P8OutputTargetVal_2_100_c)*50 + 2100;      
  }
  else 
  {
    mV = (uint16_t)(dcdc1P8OutputTarget - gDCDC_1P8OutputTargetVal_2_800_c)*25 + 2800;
  }

  return mV;
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_PrepareForPulsedMode
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
bool_t DCDC_PrepareForPulsedMode(void)
{
#if gDCDC_Enabled_d == 0
    return TRUE;
#else
   if(mpDCDCConfig == NULL)
  {
    // DCDC_Init has to be called prior to this function
    return FALSE;
  }
  if(mpDCDCConfig->dcdcMode == gDCDC_Mode_Bypass_c)
  {
    return TRUE;
  }
  DCDC->REG3 |= DCDC_REG3_DCDC_VDD_MCU_CTRL_DISABLE_STEP_MASK | DCDC_REG3_DCDC_VDD1P8CTRL_DISABLE_STEP_MASK ;
  DCDC->REG1 |= DCDC_REG1_DCDC_LOOPCTRL_EN_DF_HYST_MASK | DCDC_REG1_DCDC_LOOPCTRL_EN_CM_HYST_MASK;
  DCDC->REG2 |= DCDC_REG2_DCDC_LOOPCTRL_HYST_SIGN_MASK;
  DCDC->REG0 |= DCDC_REG0_DCDC_LP_DF_CMP_ENABLE_MASK;
  
  return TRUE;
#endif
}

///*---------------------------------------------------------------------------
//* NAME: DCDC_PrepareForContinuousMode
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
bool_t DCDC_PrepareForContinuousMode(void)
{
#if gDCDC_Enabled_d == 0
    return TRUE;
#else
   if(mpDCDCConfig == NULL)
  {
    // DCDC_Init has to be called prior to this function
    return FALSE;
  }
  if(mpDCDCConfig->dcdcMode == gDCDC_Mode_Bypass_c)
  {
    return TRUE;
  }
  DCDC->REG3 &= ~(DCDC_REG3_DCDC_VDD_MCU_CTRL_DISABLE_STEP_MASK | DCDC_REG3_DCDC_VDD1P8CTRL_DISABLE_STEP_MASK );
  DCDC->REG1 |= DCDC_REG1_DCDC_LOOPCTRL_EN_DF_HYST_MASK | DCDC_REG1_DCDC_LOOPCTRL_EN_CM_HYST_MASK;
  DCDC->REG2 |= DCDC_REG2_DCDC_LOOPCTRL_HYST_SIGN_MASK;
  DCDC->REG0 &= ~DCDC_REG0_DCDC_LP_DF_CMP_ENABLE_MASK;
  return TRUE;
#endif
}

///*---------------------------------------------------------------------------
//* NAME: DCDC_SetUpperLimitDutyCycle
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
bool_t DCDC_SetUpperLimitDutyCycle(uint8_t upperLimitDutyCycle)
{
#if gDCDC_Enabled_d == 0
    return TRUE;
#else
    if(mpDCDCConfig == NULL)
    {
        // DCDC_Init has to be called prior to this function
        return FALSE;
    }
    if(mpDCDCConfig->dcdcMode == gDCDC_Mode_Bypass_c)
    {
        return TRUE;
    }
    if(mDCDC_DutyCycleMax_c < upperLimitDutyCycle)
    {
        return FALSE;
    }
    DCDC->REG1 = REG_READ_MODIFY_FIELD(DCDC->REG1, DCDC_REG1_POSLIMIT_BUCK_IN,upperLimitDutyCycle);
    return TRUE;
#endif
}

///*---------------------------------------------------------------------------
//* NAME: DCDC_GetPowerSwitchStatus
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
bool_t DCDC_GetPowerSwitchStatus(dcdcPSwStatus_t* pDCDCPSwStatus)
{
#if gDCDC_Enabled_d == 0
  *pDCDCPSwStatus = gDCDC_PSwStatus_High_c;
  return TRUE;
#else
  if(mpDCDCConfig == NULL)
  {
    // DCDC_Init has to be called prior to this function
    *pDCDCPSwStatus = gDCDC_PSwStatus_High_c;
    return FALSE;
  }
  if( REG_READ_FIELD(DCDC->REG0, DCDC_REG0_PSWITCH_STATUS) == 0)
  {
    *pDCDCPSwStatus = gDCDC_PSwStatus_Low_c;
  }
  else
  {
    *pDCDCPSwStatus = gDCDC_PSwStatus_High_c;
  }
  return TRUE;
#endif
}

///*---------------------------------------------------------------------------
//* NAME: DCDC_ShutDown
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
void DCDC_ShutDown(void)
{
#if gDCDC_Enabled_d == 0
  return ;
#else
  if(mpDCDCConfig == NULL)
  {
    // DCDC_Init has to be called prior to this function
    return;
  }
  if(mpDCDCConfig->dcdcMode == gDCDC_Mode_Buck_c)
  {
    if( REG_READ_FIELD(DCDC->REG0, DCDC_REG0_PSWITCH_STATUS) == 0)
    {
      DCDC->REG4 = (uint32_t)0x3E770001;
      DCDC->REG4 = (uint32_t)0x3E770001;      
    }
  }
#endif
}
///*---------------------------------------------------------------------------
//* NAME: DCDC_InstallPSwitchCallback
//* DESCRIPTION: 
//* PARAMETERS:  
//* RETURN: 
//* NOTES: none
//*---------------------------------------------------------------------------*/
bool_t DCDC_InstallPSwitchCallback(pfDCDCPSwitchCallback_t pfPSwCallback, dcdcPSwIntEdge_t pSwIntEdge)
{
#if gDCDC_Enabled_d == 0
   return FALSE;
#else
//   uint32_t reg4; 
  if(mpDCDCConfig == NULL)
  {
    // DCDC_Init has to be called prior to this function
    return FALSE;
  }
  NVIC_DisableIRQ(LVD_LVW_DCDC_IRQn);
  mpfDCDCPSwitchCallback = pfPSwCallback;
  
  if(pfPSwCallback != NULL)
  {
    uint32_t reg6;
    reg6 = DCDC->REG6 & ~(DCDC_REG6_PSWITCH_INT_RISE_EN_MASK | DCDC_REG6_PSWITCH_INT_FALL_EN_MASK | DCDC_REG6_PSWITCH_INT_MUTE_MASK);
    if(gDCDC_PSwIntEdge_Rising_c & pSwIntEdge )
    {
      reg6 |= DCDC_REG6_PSWITCH_INT_RISE_EN_MASK;
    }
    if(gDCDC_PSwIntEdge_Falling_c & pSwIntEdge )
    {
      reg6 |= DCDC_REG6_PSWITCH_INT_FALL_EN_MASK;
    }
    DCDC->REG6 = reg6;
    NVIC_SetPriority(LVD_LVW_DCDC_IRQn, gDCDC_IsrPrio_c >> (8U - __NVIC_PRIO_BITS));
    OSA_InstallIntHandler(LVD_LVW_DCDC_IRQn, DCDC_PSwitchIsr);
    NVIC_EnableIRQ(LVD_LVW_DCDC_IRQn);
  }
  else
  {
    DCDC->REG6 &= ~(DCDC_REG6_PSWITCH_INT_RISE_EN_MASK | DCDC_REG6_PSWITCH_INT_FALL_EN_MASK);
  }
  return TRUE;
#endif
}
/*****************************************************************************
 *                               <<< EOF >>>                                 *
 *****************************************************************************/
