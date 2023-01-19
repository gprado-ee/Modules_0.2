
/**
------------------------------------ GLOBAL DESCRIPTION -------------------------------


[x] Setup conforme endereço do dispositivo
[ ] Bluetooth
[ ] Firebase
[ ] Separar código em arquivos diferentes

------------------------------------ DEVICES ADDRESSES SET -------------------------------


						            +-----MA----+
						            |			      |
	 Device ident bitD2-->|I/O	 	    |
	 Device ident bitD1-->|I/O	 	    |
	 Device ident bitD0-->|I/O	 	    |
						            |			      |
		     Float switch-->|I/O	 	    |
	  Water tank level*-->|ANALOG 	  |
	 Water tank level**-->|ANALOG 	  |		*step levels (0%, 25%, 50%, 75%, 100%)
	 					            |		 	      |		**continuous
						            +-----------+

						            +-----S0----+
						            |			      |-->Display
	 Device ident bitD2-->|I/O	 	    |-->LED WiFi
	 Device ident bitD1-->|I/O	 	    |-->LED MA online
	 Device ident bitD0-->|I/O	 	    |-->LED S0 online (redundant)
						            |			      |-->LED S1 online
Water tank sensor mode->|TTL pulse  |-->LED S2 online
Pump mode switch AUTO-->|I/O	 	    |-->LED S3 online (for future use: Workshop [oficina] access control )
 Pump mode switch MAN-->|I/O	 	    |-->LED S4 online (for future use)
						            |		 	      |-->LED S5 online (for future use)
						            +-----------+

                        +-----S1----+
                        |           |
   Device ident bitD2-->|I/O	 	    |-->Pump relay
   Device ident bitD1-->|I/O	 	    |-->Aux TTL
   Device ident bitD0-->|I/O	 	    |
                        |			      |
Pump voltage presence-->|I/O	 	    |
              Aux TTL-->|I/O 		    |
     Water weel level-->|ANALOG 	  |
                        |		 	      |
                        +-----------+



------------------------------------ DEVICES ADDRESSES SET -------------------------------

HEX - ID - DEVICE
 07 - MA - Master station
 00 - S0 - User interface station
 01 - S1 - Pump station
 02 - S2 - Weather station
 03 - S3 - Future use
 04 - S4 - Future use
 05 - S5 - Future use

------------------------------------ SYSTEM VARIABLES SET --------------------------------

-ID-	-VARIABLE-						          -CONTENT-
MA		Date and time					          (char: DDMMAAAAHHMM)
MA		Wifi+internet status 			      (int: 0-Offline, 1-Only Wifi, 2-Internet)
MA		Water tank analog sensor value 	(int: 0-100)
MA		Water tank digital sensor value (int: 0-100) step levels: 0%, 25%, 50%, 75%, 100%
MA		Water tank float switch status	(int: 0-Low, 1-High)
MA		Web command mode 				        (int: 0-Off, 1-On)
MA		Web command 					          (string: 'tonp', 'toffp')	
S0		Pump mode switch 				        (int: 0-Off, 1-Auto, 2-Man, 3-Bluetooth, -1 if station offline)
S0		Water tank sensor mode			    (0-Analogic, 1-Digital, -1 if station offline)
S0		Bluetooth command mode 			    (int: 0-Off, 1-On, -1 if station offline)
S0		Bluetooth command 				      (string)
S1		Pump status 					          (int: 0-Off, 1-On, -1 if station offline)
S1		Pump switch status 				      (int: 0-Off, 1-Auto, 2-Man, -1 if station offline))
S1		Water weel level 				        (int: 0-100, -1 if station offline) step levels: 0%, 25%, 50%, 75%, 100%
S2		Temperature(°C) 				        (float: value, -1 if station offline)
S2		Humidity(%) 					          (float: value, -1 if station offline)
S2		Precipitation 					        (float: value, -1 if station offline)

***Try to use struct


*/

//================================ GLOBAL DEFINITIONS ================================
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Ping.h>
#include <time.h>


