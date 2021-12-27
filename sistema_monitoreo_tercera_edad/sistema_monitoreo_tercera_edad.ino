/* Fecha: 21 de diciembre de 2021.
 * 
 * Este programa ee el sensor ultrasonico con la biblioteca de Erick Simoe.
 * En su versión 3.0.0 se mez
 * 
 * 
 
 */

/*Bibliotecas necesarias para la ejecución del programa*/
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <WiFi.h>  // Biblioteca para el control de WiFi
#include <PubSubClient.h> //Biblioteca para conexion MQTT
/*Definimos los pines necesarios para el sensor MLX90614
    SensorMLX90614------PinESP32CAM
    Vin-----------------3.3v
    GND-----------------GND
    SCL-----------------GPIO15
    SDA-----------------GPIO14
*/
#define I2C_SDA 14
#define I2C_SCL 15
//---------------------------Conectividad---------------------------------------------------
//Datos de WiFi
const char* ssid = "ARRIS-DE42" ;// Aquí debes poner el nombre de tu red
const char* password = "57E2F502D07D0720";  // Aquí debes poner la contraseña de tu red

//Datos del broker MQTT
const char* mqtt_server = "3.65.154.195"; // Si estas en una red local, coloca la IP asignada, en caso contrario, coloca la IP publica
IPAddress server(3,65,154,195);

// Objetos
WiFiClient espClient; // Este objeto maneja los datos de conexion WiFi
PubSubClient client(espClient); // Este objeto maneja los datos de conexion al broker

//-------------------Data sensors-----------------------------------------------------------------
/*creamos la instancia i2c*/
TwoWire I2CSensors = TwoWire(0);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
//Variables sensor MLX90614
float TempMed;
float TempReal;
/*Declaración de las variables que nos serviran como temporizadores*/
unsigned long timeNow;//Almacenaremos el tiempo en ms
unsigned long PreviousTime_MLX90614=0;
unsigned long timeLast_sensors;// Variable que nos permite controlar el tiempo de los sensores junto con previous
/*Declaración de las variables que nos permiten determinar el tiempo de retardo para ejecutar 
cada acción simulando un delay*/
const int Time_MLX90614=5000;//Declaramos que la medición del sensor de temperatura se ejecute cada 15s
/*Variables de conectividad wifi*/
int flashLedPin = 4;  // Para indicar el estatus de conexión
int statusLedPin = 33; // Para ser controlado por MQTT
unsigned long timeLast;// Variable de control de tiempo no bloqueante
const int wait = 5000;  // Indica la espera cada 5 segundos para envío de mensajes MQTT

void setup() {
 Serial.begin(115200);
 //inicializamos al sensor mlx90614
 mlx.begin();
 /*Inicializamos los pines del ESP32CAM como SDA y SCL*/
 I2CSensors.begin(I2C_SDA, I2C_SCL, 100000);
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

  timeLast = millis (); // Inicia el control de tiempo
}

void loop() {
 timeLast_sensors=millis();
 timeNow = millis(); // Control de tiempo para esperas no bloqueantes
 MLX90614();//llamamos a la función del sensor de temperatura
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
      client.subscribe("SignosVitales/Temperatura/CasaRetiro"); // Esta función realiza la suscripción al tema
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
  if (String(topic) == "SignosVitales/Temperatura/CasaRetiro") {  // En caso de recibirse mensaje en el tema codigoiot/respues/xaviergrajales
    if(messageTemp == "true"){
      Serial.println("Led encendido");
      digitalWrite(flashLedPin, HIGH);
    }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")
    else if(messageTemp == "false"){
      Serial.println("Led apagado");
      digitalWrite(flashLedPin, LOW);
    }// fin del else if(messageTemp == "false")
  }// fin del if (String(topic) == "SignosVitales/Temperatura/CasaRetiro"")
}

/*--------------------------------Sección de funciones de los sensores--------------------------------------------------------------*/
/*Generamos las funciones de cada sensor para tener un código más ordenado */
void MLX90614(){
  /*if((timeLast_sensors-PreviousTime_MLX90614)>=Time_MLX90614){
    PreviousTime_MLX90614=timeLast_sensors;
    TempMed=mlx.readObjectTempC();
    TempReal=TempMed+4.38;
    Serial.print("*C\tObject = "); Serial.print(TempReal); Serial.println("*C");
    Serial.println();
  }*/
   if (timeNow - timeLast > wait) { // Manda un mensaje por MQTT cada cinco segundos
    timeLast = timeNow; // Actualización de seguimiento de tiempo

    //Lectura del sensor de temperatura mlx90614 sin contemplar el error
    TempMed=mlx.readObjectTempC();//Lectura del sensor
    TempReal=TempMed+4.38;//lectura tomando en cuenta el error    
    char dataString[8]; // Define una arreglo de caracteres para enviarlos por MQTT, especifica la longitud del mensaje en 8 caracteres
    dtostrf(TempReal, 1, 2, dataString);  // Esta es una función nativa de leguaje AVR que convierte un arreglo de caracteres en una variable String
    Serial.print("La temperarura es: "); // Se imprime en monitor solo para poder visualizar que el evento sucede
    Serial.println(dataString);
    client.publish("SignosVitales/Temperatura/CasaRetiro_1", dataString); // Esta es la función que envía los datos por MQTT, especifica el tema y el valor. Es importante cambiar el tema para que sea unico SignosVitales/Temperatura/Temaconotronombre
  }// fin del if (timeNow - timeLast > wait)  
}
