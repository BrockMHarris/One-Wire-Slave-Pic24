#include <stdio.h>
#include <stdint.h>
#include "pic24_all.h"

// #include "lib/include/1-wire/Config.h"
unsigned char macro_delay;

#define LED1 (_LATA0)
#define one_wire (_RB13) //PIN RB 13
#define byte unsigned char

typedef enum  {
  WAIT_FOR_RESET,
  ROM_CMD,
  FUNCTION_CMD,
} state_t; //Different states of command

volatile state_t current_state = WAIT_FOR_RESET; //DEFAULT TO WAIT FOR RESET
volatile byte can_read = 1;

//HARD CODED SERIAL NUMBER
//byte serial_number[8] = {0x28,0x8d,0xaa,0xaa,0x08,0x00,0x00, 0xa7};
byte serial_number[8] = {0x28, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29};

//HARD CODED SENSOR VALUES
const byte scratchpad[10] = {0x00, 0x7e, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x02, 0x10, 0x25};

//FUNCTION PROTOTYPE
byte detect_reset(void);
byte detect_presence(void);
void send_presence_pulse(void);
byte read_bit (void);
byte read_byte (void);
void pull_bus_low(void);
void release_bus(void);
void write_byte (byte write_data);
void write_bit(byte write_bitt);
byte match_search(byte write_bit);
byte match_bits (byte read_bit);


void wait(short time){
         __asm__ volatile (\
                    "mov w0, _macro_delay\n"\
			  "loop: nop\n"\
					"nop\n"\
					"nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
					"nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
					"nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
                    "nop\n"\
					"dec _macro_delay\n"\
					"bra NZ, loop\n");
}

//CONFIGURE INPUT
void config_pb()  {
  CONFIG_RB13_AS_DIG_INPUT();
  //ENABLE_RB13_PULLUP();
  //_ODCB13 = 1;
  ENABLE_RB13_OPENDRAIN();
  // Give the pullup some time to take effect.
  //DELAY_US(1);
  wait(1);
}


// Change notification interrupt configuration
// -------------------------------------------
// Enable change-notification interrupts on the one wire pins
void config_cn(void) {
  // Enable change notifications on RB13 specifically.
  ENABLE_RB13_CN_INTERRUPT();
  // Clear the interrupt flag.
  CNPU1bits.CN13PUE = 0;
  _CNIF = 0;
  // Choose a priority.
  _CNIP = 1;
  // Enable the Change Notification general interrupt.
  _CNIE = 1;
  
  CONFIG_RA0_AS_DIG_OUTPUT(); //configure LEDs
}

// Change notification
// -------------------
void _ISR _CNInterrupt(void) {
    //LED1 = 1;
  _CNIF = 0;
  _CNIE = 0;
  byte buffer;
    switch (current_state) {
        case WAIT_FOR_RESET:
            //CHECK IF MASTEr sends a reset pulse
            if (detect_reset()) {
                //send a presence pulse and wait for ROM command
                //wait(30);
                send_presence_pulse();
                current_state = ROM_CMD;
                //wait(20);
            }
            break;
        case ROM_CMD:
            while(one_wire);
            buffer = read_byte();
            if (0x31 <= buffer && buffer <= 0x35) { //read rom command
                ;
                //LED1 = 1;
                int i = 0;
                while (i < 8){
                    write_byte(serial_number[i]);
                    i++;
                }
                //basically write the serial number onto the bus
            } else if (0xEC <= buffer && buffer <= 0xF4){ //search rom command
                ;
                //LED1 = 1;
                //unfolded to increase speed
                int i_2 = 0;
                while (i_2 < 8){
                    byte current_byte = serial_number[i_2];

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){ 	// sending LS-bit first
                        break;
                    }
                    current_byte >>= 1;							// shift the data byte for the next bit to send

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }
                    current_byte >>= 1;

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }
                    current_byte >>= 1;

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }
                    current_byte >>= 1;

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }
                    current_byte >>= 1;

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }
                    current_byte >>= 1;

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }
                    current_byte >>= 1;

                    while(one_wire);
                    if(!match_search(current_byte & 0x01)){
                        break;
                    }

                    i_2++;
                }
                buffer = 0;
                //LED1 = 0;
                current_state = WAIT_FOR_RESET;
                _CNIF = 0;
                return;
            } else if (0x53 <= buffer && buffer <= 0x57){ //match rom command
                ;
                int i_3 = 0;
                while (i_3 < 8) {
                    byte current_byte = serial_number[i_3];
                    int j = 0;
                    while (j < 8) {
                        while (one_wire);
                        if (!match_bits(current_byte & 0x01)) {
                            buffer = 0;
                            current_state = WAIT_FOR_RESET;
                            _CNIE = 1;
                            return;
                        }
                        current_byte >>=1 ;
                        j++;
                    }
                    i_3++;
                }
                buffer = 0;
                current_state = FUNCTION_CMD;
            } else if (0xCA <= buffer && buffer <= 0xCE){ //skip rom command
                buffer = 0;
                current_state = FUNCTION_CMD;
        }
        break;
    case FUNCTION_CMD:
        buffer = read_byte();
        if (0x42 <= buffer && buffer <= 0x46){
            //BASICALLY TELLS THE sensor to sense things and save to scratchpad
            can_read = 1;
        } else if (0xBA <= buffer && buffer <= 0xC3) {
            //write scratchpad (sensor value) onto the bus
            if (can_read == 1){
                int i_4 = 1;
                while (i_4 < 10) {
                    while (one_wire);
                    write_byte(scratchpad[i_4]);
                    i_4++;
                }
                can_read = 0;
            }
        }
        buffer = 0;
        current_state = WAIT_FOR_RESET;
}
    //LED1 =0;
 _CNIE = 1;
}

