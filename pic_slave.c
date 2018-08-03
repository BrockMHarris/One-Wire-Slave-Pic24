#include <stdio.h>
#include <stdint.h>
#include "pic24_all.h"
// #include "lib/include/1-wire/Config.h"

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
byte serial_number[8] = {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

//HARD CODED SENSOR VALUES
const byte scratchpad[10] = {0x00, 0x79, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x07, 0x10, 0x70};
byte timer;

//FUNCTION PROTOTYPE
byte detect_reset(void);
void send_presence_pulse(void);
byte read_bit (void);
byte read_byte (void);
void pull_bus_low(void);
void release_bus(void);
void write_byte (byte write_data);
void write_bit(byte write_bitt);
byte match_search(byte write_bit);
byte match_bits (byte read_bit);

//CONFIGURE INPUT

void configTimer2(){
    T2CONbits.TON = 0;      //turns timer 2 off
    T2CONbits.TCKPS = 0b01; //sets the pre-scale to 1:8
    T2CONbits.TCS = 0;      //sets the clock source to the system clock
    PR2 = 0x0007;       //sets the t2IF to 1.00ms  7.498 intervals per 1000ns this should be 1 usec
    TMR2 = 0;               //starts the timer to 0
    _T2IF = 0;              //Clears the timer flag
    T2CONbits.TON = 0;      //Turns the timer on
}

void timeDelay(int length){
    int time = 0;
    TMR2 = 0;               //starts the timer to 0
    _T2IF = 0;              //Clears the timer flag
    T2CONbits.TON = 1;
    /*
     asm{
     
     mov _time, WREG
     mov _length, W1
      
     top_loop:
        cp w0, w1
        bra Z, end
        inc WREG, W0
        mov IFS0, w2
     inner_loop:
        and #0b0010000000, w2
        bra Z, inner_loop
        GOTO top_loop
     end:
     }
     
     */
    
    
    
    //wait(DELAY_70Us);
    while(time < length){
        while(!_T2IF);
        time++;
    }
    T2CONbits.TON = 0;
}

/*
 * configures the pull up for the pin connected to the bus and sets it up as an input
 */
void config_pb()  {
  CONFIG_RB13_AS_DIG_INPUT();
  ENABLE_RB13_PULLUP();
  // Give the pullup some time to take effect.
  //DELAY_US(1);
  timeDelay(1);
}


// Change notification interrupt configuration
// -------------------------------------------
// Enable change-notification interrupts on the one wire pins
void config_cn(void) {
  // Enable change notifications on RB13 specifically.
  ENABLE_RB13_CN_INTERRUPT();
  // Clear the interrupt flag.
  _CNIF = 0;
  // Choose a priority.
  _CNIP = 1;
  // Enable the Change Notification general interrupt.
  _CNIE = 1;

  CONFIG_RA0_AS_DIG_OUTPUT(); //configure LEDs
}

// Change notification
// -------------------


/*	Function:  _CNInterrupt
 *	--------------------------------------------
 *	This is a state machine with 3 states:
 *		WAIT_FOR_RESET,
 *  		ROM_CMD,
 *  		FUNCTION_CMD
 *	inputs are:
 *		detect_reset()
 *		read_byte()
 *		one_wire (pin RB 13)
 *	outputs are:
 *		write_byte()
 *		current_byte
 *		_CNIF
 *		??
 */
void _ISR _CNInterrupt(void) {
  _CNIF = 0;
  _CNIE = 0;
  byte buffer;
    switch (current_state) {
        case WAIT_FOR_RESET:
            //CHECK IF MASTEr sends a reset pulse
            if (detect_reset()) {
                //send a presence pulse and wait for ROM command
                send_presence_pulse();
                current_state = ROM_CMD;
            }
            break;
        case ROM_CMD:
            buffer = read_byte();
            if (0x31 <= buffer && buffer <= 0x35) { //read rom command
                ;  //NOOP to keep the compiler happy for some reason
                LED1 = 1;
                int i = 0;
                while (i < 8){
                    write_byte(serial_number[i]);
                    i++;
                }
                //basically write the serial number onto the bus
            } else if (0xEC <= buffer && buffer <= 0xF4){ //search rom command
                ;  //NOOP to keep the compiler happy for some reason
                //unfolded to increase speed
                int i_2 = 0;
                while (i_2 < 8){
                    byte current_byte = serial_number[i_2];
					//look at each of the bits individually and see if it matches the values currently on the bus
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
                current_state = WAIT_FOR_RESET;
                _CNIF = 0;
                return;
            } else if (0x53 <= buffer && buffer <= 0x57){ //match rom command
                ;  //NOOP to keep the compiler happy for some reason
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
 _CNIE = 1;
}

/*
 * this checks to see if there was a reset signal sent from the master.
 * the reset sequence is low for a long time the high again.
 */
byte detect_reset(void) {
    //DELAY_US(20);
    //timeDelay(20);
    //wait(DELAY_10Us);
    wait(DELAY_10Us);
    if (!one_wire){
        //DELAY_US(480);
        //timeDelay(480);
        wait(DELAY_240Us);
        wait(DELAY_240Us);
        if (one_wire){
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

/*
 *	This is a confirmation for reset from the slave. This tells the master that
 *  there is at least one slave on the bus
 */
void send_presence_pulse(void) {
    pull_bus_low();
    //DELAY_US(70);
    //timeDelay(70);
    wait(DELAY_70Us);
    release_bus();
}

/*
 *	This sets the bus low, and configure the pin to be an output so that this 
 *  device can write to the bus
 */
void pull_bus_low(void) {
    CONFIG_RB13_AS_DIG_OUTPUT();
    one_wire = 0;
}

/*
 * resets the pin on the pic to an input so that data can be read off the bus
 * resets the change notification interrupt to be ready the next time the master
 *      writes to the slave
 */
void release_bus(void) {
    config_pb();
    config_cn();
}

/*
 *  Stores the bus data then returns it if the bus goes high 20us or 45us later
 * 
 * how the master writes bits to the slave:
 *      Write 1: Drive bus low, delay 6 ?s.
 *               Release bus, delay 64 ?s.
 * 
 *      write 0: Drive bus low, delay 60 ?s.
 *               Release bus, delay 10 ?s.
 */
byte read_bit (void) {
    byte read_data;
    //DELAY_US(25);
    //timeDelay(25);
    wait(DELAY_10Us);
    wait(DELAY_6Us);
    read_data = one_wire;
    //DELAY_US(20);
    //timeDelay(20);
    wait(DELAY_10Us);
    wait(DELAY_10Us);
    if (one_wire){
        return read_data;
    }
    //DELAY_US(25);
    //timeDelay(25);
    wait(DELAY_10Us);
    wait(DELAY_6Us);
    if (one_wire) {
        return read_data;
    }
}

/* 
 * puts a 1 into each of the bits of the byte based on whether readbit() is high or low
 * returns the resulting byte
 * 
 *  if the bit is a 1 then results is save to 10000000 then shifts results by 1 until the bus is high (due to pullup)
 */
byte read_byte (void) {

	// I unfold this and some other loops to meet very strict time limits
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
	return result;
}

/*
 * if the bit to write is a 1 then the bus pin is set to be an output and
 *      the bus remains high for 60(time units)
 * if the bit is low then this pulls the bus low for 60(time units) and then releases
 *      the bus setting it back to an input
 * 
 */
void write_bit(byte write_bitt) {
    if (write_bitt) {
        //DELAY_US(60);
        //timeDelay(60);
        wait(DELAY_60Us);
    } else {
        pull_bus_low();
        //DELAY_US(60);
        //timeDelay(60);
        wait(DELAY_60Us);
        release_bus();
    }
}

/*
 * 	This compares bit read in through the bus to a byte input. If they are they 
 *      same returns true
 */
byte match_bits (byte read_bitt) {
	byte result=0;
	if (read_bit() == read_bitt)		// read bit on 1-wire line and compare with current one
		result = 1;						// match
	return result;
}

byte match_search (byte write_bitt) {
    byte res = 0;
    write_bit(write_bitt);
    while(one_wire);
    write_bit(write_bitt ^ 0x01);
    while(one_wire);
    if (read_bit() == write_bitt) {
        res = 1;
    } else {
        res = 0;
    }
    return res;
}

/*
 * writes the bit value least significant bit to most significant bit.
 * 
 * if i want to write 10110110 this is first anded with 00000001 so that only
 *      the last bit is sent to the write_bit() function
 * we then shift the value down by one to be 01011011 and do it again until all
 *      the bits have been written
 */
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
  configTimer2(); //sets up timer 2 to be used for pwm on the motor
  config_pb();
  config_cn();
  LED1 = 0;
  while (1) {
    // Enter a low-power state, which still keeps timer3 and uart1 running.
    // Enter code to do stuff here, communications are handled via interrupts.
    IDLE();
  }
}