#include <station_s0.cpp>
#include <station_s1.cpp>
#include <station_s2.cpp>

#define device_address_A2           36
#define device_address_A1           39
#define device_address_A0           34

#define station_master_device       0x07
#define station_s0_device           0x00
#define station_s1_device           0x01
#define station_s2_device           0x02
#define station_s3_device           0x03 //For future use
#define station_s4_device           0x04 //For future use
#define station_s5_device           0x05 //For future use

#define wifi_led                    3
#define s0_active_led               2
#define s1_active_led               2
#define s2_active_led               0
#define s3_active_led               0
#define s4_active_led               0
#define s5_active_led               0

#define serial_baud_rate            9600
#define wifi_disconnected_period    1000
#define wifi_connected_period       250
#define internet_online_period      10
#define station_offline_period      1000
#define station_online_period       10
#define station_check_comm_period   3000
#define online                      1
#define offline                     0

#define destination_key                   "destination"
#define ma_data_time_key                  "ma_data_time"
#define ma_wifi_internet_key              "ma_wifi_internet"
#define ma_water_tank_analog_level_key    "ma_water_tank_analog_level"
#define ma_water_tank_digital_level_key   "ma_water_tank_digital_level"
#define ma_water_tank_float_switch_key    "ma_water_tank_float_switch"
#define s0_pump_mode_switch_key           "s0_pump_mode_switch"
#define s0_water_tank_sensor_mode_key     "s0_water_tank_sensor_mode"
#define s1_pump_status_key                "s1_pump_status"
#define s1_pump_switch_status_key         "s1_pump_switch_status"
#define s1_water_well_level_key           "s1_water_well_level"
#define s2_temperature_key                "s2_temperature"
#define s2_humidity_key                   "s2_humidity"
#define s2_precipitation_key              "s2_precipitation"


//================================ GLOBAL VARIABLES AND FLAGS ================================

byte device_address = 0x00;

// const char * wifi_ssid = "Montrel::Visitantes";
// const char * wifi_password = "*Montrel!38613070#";
const char * wifi_ssid = "Prado 2.4ghz";
const char * wifi_password = "17082019";
const char * url_to_ping = "www.google.com";
const char * ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

int wifi_led_period_ms;
int s1_active_led_period_ms; 

bool internet_status;
bool data_time_updated = true;
bool debug_mode = true;

WiFiClientSecure client;

const byte received_string_max_size = 64;
char received_string[received_string_max_size];
boolean new_data_ready = false; 


//------------------------------- SYSTEM VARIABLES -------------------------------------

const char * ma_data_time = "151220221015";
uint8_t ma_wifi_internet = 2;
uint8_t ma_water_tank_analog_level = 82;
uint8_t ma_water_tank_digital_level = 75;
uint8_t ma_water_tank_float_switch = 0;
uint8_t s0_pump_mode_switch = 1;	
uint8_t s0_water_tank_sensor_mode = 1;	
uint8_t s1_pump_status = 0;
uint8_t s1_pump_switch_status = 0;
uint8_t s1_water_well_level = 50;
float s2_temperature = 23.45;
float s2_humidity = -1;
float s2_precipitation = -1;


//------------------"parseDataExample" function variables
char temp_data[received_string_max_size];        // temporary array for use when parsing
char messageFromPC[received_string_max_size] = {0};
int integerFromPC = 0;
float floatFromPC = 0.0;



//====================================== FUNCTIONS ======================================

