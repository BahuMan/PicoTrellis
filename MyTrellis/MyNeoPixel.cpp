#include "MyTrellis.hpp"

MyNeoPixel::MyNeoPixel(MyTrellis *trel, uint16_t n, uint8_t p, neoPixelType t)
    : begun(false), wrapTrellis(trel), numLEDs(n), pin(p), brightness(0),
      pixels(NULL), endTime(0), type(t) {
}

MyNeoPixel::~MyNeoPixel() {
  if (pixels)
    delete pixels; //delete(pixels) ?
}

bool MyNeoPixel::begin(uint8_t addr, int8_t flow) {
    updateType(type);
    sleep_ms(100);
    setPin(pin);
    sleep_ms(100);
    updateLength(numLEDs);
    sleep_ms(100);

    begun=true; //?
    return true;
}

void MyNeoPixel::updateLength(uint16_t n) {
  if (pixels)
    delete pixels; // Free existing data (if any)

  // Allocate new data -- note: ALL PIXELS ARE CLEARED
  numBytes = n * ((wOffset == rOffset) ? 3 : 4);
  pixels = new uint8_t[numBytes];
    numLEDs = n;

    //switch from little-endian to big-endian:
    uint8_t buf[] = {(uint8_t)(numBytes >> 8), (uint8_t)(numBytes & 0xFF)};
    wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF_LENGTH, buf, 2);
}

void MyNeoPixel::updateType(neoPixelType t) {
    bool oldThreeBytesPerPixel = (wOffset == rOffset); // false if RGBW

    wOffset = (t >> 6) & 0b11; // See notes in header file
    rOffset = (t >> 4) & 0b11; // regarding R/G/B/W offsets
    gOffset = (t >> 2) & 0b11;
    bOffset = t & 0b11;
    is800KHz = (t < 256); // 400 KHz flag is 1<<8
    uint8_t use800KHz = 1U; //is800KHz?0xFF:0x00;
    wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SPEED, &use800KHz, 1);

    // If bytes-per-pixel has changed (and pixel data was previously
    // allocated), re-allocate to new size.  Will clear any data.
    if (pixels) {
        bool newThreeBytesPerPixel = (wOffset == rOffset);
        if (newThreeBytesPerPixel != oldThreeBytesPerPixel)
        updateLength(numLEDs);
    }
}

void MyNeoPixel::show(void) {

  if (!pixels)
    return;

  // Data latch = 300+ microsecond pause in the output stream.  Rather than
  // put a delay at the end of the function, the ending time is noted and
  // the function will simply hold off (if needed) on issuing the
  // subsequent round of data until the latch time has elapsed.  This
  // allows the mainline code to start generating the next frame of data
  // rather than stalling for the latch.
  while (!canShow())
    ;

  wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SHOW, NULL, 0);

  endTime = get_absolute_time(); // Save EOD time for latch on next call
}

// Set the output pin number
void MyNeoPixel::setPin(uint8_t p) {
  wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_PIN, &p, 1);
  pin = p;
}

// Set pixel color from separate R,G,B components:
void MyNeoPixel::setPixelColor(uint16_t n, uint8_t r, uint8_t g,
                                    uint8_t b) {

  if (n < numLEDs) {
    if (brightness) { // See notes in setBrightness()
      r = (r * brightness) >> 8;
      g = (g * brightness) >> 8;
      b = (b * brightness) >> 8;
    }
    uint8_t *p;
    if (wOffset == rOffset) { // Is an RGB-type strip
      p = &pixels[n * 3];     // 3 bytes per pixel
    } else {                  // Is a WRGB-type strip
      p = &pixels[n * 4];     // 4 bytes per pixel
      p[wOffset] = 0;         // But only R,G,B passed -- set W to 0
    }
    p[rOffset] = r; // R,G,B always stored
    p[gOffset] = g;
    p[bOffset] = b;

    uint8_t len = (wOffset == rOffset ? 3 : 4);
    uint16_t offset = n * len;

    uint8_t writeBuf[6];
    writeBuf[0] = (offset >> 8);
    writeBuf[1] = offset & 0xFF;
    writeBuf[2] = p[0]; //replaced memcp with 4 assignments
    writeBuf[3] = p[1];
    writeBuf[4] = p[2];
    writeBuf[5] = 0; //regardless of wOffset, we DON'T specify white color

    //printf("%u: %hhx %hhx %hhx %hhx %hhx %hhx\n", n, writeBuf[0], writeBuf[1], writeBuf[2], writeBuf[3], writeBuf[4], writeBuf[5]);
    wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF, writeBuf, len + 2);
  }
}

void MyNeoPixel::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b,
                                    uint8_t w) {

  if (n < numLEDs) {
    if (brightness) { // See notes in setBrightness()
      r = (r * brightness) >> 8;
      g = (g * brightness) >> 8;
      b = (b * brightness) >> 8;
      w = (w * brightness) >> 8;
    }
    uint8_t *p;
    if (wOffset == rOffset) { // Is an RGB-type strip
      p = &pixels[n * 3];     // 3 bytes per pixel (ignore W)
    } else {                  // Is a WRGB-type strip
      p = &pixels[n * 4];     // 4 bytes per pixel
      p[wOffset] = w;         // Store W
    }
    p[rOffset] = r; // Store R,G,B
    p[gOffset] = g;
    p[bOffset] = b;

    uint8_t len = (wOffset == rOffset ? 3 : 4);
    uint16_t offset = n * len;

    uint8_t writeBuf[6];
    writeBuf[0] = (offset >> 8);
    writeBuf[1] = offset;
    writeBuf[2] = p[0]; //replaced memcp with 4 assignments
    writeBuf[3] = p[1];
    writeBuf[4] = p[2];
    writeBuf[5] = p[3];

    wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF, writeBuf, len + 2);
  }
}


