#include "A36487_250.h"
#include "FIRMWARE_VERSION.h"
#include "timer.h"


#define FCY 10000000

//Global Variables
PULSE_PARAMETERS psb_params;
PSB_DATA psb_data;

//Limited scope variables
unsigned char change_pulse_width_counter;
//unsigned char state;
const unsigned int  dose_intensities[4] = {15, 95, 175, 255};  // fixed constants



unsigned char ReadDosePersonality(void);
//
//  Return:
//      0x00 = No dose personailty installed
//      0x02 = Ultra Low Dose Personality Installed
//      0x04 = Low Dose Personailty Installed
//      0x08 = Medium Dose Personality Installed
//      0x10 = High Dose Personailty Installed
//      0xFF = Problem reading personailty module
//



void ReadTrigPulseWidth(void);
unsigned char FilterTrigger(unsigned char param);
void ReadAndSetEnergy(void);
void ProgramShiftRegisters(void);
unsigned int GetInterpolationValue(unsigned int low_point, unsigned int high_point, unsigned low_value, unsigned high_value, unsigned point);
void PulseSyncStateMachine(void);
void DoA36487(void);
void DoStartupLEDs(void);


void Initialize(void);
void InitPins(void);
void InitTimer2(void);
void InitINT3(void);
void InitTimer1(void);

//Processor Setup
_FOSC(EC & CSW_FSCM_OFF); // Primary Oscillator without PLL and Startup with User Selected Oscillator Source, CLKOUT 10MHz is used to measure trigger width.
//_FWDT(WDT_ON & WDTPSA_512 & WDTPSB_2);  // Watchdog Timer is enabled, 1024ms TIMEOUT
_FWDT(WDT_ON & WDTPSA_512 & WDTPSB_8);  // 8 Second watchdog timer 
_FBORPOR(PWRT_64 & BORV_27 & PBOR_ON & MCLR_EN); // Brown out and Power on Timer settings
_FBS(WR_PROTECT_BOOT_OFF & NO_BOOT_CODE & NO_BOOT_EEPROM & NO_BOOT_RAM);
_FSS(WR_PROT_SEC_OFF & NO_SEC_CODE & NO_SEC_EEPROM & NO_SEC_RAM);
_FGS(CODE_PROT_OFF);
_FICD(PGD);


int main(void) {
    psb_data.state_machine = STATE_INIT;
    while (1) {
      PulseSyncStateMachine();
    }
}



