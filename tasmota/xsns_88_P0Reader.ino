// Conditional compilation of driver
#ifdef USE_P0_READER

// Define driver ID
#define XSNS_88  88

#define P0_READER_PERIOD     20         // In 250ms loops

#define P0_READER_BAUDRATE   300
#define P0_RX_BUF_SIZE       230
#define P0_MESSAGE_STARTED   0x01
#define P0_MESSAGE_RECEIVED  0x02

#define P0_REQUEST_MSG_SIZE  5
#define P0_METER_NAME_SIZE   25

#include <TasmotaSerial.h>

struct {
  uint8_t status = 0;
  uint8_t period_count = 0;
  uint8_t line_count = 0;
  uint8_t new_byte = 0;
  uint16_t byte_counter = 0;
  char request_msg[P0_REQUEST_MSG_SIZE] = {0x2F, 0x3F, 0x21, 0x0D, 0x0A};    // "/?!\r\n"
  char *rx_buf = nullptr;
  TasmotaSerial *serial_handle = nullptr;
} P0;

struct {
  char Meter_Name[P0_METER_NAME_SIZE] = "Meter naam";
  uint32_t Serial_Number = 0;
  uint32_t Device_Adress = 0;
  long Total_Pos_Energy = 0;
  long T1_Pos_Energy = 0;
  long T2_Pos_Energy = 0;
  long Total_Neg_Energy = 0;
  long T1_Neg_Energy = 0;
  long T2_Neg_Energy = 0;
  uint32_t Fatal_Error = 0;
  uint8_t has_data = 0;
} P0_Data;

/* Subset of OBIS (or EDIS) codes from IEC 62056 provided by the ISKRA ME-162-0012.
 * [A-B:]C.D.E[*F] where the ISKRA ME-162 does not do [A-B:] medium addressing. */

enum Obis {
  OBIS_C_1_0 = 0, // Meter serial number
  OBIS_0_0_0,     // Device address
  OBIS_1_8_0,     // Positive active energy (A+) total [Wh]
  OBIS_1_8_1,     // Positive active energy (A+) in tariff T1 [Wh]
  OBIS_1_8_2,     // Positive active energy (A+) in tariff T2 [Wh]
  OBIS_2_8_0,     // Negative active energy (A+) total [Wh]
  OBIS_2_8_1,     // Negative active energy (A+) in tariff T1 [Wh]
  OBIS_2_8_2,     // Negative active energy (A+) in tariff T2 [Wh]
  OBIS_F_F_0,     // Fatal error meter status
  OBIS_LAST
};

