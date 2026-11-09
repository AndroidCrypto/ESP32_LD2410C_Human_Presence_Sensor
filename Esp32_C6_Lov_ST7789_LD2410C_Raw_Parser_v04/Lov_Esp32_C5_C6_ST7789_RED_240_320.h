
#include <LovyanGFX.hpp>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 320
#define DISPLAY_ORIENTATION_TOP 2   // connector pins at top
#define DISPLAY_ORIENTATION_LEFT 3  // connector pins at left side

// color definitions
const uint16_t COLOR_BLACK = TFT_BLACK;
const uint16_t COLOR_WHITE = TFT_WHITE;
const uint16_t COLOR_RED = TFT_RED;
const uint16_t COLOR_GREEN = TFT_GREEN;
//const uint16_t COLOR_GREEN = TFT_LIGHTGREEN;
const uint16_t COLOR_BLUE = TFT_BLUE;
const uint16_t COLOR_CYAN = TFT_CYAN;
const uint16_t COLOR_ORANGE = TFT_ORANGE;
const uint16_t COLOR_MAGENTA = TFT_MAGENTA;
const uint16_t COLOR_SKYBLUE = 0x5DFF;
//const uint16_t COLOR_ = RGB565_;

/*
  Define these pins for the LovyanGFX library, e.g. in main header file before including this file
  #define TFT_CS    -1 // for displays without CS pin
  #define TFT_DC    10   // Data/Command
  #define TFT_RST    9   // Reset (kann auch auf -1 gesetzt werden, wenn an 3.3V)
  #define TFT_SCLK   7   // SPI Clock (SCK / SCLK)
  #define TFT_MOSI   8   // SPI Master Out Slave In (MOSI / SDIN)
  #define TFT_BL    12   // Backlight LED Pin (optional)
*/

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;  // SPI
  lgfx::Light_PWM _light_instance;
  // touch instance
  lgfx::Touch_XPT2046 _touch_instance;