void ConnectToWiFi(){
  int i;
  IPAddress ip;
  IPAddress subnet;
  IPAddress gateway;
  byte mac[6];
  
  if(debug_mode){Serial.print("\nConnecting to WiFi network");}
  
  WiFi.begin(wifi_ssid, wifi_password);
  
  for(i=1;i<=20;i++){
    if (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if(debug_mode){Serial.print(".");}
    }
    else{      
      ip = WiFi.localIP();
      subnet = WiFi.subnetMask();
      gateway = WiFi.gatewayIP();
      WiFi.macAddress(mac);
      
      if(debug_mode){
        Serial.print("\nConnection Sucessfull");
        Serial.print("\nLocal IP: ");
        Serial.print(ip);
        Serial.print("\nNetmask: ");
        Serial.print(subnet);
        Serial.print("\nGateway: ");
        Serial.print(gateway);      

        Serial.print("\nMAC address: ");
        Serial.print(mac[5],HEX);
        Serial.print(":");
        Serial.print(mac[4],HEX);
        Serial.print(":");
        Serial.print(mac[3],HEX);
        Serial.print(":");
        Serial.print(mac[2],HEX);
        Serial.print(":");
        Serial.print(mac[1],HEX);
        Serial.print(":");
        Serial.println(mac[0],HEX);
      }

      wifi_led_period_ms = wifi_connected_period;
      i=21;
      
    }
    if(i==20 && debug_mode){Serial.print("\nConnection failed");}
  }

}  

void printLocalTime(){
  //expected to be used only on intencional case, so it's ideppendent of the debug_mode flag
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

void WiFiEvent(WiFiEvent_t event){
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      log_i("Connected to access point");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      log_i("Disconnected from WiFi access point");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      log_i("WiFi client disconnected");
      break;
    default: break;
  }
}

void getSerialIncoming() {
    static boolean recvInProgress = false;
    static byte rcv_chr_idx = 0;
    char msg_start_marker = '<';
    char msg_end_marker = '>';
    char received_char;

    received_string[0] = '\0';
    new_data_ready = false;
 
    while (Serial.available() > 0 && new_data_ready == false) {
        received_char = Serial.read();
        
        if (recvInProgress == true) {
            if (received_char != msg_end_marker) {
                received_string[rcv_chr_idx] = received_char;
                rcv_chr_idx++;
                if (rcv_chr_idx >= received_string_max_size) {
                    rcv_chr_idx = received_string_max_size - 1;
                }
            }
            else {
                received_string[rcv_chr_idx] = '\0'; // terminate the string
                recvInProgress = false;
                rcv_chr_idx = 0;
                new_data_ready = true;
            }
        }

        else if (received_char == msg_start_marker) {
            recvInProgress = true;
        }
    }
	
	return;
}

void sendCmd(const char* data_str){ 
    Serial.println(data_str); 
} 



//---------------------- UNDER DEVELOPMENT Begin ------------------------