void PulseSyncStateMachine(void) {
  // LOOP Through the state machine is around 30uS Nominally, 50uS when it processes a sync command
  // Need to determine how much time processing a trigger takes.

  switch (psb_data.state_machine) {

  case STATE_INIT:
    psb_data.personality = 0;
    Initialize();
    ETMCanSlaveInitialize(FCY, ETM_CAN_ADDR_PULSE_SYNC_BOARD, _PIN_RG14, 4); 
    ETMCanSlaveLoadConfiguration(36487, 250, FIRMWARE_AGILE_REV, FIRMWARE_BRANCH, FIRMWARE_MINOR_REV);
    psb_data.personality = ReadDosePersonality();  // DPARKER THIS IS NOT WORKING
    // DPARKER ADDED FOR TESTING TO MAKE IT WORK
    psb_data.personality = 255;
    _CONTROL_NOT_READY = 1;
    _CONTROL_NOT_CONFIGURED = 1;
    PIN_CPU_HV_ENABLE_OUT = !OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = !OLL_CPU_XRAY_ENABLE;
    PIN_CPU_WARNING_LAMP_OUT = !OLL_CPU_WARNING_LAMP;
    psb_data.state_machine = STATE_WAIT_FOR_CONFIG;
    break;

  case STATE_WAIT_FOR_CONFIG:
    _CONTROL_NOT_READY = 1;
    _CONTROL_NOT_CONFIGURED = 1;
    PIN_CPU_HV_ENABLE_OUT = !OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = !OLL_CPU_XRAY_ENABLE;
    PIN_CPU_WARNING_LAMP_OUT = !OLL_CPU_WARNING_LAMP;
    while (psb_data.state_machine == STATE_WAIT_FOR_CONFIG) {
      DoA36487();
      ETMCanSlaveDoCan();
      DoStartupLEDs();
      
      if ((psb_data.led_flash_counter >= LED_STARTUP_FLASH_TIME) && (psb_data.counter_config_received == 0b1111)) {
	psb_data.state_machine = STATE_HV_OFF;
      }
    }
    break;

  case STATE_HV_OFF:
    _CONTROL_NOT_CONFIGURED = 0;
    _CONTROL_NOT_READY = 0;
    PIN_CPU_HV_ENABLE_OUT = !OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = !OLL_CPU_XRAY_ENABLE;
    PIN_CPU_WARNING_LAMP_OUT = !OLL_CPU_WARNING_LAMP;
    while (psb_data.state_machine == STATE_HV_OFF) {
      DoA36487();
      ETMCanSlaveDoCan();
      
      if (_SYNC_CONTROL_PULSE_SYNC_DISABLE_HV == 0) {
	psb_data.state_machine = STATE_HV_ENABLE;
      }
      
      if (_FAULT_REGISTER) {
	psb_data.state_machine = STATE_FAULT;
      }
    }
    break;


  case STATE_HV_ENABLE:
    _CONTROL_NOT_CONFIGURED = 0;
    _CONTROL_NOT_READY = 0;
    PIN_CPU_HV_ENABLE_OUT = OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = !OLL_CPU_XRAY_ENABLE;
    PIN_CPU_WARNING_LAMP_OUT = !OLL_CPU_WARNING_LAMP;
    while (psb_data.state_machine == STATE_HV_ENABLE) {
      DoA36487();
      ETMCanSlaveDoCan();
      
      if ((_SYNC_CONTROL_PULSE_SYNC_DISABLE_XRAY == 0) && (PIN_CUSTOMER_BEAM_ENABLE_IN == ILL_CUSTOMER_BEAM_ENABLE)) {
	psb_data.state_machine = STATE_X_RAY_ENABLE;
      }
      
      if (_SYNC_CONTROL_PULSE_SYNC_DISABLE_HV) {
	psb_data.state_machine = STATE_HV_OFF;
      }

      if (_FAULT_REGISTER) {
	psb_data.state_machine = STATE_FAULT;
      }
    }
    break;


  case STATE_X_RAY_ENABLE:
    _CONTROL_NOT_CONFIGURED = 0;
    _CONTROL_NOT_READY = 0;
    PIN_CPU_HV_ENABLE_OUT = OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = OLL_CPU_XRAY_ENABLE;
    PIN_CPU_WARNING_LAMP_OUT = OLL_CPU_WARNING_LAMP;
    while (psb_data.state_machine == STATE_X_RAY_ENABLE) {
      DoA36487();
      ETMCanSlaveDoCan();
      
      if (_SYNC_CONTROL_PULSE_SYNC_DISABLE_XRAY || _SYNC_CONTROL_PULSE_SYNC_DISABLE_HV || (PIN_CUSTOMER_BEAM_ENABLE_IN == !ILL_CUSTOMER_BEAM_ENABLE)) {
	psb_data.state_machine = STATE_HV_ENABLE;
      }
      
      if (_FAULT_REGISTER) {
	psb_data.state_machine = STATE_FAULT;
      }
    }
    break;


  case STATE_FAULT:
    _CONTROL_NOT_CONFIGURED = 0;
    _CONTROL_NOT_READY = 1;
    PIN_CPU_HV_ENABLE_OUT = !OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = !OLL_CPU_XRAY_ENABLE;
    while (psb_data.state_machine == STATE_FAULT) {
      DoA36487();
      ETMCanSlaveDoCan();
      
      if (_FAULT_REGISTER == 0) {
	psb_data.state_machine = STATE_WAIT_FOR_CONFIG;
      }
    }
    break;

      
  default:
    _CONTROL_NOT_CONFIGURED = 1;
    _CONTROL_NOT_READY = 1;
    PIN_CPU_HV_ENABLE_OUT = !OLL_CPU_HV_ENABLE;
    PIN_CPU_XRAY_ENABLE_OUT = !OLL_CPU_XRAY_ENABLE;
    psb_data.state_machine = STATE_UNKNOWN;
    while (1) {
      DoA36487();
      ETMCanSlaveDoCan();
    }
    break;
    
  } // switch (psb_data.state_machine) {

}


void __attribute__((interrupt(__save__(CORCON,SR)), no_auto_psv)) _INT3Interrupt(void) {
  // A trigger was recieved.
  // THIS DOES NOT MEAN THAT A PULSE WAS GENERATED
  // If (PIN_CPU_XRAY_ENABLE_OUT == OLL_CPU_XRAY_ENABLE)  && (PIN_CUSTOMER_XRAY_ON_IN == ILL_CUSTOMER_BEAM_ENABLE) then we "Probably" generated a pulse

  _INT3IF = 0;		// Clear Interrupt flag


  // Calculate the Trigger PRF
  // TMR1 is used to time the time between INT3 interrupts
  psb_data.last_period = TMR1;
  if (_T1IF) {
    // The timer exceed it's period of 400mS - (Will happen if the PRF is less than 2.5Hz)
    psb_data.last_period = 62501;  // This will indicate that the PRF is Less than 2.5Hz
  }
  TMR1 = 0;
  _T1IF = 0;
  
  // INT3 Trigger is delayed in hardware 40us from the input trigger, If the trigger is still high then it is TOO Long
  if (PIN_TRIG_INPUT == ILL_TRIG_ON) {
    _FAULT_TRIGGER_STAYED_ON = 1;
  }

  psb_data.trigger_complete = 1;
}

void ReadTrigPulseWidth(void)
{
      unsigned int data;
      unsigned char i;

      PIN_SPI_CLK_OUT  = 0;
      Nop();
      PIN_PW_SHIFT_OUT = !OLL_PW_SHIFT; // load the reg
      Nop();
      __delay32(1); // 100ns for 10M TCY
      PIN_PW_SHIFT_OUT = OLL_PW_SHIFT;  // enable shift
      Nop();
      __delay32(1); // 100ns for 10M TCY

      data = PIN_SPI_DATA_IN;

      for (i = 0; i < 8; i++)
      {
      	PIN_SPI_CLK_OUT = 1;
        Nop();
        data <<= 1;
        data |= PIN_SPI_DATA_IN;
      	PIN_SPI_CLK_OUT = 0;
        Nop();
        __delay32(1); // 100ns for 10M TCY
      }

      PIN_PW_SHIFT_OUT = !OLL_PW_SHIFT; // make load active when idle
      Nop();

      if (data & 0x0100)  // counter overflow
      {
          psb_data.trigger_input = 0xFF;
      }
      else
      {
          psb_data.trigger_input = data & 0xFF;
      }
      psb_data.trigger_filtered = FilterTrigger(psb_data.trigger_input);

      if (psb_data.trigger_filtered < 245)   //signify to pfn control board what mode to expect
          PIN_MODE_OUT = OLL_MODE_PORTAL;   //so it can use a different target
      else                                  //current setpoint for low energy
          PIN_MODE_OUT = OLL_MODE_GANTRY;
}