// Set pixel color from 'packed' 32-bit RGB color:
void MyNeoPixel::setPixelColor(uint16_t n, uint32_t c) {
  if (n < numLEDs) {
    uint8_t *p, r = (uint8_t)(c >> 16), g = (uint8_t)(c >> 8), b = (uint8_t)c;
    if (brightness) { // See notes in setBrightness()
      r = (r * brightness) >> 8;
      g = (g * brightness) >> 8;
      b = (b * brightness) >> 8;
    }
    uint8_t w = 0;
    if (wOffset == rOffset) {
      p = &pixels[n * 3];
    } else {
      p = &pixels[n * 4];
      w = (uint8_t)(c >> 24);
      p[wOffset] = brightness ? ((w * brightness) >> 8) : w;
    }
    p[rOffset] = r;
    p[gOffset] = g;
    p[bOffset] = b;

    uint8_t len = (wOffset == rOffset ? 3 : 4);
    uint16_t offset = n * len;

    uint8_t writeBuf[6];
    writeBuf[0] = (offset >> 8);
    writeBuf[1] = offset & 0xFF;
    writeBuf[2] = p[0]; //replaced memcp with 4 assignments
    writeBuf[3] = p[1];
    writeBuf[4] = p[2];
    writeBuf[5] = (wOffset == rOffset ? 0 : w);

    //printf("%u: %hhx %hhx %hhx %hhx %hhx %hhx\n", n, writeBuf[0], writeBuf[1], writeBuf[2], writeBuf[3], writeBuf[4], writeBuf[5]);
    wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF, writeBuf, len + 2);
  }
}

// Convert separate R,G,B into packed 32-bit RGB color.
// Packed format is always RGB, regardless of LED strand color order.
uint32_t MyNeoPixel::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Convert separate R,G,B,W into packed 32-bit WRGB color.
// Packed format is always WRGB, regardless of LED strand color order.
uint32_t MyNeoPixel::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Query color from previously-set pixel (returns packed 32-bit RGB value)
uint32_t MyNeoPixel::getPixelColor(uint16_t n) const {
  if (n >= numLEDs)
    return 0; // Out of bounds, return no color.

  uint8_t *p;

  if (wOffset == rOffset) { // Is RGB-type device
    p = &pixels[n * 3];
    if (brightness) {
      // Stored color was decimated by setBrightness().  Returned value
      // attempts to scale back to an approximation of the original 24-bit
      // value used when setting the pixel color, but there will always be
      // some error -- those bits are simply gone.  Issue is most
      // pronounced at low brightness levels.
      return (((uint32_t)(p[rOffset] << 8) / brightness) << 16) |
             (((uint32_t)(p[gOffset] << 8) / brightness) << 8) |
             ((uint32_t)(p[bOffset] << 8) / brightness);
    } else {
      // No brightness adjustment has been made -- return 'raw' color
      return ((uint32_t)p[rOffset] << 16) | ((uint32_t)p[gOffset] << 8) |
             (uint32_t)p[bOffset];
    }
  } else { // Is RGBW-type device
    p = &pixels[n * 4];
    if (brightness) { // Return scaled color
      return (((uint32_t)(p[wOffset] << 8) / brightness) << 24) |
             (((uint32_t)(p[rOffset] << 8) / brightness) << 16) |
             (((uint32_t)(p[gOffset] << 8) / brightness) << 8) |
             ((uint32_t)(p[bOffset] << 8) / brightness);
    } else { // Return raw color
      return ((uint32_t)p[wOffset] << 24) | ((uint32_t)p[rOffset] << 16) |
             ((uint32_t)p[gOffset] << 8) | (uint32_t)p[bOffset];
    }
  }
}

// Returns pointer to pixels[] array.  Pixel data is stored in device-
// native format and is not translated here.  Application will need to be
// aware of specific pixel data format and handle colors appropriately.
uint8_t *MyNeoPixel::getPixels(void) const { return pixels; }

uint16_t MyNeoPixel::numPixels(void) const { return numLEDs; }

void MyNeoPixel::clear() {
    // Clear local pixel buffer
    for (int b=0; b<numBytes; ++b) pixels[b] = 0; //replaced memset()

    // Now clear the pixels on the seesaw
    uint8_t writeBuf[numBytes+2];
    for (int b=0; b<numBytes+2; ++b) writeBuf[b] = 0; //replaced memset()
    //writeBuf[0] = 0; // NOT NEEDED; offset for the entire buffer is 0, because
    //writeBuf[1] = 0; // we start from the beginning and set all pixels to zero

    wrapTrellis->write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF, writeBuf, numBytes+2);
}

void MyNeoPixel::setBrightness(uint8_t b) { brightness = b; }