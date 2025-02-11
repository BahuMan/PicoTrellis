#include <stdio.h>
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/time.h"

//this file uses inspiration/code from:
//https://github.com/adafruit/Adafruit_BusIO/blob/055013b58dab9c3f1e016c75aac6eafb58e8d789/Adafruit_I2CDevice.cpp
//https://github.com/adafruit/Adafruit_Seesaw/blob/985b41efae3d9a8cba12a7b4d9ff0d226f9e0759/Adafruit_seesaw.cpp
//https://github.com/adafruit/Adafruit_Seesaw/blob/985b41efae3d9a8cba12a7b4d9ff0d226f9e0759/Adafruit_NeoTrellis.cpp

//specifically for the pico:
//https://www.digikey.be/en/maker/projects/raspberry-pi-pico-rp2040-i2c-example-with-micropython-and-cc/47d0c922b79342779cdbd4b37b7eb7e2
//primer on i2c:
//https://www.analog.com/en/resources/technical-articles/i2c-primer-what-is-i2c-part-1.html

#define NEO_TRELLIS_ADDR 0x2E

#define NEO_TRELLIS_NEOPIX_PIN 3

#define NEO_TRELLIS_NUM_ROWS 4
#define NEO_TRELLIS_NUM_COLS 4
#define NEO_TRELLIS_NUM_KEYS (NEO_TRELLIS_NUM_ROWS * NEO_TRELLIS_NUM_COLS)

#define NEO_TRELLIS_MAX_CALLBACKS 32

#define NEO_TRELLIS_KEY(x) (((x) / 4) * 8 + ((x) % 4))
#define NEO_TRELLIS_SEESAW_KEY(x) (((x) / 8) * 4 + ((x) % 8))

#define NEO_TRELLIS_X(k) ((k) % 4)
#define NEO_TRELLIS_Y(k) ((k) / 4)

#define NEO_TRELLIS_XY(x, y) ((y)*NEO_TRELLIS_NUM_COLS + (x))

//copied from seesaw.h:
enum {
  SEESAW_STATUS_BASE = 0x00,
  SEESAW_GPIO_BASE = 0x01,
  SEESAW_SERCOM0_BASE = 0x02,

  SEESAW_TIMER_BASE = 0x08,
  SEESAW_ADC_BASE = 0x09,
  SEESAW_DAC_BASE = 0x0A,
  SEESAW_INTERRUPT_BASE = 0x0B,
  SEESAW_DAP_BASE = 0x0C,
  SEESAW_EEPROM_BASE = 0x0D,
  SEESAW_NEOPIXEL_BASE = 0x0E,
  SEESAW_TOUCH_BASE = 0x0F,
  SEESAW_KEYPAD_BASE = 0x10,
  SEESAW_ENCODER_BASE = 0x11,
  SEESAW_SPECTRUM_BASE = 0x12,
};
/** status module function address registers
 */
enum {
  SEESAW_STATUS_HW_ID = 0x01,
  SEESAW_STATUS_VERSION = 0x02,
  SEESAW_STATUS_OPTIONS = 0x03,
  SEESAW_STATUS_TEMP = 0x04,
  SEESAW_STATUS_SWRST = 0x7F,
};
/** NeoPixel module function address registers
 */
enum {
  SEESAW_NEOPIXEL_STATUS = 0x00,
  SEESAW_NEOPIXEL_PIN = 0x01,
  SEESAW_NEOPIXEL_SPEED = 0x02,
  SEESAW_NEOPIXEL_BUF_LENGTH = 0x03,
  SEESAW_NEOPIXEL_BUF = 0x04,
  SEESAW_NEOPIXEL_SHOW = 0x05,
};
/** keypad module function address registers
 */
enum {
  SEESAW_KEYPAD_STATUS = 0x00,
  SEESAW_KEYPAD_EVENT = 0x01,
  SEESAW_KEYPAD_INTENSET = 0x02,
  SEESAW_KEYPAD_INTENCLR = 0x03,
  SEESAW_KEYPAD_COUNT = 0x04,
  SEESAW_KEYPAD_FIFO = 0x10,
};

/** keypad module edge definitions
 */
enum {
  SEESAW_KEYPAD_EDGE_HIGH = 0,
  SEESAW_KEYPAD_EDGE_LOW,
  SEESAW_KEYPAD_EDGE_FALLING,
  SEESAW_KEYPAD_EDGE_RISING,
};

