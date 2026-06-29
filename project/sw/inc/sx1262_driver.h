#ifndef SX1262_DRIVER_H
#define SX1262_DRIVER_H

// opcodes
#define SET_SLEEP_OPCODE 0x84
#define SET_STANDBY_OPCODE 0x80
#define SET_TX_OPCODE 0x83
#define SET_RX_OPCODE 0x82
#define SET_PACKET_TYPE_OPCODE 0x8A
#define GET_PACKET_TYPE_OPCODE 0x11
#define SET_DIO2_AS_RF_SW_OPCODE 0x9D
#define SET_FREQUENCY_OPCODE 0x86
#define STOP_TIMER_ON_HEADER_OPCODE 0x9F
#define SET_MODULATION_PARAMS_OPCODE 0x8B
#define SET_PA_CONFIG_OPCODE 0x95
#define SET_TX_PARAMS_OPCODE 0x8E
#define SET_LORA_SYMB_NUM_TIMEOUT_OPCODE 0xA0
#define SET_DIO_IRQ_PARAMS_OPCODE 0x08
#define SET_PACKET_PARAMS_OPCODE 0x8C
#define WRITE_BUFFER_OPCODE 0x0E
#define GET_STATUS_OPCODE 0xC0
#define GET_DEVICE_ERRORS_OPCODE 0x17
#define CLEAR_DEVICE_ERRORS_OPCODE 0x07
#define SET_DIO3_AS_TCXO_CTRL_OPCODE 0x97
 

// TCXO voltage setting (see datasheet for details on how to set this)
// It can handle between 1.7-3.3V. We will set to 3.0V
#define TCXO_VOLTAGE_SETTING 0x01 // DIO3 outputs 1.8V to supply the TCXO
// #define TCXO_SETTLE_TIME_STEPS 320 // delay is set to 5ms which is 320 steps of 15.625 us. Formula: Delay duration = Delay(23:0) *15.625µs
#define TCXO_SETTLE_TIME_STEPS 640 // delay is set to 5ms which is 320 steps of 15.625 us. Formula: Delay duration = Delay(23:0) *15.625µs

// modes
#define STANDBY_RC_MODE 0x00
#define SLEEP_RC_MODE 0x01
#define TX_MODE 0x02
#define RX_MODE 0x03


// PLL steps lookup table for different frequenies 
// (see SX1262 datasheet for details on how to calculate PLL steps for different frequencies)
#define FREQ_868MHz_PLL_STEPS 0x36400000
#define FREQ_434MHz_PLL_STEPS 0x1B200000

// LoRa modulation params
#define LORA_SF 0x09 // Spreading Factor
#define LORA_BW 0x04 // LoRa_BW_125 (125kHz real)
#define LORA_CR 0x03 // Coding Rate 4/7
#define LORA_LDRO 0x00 // Low Data Rate Optimization (disabled for SF7 and BW 125 kHz)

// LoRa PA config params
// PA duty cycle and hp max power are set based on recommended settings in the SX1262 datasheet
// See Table 13-21: PA Operating Modes with Optimal Settings
// #define PA_DC 0x04 // PA duty cycle (0x00-0x04)
#define PA_DC 0x02 // PA duty cycle (0x00-0x04)
// #define PA_HPM 0x02 // PA max power (0x00-0x07).
#define PA_HPM 0x02 // PA max power (0x00-0x07).
#define DEVICE_SEL 0x00 // 0x00 for SX1262, 0x01 for SX1261
#define PA_LUT 0x01 // always set to 0x01

// LoRa TX params
// See datasheet 13.4.4 for details
#define TX_POWER 10 //Power.  Can be -17(0xEF) to +14x0E in Low Pow mode.  -9(0xF7) to 22(0x16) in high power mode
#define RAMP_TIME_US 0x02 // Ramp time.  Can be 0x00 (10 us) to 0x07 (3.4 ms)


// IRQ params

// IRQ MASK TABLE
// bit      ||      event
// ===============================
// 0        ||      TxDone
// 1        ||      RxDone
// 2        ||      PreambleDetected
// 3        ||      SyncWordValid
// 4        ||      HeaderValid
// 5        ||      HeaderError
// 6        ||      CRCError
// 7        ||      CADDone
// 8        ||      CADDetected
// 9        ||      Timeout
// 10-15    ||      reserved / not used for LoRa

#define IRQ_MASK 0x0002 // set to 0x02 to enable RxDone interrupt only
#define DIO1_MASK 0xFFFF // map all IRQs enabled by IRQ_MASK to DIO1 (Because we only set IRQ_MASK to enable RxDone, this means we are mapping the RxDone interrupt to DIO1)
// Rest of DIOS is n/a and will just be masked to zero

// Packet parameters
#define PREAMBLE_LENGTH 0x0008  // PacketParam 1-2 = preamble length
#define HEADER_TYPE 0x00        //PacketParam3 = Header Type. 0x00 = Variable Len, 0x01 = Fixed Length (implicit)
#define PAYLOAD_LENGTH 0x04     //PacketParam4 = Payload Length (Max is 255 bytes). DEBUG: right now we only send 4 bytes of payload
#define CRC_TYPE 0x01           //PacketParam5 = CRC Type. 0x00 = Off, 0x01 = on
#define INVERT_IQ 0x00          //PacketParam6 = Invert IQ.  0x00 = Standard, 0x01 = Inverted



void sx1262_wait_while_busy();
unsigned int sx1262_sanity_check();
void sx1262_set_power_mode(unsigned int mode);
void sx1262_set_packet_to_lora();
unsigned int sx1262_get_packet();
void sx1262_set_DIO2_as_RFSW();
void sx1262_set_frequency(unsigned int pll_steps);
void sx1262_set_modulation_params();
void sx1262_set_pa_config();
void sx1262_set_tx_params();
void sx1262_set_dio_irq_params();
void sx1262_transmit(unsigned int payload);
void sx1262_configure_essentials();
void sx1262_get_status(unsigned int *chip_mode, unsigned int *command_status);
void sx1262_get_device_error(unsigned int *status, unsigned int *op_error);
void sx1262_clear_device_errors();
void sx1262_set_dio3_as_tcxo();
void sx1262_set_sync_word_private();
void sx1262_set_packet_params();
unsigned int sx1262_receive_async();
unsigned int sx1262_wait_command_completion();




#endif // SX1262_DRIVER_H