unsigned char FilterTrigger(unsigned char param)
{
    int x;

    //Establish Dose Levels to reduce jitter and provide consistent dose vs trigger width
    //Every bit represents 20ns pulse width change on the electron gun
    //Every bit also represents a 200ns pulse width change from the customer
    if (param > (DOSE_LEVELS - 1))
    {
        for (x = 0; x <= (param % DOSE_LEVELS); x++)
            param--;
    }
    else
        param = 0;

    //Ensure that at least 15 of the same width pulses in a row only change the sampled width
    if (param != psb_data.last_trigger_filtered)
    {
        change_pulse_width_counter++;
        if (change_pulse_width_counter < 15)
            param = psb_data.last_trigger_filtered;
        else
            psb_data.last_trigger_filtered = param;
    }
    else
        change_pulse_width_counter = 0;

    return param;
}

void ReadAndSetEnergy()
{
    if ((PIN_LOW_MODE_IN == HI) && (PIN_HIGH_MODE_IN == HI))
    {
        if (PIN_ENERGY_CMD_IN1 == HI)
        {
            Nop();
            PIN_AFC_TRIGGER_OK_OUT = OLL_AFC_TRIGGER_OK;    //Trigger the AFC in high energy only
            Nop();
            PIN_GUN_POLARITY_OUT = !OLL_GUN_POLARITY;
            Nop();
            PIN_ENERGY_CPU_OUT = !OLL_ENERGY_CPU;
            Nop();
            psb_data.energy = HI;
        }
        else
        {
            Nop();
            PIN_AFC_TRIGGER_OK_OUT = !OLL_AFC_TRIGGER_OK;   //Do not trigger the AFC in low energy
            Nop();
            PIN_GUN_POLARITY_OUT = !OLL_GUN_POLARITY;
            Nop();
            PIN_ENERGY_CPU_OUT = OLL_ENERGY_CPU;
            Nop();
            psb_data.energy = LOW;
        }
    }
    else
    {
        if (PIN_HIGH_MODE_IN == HI)
        {
            Nop();
            PIN_AFC_TRIGGER_OK_OUT = OLL_AFC_TRIGGER_OK;    //Trigger the AFC in single energy mode
            Nop();
            PIN_GUN_POLARITY_OUT = OLL_GUN_POLARITY;
            Nop();
            PIN_ENERGY_CPU_OUT = OLL_ENERGY_CPU;
            Nop();
            psb_data.energy = LOW;
        }
        else
        {
            Nop();
            PIN_AFC_TRIGGER_OK_OUT = OLL_AFC_TRIGGER_OK;    //Trigger the AFC in single energy mode
            Nop();
            PIN_GUN_POLARITY_OUT = OLL_GUN_POLARITY;
            Nop();
            PIN_ENERGY_CPU_OUT = !OLL_ENERGY_CPU;
            Nop();
            psb_data.energy = HI;
        }
    }
}