/** raw key event stucture for keypad module */
union keyEventRaw {
  struct {
    uint8_t EDGE : 2; ///< the edge that was triggered
    uint8_t NUM : 6;  ///< the event number
  } bit;              ///< bitfield format
  uint8_t reg;        ///< register format
};

/** extended key event stucture for keypad module */
union keyEvent {
  struct {
    uint8_t EDGE : 2;  ///< the edge that was triggered
    uint16_t NUM : 14; ///< the event number
  } bit;               ///< bitfield format
  uint16_t reg;        ///< register format
};

/** key state struct that will be written to seesaw chip keypad module */
union keyState {
  struct {
    uint8_t STATE : 1;  ///< the current state of the key
    uint8_t ACTIVE : 4; ///< the registered events for that key
  } bit;                ///< bitfield format
  uint8_t reg;          ///< register format
};

typedef void (*TrellisCallback)(keyEvent evt);
#define NEO_GRB ((1 << 6) | (1 << 4) | (0 << 2) | (2))
#define NEO_KHZ400 0x0100 // 400 KHz datastream
#define NEO_KHZ800 0x0000 // 800 KHz datastream
typedef uint16_t neoPixelType;
#define SEESAW_ADDRESS (0x49)

//@TODO: this probably doesn't belong with trellis but should be in the main.cpp
// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5

//forward declaration:
class MyTrellis;

class MyNeoPixel {
public:
    MyNeoPixel(MyTrellis *trellis, uint16_t n, uint8_t p = 6,
                    neoPixelType t = NEO_GRB + NEO_KHZ800);
    //MyNeoPixel();
    ~MyNeoPixel();
    bool begin(uint8_t addr = SEESAW_ADDRESS, int8_t flow = -1);
    void show(void), setPin(uint8_t p),
        setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b),
        setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w),
        setPixelColor(uint16_t n, uint32_t c), setBrightness(uint8_t), clear(),
        updateLength(uint16_t n), updateType(neoPixelType t);
    uint8_t *getPixels(void) const, getBrightness(void) const;
    int8_t getPin(void) { return pin; };
    uint16_t numPixels(void) const;
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b),
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    uint32_t getPixelColor(uint16_t n) const;
    //@TODO: check pico/time.h; there may be a more relevant macro/function to check elapsed time
    inline bool canShow(void) { return (get_absolute_time() - endTime) >= 300L; }


private:
    MyTrellis *wrapTrellis; //the wrapper trellis to send i2c writes to
    bool is800KHz,    // ...true if 800 KHz pixels
        begun;        // true if begin() previously called
    uint16_t numLEDs, // Number of RGB LEDs in strip
        numBytes;     // Size of 'pixels' buffer below (3 or 4 bytes/pixel)
    int8_t pin;
    uint8_t brightness,
        *pixels,      // Holds LED color values (3 or 4 bytes each)
        rOffset,      // Index of red byte within each 3- or 4-byte pixel
        gOffset,      // Index of green byte
        bOffset,      // Index of blue byte
        wOffset;      // Index of white byte (same as rOffset if no white)
    uint32_t endTime; // Latch timing reference
    uint16_t type;
};


class MyTrellis {
public:
    MyTrellis(uint8_t addr, i2c_inst_t *i2c);
    ~MyTrellis(){};

    bool begin(uint8_t addr = NEO_TRELLIS_ADDR, int8_t flow = -1);

    void registerCallback(uint8_t key, TrellisCallback (*cb)(keyEvent));
    void unregisterCallback(uint8_t key);

    void activateKey(uint8_t key, uint8_t edge, bool enable = true);

    void read(bool polling);

    MyNeoPixel pixels; ///< the onboard neopixel matrix

    //following method signatures are copied from Adafruit_seesaw class:
    void setKeypadEvent(uint8_t key, uint8_t edge, bool enable = true);
    void enableKeypadInterrupt();
    void disableKeypadInterrupt();
    uint8_t getKeypadCount();
    bool readKeypad(keyEventRaw *buf, uint8_t count);
    bool write(uint8_t regHigh, uint8_t regLow,
                uint8_t *buf = NULL, uint8_t num = 0);
    bool read(uint8_t regHigh, uint8_t regLow, uint8_t *buf,
                           uint8_t num, uint16_t delay);
protected:
    i2c_inst_t *_i2c; //my own implementation
    uint8_t _addr; // the I2C address of this board
    int8_t _flow; //comes from seesaw; pin to read for flow control (no idea)
    TrellisCallback (*_callbacks[NEO_TRELLIS_NUM_KEYS])(
      keyEvent); ///< the array of callback functions
};
