/*
  Control PID Simplificado - Levitador de Aire
  Versión con ganancias suavizadas para escala cruda (microsegundos).
*/

// Pines del Motor (Puente H)
const int pinPWM = 5; 
const int pinDireccion = 4; 

// Pines del Sensor Ultrasónico
const int trigPin = 9;
const int echoPin = 10;

// PARÁMETROS PID ADAPTADOS 
float Kp = 0.25; 
float Ki = 0.01;
float Kd = 0.7;

// float Kp = 0.7;
// float Ki = 0.9;
// float Kd = 1.0;

// Variables de Control
int setpoint = 2000;    
float errorAcumulado = 0;
int ultimoError = 0;

/* 
  NOTA SOBRE PWM_BASE: Ajusta este valor al número exacto de PWM donde 
  el motor empieza a levantar la pelota. Si 145 es mucho, bájalo a 110-120.
*/
const int PWM_BASE = 140; 

// Límites Anti-Windup (Ajustado a la nueva escala de Ki)
const float limiteIntegral = 300.0; 

void setup() {
  Serial.begin(9600);
  pinMode(pinPWM, OUTPUT);
  pinMode(pinDireccion, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  digitalWrite(pinDireccion, LOW); 
}

void loop() {
  // 1. LECTURA DEL SENSOR
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long valorReal = pulseIn(echoPin, HIGH);

  // Filtro rápido por si el sensor lee un cero o un ruido absurdo
  if (valorReal == 0 || valorReal > 3000) {
    valorReal = setpoint; // Ignora la lectura errónea para no alterar el PID
  }

  // 2. CÁLCULO DEL ERROR
  int error = valorReal - setpoint;

  // 3. TÉRMINO INTEGRAL CON ANTI-WINDUP
  errorAcumulado += error;
  errorAcumulado = constrain(errorAcumulado, -limiteIntegral, limiteIntegral);

  // 4. TÉRMINO DERIVATIVO
  int diferenciaError = error - ultimoError;
  ultimoError = error;

  // 5. CÁLCULO DE SALIDA PID
  int ajuste = (error * Kp) + (errorAcumulado * Ki) + (diferenciaError * Kd);
  int pwmFinal = PWM_BASE + ajuste;

  // 6. LIMITACIÓN DE SALIDA (Saturación de hardware)
  pwmFinal = constrain(pwmFinal, 0, 255);

  // 7. EJECUCIÓN
  analogWrite(pinPWM, pwmFinal);

  // 8. MONITOREO
  Serial.print("SP:"); Serial.print(setpoint);
  Serial.print(" | Real:"); Serial.print(valorReal);
  Serial.print(" | PWM:"); Serial.println(pwmFinal);

  // Reducido a 35ms para reaccionar más rápido antes de que la pelota gane inercia
  delay(15); 
}