void ProgramShiftRegisters(void)
{
    unsigned int p;
    unsigned int q;
    unsigned long temp;
    unsigned long bittemp;

    PIN_PW_CLR_CNT_OUT = OLL_PW_CLR_CNT;			 // clear width count
    Nop();
    PIN_PW_HOLD_LOWRESET_OUT = !OLL_PW_HOLD_LOWRESET;	 // reset start to disable pulse
    Nop();

    if (psb_data.energy == HI) {
      psb_data.dose_sample_delay    = psb_params.dose_sample_delay_high;
      psb_data.pfn_delay   = psb_params.pfn_delay_high;
      psb_data.afc_delay   = psb_params.afc_delay_high;
      psb_data.magnetron_current_sample_delay = psb_params.magnetron_current_sample_delay_high;
    } else {
      psb_data.dose_sample_delay    = psb_params.dose_sample_delay_low;
      psb_data.pfn_delay   = psb_params.pfn_delay_low;
      psb_data.afc_delay   = psb_params.afc_delay_low;
      psb_data.magnetron_current_sample_delay = psb_params.magnetron_current_sample_delay_low;
    }
     

    // do inteplation for grid delay and grid width
    for (p = 0; p < 4; p++)
    {
    	if (psb_data.trigger_filtered <= dose_intensities[p]) break;
    }
    
    if (p == 0)
    {
        if (psb_data.energy == HI) {
            psb_data.grid_delay = psb_params.grid_delay_high0;
            psb_data.grid_width = psb_params.grid_width_high0;
        }
        else {
            psb_data.grid_delay = psb_params.grid_delay_low0;
            psb_data.grid_width = psb_params.grid_width_low0;
        }
    }
    else if (p >= 4)
    {
        if (psb_data.energy == HI) {
            psb_data.grid_delay = psb_params.grid_delay_high3;
            psb_data.grid_width = psb_params.grid_width_high3;
        }
        else {
            psb_data.grid_delay = psb_params.grid_delay_low3;
            psb_data.grid_width = psb_params.grid_width_low3;
        }
    }
    else // interpolation
    {
        if (p == 1) {
            if (psb_data.energy == HI) {
                psb_data.grid_delay = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_delay_high0, psb_params.grid_delay_high1, psb_data.trigger_filtered);
                psb_data.grid_width = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_width_high0, psb_params.grid_width_high1, psb_data.trigger_filtered);
            }
            else {
                psb_data.grid_delay = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_delay_low0, psb_params.grid_delay_low1, psb_data.trigger_filtered);
                psb_data.grid_width = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_width_low0, psb_params.grid_width_low1, psb_data.trigger_filtered);
            }
        }
        else if (p == 2) {
            if (psb_data.energy == HI) {
                psb_data.grid_delay = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_delay_high1, psb_params.grid_delay_high2, psb_data.trigger_filtered);
                psb_data.grid_width = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_width_high1, psb_params.grid_width_high2, psb_data.trigger_filtered);
            }
            else {
                psb_data.grid_delay = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_delay_low1, psb_params.grid_delay_low2, psb_data.trigger_filtered);
                psb_data.grid_width = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_width_low1, psb_params.grid_width_low2, psb_data.trigger_filtered);
            }
        }
        else if (p == 3) {
            if (psb_data.energy == HI) {
                psb_data.grid_delay = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_delay_high2, psb_params.grid_delay_high3, psb_data.trigger_filtered);
                psb_data.grid_width = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_width_high2, psb_params.grid_width_high3, psb_data.trigger_filtered);
            }
            else {
                psb_data.grid_delay = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_delay_low2, psb_params.grid_delay_low3, psb_data.trigger_filtered);
                psb_data.grid_width = GetInterpolationValue(dose_intensities[p - 1], dose_intensities[p], psb_params.grid_width_low2, psb_params.grid_width_low3, psb_data.trigger_filtered);
            }
        }
    }

    for (p = 0; p < 6; p++)
    {
    	if (p == 0)
            temp = psb_data.grid_width;     //Grid Width
        else if (p == 1)
            temp = psb_data.grid_delay;     //Grid Delay
        else if (p == 2) {
            temp = psb_data.dose_sample_delay;       //RF PCB Delay  // This is not the current monitor trigger
        }
        else if (p == 3)
            temp = psb_data.pfn_delay;   //PFN Delay
        else if (p == 4)
            temp = psb_data.magnetron_current_sample_delay;      //Dosimeter delay (not used)
        else if (p == 5)
            temp = psb_data.afc_delay;   //AFC Delay
        else
            temp = 0;

        for (q = 0; q < 8; q++)
        {
            PIN_SPI_CLK_OUT = 0;
            Nop();

            bittemp = temp & 0x80;
            temp = temp << 1;

            if (bittemp == 0x80)
            {
                PIN_SPI_DATA_OUT = 1;
                Nop();
            }
            else
            {
                PIN_SPI_DATA_OUT = 0;
                Nop();
            }

            PIN_SPI_CLK_OUT = 1;
            Nop();
        }

        if (p == 1)						//Latch Gun delay and width data into shift registers
        {
            PIN_LD_DELAY_GUN_OUT = 0;
            Nop();
            PIN_LD_DELAY_GUN_OUT = 1;
            Nop();
        }
        else if (p == 3)				//Latch PFN/RF delay data into shift registers
        {
            PIN_LD_DELAY_PFN_OUT = 0;
            Nop();
            PIN_LD_DELAY_PFN_OUT = 1;
            Nop();
        }
        else if (p == 5)				//Latch AFC/Dose delay data into shift registers
        {
            PIN_LD_DELAY_AFC_OUT = 0;
            Nop();
            PIN_LD_DELAY_AFC_OUT = 1;
            Nop();
        }
    }

    PIN_PW_CLR_CNT_OUT = !OLL_PW_CLR_CNT;			 // enable width count
    Nop();
    if (PIN_TRIG_INPUT != ILL_TRIG_ON)
    {
    	PIN_PW_HOLD_LOWRESET_OUT = OLL_PW_HOLD_LOWRESET;   // clear reset only when trig pulse is low
        Nop();
    	//_FAULT_TRIGGER_STAYED_ON = 0;
    }
    else {
      //_FAULT_TRIGGER_STAYED_ON = 1;
    }
}

// calculate the interpolation value
unsigned int GetInterpolationValue(unsigned int low_point, unsigned int high_point, unsigned low_value, unsigned high_value, unsigned point)
{
   double dtemp, dslope;
   unsigned int ret = low_value;

   if (high_point > low_point)  // high point has to be bigger
   {
   	dslope = ((double)high_value - (double)low_value) / ((double)high_point - (double)low_point);
        dtemp = (double)point - (double)low_point;
        dtemp *= dslope;
        dtemp += low_value;
        ret = (unsigned)dtemp;
   }
   return (ret);
}

