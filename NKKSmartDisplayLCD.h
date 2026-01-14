#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Print.h>

class NKKSmartDisplayLCD : public Print {
public:
    NKKSmartDisplayLCD(uint8_t width, uint8_t height, uint8_t lpPin, uint8_t flmPin);

    void begin(uint32_t spiHz = 1000000, uint8_t spiMode = SPI_MODE2);

    void startRefresh(uint16_t lpPeriodUs = 277);
    void stopRefresh();

    // Framebuffer / graphics
    void clearDisplay();
    void setPixel(uint8_t x, uint8_t y, bool on);
    void drawDiagonalTest();
    void drawBarsTest();
    void sendFrame(); // compatibility (no-op)

    // HD44780-ish API (clamp, no wrap)
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void createChar(uint8_t index, const uint8_t bitmap[8]); // 0..7
    size_t write(uint8_t b) override;

    // ---- Graphics primitives ----
    void drawLine(int x0, int y0, int x1, int y1, bool on = true);

    void drawCLineH(int cx, int cy, int halfLen, bool on = true); // horizontal centered line
    void drawCLineV(int cx, int cy, int halfLen, bool on = true); // vertical centered line

    void drawRect(int x, int y, int w, int h, bool on = true);
    void fillRect(int x, int y, int w, int h, bool on = true);

    void drawCircle(int cx, int cy, int r, bool on = true);  // center-based
    void fillCircle(int cx, int cy, int r, bool on = true);  // center-based

    void drawCross(int cx, int cy, int halfLen, bool on = true); // + from center

    static void isrTick();

private:
    void sendLine(uint8_t rowIndex, bool firstRow);
    void onTick();

    // Text internals
    void drawGlyphAtCursor(uint8_t glyphCode);
    bool decodeUtf8Byte(uint8_t b, uint32_t& outCp);
    uint8_t mapCodepointToGlyph(uint32_t cp) const;
    void getGlyphColumns(uint8_t glyphCode, uint8_t outCols[5]) const;

    // Helper: bounds-checked pixel set using int coords
    inline void setPixelSafe(int x, int y, bool on) {
        if ((unsigned)x < _width && (unsigned)y < _height) setPixel((uint8_t)x, (uint8_t)y, on);
    }

private:
    uint8_t _width, _height;
    uint8_t _lpPin, _flmPin;

    static const uint8_t BYTES_PER_ROW = 5;  // 40 bits per row (first 4 dummy)
    uint8_t _fb[24][BYTES_PER_ROW];

    SPISettings _spi;

    volatile uint8_t _currentRow = 0;
    volatile bool _refreshEnabled = false;

    static NKKSmartDisplayLCD* _instance;

    // 5x7 font rendered into 6x8 cells (1px spacing)
    static const uint8_t CHAR_W = 6;
    static const uint8_t CHAR_H = 8;

    uint8_t _cols = 0;
    uint8_t _rows = 0;
    uint8_t _cursorCol = 0;
    uint8_t _cursorRow = 0;

    // UTF-8 decode state
    uint8_t  _utf8Need = 0;
    uint32_t _utf8Acc  = 0;

    // HD44780 custom chars 0..7 stored as 5 columns (each bit = row)
    uint8_t _custom[8][5] = {{0}};
};
