/* Autores: Xavier Emmanuel Domínguez Grajales.
 *  Joselyn Martínez Miranda
 *  Carlos Mauricio García Garibay
 *  
 *  Fecha: 19 de Febrero de 2022
 *  
 *  Este programa realiza la lectura de los sensores MAX30102 y MLX90614
 *  para generar un sistema de monitoreo de salud para personas de la tercera edad, 
 *  que tiene la capacidad de enviar mensajes a node red por medio de MQTT y +
 *  generar notificaciones en caso de encontrarse en situaciones de riesgo mediante 
 *  un chatbot de telegram. Para realizar este programa se utilizaron bibliotecas que
 *  facilitaron el trabajo con dichos sensores, la unica biblioteca no encontrada
 *  en arduino es la de DFRobot_MAX30102 para ello colocamos el repositorio en 
 *  donde es posible localizar la información https://github.com/DFRobot/DFRobot_MAX30102
*/

/*Conexion ESP32 y sensores
    MLX90614            Esp32    |    MAX30102       ESP32
    Vin                 3V3      |    Vin            3V ou3
    GND                 GND      |    GND            GND
    SCL                 GPIO22   |    SCL            GPIO22
    SDA                 GPIO21   |    SDA            GPIO21
*/

/*Bibliotecas necesarias para la ejecución del programa*/
#include <Wire.h>//Biblioteca que nos permite trabajar con  I2c
#include <Adafruit_MLX90614.h>//Biblioteca que facilita el control del sensor mlx90614:
#include <WiFi.h>// Biblioteca para el control de WiFi
#include <PubSubClient.h> //Biblioteca para conexion MQTT
#include <DFRobot_MAX30102.h>//Biblioteca que facilita la uitlización del sensor MAX30102
#include <WiFiClientSecure.h>//Biblioteca que permite la conectividad del Esp32 a wifi
#include <UniversalTelegramBot.h>//Biblioteca que permite la conectividad del Esp32 con telegram

#define BOT_TOKEN "Pega_tu_token" // se obtiene al momento de crear el chat bot en telegram es importante que coloques el de tu propio bot
const unsigned long BOT_MTBS = 1000; // Tiempo medio entre mensajes escanedos por telegram


//---------------------------Conectividad---------------------------------------------------
//Datos de WiFi

const char* ssid = "Nombre_de_tu_Red";// Aquí debes poner el nombre de tu red
const char* password ="Contraseña_de_tu_Red";  // Aquí debes poner la contraseña de tu red

//Datos del broker MQTT
const char* mqtt_server = "18.195.132.243"; // Si estas en una red local, coloca la IP asignada, en caso contrario, coloca la IP publica es importante verificar que esta IP se mantenga en caso con
IPAddress server(18,195,132,243);//En esta parte se coloca la IP separada por (,) en lugar de (.)

// Objetos
WiFiClient espClient; // Este objeto maneja los datos de conexion WiFi
PubSubClient client(espClient); // Este objeto maneja los datos de conexion al broker


//-------------------Data sensors-----------------------------------------------------------------
//Declaración del objeto.
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
DFRobot_MAX30102 particleSensor;  //reconocimiento de sensor

//Variables sensor MLX90614
float TempMed;//Variable que almacena el valor de la temperatura medida por el sensor
const float error_temp=3.33;//variable que almacena el error que se ha obtenido de la realización de multiples pruebas. 4.33
float TempReal;//Variable que almacena el valor de la temperatura medida + el error que posee el sensor, el cual fue encontrado después de realizar múltiples pruebas

// Variables sensor MAX30102{
const int error_ox=3;//Error medido para la oxigenación después de la realización de las pruebas
const int error_bpm=2;//Error medido para bpm después de la realización de las pruebas  86
int32_t SPO2; //SPO2
int32_t SPO2_real;//SPO2 medido después de realizar el promedio de varias muestras
int8_t SPO2Valid; //Indicador para mostrar si el calculo de SPO2 es valido
int32_t heartRate;//bpm
int32_t heartRate_real; //bpm medido después de realizar el primedio de varias muestras
int8_t heartRateValid;//Indicador para mostrar si el calculo del bpm es valido
float oxt;//variable auxiliar para el spo2 en la que se suma el error 
float bpmt;//variable axiliar para el bpm en la que se suma el error 

/*Declaración de las variables que nos serviran como temporizadores*/
unsigned long timeNow;//Almacenaremos el tiempo en ms
unsigned long timeNow_MAX;//Almacenaremos el tiempo en ms


/*Declaración de las variables que nos permiten determinar el tiempo de retardo para ejecutar 
cada acción simulando un delay*/