void DoStartupLEDs(void) {
  switch ((psb_data.led_flash_counter >> 4) & 0b11)
    {
    case 0:
      PIN_LED_READY   = !OLL_LED_ON;
      PIN_LED_XRAY_ON = !OLL_LED_ON;
      PIN_LED_SUMFLT  = !OLL_LED_ON;
      break;

    case 1:
      PIN_LED_READY   = OLL_LED_ON;
      PIN_LED_XRAY_ON = !OLL_LED_ON;
      PIN_LED_SUMFLT  = !OLL_LED_ON;
      break;

    case 2:
      PIN_LED_READY   = !OLL_LED_ON;
      PIN_LED_XRAY_ON = OLL_LED_ON;
      PIN_LED_SUMFLT  = !OLL_LED_ON;
      break;

    case 3:
      PIN_LED_READY   = !OLL_LED_ON;
      PIN_LED_XRAY_ON = !OLL_LED_ON;
      PIN_LED_SUMFLT  = OLL_LED_ON;
      break;
    }

}


void DoA36487(void) {
  unsigned long temp32;

  if (psb_data.trigger_complete) {
    // Do post trigger process
    psb_data.trigger_complete = 0;
    psb_data.time_no_trigger = 0;
    
    // This is used to detect if the trigger is high (which would cause constant pulses to the system)
    if (PIN_TRIG_INPUT != ILL_TRIG_ON) {
      ReadTrigPulseWidth();
      ReadAndSetEnergy();
    } else {  // if pulse trig stays on, set to minimum dose and flag fault
      //_FAULT_TRIGGER_STAYED_ON = 1;
      psb_data.trigger_filtered = 0;
      
    }
  
    ProgramShiftRegisters();
    psb_data.period_filtered = RCFilterNTau(psb_data.period_filtered, psb_data.last_period, RC_FILTER_4_TAU);
    

    if (_SYNC_CONTROL_HIGH_SPEED_LOGGING) {
      ETMCanSlaveLogCustomPacketC();
    }
    
    psb_data.pulses_on++;       // This counts the pulses
    ETMCanSlavePulseSyncSendNextPulseLevel(psb_data.energy, psb_data.pulses_on);
  }
  

  /*
  local_debug_data.debug_0 = psb_data.grid_delay;
  local_debug_data.debug_1 = psb_data.grid_width;
  local_debug_data.debug_2 = psb_data.dose_sample_delay;
  local_debug_data.debug_3 = psb_data.energy;

  local_debug_data.debug_4 = psb_data.trigger_input;
  local_debug_data.debug_5 = psb_data.trigger_filtered;
  local_debug_data.debug_6 = psb_data.last_trigger_filtered;
  local_debug_data.debug_7 = psb_data.personality;

  local_debug_data.debug_8 = psb_data.state_machine;
  local_debug_data.debug_9 = psb_data.pulses_on;
  local_debug_data.debug_A = psb_data.last_period;
  local_debug_data.debug_B = psb_data.period_filtered;
  local_debug_data.debug_C = psb_data.rep_rate_deci_herz;
  */

  local_debug_data.debug_0 = (psb_params.grid_delay_high3 << 8) + psb_params.grid_delay_high2;
  local_debug_data.debug_1 = (psb_params.grid_delay_high1 << 8) + psb_params.grid_delay_high0;
  local_debug_data.debug_2 = (psb_params.pfn_delay_high << 8) + psb_params.dose_sample_delay_high;
  
  local_debug_data.debug_3 = (psb_params.grid_width_high3 << 8) + psb_params.grid_width_high2;
  local_debug_data.debug_4 = (psb_params.grid_width_high1 << 8) + psb_params.grid_width_high0;
  local_debug_data.debug_5 = (psb_params.afc_delay_high << 8) + psb_params.magnetron_current_sample_delay_high;
  
  local_debug_data.debug_6 = (psb_params.grid_delay_low3 << 8) + psb_params.grid_delay_low2;
  local_debug_data.debug_7 = (psb_params.grid_delay_low1 << 8) + psb_params.grid_delay_low0;
  local_debug_data.debug_8 = (psb_params.pfn_delay_low << 8) + psb_params.dose_sample_delay_low;
  
  local_debug_data.debug_9 = (psb_params.grid_width_low3 << 8) + psb_params.grid_width_low2;
  local_debug_data.debug_A = (psb_params.grid_width_low1 << 8) + psb_params.grid_width_low0;
  local_debug_data.debug_B = (psb_params.afc_delay_low << 8) + psb_params.magnetron_current_sample_delay_low;
  



  // ---------- UPDATE LOCAL FAULTS ------------------- //

  if ((psb_data.state_machine == STATE_FAULT) && (_SYNC_CONTROL_RESET_ENABLE)) {
    _FAULT_REGISTER = 0;
  }
  
  if (PIN_PANEL_IN == 0) {
    _FAULT_PANEL = 1;
  }
  
  if (PIN_KEY_LOCK_IN == 0) {
    _FAULT_KEYLOCK = 1;
  }

  if (PIN_PFN_OK == 0) {
    _FAULT_PFN_STATUS = 1;
  }
  
  if (PIN_RF_OK == 0) {
    _FAULT_RF_STATUS = 1;
  }

  if (PIN_XRAY_CMD_MISMATCH_IN == !ILL_XRAY_CMD_MISMATCH) {
    _FAULT_TIMING_MISMATCH = 1;
  }

  // _FAULT_TRIGGER_STAYED_ON is set by INT3 Interrupt

  if ((PIN_CUSTOMER_XRAY_ON_IN) && (!PIN_CUSTOMER_BEAM_ENABLE_IN)) {
    _FAULT_X_RAY_ON_WIHTOUT_HV = 1;
  }
  
  // ---------- END UPDATE LOCAL FAULTS -------------- //




  // ------------- UPDATE STATUS -------------------- //

  _STATUS_CUSTOMER_HV_DISABLE = !PIN_CUSTOMER_BEAM_ENABLE_IN;

  _STATUS_CUSTOMER_X_RAY_DISABLE = !PIN_CUSTOMER_XRAY_ON_IN;

  _STATUS_LOW_MODE_OVERRIDE = PIN_LOW_MODE_IN;
  
  _STATUS_HIGH_MODE_OVERRIDE = PIN_HIGH_MODE_IN;
  
  etm_can_status_register.data_word_A = psb_data.rep_rate_deci_herz;
  etm_can_status_register.data_word_B = psb_data.personality;
  
  // ------------- END UPDATE STATUS --------------------- //



  if (_T2IF) {
    // Run these once every 10ms
    _T2IF = 0;

    // -------------- UPDATE CUSTOMER INTERFACE LED OUTPUTS ---------------- //
    if (_SYNC_CONTROL_PULSE_SYNC_WARMUP_LED) {
      PIN_CPU_WARMUP_OUT = OLL_CPU_WARMUP;
    } else {
      PIN_CPU_WARMUP_OUT = !OLL_CPU_WARMUP;
    }
    
    if (_SYNC_CONTROL_PULSE_SYNC_STANDBY_LED) {
      PIN_CPU_STANDBY_OUT = OLL_CPU_STANDBY;
    } else {
      PIN_CPU_STANDBY_OUT = !OLL_CPU_STANDBY;
    }
    
    if (_SYNC_CONTROL_PULSE_SYNC_READY_LED) {
      PIN_LED_READY = OLL_LED_ON;
      PIN_CPU_READY_OUT = OLL_CPU_READY;
    } else {
      PIN_LED_READY = !OLL_LED_ON;
      PIN_CPU_READY_OUT = !OLL_CPU_READY;
    }
  
    if (_SYNC_CONTROL_PULSE_SYNC_FAULT_LED) {
      PIN_LED_SUMFLT = OLL_LED_ON;
      PIN_CPU_SUMFLT_OUT = OLL_CPU_SUMFLT;
    } else {
      PIN_LED_SUMFLT = !OLL_LED_ON;
      PIN_CPU_SUMFLT_OUT = !OLL_CPU_SUMFLT;
    }
    
    // -------------- END UPDATE CUSTOMER INTERFACE LED OUTPUTS ---------------- //
    

    
    // Look for no triggers for more than a second
    // If so, send a counter reset pulse
    psb_data.time_no_trigger++;
    if (psb_data.time_no_trigger >= 100) {
      psb_data.time_no_trigger = 0;
      PIN_PW_CLR_CNT_OUT = OLL_PW_CLR_CNT;
      __delay32(10);
      PIN_PW_CLR_CNT_OUT = !OLL_PW_CLR_CNT;
    }
    



    // This is not needed as the CAN module will generate 
    psb_data.can_counter_ms += 10;
    if (etm_can_sync_message.sync_3 == 0xFFFF) {
      psb_data.can_counter_ms = 0;
      __builtin_disi(0x3FFF);
      etm_can_sync_message.sync_3 = 0;
      __builtin_disi(0x0000);
    }
    if (psb_data.can_counter_ms >= 150) {
      _FAULT_SYNC_TIMEOUT = 1;
    }
    
    psb_data.led_flash_counter++;
    
    PIN_LED_STANDBY = ((psb_data.led_flash_counter >> 5) & 0b1);


    // DPARKER - NEED TO UPDATE THE REP RATE WHEN NOT PULSING!!!!!
    // Calculate the rep rate
    temp32 = 1562500;
    temp32 /= psb_data.period_filtered;
    psb_data.rep_rate_deci_herz = temp32;
    if (_T1IF) {
      // We are pulseing at less than 2.5Hz
      // Set the rep rate to zero
      psb_data.rep_rate_deci_herz = 0;
    }

  }
}