// typedef struct { char pgm_str[6]; } obis_pgm_t;
const char Obis_[OBIS_LAST + 1][6] = {
  "C.1.0",
  "0.0.0",
  "1.8.0",
  "1.8.1",
  "1.8.2",
  "2.8.0",
  "2.8.1",
  "2.8.2",
  "F.F",
  "UNDEF",
};


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

  //Lines will look like:

  // /ISK5ME162-0012<\r><\n>
  // <2>C.1.0(00000000)<\r><\n>
  // 0.0.0(00000000)<\r><\n>
  // 1.8.0(0000000.000*kWh)<\r><\n>
  // 1.8.1(0000000.000*kWh)<\r><\n>
  // 1.8.2(0000000.000*kWh)<\r><\n>
  // 2.8.0(0000000.000*kWh)<\r><\n>
  // 2.8.1(0000000.000*kWh)<\r><\n>
  // 2.8.2(0000000.000*kWh)<\r><\n>
  // F.F(0000000)<\r><\n>
  // !<\r><\n>
  // <3>K

  // AddLog(LOG_LEVEL_INFO, PSTR("P0: P0_Parse_Line"));
  // AddLogBuffer(LOG_LEVEL_INFO, (uint8_t*) P0.rx_buf, P0.byte_counter);

  // String date = GetBuildDateAndTime();
  // AddLog(LOG_LEVEL_INFO, PSTR("P0: P0_Parse_Line %s"), date.c_str());


  // Init OBIS_Code and value fields
  char *c_OBIS_Code = P0.rx_buf;
  uint8_t OBIS_Code_len = 0;
  Obis OBIS_Code = OBIS_LAST;
  char *c_value = P0.rx_buf;
  uint8_t value_len = 0;
  long value = 0;

  // Find end of OBIS_Code and start/length of value
  while(*c_value != '(' && c_value - P0.rx_buf < P0.byte_counter) { c_value++; }
  OBIS_Code_len = c_value++ - P0.rx_buf;

  // If value field found
  if(OBIS_Code_len != P0.byte_counter){
    value_len = P0.byte_counter - OBIS_Code_len - 2; // -2 to omit '(' & ')' 
  }else{
    c_value =  P0.rx_buf;
    value_len = P0.byte_counter;
  }
  
  // AddLogBuffer(LOG_LEVEL_INFO, (uint8_t*) c_OBIS_Code, OBIS_Code_len);
  // AddLogBuffer(LOG_LEVEL_INFO, (uint8_t*) Obis_[0], OBIS_Code_len);

  // Parse OBIS code
  if(OBIS_Code_len){
    for (int i = 0; i < OBIS_LAST; i++) {
      if (memcmp(c_OBIS_Code, Obis_[i], OBIS_Code_len) == 0){
        OBIS_Code = (Obis)i;
        break;
      }
    }
  }

  // AddLog(LOG_LEVEL_INFO, PSTR("P0: OBIS %d"), (uint8_t) OBIS_Code);

  // First line is meter name without OBIS code
  if(P0.line_count == 0 && OBIS_Code == OBIS_LAST){
    memcpy(P0_Data.Meter_Name, c_value, value_len);
    P0_Data.has_data = 1;
    Energy.data_valid[0] = 0;
    return;
  }

  // Parse value, if in kWh parse to Wh 
  value = atol(c_value);
  if (value_len == 15 && c_value[7] == '.' && memcmp_P(c_value + 11, F("*kWh"), 4) == 0) {
    value = value * 1000 + atol(c_value + 8);
  }else{
    return;
  }
  
  // Save in data struct
  switch (OBIS_Code) {
    case OBIS_C_1_0: P0_Data.Serial_Number = value; break;
    case OBIS_0_0_0: P0_Data.Device_Adress = value; break;
    case OBIS_1_8_0: P0_Data.Total_Pos_Energy = value; break;
    case OBIS_1_8_1: P0_Data.T1_Pos_Energy = value; break;
    case OBIS_1_8_2: P0_Data.T2_Pos_Energy = value; break;
    case OBIS_2_8_0: P0_Data.Total_Neg_Energy = value; break;
    case OBIS_2_8_1: P0_Data.T1_Neg_Energy = value; break;
    case OBIS_2_8_2: P0_Data.T2_Neg_Energy = value; break;
    case OBIS_F_F_0: P0_Data.Fatal_Error = (uint32_t) value; break;
    default: break;
  }
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

      // Extend period timout
      if(P0.period_count <= 2){
        P0.period_count = 5;
      }

      // Wait for message start character
      if(!P0.status & P0_MESSAGE_STARTED){
        if(P0.new_byte == '/'){
          P0.status |= P0_MESSAGE_STARTED;
          P0.byte_counter = 0;
          P0.line_count = 0;

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
            P0.line_count++;
          }
        }
      }
    }
  }
  // Transmit
  if(P0.period_count <= 0){

      // AddLog(LOG_LEVEL_INFO, PSTR("P0: New period"));

      P0.serial_handle->flush();

      // Transmit requist message
      for(uint32_t i=0;i<P0_REQUEST_MSG_SIZE;i++){
          P0.serial_handle->write(P0.request_msg[i]);
      }

      // Reset period counter and status flags
      P0.period_count = P0_READER_PERIOD;
      P0.status &= ~P0_MESSAGE_RECEIVED;
      P0.status &= ~P0_MESSAGE_STARTED;
      
  }else{
      // Count down period counter
      P0.period_count--;
  }
}

#ifdef USE_WEBSERVER

// TODO move to language files
const char HTTP_Empy_Line[]            PROGMEM = "{s}&nbsp;{m}&nbsp;{e}";
const char HTTP_Meter_ID[]            PROGMEM = "{s}Meter ID:  {m}%s{e}";
// const char HTTP_Serial_Num[]          PROGMEM = "{s}Serial number{m}%d{e}";
// const char HTTP_Device_Adrs[]         PROGMEM = "{s}Device adress{m}%d{e}";

