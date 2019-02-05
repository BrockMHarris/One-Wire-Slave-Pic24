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
byte serial_number[8] = {0x28, 0xD5, 0xCC, 0x8C, 0x09, 0x00, 0x01}; // 0xF4

//HARD CODED SENSOR VALUES
//byte scratchpad[10] = {0x00, 0x7e, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x02, 0x10}; //0x25
byte scratchpad[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

byte memory[4];
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
byte CalcCRC(byte code_len, byte *code);
void C_CRC(byte *CRCVal, byte value);

/* semi accurate timing for 40Mhz clock.
 * equivalent to 1.6us for 1 loop, 10.6us for 10 loops, 100.6us for 100 loops
 * Brock Harris: 7/31/18
 */
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
					"dec _macro_delay\n"\
					"bra NZ, loop\n");
}

/*
 * configures the pull up for the pin connected to the bus and sets it up as an input
 */
void config_pb()  {
  CONFIG_RB13_AS_DIG_INPUT();
  //ENABLE_RB13_PULLUP();
  //_ODCB13 = 1;
  ENABLE_RB13_OPENDRAIN(); //disables the internal pullup resistor because we
                           //are using an external pullup resistor connected to
                           //the the bus
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

/* CRC calculator. Maxim uses a Dow CRC to check that the data was transfered
 * appropriately. The CRC is appended to the end of the Serial ID and the temperature
 * that is sent on the bus.
 * Brock Harris: 7/31/18
 */
static byte crc88540_table[256] = {
    0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
  157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
   35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
  190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
   70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
  219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
  101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
  248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
  140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
   17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
  175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
   50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
  202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
   87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
  233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
  116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

/* Call this function and provide an array to get the Dow CRC of that array.
 * Brock Harris: 7/31/18
 */
byte get_crc(byte *data, byte count)
{
    byte result=0; 

    while(count--) {
      result = crc88540_table[result ^ *data++];
    }
    
    return result;
}

// Change notification
// -------------------
/*	Function:  _CNInterrupt
 *	--------------------------------------------
 *	This is a state machine with 3 states:
 *		WAIT_FOR_RESET,
 *  	ROM_CMD,
 *  	FUNCTION_CMD
 *	inputs are:
 *		detect_reset()
 *		read_byte()
 *		one_wire (pin RB 13)
 */
void _ISR _CNInterrupt(void) {
    //LED1 = 1;
  _CNIF = 0;
  _CNIE = 0;
  byte buffer;
    switch (current_state) {
        /* Stays in this state until a valid reset is detected by the slave. Then
         * sends a presence pulse. all the devices on the bus send a presence
         * pulse at the same time. o the maset 
         */
        case WAIT_FOR_RESET:
            //CHECK IF MASTEr sends a reset pulse
            if (detect_reset()) {
                //send a presence pulse and wait for ROM command
                send_presence_pulse();
                can_read = 1; //Function commands are valid
                current_state = ROM_CMD;
                //wait(20);
            }
            break;
        case ROM_CMD:
            while(one_wire); //wait for a falling edge to begin reading. This is a hack
                            //when multiple pic on the same bus one pic would trigger
                            //right after presense was over 
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
			/* When the master is first identifying which devices are connected
             * to the bus it will send out an 0xF0 command. Every slave on the bus
             * will then respond by sending the first bit of there serial number
             * (notice that is the last bit of the first byte because of LSB to MSB)
             * at the same time they will then send the inverse of that bit to the bus
             * The master will then respond with either a 1 or a 0 and every device
             * whos bit did not match the master's bit shuts up and waits for the
             * next cycle. The devices that still remain send their next bit and so
             * on until they send their full serial number with the CRC. The master
             * can now identify them.
             */
            } else if (0xEC <= buffer && buffer <= 0xF4){ //search rom command
                ; //NOOP to keep the compiler happy for some reason
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
                current_state = WAIT_FOR_RESET; //Goes back to reset because other
                                                //devices need to be identified
                _CNIF = 0;
                return;
			/* This command is used to identify device right before a rom command
             * is sent. its saying i want to talk to you -> slave Number 69
             */
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
			/* This command is sent when there is only one slave on the bus so
             * the master does not need to identify it through its serial number
             */
            } else if (0xCA <= buffer && buffer <= 0xCE){ //skip rom command
                buffer = 0;
                current_state = FUNCTION_CMD;
            }
        break;
	/* This is where the magic happens. This state is used for sending and receiving
     * data between one particular slave and the master.
     */
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
            //LED1 = ~LED1;
            //scratchpad[9] = LED1;
        }
        
        /* Checks for the 4E command which is sent to the slave when the master
         * is trying to write to it. in the case of the temperature sensor
         * the master con only write a 9-12 to the slave and this is represented
         * by memory[2] as either a 1f, 3F, 5F, or a 7F 
         */
        else if (buffer == 0x4E){
            /* can_read is high when neither the write or read from the slave's
             *  scratch pad has occured. This is reset when a reset pulse is detected
             */
            LED1 = ~LED1;
            if(can_read == 1){ 
                memory[0] = read_byte();
                memory[1] = read_byte();
                memory[2] = read_byte();
                memory[3] = read_byte();
                LED1 = ~LED1;
                scratchpad[5] = memory[0];
                scratchpad[6] = memory[1];
                scratchpad[7] = memory[2];
                scratchpad[8] = memory[3];
                scratchpad[9] = get_crc(scratchpad, 9);
                can_read = 0;
            }
        }
        buffer = 0;
        current_state = WAIT_FOR_RESET;
}
    //LED1 =0;
 _CNIE = 1;
}

