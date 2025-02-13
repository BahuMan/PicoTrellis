#include <math.h>
#include "hardware/pio.h"
#include "MyTrellis.hpp"
#include <limits>
#include <pico/time.h>

/**************************************************************************/
/*!
    @brief  Class constructor
    @param  addr the I2C address this neotrellis object uses
    @param  i2c the I2C bus connected to this neotrellis, no default
*/
/**************************************************************************/
MyTrellis::MyTrellis(uint8_t addr, i2c_inst_t *i2c)
    : _i2c(i2c), _addr(addr), pixels(this, NEO_TRELLIS_NUM_KEYS, NEO_TRELLIS_NEOPIX_PIN, NEO_GRB + NEO_KHZ800) {

    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
    _callbacks[i] = NULL;
    }
}

/**************************************************************************/
/*!
    @brief  Begin communication with the RGB trellis.
    @param  addr optional i2c address where the device can be found. Defaults to
   NEO_TRELLIS_ADDR
    @param  flow optional flow control pin
    @returns true on success, false on error.
*/
/**************************************************************************/
bool MyTrellis::begin(uint8_t addr, int8_t flow) {
    _addr = addr;
    //why can flow == -1 ? what does that mean? no flow control?
    _flow = flow;
    if (_flow != -1)
    {
        gpio_init(flow);
        gpio_set_dir(flow, GPIO_IN);
    }
    
    uint8_t c = 0;

    this->write(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, &c, 0);
    sleep_ms(100);
    
    bool ret = pixels.begin(addr, flow);

    if (!ret) 
    return ret;

    //enableKeypadInterrupt();

    return ret;
}

/**************************************************************************/
/*!
    @brief  register a callback function on the passed key.
    @param  key the key number to register the callback on
    @param  cb the callback function that should be called when an event on that
   key happens
*/
/**************************************************************************/
void MyTrellis::registerCallback(uint8_t key,
                                           TrellisCallback (*cb)(keyEvent)) {
  _callbacks[key] = cb;
}

/**************************************************************************/
/*!
    @brief  unregister a callback on a given key
    @param  key the key number the callback is currently mapped to.
*/
/**************************************************************************/
void MyTrellis::unregisterCallback(uint8_t key) {
  _callbacks[key] = NULL;
}

/**************************************************************************/
/*!
    @brief  activate or deactivate a given key event
    @param  key the key number to map the event to
    @param  edge the edge sensitivity of the event
    @param  enable pass true to enable the passed event, false to disable it.
*/
/**************************************************************************/
void MyTrellis::activateKey(uint8_t key, uint8_t edge, bool enable) {
  setKeypadEvent(NEO_TRELLIS_KEY(key), edge, enable);
}

/**************************************************************************/
/*!
    @brief  read all events currently stored in the seesaw fifo and call any
   callbacks.
    @param  polling pass true if the interrupt pin is not being used, false if
   it is. Defaults to true.
*/
/**************************************************************************/
void MyTrellis::read(bool polling) {
  uint8_t count = getKeypadCount();
  //delayMicroseconds(500);
  if (count > 0) {
    if (polling)
      count = count + 2;
    keyEventRaw e[count];
    readKeypad(e, count);
    for (int i = 0; i < count; i++) {
      printf("%hhx ", e[i]);
      // call any callbacks associated with the key
      e[i].bit.NUM = NEO_TRELLIS_SEESAW_KEY(e[i].bit.NUM);
      if (e[i].bit.NUM < NEO_TRELLIS_NUM_KEYS &&
          _callbacks[e[i].bit.NUM] != NULL) {
        keyEvent evt = {e[i].bit.EDGE, e[i].bit.NUM};
        _callbacks[e[i].bit.NUM](evt);
      }
    }
    printf("\n");
  }
}


//following method signatures are copied from Adafruit_seesaw class:
/*!
 *****************************************************************************************
 *  @brief      activate or deactivate a key and edge on the keypad module
 *
 *  @param      key the key number to activate
 *  @param		edge the edge to trigger on
 *  @param		enable passing true will enable the passed event,
 *passing false will disable it.
 ****************************************************************************************/
void MyTrellis::setKeypadEvent(uint8_t key, uint8_t edge, bool enable) {
  keyState ks;
  ks.bit.STATE = enable;
  ks.bit.ACTIVE = (1 << edge);
  uint8_t cmd[] = {key, ks.reg};
  this->write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_EVENT, cmd, 2);
}
/**
 *****************************************************************************************
 *  @brief      enable the keypad interrupt that fires when events are in the
 *fifo.
 ****************************************************************************************/
