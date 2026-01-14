#include "NKKSmartDisplayLCD.h"
#include "Font5x7.h"

NKKSmartDisplayLCD* NKKSmartDisplayLCD::_instance = nullptr;

NKKSmartDisplayLCD::NKKSmartDisplayLCD(uint8_t width, uint8_t height, uint8_t lpPin, uint8_t flmPin)
: _width(width), _height(height), _lpPin(lpPin), _flmPin(flmPin), _spi(1000000, MSBFIRST, SPI_MODE2)
{
    _cols = _width / CHAR_W;   // 36/6 = 6
    _rows = _height / CHAR_H;  // 24/8 = 3
}

void NKKSmartDisplayLCD::begin(uint32_t spiHz, uint8_t spiMode)
{
    pinMode(_lpPin, OUTPUT);
    pinMode(_flmPin, OUTPUT);
    digitalWrite(_lpPin, LOW);
    digitalWrite(_flmPin, LOW);

    SPI.begin();
    _spi = SPISettings(spiHz, MSBFIRST, spiMode);

    clearDisplay();
    home();
}

void NKKSmartDisplayLCD::startRefresh(uint16_t lpPeriodUs)
{
    _instance = this;
    _currentRow = 0;
    _refreshEnabled = true;

    SPI.beginTransaction(_spi);

    // Timer1 CTC on Mega2560:
    // 16MHz / 8 prescaler = 2MHz = 0.5us per tick
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    uint16_t ocr = (uint16_t)(lpPeriodUs * 2u - 1u);
    OCR1A = ocr;

    TCCR1B |= (1 << WGM12);  // CTC
    TCCR1B |= (1 << CS11);   // prescaler 8
    TIMSK1 |= (1 << OCIE1A); // enable compare interrupt
    interrupts();
}

void NKKSmartDisplayLCD::stopRefresh()
{
    noInterrupts();
    TIMSK1 &= ~(1 << OCIE1A);
    interrupts();

    _refreshEnabled = false;
    SPI.endTransaction();
}

void NKKSmartDisplayLCD::clearDisplay()
{
    for (uint8_t y = 0; y < 24; y++) {
        for (uint8_t b = 0; b < BYTES_PER_ROW; b++) {
            _fb[y][b] = 0x00;
        }
    }
}

void NKKSmartDisplayLCD::clear() { clearDisplay(); }

void NKKSmartDisplayLCD::home()
{
    _cursorCol = 0;
    _cursorRow = 0;
}

void NKKSmartDisplayLCD::setCursor(uint8_t col, uint8_t row)
{
    if (_cols == 0 || _rows == 0) return;
    if (col >= _cols) col = (uint8_t)(_cols - 1);
    if (row >= _rows) row = (uint8_t)(_rows - 1);
    _cursorCol = col;
    _cursorRow = row;
}

// --- Pixel mapping (X flip + 4 dummy bits offset) ---
void NKKSmartDisplayLCD::setPixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= _width || y >= _height) return;

    // Flip X so (0,0) is top-left in our coordinate system.
    x = (uint8_t)((_width - 1) - x);

    // Shift pixel data by +4 bits (first 4 bits are dummy)
    uint16_t bitPos = (uint16_t)x + 4;
    uint8_t byteIndex = (uint8_t)(bitPos >> 3);
    uint8_t bitInByte = (uint8_t)(7 - (bitPos & 0x07));
    uint8_t mask = (uint8_t)(1 << bitInByte);

    if (on) _fb[y][byteIndex] |= mask;
    else    _fb[y][byteIndex] &= (uint8_t)~mask;
}

// --- Test patterns ---
void NKKSmartDisplayLCD::drawDiagonalTest()
{
    clearDisplay();
    uint8_t n = (_width < _height) ? _width : _height;
    for (uint8_t i = 0; i < n; i++) setPixel(i, i, true);
}

void NKKSmartDisplayLCD::drawBarsTest()
{
    clearDisplay();
    for (uint8_t y = 0; y < _height; y++) {
        for (uint8_t x = 0; x < _width; x++) {
            bool on = ((x / 2) % 2) == 0;
            setPixel(x, y, on);
        }
    }
}

