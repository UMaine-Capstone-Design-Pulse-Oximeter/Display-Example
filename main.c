#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "ssd1306.h"

#define SLEEPTIME 25

#define LED_PIN 25

int led_value = 0;

int RVal = 0;
int AVal = 0;
int IRVal = 0;

void setup_gpios(void);
void animation(void);


int main() {

    
    const uint led_pin = 25;

    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    stdio_init_all();

    sleep_ms(10000);
    gpio_put(LED_PIN, true);
    sleep_ms(4000);

    //Math Routine
    mathRoutine();


    printf("configuring pins...\n");
    setup_gpios();

    printf("jumping to animation...\n");
    animation();


    return 0;
}

//18 19 for not dev board
void setup_gpios(void) {
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);
}


void animation(void) {
    char *words[]= {"SSD1306", "DISPLAY", "RileyH"};

    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);

    for(;;) {
        for(int y=0; y<31; ++y) {
            ssd1306_draw_line(&disp, 0, y, 127, y);
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
        }

        for(int y=0, i=1; y>=0; y+=i) {
            ssd1306_draw_line(&disp, 0, 31-y, 127, 31+y);
            ssd1306_draw_line(&disp, 0, 31+y, 127, 31-y);
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
            if(y==32) i=-1;
        }

        for(int i=0; i<sizeof(words)/sizeof(char *); ++i) {
            ssd1306_draw_string(&disp, 8, 24, 2, words[i]);
            ssd1306_show(&disp);
            sleep_ms(800);
            ssd1306_clear(&disp);
        }

        for(int y=31; y<63; ++y) {
            ssd1306_draw_line(&disp, 0, y, 127, y);
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
        }
    }
}

//--------- Math Functions ---------------
int *getACDC(int arr[], int n) //find AC (max -min) and DC (min) values in an array
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
    return r;
}

int findR(int AC_RED, int DC_RED, int AC_IR, int DC_IR) // Find the R value
{
  int R = (AC_RED / DC_RED) / (AC_IR / DC_IR);
  return R;
}

float rmsValue(int arr[], int n) //find the RMS value given an arry of intigers
{
    int square = 0;
    float mean = 0.0, root = 0.0;
 
    // Calculate square.
    for (int i = 0; i < n; i++) {
        square += pow(arr[i], 2);
    }
 
    // Calculate Mean.
    mean = (square / (float)(n));
 
    // Calculate Root.
    root = sqrt(mean);
 
    return root;
}

void mathRoutine() //Display ACDC of a dummy int array
{
    //Dummy array to hold sampled values
    int arr[] = {604, 636, 763, 452, 864, 969, 29, 262, 910, 941, 142, 745, 102, 218, 309, 616, 613, 594, 419, 917, 487, 539, 230, 929, 275, 662, 270, 114, 479, 670, 640, 726, 887, 559, 973, 983, 895, 754, 720, 432, 360, 112, 397, 669, 172, 694, 888, 566, 696, 666, 409, 290, 810, 374, 851, 779, 780, 939, 653, 667, 959, 388, 556, 971, 956, 503, 189, 731, 963, 809, 862, 173, 383, 120, 404, 992, 744, 161, 260, 537, 416, 312, 522, 254, 289, 258, 575, 131, 947, 330, 894, 264, 504, 69, 512, 347, 174, 447, 40, 836};
    //Pointer to pass by refernce the resules of getACDC
    int *p;
    //Size of arry used in arrary processing
    int n = sizeof(arr)/sizeof(arr[0]);

    //Store AC DC to p
    p = getACDC(arr,n);

    //print array p
    int loop;
    for(loop = 0; loop < 2; loop++)
      printf("%d ", p[loop]);
}

//---------Timer Routine------
void IR_ON()
{
    gpio_put(LED_PIN, 1);
    printf("IR on\n");
}
void IR_OFF()
{
    gpio_put(LED_PIN, 0);
    printf("IR off\n");
}
void R_ON()
{
    gpio_put(LED_PIN, 1);
    printf("RED on\n");
}
void R_OFF()
{
    gpio_put(LED_PIN, 0);
    printf("RED off\n");
}

int getIRVal(void)
{ //find the intesity of IR
    IR_ON(); //turn on IR LED
    busy_wait_ms(50);
    //int IR_VAL = adcRead(); //read value
    int IR_VAL = 22; //Dummy value to represent a reading
    busy_wait_ms(50);
    IR_OFF(); //IR LED OFF
    return IR_VAL;
}

int getAVal(void)
{ //find the intesity of Ambient
    IR_OFF(); //IR LED OFF
    busy_wait_ms(50);
    //int A_VAL = adcRead(); //read value
    int A_VAL = 22; //Dummy value to represent a reading
    busy_wait_ms(50);
    
    return A_VAL;
}

int getRVal(void)
{ //find the insensity of RED
    R_ON(); //turn 
    busy_wait_ms(50);
    //int R_VAL = adcRead(); //read value
    int R_VAL = 21; //Dummy value to represent a reading
    busy_wait_ms(50);
    R_OFF();
    return R_VAL;
}