const char HTTP_Total_Pos_Energy[]    PROGMEM = "{s}Total consumption:{m}%d.%03d kWh{e}";
const char HTTP_T1_Pos_Energy[]       PROGMEM = "{s}Consumption tariff 1:{m}%d.%03d kWh{e}";
const char HTTP_T2_Pos_Energy[]       PROGMEM = "{s}Consumption tariff 2:{m}%d.%03d kWh{e}";
const char HTTP_Total_Neg_Energy[]    PROGMEM = "{s}Total production:{m}%d.%03d kWh{e}";
const char HTTP_T1_Neg_Energy[]       PROGMEM = "{s}Production tariff 1:{m}%d.%03d kWh{e}";
const char HTTP_T2_Neg_Energy[]       PROGMEM = "{s}Production tariff 2:{m}%d.%03d kWh{e}";

// const char HTTP_Daily_Title[]         PROGMEM = "{s}Daily{m}";
// const char HTTP_Total_Pos_Today[]     PROGMEM = "{s}Total today:{m}%d.%03d kWh{e}";
// const char HTTP_T1_Pos_Today[]        PROGMEM = "{s}Tariff 1 today:{m}%d.%03d kWh{e}";
// const char HTTP_T2_Pos_Today[]        PROGMEM = "{s}Tariff 2 today:{m}%d.%03d kWh{e}";
// const char HTTP_Total_Pos_Yesterday[] PROGMEM = "{s}Total yesterday:{m}%d.%03d kWh{e}";
// const char HTTP_T1_Pos_Yesterday[]    PROGMEM = "{s}Tariff 1 yesterday:{m}%d.%03d kWh{e}";
// const char HTTP_T2_Pos_Yesterday[]    PROGMEM = "{s}Tariff 2 yesterday:{m}%d.%03d kWh{e}";

// const char HTTP_Montly_Title[]        PROGMEM = "{s}Monthly{m}";
// const char HTTP_Total_Pos_Month[]     PROGMEM = "{s}Total this month:{m}%d.%03d kWh{e}";
// const char HTTP_T1_Pos_Month[]        PROGMEM = "{s}Tariff 1 this month:{m}%d.%03d kWh{e}";
// const char HTTP_T2_Pos_Month[]        PROGMEM = "{s}Tariff 2 this month:{m}%d.%03d kWh{e}";
// const char HTTP_Total_Pos_PrevMonth[] PROGMEM = "{s}Total prev month:{m}%d.%03d kWh{e}";
// const char HTTP_T1_Pos_PrevMonth[]    PROGMEM = "{s}Tariff 1 prev month:{m}%d.%03d kWh{e}";
// const char HTTP_T2_Pos_PrevMonth[]    PROGMEM = "{s}Tariff 2 prev month:{m}%d.%03d kWh{e}";

const char HTTP_Fatel_Error[]         PROGMEM = "{s}Fatal error:{m}%d{e}";

#endif  // USE_WEBSERVER