void getSystemVariables(){
  char answer[350] = {0};
  char str[20];
  
  strcat(answer,"<\n");
  
  strcat(answer,destination_key); 
  strcat(answer,",");
  strcat(answer,"00");  // "00" means the message is not addressed to any station, but to the requester
  strcat(answer,",\n"); 

  strcat(answer,ma_data_time_key); 
  strcat(answer,",");
  strcat(answer,ma_data_time); 
  strcat(answer,",\n");

  strcat(answer,ma_wifi_internet_key); 
  strcat(answer,",");
  strcat(answer,itoa(ma_wifi_internet,str,10)); 
  strcat(answer,",\n");

  strcat(answer,ma_water_tank_analog_level_key); 
  strcat(answer,",");
  strcat(answer,itoa(ma_water_tank_analog_level,str,10)); 
  strcat(answer,",\n");

  strcat(answer,ma_water_tank_digital_level_key); 
  strcat(answer,",");
  strcat(answer,itoa(ma_water_tank_digital_level,str,10)); 
  strcat(answer,",\n");

  strcat(answer,ma_water_tank_float_switch_key); 
  strcat(answer,",");
  strcat(answer,itoa(ma_water_tank_float_switch,str,10)); 
  strcat(answer,",\n");

  strcat(answer,s0_pump_mode_switch_key); 
  strcat(answer,",");
  strcat(answer,itoa(s0_pump_mode_switch,str,10)); 
  strcat(answer,",\n");

  strcat(answer,s0_water_tank_sensor_mode_key); 
  strcat(answer,",");
  strcat(answer,itoa(s0_water_tank_sensor_mode,str,10)); 
  strcat(answer,",\n");

  strcat(answer,s1_pump_status_key); 
  strcat(answer,",");
  strcat(answer,itoa(s1_pump_status,str,10)); 
  strcat(answer,",\n");

  strcat(answer,s1_pump_switch_status_key); 
  strcat(answer,",");
  strcat(answer,itoa(s1_pump_switch_status,str,10)); 
  strcat(answer,",\n");

  strcat(answer,s1_water_well_level_key); 
  strcat(answer,",");
  strcat(answer,itoa(s1_water_well_level,str,10)); 
  strcat(answer,",\n");

  strcat(answer,s2_temperature_key); 
  strcat(answer,",");
  strcat(answer,dtostrf(s2_temperature,4,2,str)); 
  strcat(answer,",\n");

  strcat(answer,s2_humidity_key); 
  strcat(answer,",");
  strcat(answer,dtostrf(s2_humidity,4,2,str)); 
  strcat(answer,",\n");

  strcat(answer,s2_precipitation_key); 
  strcat(answer,",");
  strcat(answer,dtostrf(s2_precipitation,4,2,str)); 
  //strcat(answer,",\n");

  strcat(answer,"\n>\0");

  
  // Serial.print("Answer length: ");
  // Serial.println(strlen(answer));  
  // Serial.print("Answer: ");
  Serial.println(answer);


      //const char * ma_data_time = "151220221015";
      // uint8_t ma_wifi_internet = 2;
      // uint8_t ma_water_tank_analog_level = 82;
      // uint8_t ma_water_tank_digital_level = 75;
      // uint8_t ma_water_tank_float_switch = 0;
      // uint8_t s0_pump_mode_switch = 1;	
      // uint8_t s0_water_tank_sensor_mode = 1;	
      // uint8_t s1_pump_status = 0;
      // uint8_t s1_pump_switch_status = 0;
      // uint8_t s1_water_well_level = 50;
      // float s2_temperature = 23.45;
      // float s2_humidity = -1;
      // float s2_precipitation = -1;


}

//---------------------- UNDER DEVELOPMENT End --------------------------

//------------------- WAITING FOR IMPROVMENT Begin ----------------------


void  parseDataExample(){
  char *received_message;
  char *arr[] = {};
  int i = 0;

  Serial.print("\n");

  strcpy(temp_data, received_string);

  received_message = strtok(temp_data, ",");

  while(received_message){
    
    arr[i] = received_message;
    received_message = strtok(NULL, ",");
    Serial.println(arr[i]);
    
    if(strcmp(arr[i],"KEY") == 0) Serial.println("Chave reconhecida");
    
    i++;
  }
}

void parseDataExample2() {      // split the data into its parts

  char * strtokIidx; // this is used by strtok() as an index

  strcpy(temp_data, received_string);
  
  strtokIidx = strtok(temp_data,",");     // get the first part - the string
  strcpy(messageFromPC, strtokIidx);      // copy it to messageFromPC

  strtokIidx = strtok(NULL, ",");         // this continues where the previous call left off
  integerFromPC = atoi(strtokIidx);       // convert this part to an integer

  strtokIidx = strtok(NULL, ",");
  floatFromPC = atof(strtokIidx);         // convert this part to a float
}

void showParsedData() {
  Serial.println("Message: ");
  Serial.println(messageFromPC);
  Serial.print("Integer: ");
  Serial.println(integerFromPC);
  Serial.print("Float: ");
  Serial.println(floatFromPC);
}

//------------------- WAITING FOR IMPROVMENT End ----------------------


//================================== FREE RTOS TASKS ====================================

void wifiLedIndication(void *arg) {
    pinMode(wifi_led, OUTPUT);
    while(1) {
        digitalWrite(wifi_led, HIGH);
        vTaskDelay(pdMS_TO_TICKS(wifi_led_period_ms));
        digitalWrite(wifi_led, LOW);
        vTaskDelay(pdMS_TO_TICKS(wifi_led_period_ms));
    }
}

