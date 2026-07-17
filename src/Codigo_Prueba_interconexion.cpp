#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ================= CONFIGURACIÓN DE PINES Y PARÁMETROS =================
// Tanque 1
const int trigPin1 = 5;
const int echoPin1 = 18;

// Tanque 2
const int trigPin2 = 4;
const int echoPin2 = 16;

// AJUSTA ESTOS VALORES SEGÚN TU PROYECTO
const float H_sensor = 16.3;  // Altura total desde el sensor hasta el fondo (cm), igual para ambos tanques
const float Offset1  = -0.9;   // Valor de ajuste fino del Tanque 1 (cm)
const float Offset2  = -0.5;   // Valor de ajuste fino del Tanque 2 (cm)

// ================= PARÁMETROS DE FILTRADO (NUEVO) =================
// Número de pulsos que se toman por cada lectura (para sacar la mediana)
const int   NUM_MUESTRAS   = 7;
// Retardo entre pulsos consecutivos del MISMO sensor (deja que el eco se disipe)
const int   RETARDO_MUESTRA_MS = 12;
// Retardo entre la lectura del sensor 1 y el sensor 2 (evita interferencia cruzada)
const int   RETARDO_ENTRE_SENSORES_MS = 40;
// Máximo cambio "creíble" en cm entre una lectura y la siguiente (nivel EMA)
// Durante el llenado el nivel sube, pero no debería saltar más de esto en ~1s
const float MAX_SALTO_CM   = 3.0;
// Cuántas lecturas consecutivas "raras" se aceptan antes de asumir que el salto es real
const int   MAX_RECHAZOS_CONSECUTIVOS = 3;
// Factor de suavizado exponencial (0 = muy suave/lento, 1 = sin suavizado)
const float ALPHA_EMA = 0.35;

const char* ssid = "Tanque_Agua_ESP32";
WebServer server(80);

// ================= ESTADO DE FILTRO POR TANQUE =================
struct EstadoFiltro {
  float ultimoValorValido = -1;   // último valor aceptado (post-mediana)
  float valorSuavizado     = -1;  // valor final tras EMA, es el que se reporta
  int   rechazosSeguidos   = 0;
  bool  inicializado       = false;
};

EstadoFiltro estadoTanque1;
EstadoFiltro estadoTanque2;

// ================= LECTURA CRUDA DE UN PULSO =================
// Devuelve la distancia medida por UN pulso, o -1 si no hubo eco (timeout / fuera de rango)
float leerPulsoCrudo(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Timeout calculado según H_sensor + margen (evita bloquear el loop innecesariamente)
  unsigned long timeoutUs = (unsigned long)((H_sensor + 50) * 2.0 / 0.034);
  long duration = pulseIn(echoPin, HIGH, timeoutUs);
  if (duration == 0) return -1.0; // sin eco = lectura inválida

  float dist_sensor = (duration * 0.034) / 2.0;

  // Descarta ecos físicamente imposibles (ruido, rebote múltiple, etc.)
  if (dist_sensor < 0.5  dist_sensor > (H_sensor + 5.0)) return -1.0;

  return dist_sensor;
}

// ================= COMPARADOR PARA ORDENAR (MEDIANA) =================
int compararFloats(const void* a, const void* b) {
  float fa = *(const float*)a;
  float fb = *(const float*)b;
  if (fa < fb) return -1;
  if (fa > fb) return 1;
  return 0;
}

// ================= LECTURA POR MEDIANA (varias muestras) =================
// Toma NUM_MUESTRAS pulsos, descarta inválidos, ordena y devuelve la mediana.
// Esto elimina picos aislados causados por oleaje/espuma durante el llenado.
float leerDistanciaMediana(int trigPin, int echoPin) {
  float muestras[NUM_MUESTRAS];
  int validas = 0;

  for (int i = 0; i < NUM_MUESTRAS; i++) {
    float d = leerPulsoCrudo(trigPin, echoPin);
    if (d > 0) {
      muestras[validas++] = d;
    }
    delay(RETARDO_MUESTRA_MS);
  }

  // Si menos de la mitad de las muestras fueron válidas, la lectura no es confiable
  if (validas < (NUM_MUESTRAS / 2 + 1)) {
    return -1.0;
  }

  qsort(muestras, validas, sizeof(float), compararFloats);
  return muestras[validas / 2]; // mediana
}