void __attribute__((interrupt, no_auto_psv)) _DefaultInterrupt(void) {
    Nop();
    Nop();
    __asm__ ("Reset");
}




void Initialize(void)
{
    InitPins();
    InitINT3(); //Trigger Interrupt
    InitTimer2();
    InitTimer1();
}

void InitPins()
{
    //Trigger Measurement Pins
    TRIS_PIN_TRIG_INPUT             = TRIS_INPUT_MODE;
    PIN_PW_SHIFT_OUT                = !OLL_PW_SHIFT;
    PIN_PW_CLR_CNT_OUT              = !OLL_PW_CLR_CNT;                // clear active
    PIN_PW_HOLD_LOWRESET_OUT        = !OLL_PW_HOLD_LOWRESET;	 // reset active
    TRIS_PIN_PW_SHIFT_OUT           = TRIS_OUTPUT_MODE;
    TRIS_PIN_PW_CLR_CNT_OUT         = TRIS_OUTPUT_MODE;
    TRIS_PIN_PW_HOLD_LOWRESET_OUT   = TRIS_OUTPUT_MODE;
    TRIS_PIN_40US_IN1               = TRIS_INPUT_MODE;
    TRIS_PIN_40US_IN2               = TRIS_INPUT_MODE;
    TRIS_PIN_TRIG_INPUT             = TRIS_INPUT_MODE;

    // Personality ID Pins
    PIN_ID_SHIFT_OUT            = !OLL_ID_SHIFT;
    TRIS_PIN_ID_SHIFT_OUT       = TRIS_OUTPUT_MODE;
    PIN_ID_CLK_OUT              = !OLL_ID_CLK;
    TRIS_PIN_ID_CLK_OUT         = TRIS_OUTPUT_MODE;
    TRIS_PIN_ID_DATA_IN         = TRIS_INPUT_MODE;

    //Spare pins (not used in current application)
    TRIS_PIN_PACKAGE_ID1_IN         = TRIS_INPUT_MODE;
    TRIS_PIN_READY_FOR_ANALOG_OUT   = TRIS_OUTPUT_MODE;
    PIN_READY_FOR_ANALOG_OUT        = OLL_READY_FOR_ANALOG;

    //Control to PFN control board for Gantry/Portal Selection
    TRIS_PIN_MODE_OUT           = TRIS_OUTPUT_MODE;

    //Hardware Status
    TRIS_PIN_KEY_LOCK_IN            = TRIS_INPUT_MODE;
    TRIS_PIN_PANEL_IN               = TRIS_INPUT_MODE;
    TRIS_PIN_XRAY_CMD_MISMATCH_IN   = TRIS_INPUT_MODE;
//    PIN_CUSTOMER_BEAM_ENABLE_IN     = !ILL_CUSTOMER_BEAM_ENABLE;
    TRIS_PIN_CUSTOMER_BEAM_ENABLE_IN = TRIS_INPUT_MODE;
    PIN_CUSTOMER_XRAY_ON_IN         = !ILL_CUSTOMER_XRAY_ON;
    TRIS_PIN_CUSTOMER_XRAY_ON_IN    = TRIS_INPUT_MODE;

    //Energy Select Pins
    TRIS_PIN_LOW_MODE_IN        = TRIS_INPUT_MODE;
    TRIS_PIN_HIGH_MODE_IN 	= TRIS_INPUT_MODE;
    PIN_ENERGY_CPU_OUT          = !OLL_ENERGY_CPU;
    TRIS_PIN_ENERGY_CPU_OUT     = TRIS_OUTPUT_MODE;
    TRIS_PIN_AFC_TRIGGER_OK_OUT = TRIS_OUTPUT_MODE;
    PIN_AFC_TRIGGER_OK_OUT      = OLL_AFC_TRIGGER_OK;
    PIN_RF_POLARITY_OUT         = OLL_RF_POLARITY;
    TRIS_PIN_RF_POLARITY_OUT    = TRIS_OUTPUT_MODE;
    PIN_HVPS_POLARITY_OUT       = !OLL_HVPS_POLARITY;
    TRIS_PIN_HVPS_POLARITY_OUT  = TRIS_OUTPUT_MODE;
    PIN_GUN_POLARITY_OUT        = !OLL_GUN_POLARITY;
    TRIS_PIN_GUN_POLARITY_OUT   = TRIS_OUTPUT_MODE;
    TRIS_PIN_ENERGY_CMD_IN1     = TRIS_INPUT_MODE;
    TRIS_PIN_ENERGY_CMD_IN2     = TRIS_INPUT_MODE;
   
    //State Hardware Control
    TRIS_PIN_CPU_HV_ENABLE_OUT      = TRIS_OUTPUT_MODE;
    PIN_CPU_HV_ENABLE_OUT           = !OLL_CPU_HV_ENABLE;
    TRIS_PIN_CPU_XRAY_ENABLE_OUT    = TRIS_OUTPUT_MODE;
    PIN_CPU_XRAY_ENABLE_OUT         = !OLL_CPU_XRAY_ENABLE;
    TRIS_PIN_CPU_WARNING_LAMP_OUT   = TRIS_OUTPUT_MODE;
    PIN_CPU_WARNING_LAMP_OUT        = !OLL_CPU_WARNING_LAMP;
    TRIS_PIN_CPU_STANDBY_OUT        = TRIS_OUTPUT_MODE;
    PIN_CPU_STANDBY_OUT             = !OLL_CPU_STANDBY;
    TRIS_PIN_CPU_READY_OUT          = TRIS_OUTPUT_MODE;
    PIN_CPU_READY_OUT               = !OLL_CPU_READY;
    TRIS_PIN_CPU_SUMFLT_OUT         = TRIS_OUTPUT_MODE;
    PIN_CPU_SUMFLT_OUT              = !OLL_CPU_SUMFLT;
    TRIS_PIN_CPU_WARMUP_OUT         = TRIS_OUTPUT_MODE;
//    PIN_CPU_WARMUP_OUT              = !OLL_CPU_WARMUP;
    
    //LEDs
    TRIS_PIN_LED_READY          = TRIS_OUTPUT_MODE;
    PIN_LED_READY               = !OLL_LED_ON;
    TRIS_PIN_LED_STANDBY        = TRIS_OUTPUT_MODE;
    PIN_LED_STANDBY             = !OLL_LED_ON;
    TRIS_PIN_LED_WARMUP         = TRIS_OUTPUT_MODE;
//    PIN_LED_WARMUP              = !OLL_LED_ON;
    TRIS_PIN_LED_XRAY_ON        = TRIS_OUTPUT_MODE;
    PIN_LED_XRAY_ON             = !OLL_LED_ON;
    TRIS_PIN_LED_SUMFLT         = TRIS_OUTPUT_MODE;
    PIN_LED_SUMFLT              = !OLL_LED_ON;

    // Pins for loading the delay lines
    PIN_SPI_CLK_OUT             = 0;
    TRIS_PIN_SPI_CLK_OUT        = TRIS_OUTPUT_MODE;
    PIN_SPI_DATA_OUT            = 0;
    TRIS_PIN_SPI_DATA_OUT       = TRIS_OUTPUT_MODE;
    TRIS_PIN_SPI_DATA_IN        = TRIS_INPUT_MODE;
    TRIS_PIN_LD_DELAY_PFN_OUT   = TRIS_OUTPUT_MODE;
    PIN_LD_DELAY_PFN_OUT        = 0;
    TRIS_PIN_LD_DELAY_AFC_OUT   = TRIS_OUTPUT_MODE;
    PIN_LD_DELAY_AFC_OUT        = 0;
    TRIS_PIN_LD_DELAY_GUN_OUT   = TRIS_OUTPUT_MODE;
    PIN_LD_DELAY_GUN_OUT        = 0;

    //Bypass these to allow xray on
    TRIS_PIN_RF_OK             = TRIS_INPUT_MODE;
    TRIS_PIN_GUN_OK            = TRIS_INPUT_MODE;
    TRIS_PIN_PFN_OK            = TRIS_INPUT_MODE;

    //Communications
    COMM_DRIVER_ENABLE_TRIS = TRIS_OUTPUT_MODE;
    COMM_DRIVER_ENABLE_PIN = 0;
    COMM_RX_TRIS = TRIS_INPUT_MODE;
    COMM_TX_TRIS = TRIS_OUTPUT_MODE;

    ADPCFG = 0b0000000011111111;
    ADCON1 = 0x0000;
}

 void InitINT3()
 {
  // Set up Interrupts
  // Set up external INT3 */
  // This is the trigger interrupt
  _INT3IF = 0;		// Clear Interrupt flag
  _INT3IE = 1;		// Enable INT3 Interrupt
  _INT3EP = 1; 	        // Interrupt on falling edge
  _INT3IP = 7;		// Set interrupt to highest priority
 }