const int wait_mlx90614 = 25000;// Indica la espera cada 25 segundos para envío de mensajes MQTT
const int wait_MAX = 35000; // Indica la espera cada 35 segundos para envío de mensajes MQTT del max30102.

/*Variables de conectividad wifi*/

int flashLedPin = 2;  // Para indicar el estatus de conexión
int statusLedPin = 19; // Para ser controlado por MQTT
unsigned long timeLast;// Variable de control de tiempo no bloqueante
unsigned long timeLast_MAX;// Variable de control de tiempo no bloqueante para el sensor max30102
  
//Variable necesaria para la utilización de telegram.
int numNewMessages;//variable auxiliar para saber si se hha enviado un nuevo mensaje
String chat_id;//variable que permite que el envio de los mensajes se lleve acabo.

/*-----------Sección de telegram---------------------------------------------------*/
WiFiClientSecure secured_client;//definición del objeto
UniversalTelegramBot bot(BOT_TOKEN, secured_client);//Permite la conexión con nuestro bot de manera segura-
unsigned long bot_lasttime; // Variable que indica la ultima vez que se realizó el escaneo de mensajes
/*-----------------Variables para comprobar status de telegram----------------*/
int SPO2andBPMstatus=0;//variable empleada para comprobar si la función del max30102 esta activa
int temperaturaStatus=0;//variable empleada pára comprobar si la función del mlx90614 esta activa