// ================= FILTRO COMPLETO: MEDIANA + RECHAZO DE SALTOS + EMA =================
// Convierte distancia -> nivel de agua, valida contra el valor anterior y suaviza.
float procesarNivel(float distanciaMediana, EstadoFiltro &estado, float offset) {
  // Si la lectura de distancia falló, mantenemos el último valor suavizado
  if (distanciaMediana < 0) {
    retur
n (estado.valorSuavizado >= 0) ? estado.valorSuavizado : 0.0;
  }

  float nivelCrudo = (H_sensor - distanciaMediana) - offset;
  if (nivelCrudo < 0) nivelCrudo = 0.0;
  if (nivelCrudo > H_sensor) nivelCrudo = H_sensor;

  // Primera lectura válida: se acepta directamente
  if (!estado.inicializado) {
    estado.ultimoValorValido = nivelCrudo;
    estado.valorSuavizado    = nivelCrudo;
    estado.inicializado      = true;
    estado.rechazosSeguidos  = 0;
    return estado.valorSuavizado;
  }

  float salto = fabs(nivelCrudo - estado.ultimoValorValido);

  if (salto <= MAX_SALTO_CM  estado.rechazosSeguidos >= MAX_RECHAZOS_CONSECUTIVOS) {
    // Lectura creíble (o llevamos varios rechazos seguidos => probablemente el nivel
    // sí cambió de verdad y no es ruido, así que la aceptamos para no "congelarnos")
    estado.ultimoValorValido = nivelCrudo;
    estado.rechazosSeguidos  = 0;
    // Suavizado exponencial: evita que el número en pantalla "tiemble"
    estado.valorSuavizado = ALPHA_EMA * nivelCrudo + (1 - ALPHA_EMA) * estado.valorSuavizado;
  } else {
    // Salto sospechoso (probable ruido de oleaje/espuma): se ignora esta muestra
    estado.rechazosSeguidos++;
  }

  return estado.valorSuavizado;
}

// Función de conveniencia que hace todo el proceso para un tanque
float leerNivelFiltrado(int trigPin, int echoPin, EstadoFiltro &estado, float offset) {
  float distMediana = leerDistanciaMediana(trigPin, echoPin);
  return procesarNivel(distMediana, estado, offset);
}