void wifiAndInternetStatusMonitoring(void *arg){
  bool ping_success;
  struct tm timeinfo;
    
  while(true){
    if ( WiFi.status() == WL_CONNECTED ){
      ping_success = Ping.ping(url_to_ping, 3);
      if(ping_success){
        wifi_led_period_ms = internet_online_period;
        internet_status = online;
        if(!data_time_updated){
          do{
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            vTaskDelay(pdMS_TO_TICKS(1000));
          }while(!getLocalTime(&timeinfo));
          printLocalTime();
          data_time_updated = 1;
        }
        
        vTaskDelay(pdMS_TO_TICKS(3000));
      }
      else{
        wifi_led_period_ms = wifi_connected_period;
        internet_status = offline;
        vTaskDelay(pdMS_TO_TICKS(3000));
      }
    }    
    else
    {      
      WiFi.disconnect();
      Serial.print( "\nWiFi disconnected" );
      wifi_led_period_ms = wifi_disconnected_period;
      vTaskDelay(pdMS_TO_TICKS(3000));

      ConnectToWiFi();
      
      if( WiFi.status() == WL_CONNECTED)
      {
       wifi_led_period_ms = wifi_connected_period;
      }
    }
    //WiFi.onEvent( WiFiEvent );
  }
}

void pumpStationLedIndication(void *arg) {
    pinMode(s1_active_led, OUTPUT);
    while(1) {
        digitalWrite(s1_active_led, HIGH);
        vTaskDelay(pdMS_TO_TICKS(s1_active_led_period_ms));
        digitalWrite(s1_active_led, LOW);
        vTaskDelay(pdMS_TO_TICKS(s1_active_led_period_ms));
    }
}





void setup() {

  Serial.begin(serial_baud_rate);
  delay(100);  

  pinMode(device_address_A2, INPUT);
  pinMode(device_address_A1, INPUT);
  pinMode(device_address_A0, INPUT);

  bitWrite(device_address,2,digitalRead(device_address_A2));
  bitWrite(device_address,1,digitalRead(device_address_A1));
  bitWrite(device_address,0,digitalRead(device_address_A0));

      //---------------------------------------forces the device choice
      device_address = 0x07; 
      Serial.print("\ndevice_address: 0x0");
      Serial.println(device_address, HEX);
      //---------------------------------------

  switch (device_address){
    case station_master_device:
      Serial.println("Inicializando station MASTER");


      break;

    case station_s0_device:
      WiFi.mode(WIFI_STA);

      ConnectToWiFi();      
      
      xTaskCreate(wifiLedIndication,"wifiLedIndication",1024,NULL,1,NULL);
      xTaskCreate(wifiAndInternetStatusMonitoring,"wifiAndInternetStatusMonitoring",4096,NULL,2,NULL);

      break;

    case station_s1_device:

      break;

    case station_s2_device:

      break;

    case station_s3_device:
      //reserved for future use
      break;

    case station_s4_device:
      //reserved for future use
      break;

    case station_s5_device:
      //reserved for future use
      break;

    default:
      //sinalize fail (every station must have a fail sinalization)
      break;
  }



    //xTaskCreate(pumpStationLedIndication,"pumpStationLedIndication",10000,NULL,1,NULL);
    //xTaskCreate(pumpStationControl,"pumpStationControl",20000,NULL,3,NULL);
 
}


void loop() { //esse loop é uma preliminar da task que vai ficar ouvindo a serial
    Serial.print(".");
    getSerialIncoming();    
    //sendCmd(received_string);
    
    if(new_data_ready){
        
        // Serial.print("Received String: ");
        // Serial.println(received_string);

        if(!strcmp(received_string,"NOP")) Serial.println("\nACK");
        else if(!strcmp(received_string,"SYSVAR")) { Serial.print("\n"); getSystemVariables();}
        else Serial.println("\nNAK");

        //parseDataExample2();
        //showParsedData();

        //parseDataExample();

        


    }

    delay(2500);    
}