/*
 * this checks to see if there was a reset signal sent from the master.
 * the reset sequence is low for a long time the high again.
 */
byte detect_reset(void) {
    //DELAY_US(20);
    int i;
    for(i = 0; i<53; i++){
        wait(9);
        if(one_wire){return 0;}
    }
    wait(7);
    wait(29);
    if(!one_wire){return 0;}
    //wait(30);
    //LED1 = ~LED1;
    return 1;
}
/*
 *	This is a confirmation for reset from the slave. This tells the master that
 *  there is at least one slave on the bus
 */
void send_presence_pulse(void) {
    //LED1=1;
    pull_bus_low();
    //DELAY_US(130);
    wait(111);
    release_bus();
    //LED1=0;
}

/*
 *	This sets the bus low, and configure the pin to be an output so that this 
 *  device can write to the bus
 */
void pull_bus_low(void) {
    CONFIG_RB13_AS_DIG_OUTPUT();
    ENABLE_RB13_OPENDRAIN();
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

/* 
 * puts a 1 into each of the bits of the byte based on whether readbit() is high or low
 * returns the resulting byte
 * 
 *  if the bit is a 1 then results is save to 10000000 then shifts results by 1 until the bus is high (due to pullup)
 */
byte read_byte (void) {
	// I unfold this and some other loops to meet very strict time limits
    //LED1= 1;
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
    //LED1 =0;
	return result;
}

/*
 * if the bit to write is a 1 then the bus pin is set to be an output and
 *      the bus remains high for 60(time units)
 * if the bit is low then this pulls the bus low for 60(time units) and then releases
 *      the bus setting it back to an input
 * The master initiates all contact with the slave. When the master is looking for
 * a bit from the slave it will pull the bus low for around 6us and then release it
 * the slave will then respond by keeping the bus low or by letting it pull up.
 */
void write_bit(byte write_bitt) {
    //LED1 = 1;
    if (write_bitt) {
        //DELAY_US(60);
        wait(67);
    } else {
        pull_bus_low();
        //DELAY_US(60);
        wait(28);
        release_bus();
    }
    //LED1 = 0;
}

/*
 * 	This compares bit read in through the bus to a byte input. If they are they 
 *      same returns true
 */
byte match_bits (byte read_bitt) {
    //LED1 = 1;
	byte result=0;
	if (read_bit() == read_bitt)		// read bit on 1-wire line and compare with current one
		result = 1;	// match
    //LED1 =0;
	return result;
}

/* sends the bit of data and then sends the inverse. It then checks what the master
 * sent back. if the master sends a bit that matches what the slave sent then this
 * function returns a 1 otherwise it returns a 0;'
 */
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
    //LED1 = 1;
    wait(2);
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
        //LED1 = 0;
}
// Main
// ====
int main(void) {
    configBasic(HELLO_MSG);
    serial_number[7] = get_crc(serial_number, 7);
    scratchpad[9] = get_crc(scratchpad, 9);
    //configClockFRCPLL_FCY40MHz();
    config_pb();
    config_cn();
    LED1 = 0;
    while (1) {
        //DELAY_US(1000);
        if(scratchpad[7] == 0x20){
            LED1=1;
        }
        else{
            LED1=0;
        }
//        LED1=~LED1;
        // Enter a low-power state, which still keeps timer3 and uart1 running.
        // Enter code to do stuff here, communications are handled via interrupts.
        //IDLE();
    }
}