byte detect_reset(void) {
    //DELAY_US(20);
    int i;
    for(i = 0; i<50; i++){
        wait(9);
        if(one_wire){return 0;}
    }
    wait(35);
    if(!one_wire){return 0;}
    //LED1 = ~LED1;
    wait(30);
    //LED1 = ~LED1;
    return 1;
}

void send_presence_pulse(void) {
    //LED1=1;
    pull_bus_low();
    //DELAY_US(130);
    wait(130);
    release_bus();
    //LED1=0;
}

void pull_bus_low(void) {
    CONFIG_RB13_AS_DIG_OUTPUT();
    ENABLE_RB13_OPENDRAIN();
    one_wire = 0;
}

void release_bus(void) {
    config_pb();
    config_cn();
}
byte read_bit (void) {
    //LED1 = 1;
    byte read_data;
    //DELAY_US(25);
    wait(25);
    read_data = one_wire;
    //DELAY_US(20);
    wait(20);
    //LED1 = 0;
    if (one_wire){
        return read_data;
    }
    //DELAY_US(25);
    wait(25);
    if (one_wire) {
        return read_data;
    }
    //return read_data;
}

byte read_byte (void) {
	// I unfold this and some other loops to meet very strict time limits
    LED1= 1;
	byte result=0;
	if (read_bit())
		result |= 0x80;				// if result is one, then set MS-bit
	while(one_wire);
	result >>= 1; 				    // shift the result to get it ready for the next bit to receive
	if (read_bit())
		result |= 0x80;
	while(one_wire);
	result >>= 1;
	if (read_bit())
		result |= 0x80;
	while(one_wire);
	result >>= 1;
	if (read_bit())
		result |= 0x80;
	while(one_wire);
	result >>= 1;
	if (read_bit())
		result |= 0x80;
	while(one_wire);
	result >>= 1;
	if (read_bit())
		result |= 0x80;
	while(one_wire);
	result >>= 1;
	if (read_bit())
		result |= 0x80;
	while(one_wire);
	result >>= 1;
	if (read_bit())
		result |= 0x80;
    LED1 =0;
	return result;
}


void write_bit(byte write_bitt) {
    //LED1 = 1;
    if (write_bitt) {
        //DELAY_US(60);
        wait(75);
    } else {
        pull_bus_low();
        //DELAY_US(60);
        wait(60);
        release_bus();
    }
    //LED1 = 0;
}

byte match_bits (byte read_bitt) {
    //LED1 = 1;
	byte result=0;
	if (read_bit() == read_bitt)		// read bit on 1-wire line and compare with current one
		result = 1;	// match
    //LED1 =0;
	return result;
}

byte match_search (byte write_bitt) {
    //LED1 = 1;
    byte res = 0;
    write_bit(write_bitt);
    while(one_wire);
    write_bit(write_bitt ^ 0x01);
    while(one_wire);
    return match_bits(write_bitt);
//    if (read_bit() == write_bitt) {
//        res = 1;
//    } else {
//        res = 0;
//    }
    //LED1 =0;
    //return res;
}

void write_byte (byte write_data)
{
		write_bit(write_data & 0x01); 	// sending LS-bit first
		while(one_wire);							// wait for master set bus low
		write_data >>= 1;					// shift the data byte for the next bit to send
		write_bit(write_data & 0x01);
		while(one_wire);
		write_data >>= 1;
		write_bit(write_data & 0x01);
		while(one_wire);
		write_data >>= 1;
		write_bit(write_data & 0x01);
		while(one_wire);
		write_data >>= 1;
		write_bit(write_data & 0x01);
		while(one_wire);
		write_data >>= 1;
		write_bit(write_data & 0x01);
		while(one_wire);
		write_data >>= 1;
		write_bit(write_data & 0x01);
		while(one_wire);
		write_data >>= 1;
		write_bit(write_data & 0x01);
}
// Main
// ====
int main(void) {
    configBasic(HELLO_MSG);
    //configClockFRCPLL_FCY40MHz();
    config_pb();
    config_cn();
    LED1 = 0;
    while (1) {
        //DELAY_US(1000);
        wait(1000);
//        LED1=~LED1;
        // Enter a low-power state, which still keeps timer3 and uart1 running.
        // Enter code to do stuff here, communications are handled via interrupts.
        //IDLE();
    }
}