void setup() {
 Serial.begin(115200);//Inicialización del puerto serial en 115200 baudios

 //declaración de los pines que nos permitiran observar que se ha realizado la conexion a wifi.
 pinMode (flashLedPin, OUTPUT);
 pinMode (statusLedPin, OUTPUT);
 digitalWrite (flashLedPin, LOW);
 digitalWrite (statusLedPin, HIGH);

 //Envio de mensajes por serial que nos permiten conocer el status de la conectividad
 Serial.println();
 Serial.println();
 Serial.print("Conectar a ");
 Serial.println(ssid);
 
 WiFi.begin(ssid, password); // Esta es la función que realiza la conexión a WiFi
 
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

 /*Inicializamos el serial para el envio de datos del sensor max30102*/
 particleSensor.begin();//se incializa el sensor max30102
 
 //Se configura el sensor max30102 para su utilización.
  particleSensor.sensorConfiguration(/*ledBrightness=*/50, /*sampleAverage=*/SAMPLEAVG_4, \
                        /*ledMode=*/MODE_MULTILED, /*sampleRate=*/SAMPLERATE_100, \
                        /*pulseWidth=*/PULSEWIDTH_411, /*adcRange=*/ADCRANGE_16384);

  //configuración del mlx90614
  mlx.begin(); //Se inicializa la comunicación I2C
  
  delay (1000); // Esta espera es solo una formalidad antes de iniciar la comunicación con el broker

  //Conexión con el broker MQTT
  client.setServer(server, 1883); // Conectarse a la IP del broker en el puerto indicado
  client.setCallback(callback); // Activar función de CallBack, permite recibir mensajes MQTT y ejecutar funciones a partir de ellos
  delay(1500);  // Esta espera es preventiva, espera a la conexión para no perder información

  /*-------------------------Sección conexión TELEGRAM configuraciones----------------------------------*/
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // agregar root certificado para api.telegram.org
  //sección que permite establecer la conectividad con telegram.
  Serial.print("Tiempo de recuperacion: ");
  configTime(0, 0, "pool.ntp.org"); // obtener la hora UTC a través de  NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void loop() {
 timeNow = millis(); // Control de tiempo para esperas no bloqueantes del mlx90614
 timeNow_MAX=millis();//Control de tiempo para esperas no bloqueantes del max30102
 
/*---------------MQTT-----------------------------------------------------------------*/
 //Verificar siempre que haya conexión al broker
  if (!client.connected()) {
    reconnect();  // En caso de que no haya conexión, ejecutar la función de reconexión, definida despues del void setup ()
  }// fin del if (!client.connected())
  client.loop(); // Esta función es muy importante, ejecuta de manera no bloqueante las funciones necesarias para la comunicación con el broker

  /*-----------------Looop de conexión TELEGRAM--------------------------------------------------*/
  //Esta parte es fundamental para el envio de mensajes en telegram, sin ella esto no sería posible.
  if (millis() - bot_lasttime > BOT_MTBS){
 
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);//nos inidica si se ha recibido un mensaje

    while (numNewMessages)//mientras se recibe el mensaje se ejecuta la acción solicitad
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  
/*------------------------Rutina para llamar a las funciones de los sensores---------------------------------------------------*/
int interruptor_sensores=0;//se utiliza esta variable como un interruptor para los sensores que nos permite utilizar la estructura switch-case


/*-------------------------Sección de condicionales para la rutina de las funciones de los sensores----------------------------*/
if(temperaturaStatus==1){
  interruptor_sensores=1;//si el sensor de temperatura esta encendido entonces el interruptor esta en 1
}
if(SPO2andBPMstatus==1){
  interruptor_sensores=2;//si el SPO2 y BPM estan encendidos entonces el interruptor esta en 2
}
if (SPO2andBPMstatus==1 && temperaturaStatus==1){
  interruptor_sensores=3;// si el SPO2, BPM y temperatura estan encendidos entonces el interruptor esta en 3
}

//La estructrura switch-case funciona como un control que n0s permite activar las funciones para el funcionamiento del dipositivo.
switch (interruptor_sensores) {
  case 1: //si el interruptor esta en 1 se activa la medición de la temperatura 
    MLX90614();//se llama a la función que permite el funcionamiento del sensor de temperatura
    break;
  case 2://si el interruptor esta en 2 se activa la medición de la oxigenación y bpm
    MAX30102();//se llama a la función que permite el funcionamiento del sensor max30102
    break;
  case 3://si el interruptor esta en 3 se activa el funcionamiento de ambos sensores teniendo así la lectura de la temperatura, bpm y oxigenación.
    MAX30102();
    MLX90614();
    break;
}
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
  String messageTemp; // Se declara la variable en la cual se generará el mensaje completo para la temperatura
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
  // El ESP32 está suscrito al tema SignosVitales/Temperatura/CasaRetiro1"
  if (String(topic) == "SignosVitales/Temperatura/CasaRetiro1") {  // En caso de recibirse mensaje en el tema SignosVitales/Temperatura/CasaRetiro1
    if(messageTemp == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro1")
    else if(messageTemp == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro1")


//-------------------------------------------------------------------------------------------------
  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageOX; // Se declara la variable en la cual se generará el mensaje completo para la oxigenación  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageOX += (char)message[i];//se concatena el mensaje para la oxigenación.
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageOX);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32C está suscrito al tema SignosVitales/Oxigenacion/CasaRetiro1
  if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1") {  // En caso de recibirse mensaje en el tema SignosVitales/Oxigenacion/CasaRetiro1
    if(messageOX == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1")
    else if(messageOX == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Oxigenacion/CasaRetiro1")



  // Indicar por serial que llegó un mensaje
  Serial.print("Llegó un mensaje en el tema: ");
  Serial.print(topic);

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageBPM; // Se declara la variable en la cual se generará el mensaje completo para el bpm 
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    Serial.print((char)message[i]);
    messageBPM += (char)message[i];//se concatena el mensaje para el bpm
  }

  // Se comprueba que el mensaje se haya concatenado correctamente
  Serial.println();
  Serial.print ("Mensaje concatenado en una sola variable: ");
  Serial.println (messageBPM);

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT

  // Ejemplo, en caso de recibir el mensaje true - false, se cambiará el estado del led soldado en la placa.
  // El ESP32 está suscrito al tema SignosVitales/bpm/CasaRetiro1
  if (String(topic) == "SignosVitales/bpm/CasaRetiro1") {  // En caso de recibirse mensaje en el tema SignosVitales/bpm/CasaRetiro1
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

/*--------------------------------Función para el sensor mlx90614-------------------------------------------------------------------*/
void MLX90614(){
   if (timeNow - timeLast > wait_mlx90614) { // Manda un mensaje por MQTT cada 25 segundos
    timeLast = timeNow; // Actualización de seguimiento de tiempo
    //Lectura del sensor de temperatura mlx90614 sin contemplar el error
    TempMed=mlx.readObjectTempC();//Lectura del sensor
    TempReal=TempMed+error_temp;//lectura tomando en cuenta el error   
    
   char dataString[8]; // Define una arreglo de caracteres para enviarlos por MQTT, especifica la longitud del mensaje en 8 caracteres
   
   //Sección que evita el ruido excesivo en el envío de datos es decir datos no reales.
   Serial.print(TempReal);
    if(TempReal<32 || TempReal>42.5 ){
      TempReal=0; 
    }
    Serial.println(chat_id);
    Serial.println(numNewMessages);


    dtostrf(TempReal, 1, 2, dataString);  // Esta es una función nativa de leguaje AVR que convierte un arreglo de caracteres en una variable String
    Serial.print("La temperarura es: "); // Se imprime en monitor solo para poder visualizar que el evento sucede
    Serial.println(dataString);//imprime los datos string en el monitor serial
    Serial.println();//unicamente se imprime un espacio
    delay(1000);//se genera una espera no bloqueante para el envio de datos.
    client.publish("SignosVitales/Temperatura/CasaRetiro1", dataString); // Esta es la función que envía los datos por MQTT, especifica el tema y el valor. Es importante cambiar el tema para que sea unico en este caso el nuestro es SignosVitales/Temperatura/casaRetiro1, pero es necesario modificar esto en otros proyectos
  
    //Sección que permite el envío de los mensajes de alerta en el chatbot de telegram si se esta en los niveles preocupantes.
    for (int j=1;j<=3;j++){//solo enviara los mensajes de alerta 3 veces en caso de que se presente una situación de riesgo
      if(TempReal<36.5 && TempReal>32){
        bot.sendMessage(chat_id, "PRECAUCIÓN: Temperatura BAJA\n", "");
        bot.sendMessage(chat_id, "La temperatura es: " + String(TempReal)+" °C");
        }
      else if(TempReal>37.5){
        bot.sendMessage(chat_id, "PRECAUCIÓN: Temperatura ALTA", "");
        bot.sendMessage(chat_id, "La temperatura es: " + String(TempReal)+" °C");
        }
    }
      
  }// fin del if (timeNow - timeLast > wait_mlx90614)  
}


/*--------------------------------Función para el sensor max30102-------------------------------------------------------------------*/
void MAX30102()
{
 if (timeNow_MAX - timeLast_MAX >wait_MAX) {// Manda un mensaje por MQTT cada 35 segundos
  timeLast_MAX=timeNow_MAX;
  particleSensor.heartrateAndOxygenSaturation(/**SPO2=*/&SPO2, /**SPO2Valid=*/&SPO2Valid, /**heartRate=*/&heartRate, /**heartRateValid=*/&heartRateValid);
  char dataStringspo2[8];
  char dataStringhb[8];
  //Esta sección nos ayuda a evitar un poco de ruido del sensor MAX30102
  oxt=SPO2-error_ox;
  bpmt=heartRate-error_bpm;

 //Sección que evita el ruido excesivo en el envío de datos es decir datos no reales.
 if(oxt<0 || oxt>100){
    oxt=0;
    }
 if(bpmt<30 || bpmt>250){
    bpmt=0;
    }
   
  dtostrf(oxt, 1, 2, dataStringspo2);// Esta es una función nativa de leguaje AVR que convierte un arreglo de caracteres en una variable String para la ocigenación
  dtostrf(bpmt, 1, 2, dataStringhb);// Esta es una función nativa de leguaje AVR que convierte un arreglo de caracteres en una variable String para el bpm
  Serial.print("SPO2: "); // Se imprime en monitor solo para poder visualizar que el evento sucede
  Serial.println(dataStringspo2);//imprime los datos string en el monitor serial del spo2
  Serial.print("Bpm: "); // Se imprime en monitor solo para poder visualizar que el evento sucede
  Serial.println(dataStringhb);//imprime los datos string en el monitor serial del bpm
  client.publish("SignosVitales/Oxigenacion/CasaRetiro1", dataStringspo2); // Esta es la función que envía los datos por MQTT, especifica el tema y el valor. Es importante cambiar el tema para que sea unico en este caso el nuestro es SignosVitales/Oxigenacion/CasaRetiro1, pero es necesario modificar esto en otros proyectos
  client.publish("SignosVitales/bpm/CasaRetiro1", dataStringhb);// Esta es la función que envía los datos por MQTT, especifica el tema y el valor. Es importante cambiar el tema para que sea unico en este caso el nuestro es SignosVitales/bpm/CasaRetiro1, pero es necesario modificar esto en otros proyectos
 }
//Sección que permite el envío de los mensajes de alerta en el chatbot de telegram si se esta en los niveles preocupantes.
   for (int j=1;j<=3;j++){//solo enviara los mensajes de alerta 3 veces en caso de que se presente una situación de riesgo
    if(oxt<90 && oxt>0){//si el spo2 es menor a 90 entonces se envian el mensaje
      bot.sendMessage(chat_id, "PRECAUCIÓN: Oxigenacion BAJA", "");
      bot.sendMessage(chat_id, "La Oxigenacion es: " + String(oxt));
      }
    if(bpmt<50 && bpmt>30){//si el bpm es menor a 50 pero mayor a 30 entonces tenemos una bpm baja
      bot.sendMessage(chat_id, "PRECAUCIÓN: BPM BAJA", "");
      bot.sendMessage(chat_id, "El BPM es:" + String(bpmt));
      }else if(bpmt>100){//si el bpm es mayor a 100 entonces tenemos una situacion de riesgo porque el bpm es alto.
      bot.sendMessage(chat_id, "PRECAUCIÓN: BPM ALTA", "");
      bot.sendMessage(chat_id, "El BPM es:" + String(bpmt));
      }
   } 
}

/*---------------------Sección de envío de mensaje telegram--------------------------*/
void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);//se envia al monitor serial si se ha recibido un nuevo mensaje

  for (int i = 0; i < numNewMessages; i++)//estructura que nos permite recorrer el mensaje que se ha enviado
  {
    //Sección que nos permite identificar el mensaje que nos ha enviado el usuario.
    chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    /*---Esta sección nos permite conocer el nombre del usuario que se comunica con el chat bot----------------*/
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    //condicionales para la variable text que nos permite cambiar el status para el funcionamiento de los sensores

    //se enciende la función de medición de temperatura
    if (text == "/TemperaturaON")
    {
      temperaturaStatus= 1;//cambia el status para la medición de temperatura
      Serial.print("temperaturaStatus= ");
      Serial.println(temperaturaStatus);
    }

    //se apaga la función de medición de temperatura
    if (text == "/TemperaturaOFF")
    {
      temperaturaStatus= 0;//cambia el status para la medición de temperatura
      Serial.print("temperaturaStatus= ");
      Serial.println(temperaturaStatus);
    }
    
    //se enciende la función de medición de bpm y spo2
    if (text == "/SPO2andBPMON")
    {
      SPO2andBPMstatus= 1;//cambia el status para la medición de spo2 y bpm
      Serial.print("SPO2andBPMstatus= ");
      Serial.println(SPO2andBPMstatus);
    }
    //se apaga la función de medición de bpm y spo2
    if (text == "/SPO2andBPMOFF")
    {
      SPO2andBPMstatus= 0;//cambia el status para la medición del spo2 y bpm
      Serial.print("SPO2andBPMstatus= ");
      Serial.println(SPO2andBPMstatus);
    }//los status de las funciones son muy importantes porque en el programa principal loop mediante la estructura switch-case nos permite tener un control del sistema

    //Estructura de if´s-else´s para conocer el status de nuestros sensores
    if (text == "/status")
    {
      if (temperaturaStatus==1)//si la el sensor de temperatura se encuentra activo entonces se envia el mensaje de que el sensor de temperatura esta activo.
      {
        bot.sendMessage(chat_id, "temperatura is ON", "");
      }
      else //si no esta encendido entonces sabemos que esta apagado y enviamos dicho mensaje
      {
        bot.sendMessage(chat_id, "temperatura is OFF", "");
      }
      if (SPO2andBPMstatus==1)//si el sensor max30102 se encuentra activo entonces enviamos el mensaje de que los servicios de medición de spo2 y bpm estan activos.
      {
        bot.sendMessage(chat_id, "SPO2andBPM is ON", "");
      }
      else//si no esta activo entonces esta apagado.
      {
        bot.sendMessage(chat_id, "SPO2andBPM is OFF", "");
      }
    }
   
    //Inicio de la comunicación con el chatbot, en esta parte se envia un mensaje inicial que presenta el menú de utilización del sistema
    if (text == "/start")
    {
      String welcome = "Bienvenido a tu servicio de monitoreo de salud " + from_name + ".\n";
      welcome += "Este servicio te pemitirá conocer tu Temperatura, SPO2 y BPM\n";
      welcome += "Selecciona el texto en azul, segun sea el caso:\n";
      welcome += "/TemperaturaON: Para visualizar tu temperatura\n";
      welcome += "/TemperaturaOFF: Para dejar de tomar la temperatura\n";
      welcome += "/SPO2andBPMON: Para visualizar la oxigenación y los latidos por minuto\n";
      welcome += "/SPO2andBPMOFF: Para dejar de visualizar la oxigenación y los latidos por minuto\n";
      welcome += "/status: Para conocer la función que tienes activa\n";
      welcome += "Instrucciones de uso: \n";
      welcome += "1. Elegir el signo vital a medir. \n";
      welcome += "Para ello seleccione el comando /TemperaturaON o /SPO2andBPMON segun sea el caso \n";
      welcome += "Es necesario apagar la medicion seleccionada con el comando /TemperaturaOFF o /SPO2andBPMOFF segun sea el caso.\n";
      welcome += "Una vez que ya no se requiera su uso.\n";
      welcome += "**NOTA:**EL ULTIMO VALOR MEDIDO POR LOS SENSORES SERÁ EL ALMACENADO \n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}