void InitTimer2(void)
{
#define T2_PERIOD_US 10000   // 10mS Period
  T2CON = (T2_ON & T2_IDLE_CON & T2_GATE_OFF & T2_PS_1_8 & T2_SOURCE_INT & T2_32BIT_MODE_OFF);
  PR2 = (unsigned int)((FCY / 1000000)*T2_PERIOD_US/8);
  TMR2 = 0;
  _T2IF = 0;

}
void InitTimer1(void) {
  // Setup Timer 1 to measure interpulse period.
  T1CON = (T1_ON & T1_IDLE_CON & T1_GATE_OFF & T1_PS_1_64 & T1_SOURCE_INT);
  PR1 = 62500;  // 400mS
}



// These defines are what is actually read from the shift register
#define HIGH_DOSE       0x77
#define MEDIUM_DOSE     0xCC
#define LOW_DOSE        0xAA
#define ULTRA_LOW_DOSE  0x99

//Pin requirements
#define PIN_ID_SHIFT_OUT	_LATC2
#define TRIS_PIN_ID_SHIFT_OUT   _TRISC2
#define OLL_ID_SHIFT            1

#define PIN_ID_CLK_OUT 		_LATC3
#define TRIS_PIN_ID_CLK_OUT	_TRISC3
#define OLL_ID_CLK   		1

