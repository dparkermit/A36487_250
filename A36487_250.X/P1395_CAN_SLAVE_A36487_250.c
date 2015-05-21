#include "P1395_CAN_SLAVE.h"
#include "A36487_250.h"

void ETMCanSlaveExecuteCMDBoardSpecific(ETMCanMessage* message_ptr) {
  unsigned int index_word;
  unsigned int temp;
  index_word = message_ptr->word3;
  switch (index_word) 
    {
      /*
	Place all board specific commands here
      */
      
    case ETM_CAN_REGISTER_PULSE_SYNC_SET_1_HIGH_ENERGY_TIMING_REG_0:
      temp = (message_ptr->word2 & 0xFF00) >> 8;
      psb_params.grid_delay_high3 = temp;
      temp = message_ptr->word2 & 0x00FF;
      psb_params.grid_delay_high2 = temp;
      temp = (message_ptr->word1 & 0xFF00) >> 8;
      psb_params.grid_delay_high1 = temp;
      temp = message_ptr->word1 & 0x00FF;
      psb_params.grid_delay_high0 = temp;
      temp = (message_ptr->word0 & 0xFF00) >> 8;
      psb_params.pfn_delay_high = temp;
      temp = message_ptr->word0 & 0x00FF;
      psb_params.dose_sample_delay_high = temp;
      psb_data.counter_config_received |= 0b0001;
      break;

    case ETM_CAN_REGISTER_PULSE_SYNC_SET_1_HIGH_ENERGY_TIMING_REG_1:
      temp = (message_ptr->word2 & 0xFF00) >> 8;
      psb_params.grid_width_high3 = temp;
      temp = message_ptr->word2 & 0x00FF;
      psb_params.grid_width_high2 = temp;
      temp = (message_ptr->word1 & 0xFF00) >> 8;
      psb_params.grid_width_high1 = temp;
      temp = message_ptr->word1 & 0x00FF;
      psb_params.grid_width_high0 = temp;
      temp = (message_ptr->word0 & 0xFF00) >> 8;
      psb_params.afc_delay_high = temp;
      temp = message_ptr->word0 & 0x00FF;
      psb_params.magnetron_current_sample_delay_high = temp;
      psb_data.counter_config_received |=0b0010;
      break;
      
    case ETM_CAN_REGISTER_PULSE_SYNC_SET_1_LOW_ENERGY_TIMING_REG_0:
      temp = (message_ptr->word2 & 0xFF00) >> 8;
      psb_params.grid_delay_low3 = temp;
      temp = message_ptr->word2 & 0x00FF;
      psb_params.grid_delay_low2 = temp;
      temp = (message_ptr->word1 & 0xFF00) >> 8;
      psb_params.grid_delay_low1 = temp;
      temp = message_ptr->word1 & 0x00FF;
      psb_params.grid_delay_low0 = temp;
      temp = (message_ptr->word0 & 0xFF00) >> 8;
      psb_params.pfn_delay_low = temp;
      temp = message_ptr->word0 & 0x00FF;
      psb_params.dose_sample_delay_low = temp;
      psb_data.counter_config_received |= 0b0100;
      break;

    case ETM_CAN_REGISTER_PULSE_SYNC_SET_1_LOW_ENERGY_TIMING_REG_1:
      temp = (message_ptr->word2 & 0xFF00) >> 8;
      psb_params.grid_width_low3 = temp;
      temp = message_ptr->word2 & 0x00FF;
      psb_params.grid_width_low2 = temp;
      temp = (message_ptr->word1 & 0xFF00) >> 8;
      psb_params.grid_width_low1 = temp;
      temp = message_ptr->word1 & 0x00FF;
      psb_params.grid_width_low0 = temp;
      temp = (message_ptr->word0 & 0xFF00) >> 8;
      psb_params.afc_delay_low = temp;
      temp = message_ptr->word0 & 0x00FF;
      psb_params.magnetron_current_sample_delay_low = temp;
      psb_data.counter_config_received |= 0b1000;
      break;

      /*
    case ETM_CAN_REGISTER_PULSE_SYNC_SET_1_CUSTOMER_LED_OUTPUT:
      psb_data.led_state = message_ptr->word0;
      break;      
      */

    default:
      local_can_errors.invalid_index++;
      break;
    }
}


void ETMCanSlaveLogCustomPacketC(void) {
  /* 
     Use this to log Board specific data packet
     This will get executed once per update cycle (1.6 seconds) and will be spaced out in time from the other log data
  */

  ETMCanSlaveLogData(ETM_CAN_DATA_LOG_REGISTER_PULSE_SYNC_FAST_TRIGGER_DATA,
		     psb_data.pulses_on,
		     7,//(psb_data.trigger_input << 8) & psb_data.trigger_filtered,
		     8,//(psb_data.grid_width << 8) & psb_data.grid_delay,
		     9);  
}


void ETMCanSlaveLogCustomPacketD(void) {
  /* 
     Use this to log Board specific data packet
     This will get executed once per update cycle (1.6 seconds) and will be spaced out in time from the other log data
  */

  ETMCanSlaveLogData(ETM_CAN_DATA_LOG_REGISTER_PULSE_SYNC_SLOW_TIMING_DATA_0,
		     (psb_params.grid_delay_high3 << 8) & psb_params.grid_delay_high2,
		     (psb_params.grid_delay_high1 << 8) &  psb_params.grid_delay_high0,
		     (psb_params.pfn_delay_high << 8) & psb_params.dose_sample_delay_high,
		     (psb_params.grid_width_high3 << 8) & psb_params.grid_width_high2);
}


void ETMCanSlaveLogCustomPacketE(void) {
  /* 
     Use this to log Board specific data packet
     This will get executed once per update cycle (1.6 seconds) and will be spaced out in time from the other log data
  */

  ETMCanSlaveLogData(ETM_CAN_DATA_LOG_REGISTER_PULSE_SYNC_SLOW_TIMING_DATA_1,
		     (psb_params.grid_width_high1 << 8) & psb_params.grid_width_high0,
		     (psb_params.afc_delay_high << 8) & psb_params.magnetron_current_sample_delay_high,
		     (psb_params.grid_delay_low3 << 8) & psb_params.grid_delay_low2,
		     (psb_params.grid_delay_low1 << 8) & psb_params.grid_delay_low0);
}

void ETMCanSlaveLogCustomPacketF(void) {
  /* 
     Use this to log Board specific data packet
     This will get executed once per update cycle (1.6 seconds) and will be spaced out in time from the other log data
  */

  ETMCanSlaveLogData(ETM_CAN_DATA_LOG_REGISTER_PULSE_SYNC_SLOW_TIMING_DATA_2,
		     (psb_params.pfn_delay_low << 8) & psb_params.dose_sample_delay_low,
		     (psb_params.grid_width_low3 << 8) & psb_params.grid_width_low2,
		     (psb_params.grid_width_low1 << 8) & psb_params.grid_width_low0,
		     (psb_params.afc_delay_low << 8) & psb_params.magnetron_current_sample_delay_low);
}


  



