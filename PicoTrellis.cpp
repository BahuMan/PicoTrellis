#include <stdio.h>
#include <pico\version.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "MyTrellis.hpp"

//forward declaration of 2 functions defined at EOF:
uint8_t map(uint8_t v, uint8_t fromLow, uint8_t fromHigh, uint8_t toLow, uint8_t toHigh);
uint32_t Wheel(MyTrellis *trellis, uint8_t WheelPos);

/*
//define a callback for key presses
TrellisCallback blink(keyEvent evt){
  // Check is the pad pressed?
  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    trellis.pixels.setPixelColor(evt.bit.NUM, 0 , 0, 255); //on rising
  } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
  // or is the pad released?
    trellis.pixels.setPixelColor(evt.bit.NUM, 0); //off falling
  }

  // Turn on/off the neopixels!
  trellis.pixels.show();

  return 0;
}
*/

int main()
{
    stdio_init_all();
    sleep_ms(5000);
    printf("stdio initialized\n");

    i2c_inst_t *myI2C = i2c_get_instance(0);
    printf("I2C retrieved\n");
    //Initialize I2C port at 800 kHz
    i2c_init(myI2C, 800 * 1000);
    // Initialize I2C pins
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    printf("I2C initialized\n");
    
    MyTrellis trellis(NEO_TRELLIS_ADDR, myI2C);

    //@TODO: for now these are set inside the trellis constructor
    //       but I think they belong here
    //gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    //gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    while (!trellis.begin()) {
        printf("Could not initialize MyTrellis\n");
        sleep_ms(1000);
    }
    
    //activate all keys and set callbacks
    //for(int i=0; i<NEO_TRELLIS_NUM_KEYS; i++){
    //    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    //    trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING);
    //    trellis.registerCallback(i, blink);
    //}

    while (true) {
        //do a little animation to show we're on
        for (uint16_t i=0; i<trellis.pixels.numPixels(); i++) {
          trellis.pixels.setPixelColor(i, 100, 100, 255);
          trellis.pixels.show();
          sleep_ms(50);
        }
        for (uint16_t i=0; i<trellis.pixels.numPixels(); i++) {
          trellis.pixels.setPixelColor(i, 0x000000);
          trellis.pixels.show();
          sleep_ms(50);
        }
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}

uint8_t map(uint8_t v, uint8_t fromLow, uint8_t fromHigh, uint8_t toLow, uint8_t toHigh) {
    uint8_t fromRange = fromHigh - fromLow;
    uint8_t toRange = toHigh - toLow;
    return (v - fromLow) * toRange / fromRange + toLow;
}

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(MyTrellis *trellis, uint8_t WheelPos) {
    if(WheelPos < 85) {
        return trellis->pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }
    else if(WheelPos < 170) {
    WheelPos -= 85;
        return trellis->pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    else {
        WheelPos -= 170;
        return trellis->pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    return 0;
}