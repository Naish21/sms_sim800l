#define RXD2 16
#define TXD2 17

//Create software serial object to communicate with SIM800L
//Serial1.begin(115200);
//SoftwareSerial mySerial(3, 2); //SIM800L Tx & Rx is connected to Arduino #3 & #2

void setup() {
  // Note the format for setting a serial port is as follows: Serial1.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Initializing (10s)...");
  delay(10000);
  Serial1.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  Serial1.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  Serial1.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  Serial1.println("AT+CREG?"); //Check whether it has registered in the network
  updateSerial();
  Serial1.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  Serial1.println("AT+CMGS=\"+346xxxxxxxx\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  Serial1.println("SMS Test from SIM800L: OK!"); //text content
  updateSerial();
  Serial1.write(26);
  
}

void loop()
{
  updateSerial();
}

void updateSerial()
{
  delay(1000);
  while(Serial.available()) {
    Serial1.write(Serial.read());//Forward what Software Serial received to Serial Port
  }
  while(Serial1.available()) {
    Serial.write(Serial1.read());//Forward what Serial received to Software Serial Port
  }
}