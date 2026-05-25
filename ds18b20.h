#define ONE_WIRE_PIN _RB2
#define ONE_WIRE_DIR _TRISB2

void output_low();
void output_float();
void output_bit(int val);
int input();
void onewire_reset();
void onewire_write(int data);
short int onewire_read();
float ds1820_read();
