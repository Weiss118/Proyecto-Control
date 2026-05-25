import tkinter as tk
from tkinter import ttk, messagebox
import serial
import time
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
import collections

# --- CONFIGURACIÓN SERIAL ---
# Cambia 'COM3' por el puerto donde esté conectado tu Arduino (ej. '/dev/ttyACM0' en Linux/Mac)
PUERTO_SERIAL = 'COM14'
BAUD_RATE = 9600

class LevitadorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Control PID - Levitador")
        self.root.geometry("1000x600")

        # Estado del sistema
        self.sistema_encendido = False
        
        # Datos para la gráfica (guarda los últimos 100 puntos)
        self.tiempos = collections.deque(maxlen=100)
        self.distancias = collections.deque(maxlen=100)
        self.inicio_tiempo = time.time()

        # Intento de conexión con Arduino
        try:
            self.arduino = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=0.1)
            time.sleep(2) # Esperar a que Arduino reinicie tras la conexión
        except serial.SerialException:
            self.arduino = None
            messagebox.showwarning("Advertencia", f"No se pudo conectar al puerto {PUERTO_SERIAL}. Iniciando en modo simulación/sin conexión.")

        self.crear_interfaz()

    def crear_interfaz(self):
        # --- ZONA DE GRÁFICA (Izquierda) ---
        frame_grafica = tk.Frame(self.root)
        frame_grafica.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        self.figura = Figure(figsize=(7, 5), dpi=100)
        self.ax = self.figura.add_subplot(111)
        self.ax.set_title("Comportamiento del Levitador")
        self.ax.set_xlabel("Tiempo (s)")
        self.ax.set_ylabel("Distancia")
        self.ax.set_ylim(0, 45) # Límite acotado de 0 a 45
        self.linea, = self.ax.plot([], [], 'b-')

        self.canvas = FigureCanvasTkAgg(self.figura, master=frame_grafica)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # --- ZONA DE CONTROLES (Derecha) ---
        frame_controles = tk.Frame(self.root, width=250)
        frame_controles.pack(side=tk.RIGHT, fill=tk.Y, padx=10, pady=10)

        # Título Controles
        tk.Label(frame_controles, text="Parámetros de Control", font=("Arial", 14, "bold")).pack(pady=10)

        # Campos de entrada
        self.entradas = {}
        campos = ["Distancia (Setpoint)", "Kp", "Ki", "Kd"]
        
        for campo in campos:
            frame_campo = tk.Frame(frame_controles)
            frame_campo.pack(pady=5, fill=tk.X)
            tk.Label(frame_campo, text=campo + ":", width=18, anchor="w").pack(side=tk.LEFT)
            entry = ttk.Entry(frame_campo, width=10)
            entry.pack(side=tk.RIGHT)
            self.entradas[campo] = entry

        # Botón Enviar Parámetros
        btn_enviar = tk.Button(frame_controles, text="Enviar Parámetros", bg="#4CAF50", fg="white", command=self.enviar_parametros)
        btn_enviar.pack(pady=20, fill=tk.X)

        # Separador
        ttk.Separator(frame_controles, orient='horizontal').pack(fill='x', pady=10)

        # Botón Encender/Apagar
        self.btn_estado = tk.Button(frame_controles, text="ENCENDER LEVITADOR", bg="#2196F3", fg="white", font=("Arial", 12, "bold"), command=self.toggle_estado)
        self.btn_estado.pack(pady=20, fill=tk.X, ipady=10)

        # Iniciar animación de la gráfica
        self.ani = FuncAnimation(self.figura, self.actualizar_grafica, interval=100, blit=False)

    def enviar_parametros(self):
        try:
            # Leer los valores de los Entry
            dist = float(self.entradas["Distancia (Setpoint)"].get())
            kp = float(self.entradas["Kp"].get())
            ki = float(self.entradas["Ki"].get())
            kd = float(self.entradas["Kd"].get())

            # Formato a enviar: D:valor,P:valor,I:valor,D:valor\n
            comando = f"P:{dist},{kp},{ki},{kd}\n"
            
            if self.arduino and self.arduino.is_open:
                self.arduino.write(comando.encode('utf-8'))
                print(f"Enviado: {comando.strip()}")
            else:
                print(f"[Simulación] Comando preparado: {comando.strip()}")

        except ValueError:
            messagebox.showerror("Error", "Por favor, ingresa solo valores numéricos en los campos.")

    def toggle_estado(self):
        self.sistema_encendido = not self.sistema_encendido
        
        if self.sistema_encendido:
            self.btn_estado.config(text="APAGAR LEVITADOR", bg="#f44336") # Rojo
            comando = "ESTADO:1\n"
        else:
            self.btn_estado.config(text="ENCENDER LEVITADOR", bg="#2196F3") # Azul
            comando = "ESTADO:0\n"

        if self.arduino and self.arduino.is_open:
            self.arduino.write(comando.encode('utf-8'))
            print(f"Enviado: {comando.strip()}")
        else:
            print(f"[Simulación] Comando preparado: {comando.strip()}")

    def actualizar_grafica(self, frame):
        # Leer datos de Arduino si está conectado
        if self.arduino and self.arduino.is_open:
            try:
                if self.arduino.in_waiting > 0:
                    linea_serial = self.arduino.readline().decode('utf-8').strip()
                    # Se asume que el arduino envía solo el número de la distancia (ej. "25.4")
                    distancia_actual = float(linea_serial)
                    tiempo_actual = time.time() - self.inicio_tiempo
                    
                    self.tiempos.append(tiempo_actual)
                    self.distancias.append(distancia_actual)
            except Exception as e:
                pass # Ignorar errores de lectura o de conversión en tiempo real

        # Actualizar los datos de la línea en la gráfica
        if self.tiempos and self.distancias:
            self.linea.set_data(self.tiempos, self.distancias)
            self.ax.set_xlim(max(0, self.tiempos[-1] - 10), self.tiempos[-1] + 1) # Mostrar los últimos 10 segundos

        return self.linea,

if __name__ == "__main__":
    root = tk.Tk()
    app = LevitadorGUI(root)
    root.mainloop()