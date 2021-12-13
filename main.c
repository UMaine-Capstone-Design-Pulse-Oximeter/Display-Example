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
static const int cal_f_b = 40;
static const int cal_f_c = 77;

//Global variables to hold AC/DC RED and IR values
int sampledIR[SAMPLE_SIZE];
int ACDC_IR[2];
int sampledR[SAMPLE_SIZE];
int ACDC_R[2];

float spO2_avg_arr[SAMPLE_SIZE_ROLL_AVG];
float spO2_avg_arr_temp[SAMPLE_SIZE_ROLL_AVG];

void setup_gpios(void);

void sampleIR();
void sampleR();

void printArray(int arr[], int n);
void printfloatArray(float arr[], int n);

void getACDC(int arr[], int n, int color);
float find_spO2(int AC_IR, int DC_IR, int AC_R, int DC_R);

int getIRVal(void);
int getRVal(void);



int main() {
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
        sampleIR();
        sampleR();

        //Print Sampled IR and RED Values to Serial Moniter
        printf("Sampled IR Values\n");
        printArray(sampledIR,SAMPLE_SIZE);
        printf("Sampled RED Values\n");
        printArray(sampledR,SAMPLE_SIZE);

        //Calculate ACDC
        getACDC(sampledIR,SAMPLE_SIZE,1);
        getACDC(sampledR,SAMPLE_SIZE,0);

        //Print AC/DC IR and RED Values to Serial Moniter
        printf("AC DC IR Values\n");
        printArray(ACDC_IR,2);
        printf("AC DC RED Values\n");
        printArray(ACDC_R,2);

        //Calculate spO2 and Print to Serial Moniter
        spO2 = find_spO2(ACDC_IR[0],ACDC_IR[1],ACDC_R[0],ACDC_R[1]);
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
void getACDC(int arr[], int n, int color) //find AC (max -min) and DC (min) values in an array
{
    static int r[2];
    int i;

    int max = arr[0];
    int min = arr[0];
    
    for (i = 1; i < n; i++){
      if (arr[i] > max)
          max = arr[i];
      if (arr[i] < min)
          min = arr[i];
    }

    r[0] = max - min;
    r[1] = min;
    
    if(color){
        ACDC_IR[0] = r[0];
        ACDC_IR[1] = r[1];
    } else{
        ACDC_R[0] = r[0];
        ACDC_R[1] = r[1];
    }
}


float find_spO2(int AC_IR, int DC_IR, int AC_R, int DC_R) // Find the R value
{
    float numerator = (float) AC_R / DC_R;
    float denominator = (float) AC_IR / DC_IR;
    float R = numerator / denominator;

    float spO2 = ( (cal_f_a*R*R) + (cal_f_b*R) + (cal_f_c));
    return spO2;
}


void mathRoutine() //Display ACDC of a dummy int array

{
    //Dummy array to hold sampled values
    int arr[] = {604, 636, 763, 452, 864, 969, 29, 262, 910, 941, 142, 745, 102, 218, 309, 616, 613, 594, 419, 917, 487, 539, 230, 929, 275, 662, 270, 114, 479, 670, 640, 726, 887, 559, 973, 983, 895, 754, 720, 432, 360, 112, 397, 669, 172, 694, 888, 566, 696, 666, 409, 290, 810, 374, 851, 779, 780, 939, 653, 667, 959, 388, 556, 971, 956, 503, 189, 731, 963, 809, 862, 173, 383, 120, 404, 992, 744, 161, 260, 537, 416, 312, 522, 254, 289, 258, 575, 131, 947, 330, 894, 264, 504, 69, 512, 347, 174, 447, 40, 836};
    //Pointer to pass by refernce the results of get ACDC
    int *p;
    //Size of array used in arrary processing
    int n = sizeof(arr)/sizeof(arr[0]);

    //Store AC DC to p
    getACDC(arr,n,1);

    //print array p
    printArray(p,2);
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

//Utility Function makes random initiger, was used in development to simulate DAC sample
int makeRandom(int lower, int upper)
{
    int num = (rand() %(upper - lower + 1)) + lower;
    return num;
}
// -------Measure Functions ----------
void sampleIR()
{
    int i;
    for (i = 0; i < SAMPLE_SIZE; i++){
        sampledIR[i] = getIRVal();
    }

}

void sampleR()
{
    int i;
    for (i = 0; i < SAMPLE_SIZE; i++){
        sampledR[i] = getRVal();
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

