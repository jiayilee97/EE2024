/*****************************************************************************
 *   GPIO Speaker & Tone Example
 *
 *   CK Tham, EE2024
 *
 ******************************************************************************/

#include "stdio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "rgb.h"
#include "oled.h"
#include "temp.h"
#include "light.h"
#include "led7seg.h"
#include "string.h"

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);
#define OBSTACLE_NEAR_THRESHOLD 3000
#define TEMP_HIGH_THRESHOLD 40 // rmb to change back to 28
#define ACC_THRESHOLD 0.4

volatile uint32_t msTicks;
volatile uint32_t sw3;
volatile uint32_t sw4;
volatile uint32_t currentTick3;

void SysTick_Handler(void){
	msTicks++;
}

uint32_t getMsTicks()
{
	return msTicks;
}

void systick_delay (uint32_t delayTicks) {
  uint32_t currentTicks;

  currentTicks = msTicks;	// read current tick counter
  // Now loop until required number of ticks passes
  while ((msTicks - currentTicks) < delayTicks);
}

// Interval in us
static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

static void playNote(uint32_t note, uint32_t durationMs)
{
    uint32_t t = 0;

    if (note > 0) {
        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note/2); // us timer

            NOTE_PIN_LOW();
            Timer0_us_Wait(note/2);

            t += note;
        }
    }
    else {
    	Timer0_Wait(durationMs); // ms timer
    }
}

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;
    /* number of ms */
    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
    switch (ch) {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}

static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        Timer0_Wait(pause);
    }
}

static uint8_t * song = (uint8_t*)"C1.C1,D2,C2,F2,E4,";

static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_GPIO(void)
{
	// Initialize button SW4 (not really necessary since default configuration)
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1<<31, 0);

	//Initialize RGB
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, 1<<1, 1);

	//Initialize SW3
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 4;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(0, 1<<4, 0);

	//Initialize Temperature Sensor
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(0, 1<<2, 0);

    /* ---- Speaker ------> */
//    GPIO_SetDir(2, 1<<0, 1);
//    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);

    // Main tone signal : P0.26
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
    /* <---- Speaker ------ */
}

void ready_uart(void)
{
	// PINSEL Configuration
	PINSEL_CFG_Type CPin;
	    CPin.OpenDrain = 0;
	    CPin.Pinmode = 0;
	    CPin.Funcnum = 2;
	    CPin.Pinnum = 0;
	    CPin.Portnum = 0;
	PINSEL_ConfigPin(&CPin);
	    CPin.Pinnum = 1;
	    CPin.Portnum = 0;
	PINSEL_ConfigPin(&CPin);

	// Initialise and enable the UART. Not enabling the UART will lead to a hard fault
	UART_CFG_Type UCfg;
	    UCfg.Baud_rate = 115200;
	    UCfg.Databits = UART_DATABIT_8;
	    UCfg.Parity = UART_PARITY_NONE;
	    UCfg.Stopbits = UART_STOPBIT_1;

	// supply power & setup working parameters for UART3
	UART_Init(LPC_UART3, &UCfg);

	// enable transmit for uart3
	UART_TxCmd(LPC_UART3, ENABLE);

	// FIFO configuration- For system enhancements only
	//
}

void EINT3_IRQHandler(void){
	//P0.4 Button SW3
	if ((LPC_GPIOINT->IO0IntStatF >> 4) & 0x01){
		if(sw3==1){
		sw3=3;

		}
		else if(sw3==2){
			if((msTicks-currentTick3)<1000){
				sw3=4;//proceed to return mode
			}
			else if (((msTicks-currentTick3)>=1000) || (currentTick3==0)){
				currentTick3=msTicks;
			}
		}
		else if(sw3==4){
			sw3=1;
		}
		LPC_GPIOINT->IO0IntClr = (1 << 4);
	}
	//UART

	if((LPC_GPIOINT->IO2IntStatF >> 10)& 0x1){
		uint8_t data =0;
		uint32_t len =0;
		uint8_t line[64];

		char MsgPrint[20]="Hello world EE2024\n";
		UART_Send(LPC_UART3,(uint8_t *)MsgPrint,strlen(MsgPrint),BLOCKING);
		UART_Receive(LPC_UART3,&data,1,BLOCKING);
		UART_Send(LPC_UART3,&data,1,BLOCKING);
		len=0;
		do
		{
			UART_Receive(LPC_UART3,&data,1,BLOCKING);
			if(data!='\r'){
				len++;
				line[len-1]=data;
			}
		}while ((len<64) &&(data!='\r'));
		line[len]=0;
		UART_SendString(LPC_UART3,&line);
		LPC_GPIOINT->IO0IntClr=(1<<10);
	}
}


