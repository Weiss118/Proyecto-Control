#include <NewPing.h>

// Pines del Hardware
const int E1 = 5;  // PWM Motor
const int M1 = 4;  // Dirección Motor
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

// Configuración del Sensor (Distancia ajustada para tubo de 40cm)
const int MAX_DISTANCE = 50; 
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// --- PARÁMETROS DEL SISTEMA DE CONTROL ---
float SETPOINT_OBJETIVO = 10.0; 
float setpointActual = 10.0;  
const float VELOCIDAD_RAMPA = 0.25; 

// Variables PID (Modificables desde Python)
float Kp = 5.5;      
float Ki = 0.45;     
float Kd = 2.2;      
const int PWM_BASE = 145; // Feedforward para contrarrestar la gravedad

// Variables de estado y filtros
bool sistemaEncendido = false;
float input = 0.0;     // Distancia de la pelota (Filtrada)
float lastInput = 0.0; // Memoria para la Derivada
float integralSum = 0.0;
int pwmCalculado = 0;

// Control de tiempo para bucle no bloqueante
unsigned long lastTime = 0;
const int sampleTime = 50; // El PID se ejecuta cada 50 ms
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
        if (nuevoSetpoint >= 1 && nuevoSetpoint <= 45) {
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
    // Manejo de lectura nula o pelota fuera de rango
    if (rawDistance == 0 || rawDistance > 60) {
      rawDistance = 52.0; 
    }
    
    // Suavizado de la lectura (Low-Pass Filter)
    input = (alpha * rawDistance) + ((1.0 - alpha) * lastInput);

    // --- LÓGICA PID ---
    if (sistemaEncendido) {
      
      // A. Rampa suave del Setpoint
      if (setpointActual < SETPOINT_OBJETIVO) {
        setpointActual += VELOCIDAD_RAMPA; 
        if (setpointActual > SETPOINT_OBJETIVO) setpointActual = SETPOINT_OBJETIVO;
      } 
      else if (setpointActual > SETPOINT_OBJETIVO) {
        setpointActual -= VELOCIDAD_RAMPA; 
        if (setpointActual < SETPOINT_OBJETIVO) setpointActual = SETPOINT_OBJETIVO;
      }

      // B. Rescate si la pelota está en el fondo
      if (input >= 47) {
        pwmCalculado = 235; 
        integralSum = 0; // Evita inestabilidad al subir
      } 
      else {
        // Asumiendo sensor en la parte superior: 
        // Error positivo = Pelota demasiado baja (distancia mayor al setpoint)
        float error = input - setpointActual;
        
        // Conversión del tiempo a segundos para independizar Ki y Kd
        float dt_sec = dt / 1000.0;

        // Proporcional
        float pTerm = Kp * error;

        // Integral con Anti-Windup (+/- 50 puntos de PWM máximo)
        integralSum += (Ki * error * dt_sec);
        integralSum = constrain(integralSum, -50.0, 50.0); 
        
        // Derivativo sobre la MEDICIÓN para mitigar el "Derivative Kick"
        float dTerm = Kd * ((input - lastInput) / dt_sec);

        // Salida = Esfuerzo base (Gravedad) + Esfuerzo PID
        float salidaFlotante = PWM_BASE + pTerm + integralSum + dTerm;
        
        // Saturación segura para el motor
        pwmCalculado = constrain((int)salidaFlotante, 110, 255);
      }

      // Aplicar potencia
      analogWrite(E1, pwmCalculado);
    }

    // Actualizar memoria para el próximo ciclo
    lastInput = input;
    lastTime = now;

    // --- MONITOREO SERIAL (Para la gráfica de Python) ---
    // Enviamos el Setpoint Actual y la Distancia separados por una coma
    Serial.print(setpointActual);
    Serial.print(",");
    Serial.println(input); 
  }
}
