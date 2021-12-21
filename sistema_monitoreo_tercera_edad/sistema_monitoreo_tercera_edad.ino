
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
/*Definimos los pines necesarios para el sensor MLX90614
    SensorMLX90614------PinESP32CAM
    Vin-----------------3.3v
    GND-----------------GND
    SCL-----------------GPIO15
    SDA-----------------GPIO14
*/
#define I2C_SDA 14
#define I2C_SCL 15
/*creamos la instancia i2c*/
TwoWire I2CSensors = TwoWire(0);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

/*Declaración de las variables que nos serviran como temporizadores*/
unsigned long TimeNow=0;
unsigned long PreviousTime_MLX90614=0;
/*Declaración de las variables que nos permiten determinar el tiempo de retardo para ejecutar 
cada acción simulando un delay*/
const int Time_MLX90614=15000;//Declaramos que la medición del sensor de temperatura se ejecute cada 15s


void setup() {
 Serial.begin(115200);  
 mlx.begin();
 /*Inicializamos los pines del ESP32CAM como SDA y SCL*/
 I2CSensors.begin(I2C_SDA, I2C_SCL, 100000);
    
}

void loop() {
 TimeNow=millis();
 MLX90614();//llamamos a la función del sensor de temperatura
 
}
/*Generamos las funciones de cada sensor para tener un código más ordenado */
void MLX90614(){
  if((TimeNow-PreviousTime_MLX90614)>=Time_MLX90614){
    PreviousTime_MLX90614=TimeNow;
    Serial.print("*C\tObject = "); Serial.print(mlx.readObjectTempC()); Serial.println("*C");
    Serial.println();
  }
}