void MyTrellis::enableKeypadInterrupt() {
    uint8_t c = 0x01;
    this->write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_INTENSET, &c, 1);
}
/**
 *****************************************************************************************
 *  @brief      disable the keypad interrupt that fires when events are in the
 *fifo.
 ****************************************************************************************/
void MyTrellis::disableKeypadInterrupt() {
    uint8_t c = 0x01;
    this->write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_INTENCLR, &c, 1);
}
/**
 *****************************************************************************************
 *  @brief      Get the number of events currently in the fifo
 *  @return     the number of events in the fifo
 ****************************************************************************************/
uint8_t MyTrellis::getKeypadCount() {
    uint8_t ret;
    this->read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &ret, 1, 500);
    printf("%hhu: ", ret);
    return ret;
}
/**
 *****************************************************************************************
 *  @brief      Read all keyEvents into the passed buffer
 *
 *  @param      buf pointer to where the keyEvents should be stored
 *  @param		count the number of events to read
 *  @returns    True on I2C read success
 ****************************************************************************************/
bool MyTrellis::readKeypad(keyEventRaw *buf, uint8_t count) {
  return this->read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_FIFO, (uint8_t *)buf,
                    count, 1000);
}

/*!
 *****************************************************************************************
 *  @brief      Write a specified number of bytes to the seesaw from the passed
 *buffer.
 *
 *  @param      regHigh the module address register (ex. SEESAW_GPIO_BASE)
 *  @param	regLow the function address register (ex. SEESAW_GPIO_BULK_SET)
 *  @param	buf the buffer the the bytes from
 *  @param	num the number of bytes to write.
 *  @returns    True on I2C write success
 ****************************************************************************************/
bool MyTrellis::write(uint8_t regHigh, uint8_t regLow,
                            uint8_t *buf, uint8_t num) {
    uint8_t prefix[2+num];
    prefix[0] = (uint8_t)regHigh;
    prefix[1] = (uint8_t)regLow;
    for (int cp=0; cp<num; ++cp) prefix[2+cp] = buf[cp];

    if (_flow != -1)
        while (!gpio_get(_flow))
        sleep_ms(10); //@TODO: remove if possible;

    int res = i2c_write_blocking(_i2c, _addr, prefix, 2+num, false);
    if (res != (2+num) || res == PICO_ERROR_GENERIC) {
      printf("write totality %hhu:%hhu failed %i\n", regHigh, regLow, res);
      return false;
    }
    //else {
    //  printf("write totality %hhu:%hhu SUCCESS %i\n", regHigh, regLow, res);
    //}

    return true;
}

/**
 *****************************************************************************************
 *  @brief      Read a specified number of bytes into a buffer from the seesaw.
 *
 *  @param      regHigh the module address register (ex. SEESAW_STATUS_BASE)
 *	@param		regLow the function address register (ex. SEESAW_STATUS_VERSION)
 *	@param		buf the buffer to read the bytes into
 *	@param		num the number of bytes to read.
 *	@param		delay an optional delay in between setting the read
 *register and reading out the data. This is required for some seesaw functions
 *(ex. reading ADC data)
 *  @returns    True on I2C read success
 ****************************************************************************************/
bool MyTrellis::read(uint8_t regHigh, uint8_t regLow, uint8_t *buf,
                           uint8_t num, uint16_t optionalDelay) {
    uint8_t pos = 0;
    uint8_t prefix[2];
    prefix[0] = (uint8_t)regHigh;
    prefix[1] = (uint8_t)regLow;

    if (_flow != -1) {
      while (!gpio_get(_flow)) //(!::digitalRead(_flow))
        sleep_ms(10);
    }

    int res = i2c_write_blocking(_i2c, _addr, prefix, 2, false);
    if (res != 2 || res == PICO_ERROR_GENERIC) {
      printf("write before read %hhx:%hhx failed with %i\n", regHigh, regLow, res);
      return false;
    }

    //@TODO: tune this
    sleep_ms(optionalDelay);

    if (_flow != -1) {
      while (!gpio_get(_flow)) //(!::digitalRead(_flow))
        sleep_ms(10);
    }

    res = i2c_read_blocking(_i2c, _addr, buf, num, false);
    if (res != num || res == PICO_ERROR_GENERIC) {
      printf("read %hhx:%hhx failed %i\n", regHigh, regLow, res);
      return false;
    }
    return (res == num);
}
