#include <NewPing.h>

// Pines del Hardware
const int E1 = 5;  // PWM Motor
const int M1 = 4;  // Dirección Motor
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

const int MAX_DISTANCE = 80; 
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// --- PARÁMETROS DEL SISTEMA DE CONTROL ---
float SETPOINT_OBJETIVO = 10.0; 
float setpointActual = 10.0;  
const float VELOCIDAD_RAMPA = 0.25; 

// Variables PID (Modificables desde Python)
float Kp = 8;      
float Ki = 0.75;     
float Kd = 3.2;      
const int PWM_BASE = 145; // Feedforward para contrarrestar la gravedad

// Variables de estado y filtros
bool sistemaEncendido = false;
float input = 0.0;     // Distancia de la pelota (Filtrada)
float lastInput = 0.0; // Memoria para la Derivada
float integralSum = 0.0;
int pwmCalculado = 0;

// Control de tiempo para bucle no bloqueante
unsigned long lastTime = 0;
const int sampleTime = 20; // El PID se ejecuta cada 15 ms
const float alpha = 0.4;   // Factor de filtro EMA (0.0 a 1.0)

void setup() {
  // 115200 baudios es crítico para enviar datos fluidos a la GUI
  Serial.begin(115200); 
  Serial.setTimeout(10); // Evita que Serial.readStringUntil bloquee el PID
  
  pinMode(E1, OUTPUT);
  pinMode(M1, OUTPUT);
  
  // Sentido de giro fijo (según tu código original)
  digitalWrite(M1, LOW); 
}

void loop() {
  // 1. GESTIÓN DE COMUNICACIÓN CON PYTHON (No bloqueante)
  if (Serial.available() > 0) {
    String entrada = Serial.readStringUntil('\n');
    
    // Comando de Estado (ESTADO:1 o ESTADO:0)
    if (entrada.startsWith("ESTADO:")) {
      int estado = entrada.substring(7).toInt();
      sistemaEncendido = (estado == 1);
      
      if (!sistemaEncendido) {
        analogWrite(E1, 0);       // Apagar motor de inmediato
        integralSum = 0;          // Limpiar memoria PID (Anti-Windup)
        setpointActual = SETPOINT_OBJETIVO; // Reiniciar rampa
      }
    } 
    // Comando de Parámetros PID (P:setpoint,Kp,Ki,Kd)
    else if (entrada.startsWith("P:")) {
      entrada.remove(0, 2); 
      
      int coma1 = entrada.indexOf(',');
      int coma2 = entrada.indexOf(',', coma1 + 1);
      int coma3 = entrada.indexOf(',', coma2 + 1);
      
      if (coma1 > 0 && coma2 > 0 && coma3 > 0) {
        float nuevoSetpoint = entrada.substring(0, coma1).toFloat();
        if (nuevoSetpoint >= 0 && nuevoSetpoint <= 40) {
            SETPOINT_OBJETIVO = nuevoSetpoint;
        }
        Kp = entrada.substring(coma1 + 1, coma2).toFloat();
        Ki = entrada.substring(coma2 + 1, coma3).toFloat();
        Kd = entrada.substring(coma3 + 1).toFloat();
      }
    }
  }

  // 2. BUCLE DE CONTROL DE TIEMPO ESTRICTO
  unsigned long now = millis();
  unsigned long dt = now - lastTime;

  if (dt >= sampleTime) {
    
    // --- LECTURA Y FILTRADO DEL SENSOR ---
    float rawDistance = sonar.ping_cm();
    
    // Manejo de lectura nula o pelota fuera de rango físico máximo (50 cm absolutos)
    if (rawDistance == 0 || rawDistance > 70.0) {
      rawDistance = 70.0; 
    }
    
    // Ajuste de tu CERO FÍSICO: Restamos los 7.5 cm de la parte superior
    rawDistance = rawDistance - 7.5;
    
    // Evitar que la distancia sea negativa si la pelota sube más allá de la marca 0
    if (rawDistance < 0.0) {
      rawDistance = 0.0;
    }
    
    // Suavizado de la lectura (Low-Pass Filter)
    input = (alpha * rawDistance) + ((1.0 - alpha) * lastInput);

    // --- LÓGICA PID ---
    if (sistemaEncendido) {
      
      // La rampa está comentada actualmente, va directo al objetivo
      setpointActual = SETPOINT_OBJETIVO;

      // B. Rescate si la pelota está en el fondo (ajustado a la nueva escala relativa)
      // Si el máximo útil es 40, a partir de 42 consideramos que se cayó
      if (input >= 50.0) {
        pwmCalculado = 235; 
        integralSum = 0.0; // Evita inestabilidad al subir
      } 
      else {
        // Error positivo = Pelota demasiado baja (distancia mayor al setpoint)
        float error = input - setpointActual;
        float dt_sec = dt / 1000.0;

        // Proporcional
        float pTerm = Kp * error;

        // Derivativo sobre la MEDICIÓN (Mitiga Derivative Kick)
        float dTerm = Kd * ((input - lastInput) / dt_sec);

        // --- SOLUCIÓN: ANTI-WINDUP POR INTEGRACIÓN CONDICIONAL ---
        // 1. Calculamos cuál sería el esfuerzo SIN sumar nueva integral
        float esfuerzoPrevio = PWM_BASE + pTerm + integralSum + dTerm;

        // 2. Evaluamos si el actuador ya está saturado y el error sigue empujando en esa dirección
        bool saturadoArriba = (esfuerzoPrevio >= 255.0 && error > 0);
        bool saturadoAbajo = (esfuerzoPrevio <= 10.0 && error < 0);

        // 3. Solo integramos si NO estamos agravando la saturación
        if (!saturadoArriba && !saturadoAbajo) {
          // Método discreto Euler hacia adelante
          integralSum += (Ki * error * dt_sec);
          
          // Mantenemos tu límite duro por seguridad extra
          integralSum = constrain(integralSum, -50.0f, 50.0f); 
        }

        // Salida = Esfuerzo base (Gravedad) + Esfuerzo PID
        float salidaFlotante = PWM_BASE + pTerm + integralSum + dTerm;
        
        // Saturación final segura para el motor
        pwmCalculado = constrain((int)salidaFlotante, 10, 255);
      }

      // Aplicar potencia
      analogWrite(E1, pwmCalculado);
    }

    // Actualizar memoria para el próximo ciclo
    lastInput = input;
    lastTime = now;

    // --- MONITOREO SERIAL (Para la gráfica de Python) ---
    // Enviamos el Setpoint Actual, la Distancia y el PWM separados por comas
    Serial.print(setpointActual);
    Serial.print(",");
    Serial.print(input); 
    Serial.print(",");
    Serial.println(pwmCalculado);
  }
}