// --- Custom chars 0..7 (HD44780 style) ---
void NKKSmartDisplayLCD::createChar(uint8_t index, const uint8_t bitmap[8])
{
    if (index > 7) return;

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t colBits = 0;
        for (uint8_t row = 0; row < 8; row++) {
            if (bitmap[row] & (1 << (4 - col))) colBits |= (1 << row);
        }
        _custom[index][col] = colBits;
    }
}

// --- Print support (clamp, no wrap) ---
size_t NKKSmartDisplayLCD::write(uint8_t b)
{
    if (b == '\r') return 1;

    if (b == '\n') {
        if (_cursorRow + 1 < _rows) _cursorRow++;
        _cursorCol = 0;
        return 1;
    }

    uint32_t cp = 0;
    if (!decodeUtf8Byte(b, cp)) return 1;

    uint8_t glyph = mapCodepointToGlyph(cp);
    drawGlyphAtCursor(glyph);
    return 1;
}

bool NKKSmartDisplayLCD::decodeUtf8Byte(uint8_t b, uint32_t& outCp)
{
    if (_utf8Need == 0) {
        if ((b & 0x80) == 0) { outCp = b; return true; }
        if ((b & 0xE0) == 0xC0) { _utf8Need = 1; _utf8Acc = (uint32_t)(b & 0x1F); return false; }
        outCp = '?';
        return true;
    }

    if ((b & 0xC0) != 0x80) { _utf8Need = 0; outCp = '?'; return true; }

    _utf8Acc = (_utf8Acc << 6) | (uint32_t)(b & 0x3F);
    _utf8Need--;

    if (_utf8Need == 0) { outCp = _utf8Acc; return true; }
    return false;
}

uint8_t NKKSmartDisplayLCD::mapCodepointToGlyph(uint32_t cp) const
{
    if (cp >= 0x20 && cp <= 0x7E) return (uint8_t)cp;

    switch (cp) {
        case 0x00C4: return 0x80; // Ä
        case 0x00D6: return 0x81; // Ö
        case 0x00DC: return 0x82; // Ü
        case 0x00E4: return 0x83; // ä
        case 0x00F6: return 0x84; // ö
        case 0x00FC: return 0x85; // ü
        case 0x00DF: return 0x86; // ß
        default:     return '?';
    }
}

void NKKSmartDisplayLCD::getGlyphColumns(uint8_t glyphCode, uint8_t outCols[5]) const
{
    if (glyphCode <= 7) {
        for (uint8_t i = 0; i < 5; i++) outCols[i] = _custom[glyphCode][i];
        return;
    }

    if (glyphCode >= 0x80 && glyphCode <= 0x86) {
        static const uint8_t ext[][5] PROGMEM = {
            {0x7E,0x11,0x11,0x11,0x7E}, // Ä base
            {0x3E,0x41,0x41,0x41,0x3E}, // Ö base
            {0x3F,0x40,0x40,0x40,0x3F}, // Ü base
            {0x20,0x54,0x54,0x54,0x78}, // ä base
            {0x38,0x44,0x44,0x44,0x38}, // ö base
            {0x3C,0x40,0x40,0x20,0x7C}, // ü base
            {0x7F,0x09,0x49,0x49,0x36}, // ß approx
        };
        uint8_t idx = (uint8_t)(glyphCode - 0x80);
        for (uint8_t i = 0; i < 5; i++) outCols[i] = pgm_read_byte(&ext[idx][i]);

        if (glyphCode != 0x86) { outCols[1] |= 0x01; outCols[3] |= 0x01; }
        return;
    }

    if (glyphCode < 0x20 || glyphCode > 0x7E) glyphCode = '?';
    uint16_t off = (uint16_t)(glyphCode - 0x20) * 5;
    for (uint8_t i = 0; i < 5; i++) outCols[i] = pgm_read_byte(&Font5x7[off + i]);
}

