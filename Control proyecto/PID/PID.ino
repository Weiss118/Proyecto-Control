/*
  Control PID con Setpoint Suave (Rampa de Transición)
  Ajustado para Tubo Largo (Juego de 40cm) - ADAPTADO PARA PYTHON GUI
*/

// Pines del Motor (Puente H)
const int E1 = 5; 
const int M1 = 4; 

// Pines del Sensor Ultrasónico
const int trigPin = 9;
const int echoPin = 10;

// PARÁMETROS DEL SISTEMA DE CONTROL
int SETPOINT_OBJETIVO = 10; 
float setpointActual = 10.0;  

const float VELOCIDAD_RAMPA = 0.25; 

// Variables PID (Modificables desde Python)
float Kp = 5.5;      
float Ki = 0.45;     
float Kd = 2.2;      
const int PWM_BASE = 145; 

// Variables de control
long duracion;
int distancia;
int ultimoError = 0;
float errorAcumulado = 0;
int pwmCalculado;

// Variable de estado (Encendido/Apagado desde GUI)
bool sistemaEncendido = false;

void setup() {
  Serial.begin(9600);
  pinMode(E1, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  digitalWrite(M1, LOW); 
}

void loop() {
  // 1. LEER EL PUERTO SERIAL (Escuchando a Python)
  if (Serial.available() > 0) {
    String entrada = Serial.readStringUntil('\n');
    
    // Si Python manda orden de Encender/Apagar (ESTADO:1 o ESTADO:0)
    if (entrada.startsWith("ESTADO:")) {
      int estado = entrada.substring(7).toInt();
      sistemaEncendido = (estado == 1);
      
      // ACCIÓN INMEDIATA AL APAGAR: 
      // Limpiamos la memoria del PID y apagamos el motor de golpe
      if (!sistemaEncendido) {
        analogWrite(E1, 0);
        errorAcumulado = 0; 
        ultimoError = 0;
        setpointActual = SETPOINT_OBJETIVO; // Reiniciar la rampa
      }
    } 
    // Si Python manda parámetros (Formato -> P:distancia,Kp,Ki,Kd)
    else if (entrada.startsWith("P:")) {
      entrada.remove(0, 2); // Quitar el "P:" inicial
      
      int coma1 = entrada.indexOf(',');
      int coma2 = entrada.indexOf(',', coma1 + 1);
      int coma3 = entrada.indexOf(',', coma2 + 1);
      
      if (coma1 > 0 && coma2 > 0 && coma3 > 0) {
        int nuevoSetpoint = entrada.substring(0, coma1).toInt();
        if (nuevoSetpoint >= 1 && nuevoSetpoint <= 45) {
            SETPOINT_OBJETIVO = nuevoSetpoint;
        }
        Kp = entrada.substring(coma1 + 1, coma2).toFloat();
        Ki = entrada.substring(coma2 + 1, coma3).toFloat();
        Kd = entrada.substring(coma3 + 1).toFloat();
      }
    }
  }

  // 2. LEER LA DISTANCIA ACTUAL (Siempre se lee para que la gráfica en Python no se detenga)
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  duracion = pulseIn(echoPin, HIGH);
  distancia = (duracion * 0.0177) - 0.70;
  
  if (distancia > 60 || distancia <= 0) {
    distancia = 52; 
  }

  // 3. LÓGICA DE CONTROL (Solo se ejecuta si está encendido)
  if (sistemaEncendido) {
    
    // Generar la rampa suave para el setpoint
    if (setpointActual < SETPOINT_OBJETIVO) {
      setpointActual += VELOCIDAD_RAMPA; 
      if (setpointActual > SETPOINT_OBJETIVO) setpointActual = SETPOINT_OBJETIVO;
    } 
    else if (setpointActual > SETPOINT_OBJETIVO) {
      setpointActual -= VELOCIDAD_RAMPA; 
      if (setpointActual < SETPOINT_OBJETIVO) setpointActual = SETPOINT_OBJETIVO;
    }

    // Algoritmo PID
    if (distancia >= 47) {
      pwmCalculado = 235; 
      errorAcumulado = 0; 
      ultimoError = 0;
    } 
    else {
      float error = distancia - setpointActual;
      errorAcumulado += error;
      errorAcumulado = constrain(errorAcumulado, -150, 150); 
      
      float diferenciaError = error - ultimoError;
      float P = error * Kp;
      float I = error 
      pwmCalculado = PWM_BASE + P + I + D;
      ultimoError = error;
    }

    if (distancia < 47) {
      pwmCalculado = constrain(pwmCalculado, 110, 255); 
    } else {
      pwmCalculado = constrain(pwmCalculado, 0, 255);
    }

    // Aplicar potencia al motor
    analogWrite(E1, pwmCalculado);
  }

  // 4. MONITOREO SERIAL (Exclusivo para la gráfica de Python)
  Serial.println(distancia);

  delay(60); 
}
