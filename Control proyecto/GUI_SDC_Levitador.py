import tkinter as tk
from tkinter import ttk, messagebox
import serial 
import time
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
import collections

# --- CONFIGURACIÓN SERIAL ---
PUERTO_SERIAL = 'COM3' # <-- Cambia esto a tu puerto real
BAUD_RATE = 115200     # <-- Ajustado a tu nuevo código de Arduino

class LevitadorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Control PID Avanzado - Levitador")
        self.root.geometry("1050x650")

        self.sistema_encendido = False
        datosGuardados = 600
        # Datos para la gráfica (guarda los últimos 100 puntos)
        self.tiempos = collections.deque(maxlen=datosGuardados)
        self.distancias = collections.deque(maxlen=datosGuardados)
        self.setpoints = collections.deque(maxlen=datosGuardados) # Nueva lista para la rampa suave
        self.inicio_tiempo = time.time()

        # Conexión Serial
        try:
            self.arduino = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=0.05)
            time.sleep(2) 
        except serial.SerialException:
            self.arduino = None
            messagebox.showwarning("Advertencia", f"No se pudo conectar al puerto {PUERTO_SERIAL}.")

        self.crear_interfaz()

    def crear_interfaz(self):
        # --- ZONA DE GRÁFICA ---
        frame_grafica = tk.Frame(self.root)
        frame_grafica.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        self.figura = Figure(figsize=(7, 5), dpi=100)
        self.ax = self.figura.add_subplot(111)
        self.ax.set_title("Comportamiento del Levitador")
        self.ax.set_xlabel("Tiempo (s)")
        self.ax.set_ylabel("Distancia (cm)")
        self.ax.set_ylim(0, 65) 
        
        # Dos líneas: Setpoint y Distancia Real
        self.linea_setpoint, = self.ax.plot([], [], 'r--', linewidth=2, label="Setpoint (Objetivo)")
        self.linea, = self.ax.plot([], [], 'b-', linewidth=2, label="Pelota (Real)")
        self.ax.legend(loc="upper right")

        self.canvas = FigureCanvasTkAgg(self.figura, master=frame_grafica)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # --- ZONA DE CONTROLES ---
        frame_controles = tk.Frame(self.root, width=280)
        frame_controles.pack(side=tk.RIGHT, fill=tk.Y, padx=10, pady=10)

        tk.Label(frame_controles, text="Parámetros PID", font=("Arial", 14, "bold")).pack(pady=10)

        self.entradas = {}
        # Valores por defecto basados en tu código
        campos = {"Distancia (Setpoint)": "15", "Kp": "8", "Ki": "0.75", "Kd": "3.2"}
        
        for campo, valor_defecto in campos.items():
            frame_campo = tk.Frame(frame_controles)
            frame_campo.pack(pady=8, fill=tk.X)
            tk.Label(frame_campo, text=campo + ":", width=18, anchor="w").pack(side=tk.LEFT)
            entry = ttk.Entry(frame_campo, width=10)
            entry.insert(0, valor_defecto)
            entry.pack(side=tk.RIGHT)
            self.entradas[campo] = entry

        btn_enviar = tk.Button(frame_controles, text="Enviar Parámetros", bg="#4CAF50", fg="white", font=("Arial", 10, "bold"), command=self.enviar_parametros)
        btn_enviar.pack(pady=15, fill=tk.X)

        ttk.Separator(frame_controles, orient='horizontal').pack(fill='x', pady=15)

        self.btn_estado = tk.Button(frame_controles, text="ENCENDER LEVITADOR", bg="#2196F3", fg="white", font=("Arial", 12, "bold"), command=self.toggle_estado)
        self.btn_estado.pack(pady=10, fill=tk.X, ipady=15)
        self.lbl_pwm = tk.Label(frame_controles, text="PWM Actual: 0", font=("Arial", 14, "bold"), fg="#FF5722")
        self.lbl_pwm.pack(pady=15)

        # Animación súper fluida (30ms)
        self.ani = FuncAnimation(self.figura, self.actualizar_grafica, interval=30, blit=False, cache_frame_data=False)
    def enviar_parametros(self):
        try:
            dist = float(self.entradas["Distancia (Setpoint)"].get())
            kp = float(self.entradas["Kp"].get())
            ki = float(self.entradas["Ki"].get())
            kd = float(self.entradas["Kd"].get())

            comando = f"P:{dist},{kp},{ki},{kd}\n"
            
            if self.arduino and self.arduino.is_open:
                self.arduino.write(comando.encode('utf-8'))
                print(f"Enviado: {comando.strip()}")

        except ValueError:
            messagebox.showerror("Error", "Ingresa solo valores numéricos.")

    def toggle_estado(self):
        self.sistema_encendido = not self.sistema_encendido
        
        if self.sistema_encendido:
            self.btn_estado.config(text="APAGAR LEVITADOR", bg="#f44336")
            comando = "ESTADO:1\n"
        else:
            self.btn_estado.config(text="ENCENDER LEVITADOR", bg="#2196F3")
            comando = "ESTADO:0\n"

        if self.arduino and self.arduino.is_open:
            self.arduino.write(comando.encode('utf-8'))

    def actualizar_grafica(self, frame):
        if self.arduino and self.arduino.is_open:
            try:
                while self.arduino.in_waiting > 0:
                    linea_serial = self.arduino.readline().decode('utf-8').strip()
                    
                    if linea_serial:
                        # Separar los datos por la coma (Setpoint, Distancia)
                        datos = linea_serial.split(',')
                        
                        if len(datos) == 3: # Ahora recibimos Setpoint, Distancia y PWM
                            sp_actual = float(datos[0])
                            distancia_actual = float(datos[1])
                            pwm_actual = int(datos[2]) # Extraer el valor PWM
                            tiempo_actual = time.time() - self.inicio_tiempo
                            
                            self.tiempos.append(tiempo_actual)
                            self.setpoints.append(sp_actual)
                            self.distancias.append(distancia_actual)
                            
                            # Actualizar la interfaz con el valor PWM
                            self.lbl_pwm.config(text=f"PWM Actual: {pwm_actual}")
            except Exception:
                pass 

        if self.tiempos and self.distancias:
            self.linea_setpoint.set_data(self.tiempos, self.setpoints)
            self.linea.set_data(self.tiempos, self.distancias)
            
            tiempo_reciente = self.tiempos[-1]
            self.ax.set_xlim(max(0, tiempo_reciente - 10), max(10, tiempo_reciente))

        return self.linea_setpoint, self.linea

if __name__ == "__main__":
    root = tk.Tk()
    app = LevitadorGUI(root)
    root.mainloop()