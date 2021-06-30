// Conditional compilation of driver
#ifdef USE_P0_READER

// Define driver ID
#define XSNS_88  88

#define P0_READER_PERIOD     50         // In 100ms loops

#define P0_READER_BAUDRATE   300
#define P0_RX_BUF_SIZE       230
#define P0_MESSAGE_STARTED   0x01
#define P0_MESSAGE_RECEIVED  0x02

#define P0_REQUEST_MSG_SIZE  5

#include <TasmotaSerial.h>

struct {
  uint8_t status = 0;
  uint8_t period_count = 0;
  uint8_t new_byte = 0;
  uint16_t byte_counter = 0;
  char request_msg[P0_REQUEST_MSG_SIZE] = {0x2F, 0x3F, 0x21, 0x0D, 0x0A};    // "/?!\r\n"
  char *rx_buf = nullptr;
  TasmotaSerial *serial_handle = nullptr;
} P0;


/********************************************************************************************/

void P0_Reader_Init() {
  // Check if pins are set by user
  if (PinUsed(GPIO_P0READER_RX) && PinUsed(GPIO_P0READER_TX)) {

    // Create serial handle
    P0.serial_handle = new TasmotaSerial(Pin(GPIO_P0READER_RX), Pin(GPIO_P0READER_TX), 1);

    // Set baudrate and serial mode
    if (P0.serial_handle->begin(P0_READER_BAUDRATE, SERIAL_7E1)) {

      // Check if hardware serial possible
      if (P0.serial_handle->hardwareSerial()) {
        SetSerial(P0_READER_BAUDRATE, TS_SERIAL_7E1);
        ClaimSerial();
        P0.rx_buf = TasmotaGlobal.serial_in_buffer;  // Use idle serial buffer to save RAM
      } else {
        P0.rx_buf = (char*)(malloc(P0_RX_BUF_SIZE));
      }
    }
  }
}

void P0_Parse_Line(){

  AddLog(LOG_LEVEL_INFO, PSTR("P0: P0_Parse_Line"));
  AddLogBuffer(LOG_LEVEL_INFO, (uint8_t*) P0.rx_buf, P0.byte_counter);
}

void P0_Reader_Tick(){
  // Check for serial
  if (!P0.serial_handle) { return; }

  // Waiting for request message to be send
  if(!(P0.status & P0_MESSAGE_RECEIVED)){

    // Start receiving data
    while(P0.serial_handle->available()){
      yield();
      P0.new_byte = P0.serial_handle->read();

      // Wait for message start character
      if(!P0.status & P0_MESSAGE_STARTED){
        if(P0.new_byte == '/'){
          P0.status |= P0_MESSAGE_STARTED;
          P0.byte_counter = 0;

        }else{
          continue;
        }

      // Receiving message
      }else{
        P0.rx_buf[P0.byte_counter++] = P0.new_byte;

        // Wait for end of line characters
        if(P0.byte_counter >= 2){
          if(P0.rx_buf[P0.byte_counter - 2] == '\r' && P0.rx_buf[P0.byte_counter - 1] == '\n'){

            // Check for and remove end of message character
            if(P0.byte_counter >= 3 && P0.rx_buf[P0.byte_counter - 3] == '!'){
              P0.status |= P0_MESSAGE_RECEIVED;
              P0.byte_counter -= 3;
            }else{
              P0.byte_counter -= 2;
            }

            // Parse line
            P0_Parse_Line();
            P0.byte_counter = 0;
          }
        }
      }
    }
  }
  // Transmit
  if(P0.period_count <= 0){

      P0.serial_handle->flush();

      // Transmit requist message
      for(uint32_t i=0;i<P0_REQUEST_MSG_SIZE;i++){
          P0.serial_handle->write(P0.request_msg[i]);
      }

      P0.period_count = P0_READER_PERIOD;
      P0.status &= ~P0_MESSAGE_RECEIVED;
      P0.status &= ~P0_MESSAGE_STARTED;
      
  }else{
      P0.period_count--;
  }
}

#ifdef USE_WEBSERVER
void P0_Reader_Show(void) {
  if (!P0.rx_buf) { return; }
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
      P0_Reader_Tick();
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