#define PIN_ID_DATA_IN          _RC4
#define TRIS_PIN_ID_DATA_IN     _TRISC4
#define ILL_ID_DATA  		1


unsigned char ReadDosePersonality() {
      unsigned int data;
      unsigned char i, data1, data2;

      PIN_ID_CLK_OUT   = !OLL_ID_CLK;
      PIN_ID_SHIFT_OUT = !OLL_ID_SHIFT; // load the reg
      __delay32(1); // 100ns for 10M TCY
      PIN_ID_SHIFT_OUT = OLL_ID_SHIFT;  // enable shift
      __delay32(1); // 100ns for 10M TCY

      data = PIN_ID_DATA_IN;

      for (i = 0; i < 8; i++)
      {
      	PIN_ID_CLK_OUT = OLL_ID_CLK;
        data <<= 1;
        data |= PIN_ID_DATA_IN;
      	PIN_ID_CLK_OUT = !OLL_ID_CLK;
        __delay32(1); // 100ns for 10M TCY
      }

      //if bits do not match then bad module
      data1 = data & 0x01;
      data2 = data & 0x10;
      if (data1 != (data2 >> 4))
          return 0xFF;
      data1 = data & 0x02;
      data2 = data & 0x20;
      if (data1 != (data2 >> 4))
          return 0xFF;
      data1 = data & 0x04;
      data2 = data & 0x40;
      if (data1 != (data2 >> 4))
          return 0xFF;
      data1 = data & 0x08;
      data2 = data & 0x80;
      if (data1 != (data2 >> 4))
          return 0xFF;

      //bit 3 is 1 except when 0,1,2 are 1
      data1 = data & 0x08;
      data2 = data & 0x07;
      if (data1 != data2)
            return 0xFF;

      if (data == ULTRA_LOW_DOSE)
          return 0x02;
      else if (data == LOW_DOSE)
          return 0x04;
      else if (data == MEDIUM_DOSE)
          return 0x08;
      else if (data == HIGH_DOSE)
          return 0x10;
      else if (data == 0xFF)
          return data;
      else
        return 0;
}