void NKKSmartDisplayLCD::drawGlyphAtCursor(uint8_t glyphCode)
{
    if (_cursorRow >= _rows) return;
    if (_cursorCol >= _cols) return;

    int x0 = (int)_cursorCol * CHAR_W;
    int y0 = (int)_cursorRow * CHAR_H;

    uint8_t cols[5];
    getGlyphColumns(glyphCode, cols);

    for (uint8_t x = 0; x < 5; x++) {
        uint8_t bits = cols[x];
        for (uint8_t y = 0; y < 7; y++) {
            bool on = (bits >> y) & 0x01;
            setPixelSafe(x0 + x, y0 + y, on);
        }
    }

    for (uint8_t y = 0; y < 7; y++) setPixelSafe(x0 + 5, y0 + y, false);

    if (_cursorCol + 1 < _cols) _cursorCol++;
    else _cursorCol = _cols; // clamp
}

// ---------------- Graphics primitives ----------------

void NKKSmartDisplayLCD::drawLine(int x0, int y0, int x1, int y1, bool on)
{
    // Bresenham
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixelSafe(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void NKKSmartDisplayLCD::drawCLineH(int cx, int cy, int halfLen, bool on)
{
    drawLine(cx - halfLen, cy, cx + halfLen, cy, on);
}

void NKKSmartDisplayLCD::drawCLineV(int cx, int cy, int halfLen, bool on)
{
    drawLine(cx, cy - halfLen, cx, cy + halfLen, on);
}

void NKKSmartDisplayLCD::drawRect(int x, int y, int w, int h, bool on)
{
    if (w <= 0 || h <= 0) return;
    drawLine(x, y, x + w - 1, y, on);
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, on);
    drawLine(x, y, x, y + h - 1, on);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, on);
}

void NKKSmartDisplayLCD::fillRect(int x, int y, int w, int h, bool on)
{
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            setPixelSafe(xx, yy, on);
        }
    }
}

void NKKSmartDisplayLCD::drawCircle(int cx, int cy, int r, bool on)
{
    if (r < 0) return;
    int x = r;
    int y = 0;
    int err = 1 - x;

    while (x >= y) {
        setPixelSafe(cx + x, cy + y, on);
        setPixelSafe(cx + y, cy + x, on);
        setPixelSafe(cx - y, cy + x, on);
        setPixelSafe(cx - x, cy + y, on);
        setPixelSafe(cx - x, cy - y, on);
        setPixelSafe(cx - y, cy - x, on);
        setPixelSafe(cx + y, cy - x, on);
        setPixelSafe(cx + x, cy - y, on);

        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void NKKSmartDisplayLCD::fillCircle(int cx, int cy, int r, bool on)
{
    if (r < 0) return;

    // Draw horizontal spans
    for (int y = -r; y <= r; y++) {
        int xSpan = (int)(sqrt((double)r * r - (double)y * y) + 0.5);
        drawLine(cx - xSpan, cy + y, cx + xSpan, cy + y, on);
    }
}

void NKKSmartDisplayLCD::drawCross(int cx, int cy, int halfLen, bool on)
{
    drawCLineH(cx, cy, halfLen, on);
    drawCLineV(cx, cy, halfLen, on);
}

// --- Refresh engine ---
void NKKSmartDisplayLCD::sendLine(uint8_t rowIndex, bool firstRow)
{
    if (firstRow) digitalWrite(_flmPin, HIGH);

    // Force dummy bits to 0 (top 4 bits of first byte)
    _fb[rowIndex][0] &= 0x0F;

    for (uint8_t i = 0; i < BYTES_PER_ROW; i++) {
        SPI.transfer(_fb[rowIndex][i]);
    }

    digitalWrite(_lpPin, HIGH);
    delayMicroseconds(1);
    digitalWrite(_lpPin, LOW);

    if (firstRow) digitalWrite(_flmPin, LOW);
}

void NKKSmartDisplayLCD::sendFrame()
{
    // interrupt-driven refresh; intentionally empty
}

void NKKSmartDisplayLCD::isrTick()
{
    if (_instance) _instance->onTick();
}

void NKKSmartDisplayLCD::onTick()
{
    if (!_refreshEnabled) return;

    bool first = (_currentRow == 0);
    sendLine(_currentRow, first);

    _currentRow++;
    if (_currentRow >= _height) _currentRow = 0;
}

ISR(TIMER1_COMPA_vect)
{
    NKKSmartDisplayLCD::isrTick();
}
