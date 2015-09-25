#include <msp430.h>
#include <stdio.h>
//definitions

#define step_size 1
#define PWM_freq 160 									//set frequency to 100 kHz
#define TOP_DUTY 128									//set max duty cycle to 80%
#define BOTTOM_DUTY 32									//set min duty cycle to 20%
#define MAX_CURR 5.0									//set max curr to 5 A
#define DELAY 1500										// 0.5s * (12000/4)Hz
#define TIMER_ON 	TA1CTL=TASSEL_1+MC_1+ID_2			// use ACLK, timer on, divide by 4
#define TIMER_OFF 	TA1CTL=TASSEL_1+MC_0+ID_2			// use ACLK, timer off, divide by 4

// Global Variables
unsigned int adc[32][4] = {0};							//array to store adc values
float curr_in = 0;										//variable to store current in
float volt_in = 0;										//variable to store voltage in
float curr_out = 0;										//variable to store current out
float volt_out = 0;										//variable to store voltage out

unsigned int duty_cycle = 80;							//variable to store current duty cycle, default to 50%

typedef enum {inc, dec} direction;						//enumeration to handle increasing and decreasing state for P&O algorithm
direction dir = inc;
unsigned char updown = 1;								// 0 is down, 1 is up
volatile float vo = 0;
volatile float io = 0;
volatile float po = 0;

//Function Prototypes
void Setup_MSP(void);									// Setup watchdog timer, clockc, ADC ports
void Setup_ADC(void);									// Setup input channels and sequences,
void Setup_PWM(void);									// Setup output pin, PWM freq, initial duty cycle, PWM mode, and clock setup
void Setup_Timer(void);									// Setup timer to allow processor to enter low power mode between adjustments.
void Read_ADC(void);									// This function reads the ADC and stores values
char Circuit_Breaker(void);								// returns false if current values are within CURR_MAX, true otherwise
void PandO(void);										// function which adjusts duty cycle via the Perturb and Observe Algorithm
void PandO2(void);										// simplified PandO algorithm
void IncCond(void);										// function which adjusts duty cycle via the IncCond Algorithm
void IncCond2(void);									// IncCond algorithm using i/v equations
void SetPoint(float point);								// sets output voltage to a certain point

int main(void)
{
 	Setup_MSP();										//settings for the MSP in general
	Setup_ADC();										//settings for ADC
	Setup_PWM();										//settings for PWM
	Setup_Timer();
	while (1)
	{
		Read_ADC();										// get current measurements from ADC and convert to voltages and currents
		PandO();										// adjust operating point if needed using Perturb and Observe Algorithm
		TIMER_ON;										// set timer 
		LPM0;
	}
}

//Set up basic configuration of MSP430
void Setup_MSP(void)
{
	  WDTCTL = WDTPW + WDTHOLD;                 		// Stop WDT
	  BCSCTL1 = CALBC1_16MHZ;     						// Set range
	  DCOCTL = CALDCO_16MHZ;     						// Set DCO step and modulation
	  __enable_interrupt();								// Enable Interupts
}

//Set up Analog to Digital Converter
void Setup_ADC(void)
{
	ADC10CTL1 = INCH_3 + CONSEQ_3;            					// A3/A2/A1/A0, multi sequence
	ADC10CTL0 = ADC10SHT_3 + MSC + ADC10ON + ADC10IE + REFON + SREF_1;
	P1DIR = 0;													// all inputs
	ADC10AE0 |= BIT0 + BIT1 + BIT2 + BIT3;                      // Disable digital I/O on P1.0 to P1.3
	ADC10DTC1 = 128;                         					// 64 conversions
}

//Set up Pulse Width Modulation
void Setup_PWM(void)
{
	P1DIR |= BIT6;              								// PWM output pin
	P1SEL |= BIT6;              								// P1.6 controlled by Pulse width modulation
	TA0CCR0 = PWM_freq;											// PWM Frequency is 100 kHz
	TA0CCR1 = duty_cycle;										// Initial Duty Cycle is 50%
	TA0CCTL1 = OUTMOD_7;										// set/reset mode
	TACTL = TASSEL_2 + MC_1; 									// Chooses SMCLK, and Up Mode.
}

//Set up timer for sleeping between readings
void Setup_Timer(void)
{
	BCSCTL3 |= LFXT1S_2;										// set ACLK to use VLO
	TA1CCR0 = DELAY;											// set the timer value
	TA1CCTL0 = CCIE;											// enable the interrupts from TimerA0 CC0
	TIMER_OFF;													// stop the timer
}

