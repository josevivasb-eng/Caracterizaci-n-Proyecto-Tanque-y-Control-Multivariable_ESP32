#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// --- CONFIGURACIÓN DE PINES Y PARÁMETROS ---
const int trigPin = 5; 
const int echoPin = 18;

// AJUSTA ESTOS VALORES SEGÚN TU PROYECTO
const float H_sensor = 16.3; // Altura total desde el sensor hasta el fondo (cm)
const float Offset = 0.0;     // Valor de ajuste fino (cm)

const char* ssid = "Tanque_Agua_ESP32"; 
WebServer server(80);

// --- FUNCIÓN DE LECTURA DEL SENSOR ---
float leerNivel() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000); // Timeout de 30ms
  if (duration == 0) return 0.0;
  
  float dist_sensor = (duration * 0.034) / 2.0;
  float nivel = (H_sensor - dist_sensor) - Offset;
  return (nivel < 0) ? 0.0 : nivel;
}

// --- HTML / JS ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>Monitor Tanque Piloto</title>
    <!-- Usaremos un gráfico simple con elementos HTML si la librería falla -->
    <style>
        body { font-family: sans-serif; background-color: #eef2f5; display: flex; flex-direction: column; align-items: center; padding: 20px; }
        .tank-card { background: white; padding: 20px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); text-align: center; width: 350px; }
        #tank-container { position: relative; width: 100px; height: 300px; background-color: #e0e0e0; border: 5px solid #555; border-radius: 10px; margin: 10px auto; overflow: hidden; }
        #water-level { position: absolute; bottom: 0; width: 100%; height: 0%; background-color: #007bff; transition: height 0.3s; }
        button { padding: 10px 20px; cursor: pointer; background: #28a745; color: white; border: none; border-radius: 5px; font-weight: bold; margin-top: 10px; }
        button:disabled { background: #ccc; }
        #timer-display { font-size: 18px; margin: 10px 0; font-weight: bold; color: #d9534f; }
        #log-area { font-size: 12px; height: 100px; overflow-y: scroll; border: 1px solid #ccc; margin-top: 10px; text-align: left; padding: 5px; }
    </style>
</head>
<body>
    <div class="tank-card">
        <h1>Tanque Piloto</h1>
        <div id="tank-container"><div id="water-level"></div></div>
        <div id="readout" style="font-size: 24px; font-weight: bold;">-- cm</div>
        <button id="btn-start" onclick="iniciarCaptura()">Iniciar Captura (30s)</button>
        <div id="timer-display">Tiempo: 0s</div>
        <div id="log-area">Esperando datos...</div>
    </div>

    <script>
        var isCapturing = false;
        var muestras = 0; // Cambiamos contador de tiempo por contador de muestras
        var datosCapturados = []; 

        function descargarCSV() {
            let csvContent = "data:text/csv;charset=utf-8,Muestra(s),Nivel(cm)\n";
            datosCapturados.forEach(d => { csvContent += d.t + "," + d.v + "\n"; });
            var encodedUri = encodeURI(csvContent);
            var link = document.createElement("a");
            link.setAttribute("href", encodedUri);
            link.setAttribute("download", "datos_tanque.csv");
            document.body.appendChild(link);
            link.click();
        }

        function iniciarCaptura() {
            isCapturing = true;
            muestras = 0;
            datosCapturados = [];
            document.getElementById('btn-start').disabled = true;
            document.getElementById('log-area').innerHTML = "Capturando...<br>";
        }

        setInterval(function() {
            fetch('/nivel')
            .then(res => res.json())
            .then(data => {
                document.getElementById('readout').innerText = data.nivel.toFixed(1) + ' cm';
                document.getElementById('water-level').style.height = (data.nivel / 150 * 100) + '%';
                
                if (isCapturing) {
                    muestras++;
                    datosCapturados.push({t: muestras, v: data.nivel});
                    document.getElementById('log-area').innerHTML += muestras + "s: " + data.nivel.toFixed(1) + "cm<br>";
                    document.getElementById('timer-display').innerText = "Tiempo: " + muestras + "s";

                    // Detener exactamente en 30 muestras
                    if (muestras >= 30) {
                        isCapturing = false;
                        document.getElementById('btn-start').disabled = false;
                        alert("Captura de 30 muestras finalizada.");
                        descargarCSV();
                    }
                }
            })
            .catch(err => console.log("Error de conexión"));
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", index_html); }

void handleNivel() {
    float nivelActual = leerNivel(); // Llamada a tu función
    String json = "{\"nivel\":" + String(nivelActual) + "}";
    server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  WiFi.softAP(ssid); 
  server.on("/", handleRoot);
  server.on("/nivel", handleNivel);
  server.begin();
  Serial.println("Servidor iniciado en http://192.168.4.1/");
}

void loop() { server.handleClient(); }