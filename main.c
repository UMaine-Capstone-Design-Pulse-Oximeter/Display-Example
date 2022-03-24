// Pulse Oximeter Capstone Design Project
// Riley Hagerstrom and Cully Richard

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "ssd1306.h"

#define SLEEPTIME 25

#define LED_PIN 25

#define SAMPLE_SIZE 100
#define SAMPLE_SIZE_ROLL_AVG 20

#define SAMPLE_WAIT 1

#define SSD1306_WRITE_ADDRESS 0x3C
#define DAC7574_WRITE_ADDRESS 0x4c
#define IDK_ADDRESS 0x48

//Define GPIO Pins

//Setup Display GPIO Pins
// 18 19 for custom board
// 2 3 for dev board
const uint display_sda_pin = 18;
const uint display_scl_pin = 19;
const uint red_led_pin = 16;
const uint ir_led_pin = 17;

//Calibration constats for spO2 calculation
static const int cal_f_a = 0;
static const int cal_f_b = 37;
static const int cal_f_c = 59;

struct Light {
  int sampledIR[SAMPLE_SIZE];
  int ACDC_IR_AC;
  int ACDC_IR_DC;
  
  int sampledR[SAMPLE_SIZE];
  int ACDC_R_AC;
  int ACDC_R_DC;
};



void setup_gpios(void);

void sampleIR(struct Light *a1);
void sampleR(struct Light *a1);

void printArray(int arr[], int n);
void printfloatArray(float arr[], int n);

void displayACDC(struct Light *a1);
void display_sampled(struct Light *a1,int n);

void getACDC(struct Light *a1);

float find_spO2(struct Light *a1);

int getIRVal(void);
int getRVal(void);



int main() {

    float spO2_avg_arr[SAMPLE_SIZE_ROLL_AVG];
    float spO2_avg_arr_temp[SAMPLE_SIZE_ROLL_AVG];

    struct Light A = { {18,6,39,7,95,66,57,26,33,26}, 0, 0, {45,92,18,89,53,46,91,66,83,90}, 0, 0 };


    //Initialize Serial port
    stdio_init_all();

    //Sleep to wait for user to load serial moniter
    sleep_ms(1000);
    //Setup display and onboard led variables
    printf("configuring pins...\n");
    setup_gpios();

    //Ports
    i2c_inst_t *i2c = i2c1;

    //Initialize Display
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, SSD1306_WRITE_ADDRESS, i2c1);
    ssd1306_clear(&disp);

    //Initialize spO2 global variables
    float spO2 = 0;

    //Initialize spO2 average
    float spO2_avg  = 0;

    //Initialize Counter
    int i;

    for(i = 0; i < SAMPLE_SIZE_ROLL_AVG; i++){
        spO2_avg_arr[i] = 0;
    }

    while (true)
    {
        //Sample R and IR
        sampleIR(&A);
        sampleR(&A);

        //Print Sampled IR and RED Values to Serial Moniter
        display_sampled(&A,SAMPLE_SIZE);

        //Calculate ACDC
        getACDC(&A);

        //Print AC/DC IR and RED Values to Serial Moniter
        displayACDC(&A);

        //Calculate spO2 and Print to Serial Moniter
        spO2 = find_spO2(&A);
        printf("Calculated SpO2 Value %f \n", spO2);

        //Update the spO2 rolling average array
        for (i = 0; i < SAMPLE_SIZE_ROLL_AVG - 1; i++){
            spO2_avg_arr[i] = spO2_avg_arr[i+1];
        }
        spO2_avg_arr[SAMPLE_SIZE_ROLL_AVG-1] = spO2;

        //Print Updated Arrary to Moniter
        printfloatArray(spO2_avg_arr,SAMPLE_SIZE_ROLL_AVG);

        //Calculate the rolling average
        float sum = 0;
        for(int i = 0; i < SAMPLE_SIZE_ROLL_AVG; i++){
            sum = sum + spO2_avg_arr[i];
        }
        spO2_avg = sum / SAMPLE_SIZE_ROLL_AVG;

        //Update display with rolling average
        printf("Updating Display...\n");
        char buffer[15];
        
        snprintf(buffer,15,"%.0f",spO2_avg);
        ssd1306_draw_string(&disp, 8, 24, 2, buffer);
        ssd1306_show(&disp);
        ssd1306_clear(&disp);
  
        sleep_ms(25);
        memset(buffer, 0, 10);
        sleep_ms(25);
    }
    return 0;
}

//----------Setup and Display Functions----------
void setup_gpios(void) {

    //Setup Devboard onboard GPIO
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT); 

    //Setup Display GPIO
    // 18 19 for custom board
    // 2 3 for dev board
    i2c_init(i2c1, 400000);
    gpio_set_function(display_sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(display_scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(display_sda_pin);
    gpio_pull_up(display_scl_pin);

    gpio_init(red_led_pin);
    gpio_set_dir(red_led_pin, GPIO_OUT);
    gpio_init(ir_led_pin);
    gpio_set_dir(ir_led_pin,GPIO_OUT);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(0);
}


void animationUpdate(char word[]) //Write a string to display
{
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);

    ssd1306_draw_string(&disp, 8, 24, 2, word);
    ssd1306_show(&disp);
    ssd1306_clear(&disp);
}


