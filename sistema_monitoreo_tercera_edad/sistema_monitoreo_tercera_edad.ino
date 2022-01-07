/* Fecha: 21 de diciembre de 2021.
 * 

 * 
 * 
 
 */

/*Bibliotecas necesarias para la ejecución del programa*/
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <WiFi.h>  // Biblioteca para el control de WiFi
#include <PubSubClient.h> //Biblioteca para conexion MQTT
#include <DFRobot_MAX30102.h>
/*Conexion ESP32 y sensores
    MLX90614            Esp32    |    MAX30102       ESP32
    Vin                 3.3v     |    Vin            3V3
    GND                 GND      |    GND            GND
    SCL                 GPIO22   |    SCL            GPIO22
    SDA                 GPIO21   |    SDA            GPIO21
*/

//---------------------------Conectividad---------------------------------------------------
//Datos de WiFi
const char* ssid = "Totalplay-559E" ;// Aquí debes poner el nombre de tu red
const char* password = "559EB28CPBfPW9Vu";  // Aquí debes poner la contraseña de tu red

//Datos del broker MQTT
const char* mqtt_server = "3.65.154.195"; // Si estas en una red local, coloca la IP asignada, en caso contrario, coloca la IP publica
IPAddress server(3,65,154,195);

// Objetos
WiFiClient espClient; // Este objeto maneja los datos de conexion WiFi
PubSubClient client(espClient); // Este objeto maneja los datos de conexion al broker

//-------------------Data sensors-----------------------------------------------------------------
//Declaración del objeto.
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
DFRobot_MAX30102 particleSensor;  //reconocimiento de sensor
//Variables sensor MLX90614
float TempMed;
float TempReal;
// Variables sensor MAX30102
int32_t SPO2; //SPO2
int8_t SPO2Valid; //Flag to display if SPO2 calculation is valid
int32_t heartRate; //Heart-rate
int8_t heartRateValid;

/*Declaración de las variables que nos serviran como temporizadores*/
unsigned long timeNow;//Almacenaremos el tiempo en ms
unsigned long timeNow_MAX;//Almacenaremos el tiempo en ms
unsigned long PreviousTime_MLX90614=0;
unsigned long PreviousTime_MAX30102=0;
unsigned long timeLast_sensors;// Variable que nos permite controlar el tiempo de los sensores junto con previous
/*Declaración de las variables que nos permiten determinar el tiempo de retardo para ejecutar 
cada acción simulando un delay*/
const int Time_MLX90614=5000;//Declaramos que la medición del sensor de temperatura se ejecute cada 15s
const int Time_MAX30102=5000;//Declaramos que la medición del sensor de temperatura se ejecute cada 15s
/*Variables de conectividad wifi*/
int flashLedPin = 2;  // Para indicar el estatus de conexión
int statusLedPin = 19; // Para ser controlado por MQTT
unsigned long timeLast;// Variable de control de tiempo no bloqueante
unsigned long timeLast_MAX;// Variable de control de tiempo no bloqueante
const int wait = 5000;  // Indica la espera cada 5 segundos para envío de mensajes MQTT
const int wait_MAX = 5000;  // Indica la espera cada 5 segundos para envío de mensajes MQTT

void setup() {
 Serial.begin(115200);
 //inicializamos al sensor mlx90614
 mlx.begin();
  //Init serial
  Serial.begin(115200);
 
  while (!particleSensor.begin()) {
    Serial.println("MAX30102 was not found");
    delay(1000);
  }

 
  particleSensor.sensorConfiguration(/*ledBrightness=*/50, /*sampleAverage=*/SAMPLEAVG_4, \
                        /*ledMode=*/MODE_MULTILED, /*sampleRate=*/SAMPLERATE_100, \
                        /*pulseWidth=*/PULSEWIDTH_411, /*adcRange=*/ADCRANGE_16384);

  //Setup del mlx90614
  mlx.begin(); //Se inicia el sensor
                        
 /*Inicializamos los pines del ESP32CAM como SDA y SCL*/
 //declaración de los pines que nos permitiran declarar la conectividad a internet.
 pinMode (flashLedPin, OUTPUT);
 pinMode (statusLedPin, OUTPUT);
 digitalWrite (flashLedPin, LOW);
 digitalWrite (statusLedPin, HIGH);

  Serial.println();
  Serial.println();
  Serial.print("Conectar a ");
  Serial.println(ssid);
 
  WiFi.begin(ssid, password); // Esta es la función que realiz la conexión a WiFi
 
  while (WiFi.status() != WL_CONNECTED) { // Este bucle espera a que se realice la conexión
    digitalWrite (statusLedPin, HIGH);
    delay(500); //dado que es de suma importancia esperar a la conexión, debe usarse espera bloqueante
    digitalWrite (statusLedPin, LOW);
    Serial.print(".");  // Indicador de progreso
    delay (5);
  }
  
  // Cuando se haya logrado la conexión, el programa avanzará, por lo tanto, puede informarse lo siguiente
  Serial.println();
  Serial.println("WiFi conectado");
  Serial.println("Direccion IP: ");
  Serial.println(WiFi.localIP());

  // Si se logro la conexión, encender led
  if (WiFi.status () > 0){
  digitalWrite (statusLedPin, LOW);
  }
  
  delay (1000); // Esta espera es solo una formalidad antes de iniciar la comunicación con el broker

  //Conexión con el broker MQTT
  client.setServer(server, 1883); // Conectarse a la IP del broker en el puerto indicado
  client.setCallback(callback); // Activar función de CallBack, permite recibir mensajes MQTT y ejecutar funciones a partir de ellos
  delay(1500);  // Esta espera es preventiva, espera a la conexión para no perder información
}