void P0_Reader_Show(bool json) {
  if (!P0.serial_handle || !P0_Data.has_data)  return;

  if(json){
    //Append all P0Reader data to Json string
    ResponseAppend_P(PSTR(",\"P0Reader\":{\"MID\":\"%s\",\"TC\":%d,\"CT1\":%d,\"CT2\":%d,\"TP\":%d,\"PT1\":%d,\"PT2\":%d}"),
                     P0_Data.Meter_Name,
                     P0_Data.Total_Pos_Energy,
                     P0_Data.T1_Pos_Energy,
                     P0_Data.T2_Pos_Energy,
                     P0_Data.Total_Neg_Energy,
                     P0_Data.T1_Neg_Energy,
                     P0_Data.T2_Neg_Energy);
  }else{

#ifdef USE_WEBSERVER

    // Print all P0Reader data to web interface
    WSContentSend_PD(HTTP_Meter_ID, P0_Data.Meter_Name);

    // if(P0_Data.Serial_Number) WSContentSend_PD(HTTP_Serial_Num, P0_Data.Serial_Number);
    // if(P0_Data.Device_Adress) WSContentSend_PD(HTTP_Device_Adrs, P0_Data.Device_Adress);
    WSContentSend_PD(HTTP_Empy_Line);
    WSContentSend_PD(HTTP_Total_Pos_Energy, P0_Data.Total_Pos_Energy/1000, P0_Data.Total_Pos_Energy%1000);
    WSContentSend_PD(HTTP_T1_Pos_Energy, P0_Data.T1_Pos_Energy/1000, P0_Data.T1_Pos_Energy%1000);
    WSContentSend_PD(HTTP_T2_Pos_Energy, P0_Data.T2_Pos_Energy/1000, P0_Data.T2_Pos_Energy%1000);
    
    // Only show negative envery when larger then threshold
    if(P0_Data.Total_Neg_Energy > 500) WSContentSend_PD(HTTP_Total_Neg_Energy, P0_Data.Total_Neg_Energy/1000, P0_Data.Total_Neg_Energy%1000);
    if(P0_Data.T1_Neg_Energy > 500) WSContentSend_PD(HTTP_T1_Neg_Energy, P0_Data.T1_Neg_Energy/1000, P0_Data.T1_Neg_Energy%1000);
    if(P0_Data.T2_Neg_Energy > 500) WSContentSend_PD(HTTP_T2_Neg_Energy, P0_Data.T2_Neg_Energy/1000, P0_Data.T2_Neg_Energy%1000);

    // long Total_Pos_Today = P0_Data.Total_Pos_Energy - P0_His.Total_Pos_Today_Stamp;
    // long T1_Pos_Today = P0_Data.T1_Pos_Energy - P0_His.T1_Pos_Today_Stamp;
    // long T2_Pos_Today = P0_Data.T2_Pos_Energy - P0_His.T2_Pos_Today_Stamp;

    // long Total_Pos_Month = P0_Data.Total_Pos_Energy - P0_His.Total_Pos_Month_Stamp;
    // long T1_Pos_Month = P0_Data.T1_Pos_Energy - P0_His.T1_Pos_Month_Stamp;
    // long T2_Pos_Month = P0_Data.T2_Pos_Energy - P0_His.T2_Pos_Month_Stamp;

    // WSContentSend_PD(HTTP_Empy_Line);
    // WSContentSend_PD(HTTP_Daily_Title);
    // WSContentSend_PD(HTTP_Total_Pos_Today, Total_Pos_Today/1000, Total_Pos_Today%1000);
    // WSContentSend_PD(HTTP_T1_Pos_Today, T1_Pos_Today/1000, T1_Pos_Today%1000);
    // WSContentSend_PD(HTTP_T2_Pos_Today, T2_Pos_Today/1000, T2_Pos_Today%1000);
    // WSContentSend_PD(HTTP_Total_Pos_Yesterday, P0_His.Total_Pos_Yesterday/1000, P0_His.Total_Pos_Yesterday%1000);
    // WSContentSend_PD(HTTP_T1_Pos_Yesterday, P0_His.T1_Pos_Yesterday/1000, P0_His.T1_Pos_Yesterday%1000);
    // WSContentSend_PD(HTTP_T2_Pos_Yesterday, P0_His.T2_Pos_Yesterday/1000, P0_His.T2_Pos_Yesterday%1000);

    // WSContentSend_PD(HTTP_Empy_Line);
    // WSContentSend_PD(HTTP_Montly_Title);
    // WSContentSend_PD(HTTP_Total_Pos_Month, Total_Pos_Month/1000, Total_Pos_Month%1000);
    // WSContentSend_PD(HTTP_T1_Pos_Month, T1_Pos_Month/1000, T1_Pos_Month%1000);
    // WSContentSend_PD(HTTP_T2_Pos_Month, T2_Pos_Month/1000, T2_Pos_Month%1000);
    // WSContentSend_PD(HTTP_Total_Pos_PrevMonth, P0_His.Total_Pos_PrevMonth/1000, P0_His.Total_Pos_PrevMonth%1000);
    // WSContentSend_PD(HTTP_T1_Pos_PrevMonth, P0_His.T1_Pos_PrevMonth/1000, P0_His.T1_Pos_PrevMonth%1000);
    // WSContentSend_PD(HTTP_T2_Pos_PrevMonth, P0_His.T2_Pos_PrevMonth/1000, P0_His.T2_Pos_PrevMonth%1000);

    // Display Error
    if(P0_Data.Fatal_Error) WSContentSend_PD(HTTP_Fatel_Error, P0_Data.Fatal_Error);
    
#endif  // USE_WEBSERVER
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns88(byte function) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      P0_Reader_Init();
      break;
    case FUNC_EVERY_250_MSECOND:
      P0_Reader_Tick();
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      P0_Reader_Show(0);
      break;
#endif  // USE_WEBSERVER
    case FUNC_JSON_APPEND:
      P0_Reader_Show(1);
    break;

  }
  return result;
}


#endif // USE_P0Reader