//--------- Math Functions ---------------
void getACDC(struct Light *a1) 
{

    int n = SAMPLE_SIZE;
    int i;

    int maxIR = a1->sampledIR[0];
    int minIR = a1->sampledIR[0];

    int maxR = a1->sampledR[0];
    int minR = a1->sampledR[0];
  
    for (i = 1; i < n; i++){
      if (a1->sampledIR[i] > maxIR)
          maxIR = a1->sampledIR[i];
      if (a1->sampledIR[i] < minIR)
          minIR = a1->sampledIR[i];
      
      if (a1->sampledR[i] > maxR)
          maxR = a1->sampledR[i];
      if (a1->sampledR[i] < minR)
          minR = a1->sampledR[i];
    }

    a1->ACDC_IR_AC = maxIR - minIR;
    a1->ACDC_IR_DC = minIR;

    a1->ACDC_R_AC = maxR - minR;
    a1->ACDC_R_DC = minR;
}


float find_spO2(struct Light *a1){
    float numerator = (float) a1->ACDC_R_AC / a1->ACDC_R_DC;
    float denominator = (float) a1->ACDC_IR_AC / a1->ACDC_IR_DC;
    float R = numerator / denominator;

    float spO2 = ( (cal_f_a*R*R) + (cal_f_b*R) + (cal_f_c));
    return spO2;
}


//Utility Function prints initger array
void printArray(int arr[], int n)
{   
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("]\n");
}

//Utility Function prints float array
void printfloatArray(float arr[], int n)
{   
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%.2f ", arr[i]);
    }
    printf("]\n");
}




void displayACDC(struct Light *a1){
  printf("Sampled IR First Value: %d\n",a1->sampledIR[0]);
  printf("ACDC_IR_AC: %d  ",a1->ACDC_IR_AC);
  printf("ACDC_IR_DC: %d\n",a1->ACDC_IR_DC);
  printf("ACDC_R_AC: %d   ",a1->ACDC_R_AC);
  printf("ACDC_R_DC: %d\n",a1->ACDC_R_DC);
  
}


void display_sampled(struct Light *a1,int n)
{   
    printf("Sampled IR Values: [");
    for (int i = 0; i < n; i++) {
        printf("%d ", a1->sampledIR[i]);
    }
    printf("]\n");

    printf("Sampled RED Values: [");
    for (int i = 0; i < n; i++) {
        printf("%d ", a1->sampledR[i]);
    }
    printf("]\n");
}



//Utility Function makes random initiger, was used in development to simulate DAC sample
int makeRandom(int lower, int upper)
{
    int num = (rand() %(upper - lower + 1)) + lower;
    return num;
}
// -------Measure Functions ----------
void sampleIR(struct Light *a1)
{
    int i;
    for (i = 0; i < SAMPLE_SIZE; i++){
        a1->sampledIR[i] = getIRVal();
    }

}

void sampleR(struct Light *a1)
{
    int i;
    for (i = 0; i < SAMPLE_SIZE; i++){
        a1->sampledR[i] = getRVal();
    }
}

void IR_ON()
{
    gpio_put(ir_led_pin, 1);
    gpio_put(red_led_pin,0);
    //printf("IR on\n");
}
void IR_OFF()
{
    gpio_put(ir_led_pin, 0);
    gpio_put(red_led_pin,0);
    //printf("IR off\n");
}
void R_ON()
{
    gpio_put(red_led_pin, 1);
    gpio_put(ir_led_pin,0);
    //printf("RED on\n");
}
void R_OFF()
{
    gpio_put(red_led_pin, 0);
    gpio_put(ir_led_pin,0);
    //printf("RED off\n");
}

int getIRVal(void) //find the intensity of IR
{ 
    IR_ON(); //turn on IR LED
    busy_wait_ms(SAMPLE_WAIT);
    int IR_VAL = adc_read(); //read value
    //int IR_VAL = makeRandom(1,100); //Dummy value to represent a reading
    busy_wait_ms(SAMPLE_WAIT);
    IR_OFF(); //IR LED OFF
    return IR_VAL;
}

int getAVal(void) //find the intensity of Ambient
{ 
    IR_OFF(); //IR LED OFF
    busy_wait_ms(SAMPLE_WAIT);
    int A_VAL = adc_read(); //read value
    //int A_VAL = makeRandom(1,100); //Dummy value to represent a reading
    busy_wait_ms(SAMPLE_WAIT);
    
    return A_VAL;
}

int getRVal(void) //find the intensity of RED
{ 
    R_ON(); //turn 
    busy_wait_ms(SAMPLE_WAIT);
    int R_VAL = adc_read(); //read value
    //int R_VAL = makeRandom(1,100); //Dummy value to represent a reading
    busy_wait_ms(SAMPLE_WAIT);
    R_OFF();
    return R_VAL;
}