void loop() {
 timeLast_sensors=millis();
 timeNow = millis(); // Control de tiempo para esperas no bloqueantes
 MLX90614();//llamamos a la función del sensor de temperatura
 timeNow_MAX=millis();
 MAX30102();//llamamos a la función del sensor de oxigenacion
/*---------------MQTT-----------------------------------------------------------------*/
 //Verificar siempre que haya conexión al broker
  if (!client.connected()) {
    reconnect();  // En caso de que no haya conexión, ejecutar la función de reconexión, definida despues del void setup ()
  }// fin del if (!client.connected())
  client.loop(); // Esta función es muy importante, ejecuta de manera no bloqueante las funciones necesarias para la comunicación con el broker
 
}
/*------------------------------Sección de funciones de conectividad a MQTT------------------------------------------------------------*/
// Función para reconectarse
void reconnect() {
  // Bucle hasta lograr conexión
  while (!client.connected()) { // Pregunta si hay conexión
    Serial.print("Tratando de contectarse...");
    // Intentar reconexión
    if (client.connect("ESP32CAMClient")) { //Pregunta por el resultado del intento de conexión
      Serial.println("Conectado");
      client.subscribe("SignosVitales/Temperatura/CasaRetiro1"); // Esta función realiza la suscripción al tema
      client.subscribe("SignosVitales/Oxigenacion/CasaRetiro1");
      client.subscribe("SignosVitales/bpm/CasaRetiro1");
    }// fin del  if (client.connect("ESP32CAMClient"))
    else {  //en caso de que la conexión no se logre
      Serial.print("Conexion fallida, Error rc=");
      Serial.print(client.state()); // Muestra el codigo de error
      Serial.println(" Volviendo a intentar en 5 segundos");
      // Espera de 5 segundos bloqueante
      delay(5000);
      Serial.println (client.connected ()); // Muestra estatus de conexión
    }// fin del else
  }// fin del bucle while (!client.connected())
}// fin de void reconnect(


// Esta función permite tomar acciones en caso de que se reciba un mensaje correspondiente a un tema al cual se hará una suscripción
 void callback(char* topic, byte* message, unsigned int length) {
  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageTemp; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageTemp);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32CAM está suscrito al tema SignosVitales/Temperatura/CasaRetiro"
  if (String(topic) == "SignosVitales/Temperatura/CasaRetiro1") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageTemp == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")
    else if(messageTemp == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")


//-------------------------------------------------------------------------------------------------
  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageOX; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageOX += (char)message[i];
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageOX);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32CAM está suscrito al tema SignosVitales/Temperatura/CasaRetiro"
  if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageOX == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1"")
    else if(messageOX == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")



  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageBPM; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageBPM += (char)message[i];
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageBPM);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32CAM está suscrito al tema SignosVitales/Temperatura/CasaRetiro"
  if (String(topic) == "SignosVitales/bpm/CasaRetiro1") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageBPM == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/bpm/CasaRetiro1"")
    else if(messageBPM == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/bpm/CasaRetiro1"")
}




/*--------------------------------Sección de funciones de los sensores--------------------------------------------------------------*/
/*Generamos las funciones de cada sensor para tener un código más ordenado */
void MLX90614(){
   if (timeNow - timeLast > wait) { // Manda un mensaje por MQTT cada cinco segundos
    timeLast = timeNow; // Actualización de seguimiento de tiempo

    //Lectura del sensor de temperatura mlx90614 sin contemplar el error
    TempMed=mlx.readObjectTempC();//Lectura del sensor
    TempReal=TempMed+4.38;//lectura tomando en cuenta el error    
    char dataString[8]; // Define una arreglo de caracteres para enviarlos por MQTT, especifica la longitud del mensaje en 8 caracteres
    dtostrf(TempReal, 1, 2, dataString);  // Esta es una función nativa de leguaje AVR que convierte un arreglo de caracteres en una variable String
    Serial.print("La temperarura es: "); // Se imprime en monitor solo para poder visualizar que el evento sucede
    Serial.println(dataString);
    client.publish("SignosVitales/Temperatura/CasaRetiro1", dataString); // Esta es la función que envía los datos por MQTT, especifica el tema y el valor. Es importante cambiar el tema para que sea unico SignosVitales/Temperatura/Temaconotronombre
  }// fin del if (timeNow - timeLast > wait)  
}



void MAX30102()
{
 if (timeNow_MAX - timeLast_MAX >wait_MAX) {
  timeLast_MAX=timeNow_MAX;
  Serial.println(F("Espera 4 segundos"));
  particleSensor.heartrateAndOxygenSaturation(/**SPO2=*/&SPO2, /**SPO2Valid=*/&SPO2Valid, /**heartRate=*/&heartRate, /**heartRateValid=*/&heartRateValid);
  char dataStringspo2[8];
  char dataStringhb[8];
  //Esta sección nos ayuda a evitar un poco de ruido del sensor MAX30102
  if(SPO2<0){
    SPO2=0;
    }
  if(heartRate<0){
    heartRate=0;
    }
  dtostrf(SPO2, 1, 2, dataStringspo2);
  dtostrf(heartRate, 1, 2, dataStringhb);
  Serial.print("SPO2: ");
  Serial.println(dataStringspo2);
  Serial.print("Bpm: ");
  Serial.println(dataStringhb);
  client.publish("SignosVitales/Oxigenacion/CasaRetiro1", dataStringspo2);
  client.publish("SignosVitales/bpm/CasaRetiro1", dataStringhb);
 }
}