// Timer A1 interrupt service routine
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A1 (void)
{
	TIMER_OFF;													//Disable the times
	__low_power_mode_off_on_exit();								//Exit low power mode in order to take reading and change duty cycle
}

//Function to read from the ADC
//ADC values from 4 pins are taken 32 time then averaged
//Values are returned through global variables
void Read_ADC(void)
{
    ADC10CTL0 &= ~ENC;
    while (ADC10CTL1 & BUSY);              						// Wait if ADC10 core is active
    ADC10SA = (unsigned int)adc;								// Copies data in ADC10SA to unsigned int adc array
    ADC10CTL0 |= ENC + ADC10SC;             					// Sampling and conversion start

    __bis_SR_register(CPUOFF + GIE);        					// LPM0, wait for ADC to read, ADC10_ISR will force exit
       volatile int i = 0;										// variable for for loop
    volatile unsigned int curr_in_sum = 0;						// store sum of curr_in
    volatile unsigned int volt_in_sum = 0;						// store sum of volt_in
    volatile unsigned int curr_out_sum = 0;						// store sum of curr_out
    volatile unsigned int volt_out_sum = 0;						// store sum of volt_out
    for (i = 31; i>=0; i--)										//read through all 16 samples of each channel
    {															//create a sum of all the samples of each input for average
    	curr_in_sum += adc[i][3];
    	volt_in_sum += adc[i][2];
    	curr_out_sum += adc[i][1];
    	volt_out_sum += adc[i][0];
    }
    //divide each sum by 16 to get the average
    curr_in = (curr_in_sum / 32)*0.01346 + 0.01594;				//equation to calculate curr_in
    volt_in = (volt_in_sum / 32)*0.0661 - 0.27437;				//equation to calculate volt_in
    curr_out = (curr_out_sum / 32)*0.013 + 0.019945;			//equation to calculate curr_out
    volt_out = (volt_out_sum / 32)*0.0614 - 0.0524;				//equation to calculate volt_out
}

// ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
	__bic_SR_register_on_exit(CPUOFF);        					// Clear CPUOFF bit from 0(SR)
}

// Implementation of Perturb and Observe Algorithm 
void PandO(void)
{
	volatile float p = curr_in*volt_in;							// multiply current times voltage to get power, use long since multiplication could overflow an int16
	//volatile float p = curr_out*volt_out;
	switch (dir)												// switch based on whether the the duty cycle was last incremented or decremented
	{
		case inc:												// if incremented
			if (p > po)											// if power has increased
			{
				if (duty_cycle < TOP_DUTY)
				{
					duty_cycle += step_size;					// continue incrementing duty cycle
				}
				else
				{
					duty_cycle -= step_size;					// decrement duty cycle
					dir = dec;
				}
			}
			else if (p < po)									// if power has decreased
			{
				if (duty_cycle > BOTTOM_DUTY)
				{
					duty_cycle -= step_size;					// decrement duty cycle
					dir = dec;									// change direction to dec
				}
				else
				{
					duty_cycle += step_size;					// if power has increased
					dir = inc;									// change direction to inc
				}

			}
			break;
		case dec:												// if decremented
			if (p > po)											// if power has increased
			{
				if (duty_cycle > BOTTOM_DUTY)
				{
					duty_cycle -= step_size;					// decrement duty cycle
				}
				else
				{
					duty_cycle += step_size;					// if power has increased
					dir = inc;									// change direction to inc
				}
			}
			else if (p < po)									// if power has decreased
			{
				if (duty_cycle < TOP_DUTY)
				{
					duty_cycle += step_size;					// if power has increased
					dir = inc;									// change direction to inc
				}
				else
				{
					duty_cycle -= step_size;					// decrement duty cycle
					dir = dec;
				}
			}
			break;
	}
	TA0CCR1 = duty_cycle;										//update duty cycle
	po = p;														// store power to compare in next iteration
}

//Not yet implemented
//Would stop DC-DC converter if an over current event occurred
//Currently no way to protect against over voltage
char Circuit_Breaker(void)
{
	if (curr_in > MAX_CURR || curr_out > MAX_CURR)
	{
		duty_cycle = 0;
		TA0CCR1 = duty_cycle;										//update duty cycle
		return 1;
	}
	else
	{
		return 0;
	}
}