// ================= HTML / JS (sin cambios funcionales) =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>Monitor Tanques Piloto</title>
    <style>
        body { font-family: sans-serif; background-color: #eef2f5; display: flex; flex-direction: column; align-items: center; padding: 20px; }
        .dashboard { display: flex; gap: 20px; justify-content: center; flex-wrap: wrap; margin-bottom: 20px; }
        .tank-card { background: white; padding: 20px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); text-align: center; width: 250px; }
        .control-card { background: white; padding: 20px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.1); text-align: center; width: 540px; margin-top: 10px; }
        .tank-container { position: relative; width: 100px; height: 300px; background-color: #e0e0e0; border: 5px solid #555; border-radius: 10px; margin: 10px auto; overflow: hidden; }
        .water-level { position: absolute; bottom: 0; width: 100%; height: 0%; background-color: #007bff; transition: height 0.3s; }
        button { padding: 10px 20px; cursor: pointer; background: #28a745; color: white; border: none; border-radius: 5px; font-weight: bold; margin-top: 10px; }
        button:disabled { background: #ccc; }
        #timer-display { font-size: 18px; margin: 10px 0; font-weight: bold; color: #d9534f; }
        #log-area { font-size: 12px; height: 120px; overflow-y: scroll; border: 1px solid #ccc; margin-top: 10px; text-align: left; padding: 5px; font-family: monospace; }
    </style>
</head>
<body>
    <h1>Sistema de Monitoreo de Tanques</h1>

    <div class="dashboard">
        <div class="tank-card">
            <h2>Tanque 1</h2>
            <div class="tank-container"><div id="water-level-1" class="water-level"></div></div>
            <div id="readout-1" style="font-size: 24px; font-weight: bold;">-- cm</div>
        </div>

        <div class="tank-card">
            <h2>Tanque 2</h2>
            <div class="tank-container"><div id="water-level-2" class="water-level"></div></div>
            <div id="readout-2" style="font-size: 24px; font-weight: bold;">-- cm</div>
        </div>
    </div>

    <div class="control-card">
        <button id="btn-start" onclick="iniciarCaptura()">Iniciar Captura Simultánea (30s)</button>
        <div id="timer-display">Tiempo: 0s</div>
        <div id="log-area">Esperando datos...</div>
    </div>

    <script>
        var isCapturing = false;
        var muestras = 0;
        var datosCaptu
rados = [];

        function descargarCSV() {
            let csvContent = "data:text/csv;charset=utf-8,Muestra(s),Tanque 1 (cm),Tanque 2 (cm)\n";
            datosCapturados.forEach(d => { csvContent += d.t + "," + d.v1 + "," + d.v2 + "\n"; });
            var encodedUri = encodeURI(csvContent);
            var link = document.createElement("a");
            link.setAttribute("href", encodedUri);
            link.setAttribute("download", "datos_tanques_dobles.csv");
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }

        function iniciarCaptura() {
            isCapturing = true;
            muestras = 0;
            datosCapturados = [];
            document.getElementById('btn-start').disabled = true;
            document.getElementById('log-area').innerHTML = "Capturando...<br>Muestra | T1 (cm) | T2 (cm)<br>-----------------------<br>";
        }

        setInterval(function() {
            fetch('/nivel')
            .then(res => res.json())
            .then(data => {
                document.getElementById('readout-1').innerText = data.nivel1.toFixed(1) + ' cm';
                document.getElementById('water-level-1').style.height = (data.nivel1 / 16.3 * 100) + '%';

                document.getElementById('readout-2').innerText = data.nivel2.toFixed(1) + ' cm';
                document.getElementById('water-level-2').style.height = (data.nivel2 / 16.3 * 100) + '%';

                if (isCapturing) {
                    muestras++;
                    datosCapturados.push({t: muestras, v1: data.nivel1, v2: data.nivel2});

                    document.getElementById('log-area').innerHTML +=
                        muestras + "s | " + data.nivel1.toFixed(1) + " cm | " + data.nivel2.toFixed(1) + " cm<br>";

                    document.getElementById('timer-display').innerText = "Tiempo: " + muestras + "s";

                    var logArea = document.getElementById('log-area');
                    logArea.scrollTop = logArea.scrollHeight;

                    if (muestras >= 30) {
                        isCapturing = false;
                        document.getElementById('btn-start').disabled = false;
                        alert("Captura simultánea finalizada.");
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

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleNivel() {
    // Se lee el tanque 1 completo (mediana de varias muestras),
    // luego se espera un poco y se lee el tanque 2.
    // Esto evita que el eco de un sensor sea captado por el otro.
    float n1 = leerNivelFiltrado(trigPin1, echoPin1, estadoTanque1, Offset1);
    delay(RETARDO_ENTRE_SENSORES_MS);
    float n2 = leerNivelFiltrado(trigPin2, echoPin2, estadoTanque2, Offset2);

    String json = "{\"nivel1\":" + String(n1, 1) + ",\"nivel2\":" + String(n2, 1) + "}";
    server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);

  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);

  digitalWrite(trigPin1, LOW);
  digitalWrite(trigPin2, LOW);

  WiFi.softAP(ssid);
  server.on("/", handleRoot);
  server.on("/nivel", handleNivel);
  server.begin();
  Serial.println("Servidor iniciado en http://192.168.4.1/");
}

void loop() {
  server.handleClient();
}