void stationary_mode(){

}

void launch_mode(){

}

void return_mode(){

}

int main (void) {

	int32_t xoff = 0;
	int32_t yoff = 0;
	int32_t zoff = 0;
	int8_t x = 0;
	int8_t y = 0;
	int8_t z = 0;
    uint8_t btn3 = 1;
    uint8_t btn4 = 1;


    init_GPIO();
    init_i2c();
    init_ssp();

    ready_uart();

    led7seg_init();
    pca9532_init();
    joystick_init();
    oled_init();
    light_init();
    acc_init();
    rgb_init();
    temp_init(&getMsTicks);

    acc_read(&x, &y, &z);
	xoff = 0-x;
	yoff = 0-y;
	zoff = 64-z;

    // Enable GPIO Interrupt P2.10, UART
    LPC_GPIOINT->IO2IntEnF |= 1<<10;
    // Enable GPIO Interrupt P0.4 , SW3
    LPC_GPIOINT->IO0IntEnF |= 1<<4;
    // Clear EINT3
	NVIC_ClearPendingIRQ(EINT3_IRQn);
    // Enable EINT3 interrupt
    NVIC_EnableIRQ(EINT3_IRQn);

	light_enable();
	light_setRange(LIGHT_RANGE_4000);

	int32_t light_val;
	int32_t i;
	char LightPrint[20];
	char oledPrint[20];
	char mode="s";
	int32_t countdown[16]={'F','E','D','C','B','A','9','8','7','6','5','4','3','2','1','0'};
	int32_t lightnumber;
	int32_t lightarray[16]={0x1,0x3,0x7,0xF,0x1F,0x3F,0x7F,0xFF,0x1FF,0x3FF,0x7FF,0xFFF,0x1FFF,0x3FFF,0x7FFF,0xFFFF};
	int32_t currentTick2;

	if(SysTick_Config(SystemCoreClock/1000)){
					while(1);//capture error
	}

	sw3=1;
	currentTick3=0;
    while (1){

//stationary mode
    	if(sw3==1){
    	sprintf(oledPrint,"Temp:%0.1f", (temp_read())/10.0);
		oled_clearScreen(OLED_COLOR_BLACK);
		oled_putString(0,0,&("Stationary"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
		oled_putString(0,20,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
		led7seg_setChar('F',FALSE);
		btn3 = (GPIO_ReadValue(0) >> 4) & 0x01;
    	}
//stationary mode ends
//launch mode

    	if(sw3 == 3){
    		oled_clearScreen(OLED_COLOR_BLACK);
			acc_read(&x, &y, &z);
			x = (x+xoff);
			y = (y+yoff);
			z = (z+zoff);
			oled_putString(0,0,&("Stationary"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
			sprintf(oledPrint,"X,Y:%0.1f,%0.1f", x/64.0,y/64.0);
			oled_putString(0,20,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
			sprintf(oledPrint,"Temp:%0.1f", (temp_read())/10.0);
			oled_putString(0,40,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);

    		sw3=2;
			currentTick2=msTicks;
			while((msTicks-currentTick2)<4000){
				rgb_setLeds(0x07);
				systick_delay(333);
				rgb_setLeds(0x04);
				systick_delay(333);
				rgb_setLeds(0x01);
				systick_delay(333);
				rgb_setLeds(0x04);
				systick_delay(333);
			}
			for(i=0;i<16;i++){
				if(((temp_read())/10.0)<=TEMP_HIGH_THRESHOLD){
				led7seg_setChar(countdown[i],FALSE);
				systick_delay(1000);
				}
				else if(((temp_read()/10.0))>TEMP_HIGH_THRESHOLD){
					sw4=2;
					while(sw4==2){
						led7seg_setChar(countdown[0],FALSE);
						oled_putString(0,40,&("Temp. too high"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
						systick_delay(1000);
						rgb_setLeds(0x01);
						systick_delay(333);
						rgb_setLeds(0x04);
						systick_delay(333);
						if(((GPIO_ReadValue(1) >> 31) & 0x01)==0 && ((temp_read())/10.0)<=TEMP_HIGH_THRESHOLD){//rmb to add && ((temp_read())/10.0)<=TEMP_HIGH_THRESHOLD
							sw4=1;
							sw3=1;
							i=16;
						}
					}

				}
			}
			if(sw3==2){
			led7seg_setChar('0',FALSE);
			oled_clearScreen(OLED_COLOR_BLACK);
			acc_read(&x, &y, &z);
			x = x+xoff;
			y = y+yoff;
			z = z+zoff;
			sprintf(oledPrint,"X,Y:%0.1f,%0.1f", x/64.0,y/64.0);
			oled_putString(0,0,&("Launch"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
			oled_putString(0,20,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
			sprintf(oledPrint,"Temp:%0.1f", (temp_read())/10.0);
			oled_putString(0,40,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
			if((x/64.0)>=ACC_THRESHOLD || (x/64.0)<=-(ACC_THRESHOLD) || (y/64.0)>=ACC_THRESHOLD || (y/64.0)<=-(ACC_THRESHOLD)){
				sw4=2;
				while(sw4==2){
				oled_clearScreen(OLED_COLOR_BLACK);
				acc_read(&x, &y, &z);
				x = (x+xoff);
				y = (y+yoff);
				z = (z+zoff);
				oled_putString(0,0,&("Launch"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				sprintf(oledPrint,"X,Y:%0.1f,%0.1f", x/64.0,y/64.0);
				oled_putString(0,20,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				sprintf(oledPrint,"Temp:%0.1f", (temp_read())/10.0);
				oled_putString(0,40,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				oled_putString(0,50,&("Veer off course"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				rgb_setLeds(0x06);
				systick_delay(333);
				if(((temp_read()/10.0))>TEMP_HIGH_THRESHOLD){
					rgb_setLeds(0x01);
					systick_delay(333);
				}else{
					rgb_setLeds(0x04);
					systick_delay(333);
				}
				if(((((GPIO_ReadValue(1) >> 31) & 0x01)==0) || (sw3==4)) && (x/64.0)<=ACC_THRESHOLD && (x/64.0)>=-(ACC_THRESHOLD) && (y/64.0)<=ACC_THRESHOLD && (y/64.0)>=-(ACC_THRESHOLD)){
					sw4=1;
					sw3=4;
				}

				}
			}

			}

    	}
//launch mode ends
//return mode

			if (sw3 == 4){
				led7seg_setChar('0',FALSE);
				oled_clearScreen(OLED_COLOR_BLACK);
				oled_putString(0,0,&("Return"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				sprintf(oledPrint,"Light:%d lux", light_read());
				lightnumber=(light_read())/(4000/16);
				pca9532_setLeds(lightarray[lightnumber],0);
				oled_putString(0,20,(uint8_t*)oledPrint,OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				if(light_read()>OBSTACLE_NEAR_THRESHOLD){
					oled_putString(0,40,&("Obstacle near"),OLED_COLOR_WHITE,OLED_COLOR_BLACK);
				}
				systick_delay(2000);
				pca9532_setLeds(0,0xFFFF);
			}

//return mode ends

    }

}
