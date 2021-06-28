// Conditional compilation of driver
#ifdef USE_P0_READER

// Define driver ID
#define XSNS_88  88

#define P0_READER_PERIOD     20         // In 100ms loops

#define P0_READER_BAUDRATE   300
#define P0_TIMEOUT_COUNT     10         // In 100ms loops
#define P0_RX_BUF_SIZE       230
#define P0_HAS_SENT          0x01

#define P0_REQUEST_MSG_SIZE  5

#include <TasmotaSerial.h>
TasmotaSerial *P0_Reader_Serial = nullptr;

struct {
  uint8_t status = 0;
  uint8_t timeout_count = 0;
  uint8_t period_count = 0;
  char request_msg[P0_REQUEST_MSG_SIZE] = {0x2F, 0x3F, 0x21, 0x0D, 0x0A};    // "/?!\r\n"
  char rx_buf[P0_RX_BUF_SIZE];
} P0;




/********************************************************************************************/

void P0_Reader_Init() {
  if (PinUsed(GPIO_P0READER_RX) && PinUsed(GPIO_P0READER_TX)) {
    P0_Reader_Serial = new TasmotaSerial(Pin(GPIO_P0READER_RX), Pin(GPIO_P0READER_TX), 1, 0, 256);
    if (P0_Reader_Serial->begin(P0_READER_BAUDRATE, SERIAL_7E1)) {
      if (P0_Reader_Serial->hardwareSerial()) {
        SetSerial(P0_READER_BAUDRATE, TS_SERIAL_7E1);
        ClaimSerial();
      }
    }
  }

  AddLog(LOG_LEVEL_INFO, PSTR("P0: Init"));
}

void P0_Reader_Read(){
    if (!P0_Reader_Serial) { return; }

    // Waiting for full message or timeout
    if(P0.status & P0_HAS_SENT){

        // Full message recieved
        if(P0_Reader_Serial->available() >= 228){
            P0.status &= ~P0_HAS_SENT;

            P0.timeout_count = P0_TIMEOUT_COUNT;
            P0_Reader_Serial->read(P0.rx_buf, P0_RX_BUF_SIZE);

            AddLog(LOG_LEVEL_INFO, PSTR("P0: Full message recieved"));

            //TODO parse msg

        // Timeout
        }else if(P0.timeout_count <= 0){
            P0_Reader_Serial->flush();
            P0.status &= ~P0_HAS_SENT;

            AddLog(LOG_LEVEL_INFO, PSTR("P0: Timeout"));
        
        // No full message, no timeout
        }else{
            P0.timeout_count--;
        }
    }

    // Transmit
    if(P0.period_count <= 0){
        AddLog(LOG_LEVEL_INFO, PSTR("P0: Transmitting"));

        for(uint32_t i=0;i<P0_REQUEST_MSG_SIZE;i++){
            P0_Reader_Serial->write(P0.request_msg[i]);
        }

        P0.period_count = P0_READER_PERIOD;
        P0.status |= P0_HAS_SENT;

    }else{
        P0.period_count--;
    }
    
    
}

#ifdef USE_WEBSERVER
void P0_Reader_Show(void) {
  if (!P0_Reader_Serial) { return; }
  WSContentSend_PD(PSTR("{s}P0 Reader UID{m}%d {e}"), P0.period_count);
}
#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns88(byte function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      P0_Reader_Init();
      break;
    case FUNC_EVERY_100_MSECOND:
      P0_Reader_Read();
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      P0_Reader_Show();
      break;
#endif  // USE_WEBSERVER
  }
  return result;
}


#endif // USE_P0Reader