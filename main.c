#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "ssd1306.h"

#define SLEEPTIME 25

#define LED_PIN 25

#define SAMPLE_SIZE 10


int sampledIR[SAMPLE_SIZE];
int ACDC_IR[2];
int sampledR[SAMPLE_SIZE];
int ACDC_R[2];

void setup_gpios(void);
void sampleIR();
void sampleR();
void printArray(int arr[], int n);
void getACDC(int arr[], int n, int color);
double findR(int AC_IR, int DC_IR, int AC_R, int DC_R);
void setup_gpios(void);
int getIRVal(void);
int getRVal(void);



int main() {
    //Initilize Serial port
    stdio_init_all();
    //Sleep to wait for user to load serial moniter
    sleep_ms(10000);
    //Setup display and onboard led variables
    printf("configuring pins...\n");
    setup_gpios();

    //test for ACDC function
    //mathRoutine();

    //Initilize Display
    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);

    double R = 0;

    while (true)
    {
        //Sample R and IR
        sampleIR();
        sampleR();

        printf("Sampled IR Values\n");
        printArray(sampledIR,SAMPLE_SIZE);
        printf("Sampled RED Values\n");
        printArray(sampledR,SAMPLE_SIZE);

        
        //Calculate ACDC and R
        getACDC(sampledIR,SAMPLE_SIZE,1);
        getACDC(sampledR,SAMPLE_SIZE,0);

        printf("AC DC IR Values\n");
        printArray(ACDC_IR,2);
        printf("AC DC RED Values\n");
        printArray(ACDC_R,2);

        R = findR(ACDC_IR[0],ACDC_IR[1],ACDC_R[0],ACDC_R[1]);
        printf("Calculated R Value %f \n", R);

        printf("jumping to animation...\n");
        char buffer[15];
        
        snprintf(buffer,15,"%f",R);
        ssd1306_draw_string(&disp, 8, 24, 2, buffer);
        ssd1306_show(&disp);
        ssd1306_clear(&disp);
  
        sleep_ms(25);
        
    
        memset(buffer, 0, 10);
    
        sleep_ms(25);


    }
    

    return 0;
}
// 18 19
// 2 3

void setup_gpios(void) {

    //Setup Devboard onboard GPIO
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT); 

    //Setup Display GPIO
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);
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

double findR(int AC_IR, int DC_IR, int AC_R, int DC_R) // Find the R value
{
    double numerator = (double) AC_R / DC_R;
    double denominator = (double) AC_IR / DC_IR;
    double R = numerator / denominator;
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
    getACDC(arr,n,1);

    //print array p
    printArray(p,2);
}

void printArray(int arr[], int n)
{   
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("]\n");
}

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

int getIRVal(void) //find the intesity of IR
{ 
    IR_ON(); //turn on IR LED
    busy_wait_ms(10);
    //int IR_VAL = adcRead(); //read value
    int IR_VAL = makeRandom(1,100); //Dummy value to represent a reading
    busy_wait_ms(10);
    IR_OFF(); //IR LED OFF
    return IR_VAL;
}

int getAVal(void) //find the intesity of Ambient
{ 
    IR_OFF(); //IR LED OFF
    busy_wait_ms(10);
    //int A_VAL = adcRead(); //read value
    int A_VAL = makeRandom(1,100); //Dummy value to represent a reading
    busy_wait_ms(10);
    
    return A_VAL;
}

int getRVal(void) //find the insensity of RED
{ 
    R_ON(); //turn 
    busy_wait_ms(10);
    //int R_VAL = adcRead(); //read value
    int R_VAL = makeRandom(1,100); //Dummy value to represent a reading
    busy_wait_ms(10);
    R_OFF();
    return R_VAL;
}