public:

  LGFX(void) {
    {  // Configure the bus control settings.
      // バス制御の設定を行います。
      auto cfg = _bus_instance.config();  // Get the bus configuration structure. バス設定用の構造体を取得します。

      // Configuring the SPI bus SPIバスの設定
      // Select the SPI to use ESP32-S2,C3: SPI2_HOST or SPI3_HOST / ESP32: VSPI_HOST or HSPI_HOST
      cfg.spi_host = SPI2_HOST;
      //cfg.spi_host = VSPI_HOST;     // 使用するSPIを選択  ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      //cfg.spi_host = HSPI_HOST;     // 使用するSPIを選択  ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      // * Due to the ESP-IDF version upgrade, the VSPI_HOST and HSPI_HOST descriptions are deprecated, so if an error occurs, please use SPI2_HOST and SPI3_HOST instead.
      // ※ ESP-IDFバージョンアップに伴い、VSPI_HOST , HSPI_HOSTの記述は非推奨になるため、エラーが出る場合は代わりにSPI2_HOST , SPI3_HOSTを使用してください。
      cfg.spi_mode = 3;  // Sets the SPI communication mode (0 to 3) SPI通信モードを設定 (0 ~ 3)
      // SPI clock during transmission (Maximum 80MHz, rounded to an integer value of 80MHz)
      //cfg.freq_write = 40000000;  // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
      //cfg.freq_write = 80000000;    // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
      cfg.freq_write = 80000000;
      //cfg.freq_write = 55000000;    // 送信時のSPIクロック (最大80MHz, 80MHzを整数で割った値に丸められます)
      cfg.freq_read = 16000000;  // SPI clock when receiving 受信時のSPIクロック
      cfg.spi_3wire = true;      // Set to true if receiving is done on the MOSI pin 受信をMOSIピンで行う場合はtrueを設定
      cfg.use_lock = false;      // Set to true if you want to use transaction locking. トランザクションロックを使用する場合はtrueを設定
      // Set the DMA channel to use (0 = no DMA / 1 = 1ch / 2 = ch / SPI_DMA_CH_AUTO = automatic setting)
      cfg.dma_channel = SPI_DMA_CH_AUTO;  // 使用するDMAチャンネルを設定 (0=DMA不使用 / 1=1ch / 2=ch / SPI_DMA_CH_AUTO=自動設定)
      // * With the ESP-IDF version upgrade, SPI_DMA_CH_AUTO (automatic setting) is now recommended for the DMA channel. Specifying 1ch or 2ch is no longer recommended.
      // ※ ESP-IDFバージョンアップに伴い、DMAチャンネルはSPI_DMA_CH_AUTO(自動設定)が推奨になりました。1ch,2chの指定は非推奨になります。
      cfg.pin_sclk = TFT_SCLK;    // set SPI SCLK pin number SCK
      cfg.pin_mosi = TFT_MOSI;    // set MOSI pin number of SPI SDI
      cfg.pin_miso = -1;          // set SPI's MISO pin number (-1 = disable) SDO // NOTE: this is not physically connected to the display pin
      cfg.pin_dc = TFT_DC;        // set SPI D/C pin number (-1 = disable) RS
      // If you use the same SPI bus as the SD card, be sure to set MISO without omitting it.
      // SDカードと共通のSPIバスを使う場合、MISOは省略せず必ず設定してください。

      _bus_instance.config(cfg);               // The setting value is reflected in the bus. 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance);  // Place the bus on the panel. バスをパネルにセットします。
    }

    {  // Configure the display panel control settings.
      // 表示パネル制御の設定を行います。
      auto cfg = _panel_instance.config();  // Gets the structure for display panel settings. 表示パネル設定用の構造体を取得します。

      cfg.pin_cs = TFT_CS;    // pin number to which CS is connected (-1 = disable)
      cfg.pin_rst = TFT_RST;  // pin number to which RST is connected (-1 = disable)
      cfg.pin_busy = -1;      // pin number to which BUSY is connected (-1 = disable)

      // * The following settings are set to general default values ​​for each panel, so please try commenting out any items you are unsure about.
      // ※ 以下の設定値はパネル毎に一般的な初期値が設定されていますので、不明な項目はコメントアウトして試してみてください。
      cfg.panel_width = DISPLAY_WIDTH;    // actual displayable width
      cfg.panel_height = DISPLAY_HEIGHT;  // actual displayable height
      cfg.offset_x = 0;                   // Panel offset in the X direction パネルのX方向オフセット量
      cfg.offset_y = 0;                   // Panel offset in the Y direction パネルのY方向オフセット量
      cfg.offset_rotation = 0;            // Rotation direction offset 0~7 (4~7 are upside down) 回転方向の値のオフセット 0~7 (4~7は上下反転)
      cfg.dummy_read_pixel = 16;          // Number of dummy read bits before pixel readout ピクセル読出し前のダミーリードのビット数
      cfg.dummy_read_bits = 1;            // Number of dummy read bits before reading non-pixel data ピクセル以外のデータ読出し前のダミーリードのビット数
      cfg.readable = true;                // Set to true if data can be read データ読出しが可能な場合 trueに設定
      cfg.invert = false;                 // Set to true if the panel's brightness is reversed パネルの明暗が反転してしまう場合 trueに設定
      cfg.rgb_order = false;              // Set to true if the red and blue of the panel are swapped. パネルの赤と青が入れ替わってしまう場合 trueに設定
      cfg.dlen_16bit = false;             // Set to true for panels that transmit data in 16-bit units via 16-bit parallel or SPI. 16bitパラレルやSPIでデータ長を16bit単位で送信するパネルの場合 trueに設定
      // If the bus is shared with the SD card, set it to true (bus control is performed using drawJpgFile, etc.)
      cfg.bus_shared = false;  // SDカードとバスを共有している場合 trueに設定(drawJpgFile等でバス制御を行います)

      // The following settings should only be used if the display is misaligned when using a driver with a variable pixel count, such as ST7735 or ILI9163.
      // 以下はST7735やILI9163のようにピクセル数が可変のドライバで表示がずれる場合にのみ設定してください。
      //  cfg.memory_width     =   135;  // Maximum width supported by the driver IC ドライバICがサポートしている最大の幅
      //  cfg.memory_height    =   240;  // Maximum height supported by the driver IC ドライバICがサポートしている最大の高さ

      _panel_instance.config(cfg);
    }

    {  // Set the backlight control (delete if not required).
      // バックライト制御の設定を行います。（必要なければ削除）
      auto cfg = _light_instance.config();  // Gets the backlight settings structure. バックライト設定用の構造体を取得します。

      cfg.pin_bl = TFT_BL;  // The pin number to which the backlight is connected バックライトが接続されているピン番号
      cfg.invert = false;   // true if the backlight brightness is inverted バックライトの輝度を反転させる場合 true
      cfg.freq = 44100;     // Backlight PWM frequency バックライトのPWM周波数
      cfg.pwm_channel = 7;  // PWM channel number to use 使用するPWMのチャンネル番号

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);  // Place the backlight on the panel. バックライトをパネルにセットします。
    }

    setPanel(&_panel_instance);  // Set the panel to be used. 使用するパネルをセットします。
  }
};

LGFX tft;

void fillScreen(uint16_t c) {
  tft.fillScreen(c);
}

void setTextSize(uint8_t size) {
  tft.setTextSize(size);
}

void setTextColor(uint16_t c) {
  tft.setTextColor(c);
}

void setTextColor(uint16_t c, uint16_t bg) {
  tft.setTextColor(c, bg);
}

void setCursor(uint16_t x, uint16_t y) {
  tft.setCursor(x, y);
}

void println(const char* text) {
  tft.println(text);
}

void print(const char* text) {
  tft.print(text);
}

void print(float f) {
  tft.print(f);
}

/*
// This is not available with LovyanGFX
void getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  // Arduino_GFX.h void getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
  tft.getTextBounds(string, x, y, x1, y1, w, h);
}
*/

void setTextWrap(bool wrap) {
  tft.setTextWrap(wrap);
}

void setOrientation(uint8_t orientation) {
  tft.setRotation(orientation);
}

void setBrightness(uint8_t brightness) {
  analogWrite(TFT_BL, brightness);
}

bool initDisplay() {

  /*
#if defined(TFT_BL) && (TFT_BL >= 0)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
*/

  bool suc = tft.init();
  if (!suc) return false;
  fillScreen(COLOR_BLACK);
  return true;
}
