#include "p33Fxxxx.h"
#include "ds18b20.h"

/******************* 1-wire communication functions ********************/
void output_low() {	ONE_WIRE_PIN = 0;	ONE_WIRE_DIR = 0;}

void output_float() {	ONE_WIRE_DIR = 1;}

void output_bit(int val) {	ONE_WIRE_DIR = 0;	ONE_WIRE_PIN = val;}

int input() { 	return(ONE_WIRE_PIN);}

/************onewire_reset*************************************************/
/*This function initiates the 1wire bus */
/*********************************************************************/
void onewire_reset()  // OK if just using a single permanently connected device
{
 output_float(); // float 1-wire high
 output_low();
 delayus( 550 ); // pull 1-wire low for reset pulse
 output_float(); // float 1-wire high
 delayus( 70 );	 // wait 70us and checks for slave's response
 // no check here...
 delayus( 500 ); // wait-out remaining initialisation window.
 output_float();
}

/*********************** onewire_write() ********************************/
/*This function writes a byte to the sensor.*/
/*********************************************************************/
void onewire_write(int data)
{
 int count;
 for (count=0; count<8; ++count)
 {
  output_low();
  delayus( 2 ); // pull 1-wire low to initiate write time-slot.

  output_bit(data&0x01);	// sends one bit (the lowest one)
  data = data >> 1;		// shifts right 1 bit

  delayus( 60 ); // wait until end of write slot.
  output_float(); // set 1-wire high again,
  delayus( 2 ); // for more than 1us minimum.
 }
}

/*********************** read1wire() *********************************/
/*This function reads the 8 -bit data via the 1-wire sensor. */
/*********************************************************************/
short int onewire_read()
{
 short int count, data;

 data = 0;
 for (count=0; count<8; ++count)
 {
  output_low();
  delayus( 2 ); // pull 1-wire low to initiate read time-slot.
  output_float(); // now let 1-wire float high,
  delayus( 8 ); // let device state stabilise,
 
  data = data >> 1;
  data = data | (input()<<7);

  delayus( 120 ); // wait until end of read slot.
 }
 return( data );
} 

float ds1820_read()
{
 short int busy=0, temp_LSB, temp_MSB;
 short int byte2, byte3, byte4, byte5, byte6, byte7;
 int cod_temp;
 float temp, temp_prec;

 onewire_reset();
 onewire_write(0xCC);
 onewire_write(0x44);

 while (busy == 0)
  busy = onewire_read();

 onewire_reset();
 onewire_write(0xCC);
 onewire_write(0xBE);
 temp_LSB = onewire_read();
 temp_MSB = onewire_read();
 byte2 = onewire_read();
 byte3 = onewire_read();
 byte4 = onewire_read();
 byte5 = onewire_read();
 byte6 = onewire_read();
 byte7 = onewire_read();

 cod_temp = (temp_MSB<<8) + temp_LSB;
 //temp = (float) cod_temp / 2.0;   //Calculation for DS18S20 with 0.5 deg C resolution
 temp = (float) cod_temp / 16.0;   //Calculation for DS18B20 with 0.125 deg C resolution
 temp_prec = temp - 0.25 + (byte7 - byte6) / byte7;
 //return(temp_prec);
 return(temp);
} 
