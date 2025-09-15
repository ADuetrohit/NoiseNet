#include <WiFi.h>
#include <WebServer.h>
#include <driver/i2s.h>

// Wi-Fi credentials
const char* ssid = "Galaxy";
const char* password = "123456789";

// Web server
WebServer server(80);

// I2S pin configuration
#define I2S_WS  25   // LRCL (WS)
#define I2S_SD  32   // DOUT
#define I2S_SCK 26   // BCLK
#define SAMPLE_BUFFER_SIZE 1024

String getNoiseLevel();
void handleRoot();
void handleData();

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // I2S config
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1, 
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("🌐 Web server started!");
}

String getNoiseLevel() {
  int32_t samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read;
  i2s_read(I2S_NUM_0, (void*)samples, sizeof(samples), &bytes_read, portMAX_DELAY);
  int num_samples = bytes_read / sizeof(int32_t);

  long total = 0;
  for (int i = 0; i < num_samples; i++) total += abs(samples[i] >> 14);
  int avg = total / num_samples;

  if (avg < 200) return "Low";
  else if (avg < 800) return "Moderate";
  else return "High";
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 Noise Dashboard</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; background: #f4f6f9; }
    h1 { text-align: center; padding: 20px; margin: 0; background: #222; color: white; }
    .block { margin: 20px; padding: 20px; background: white; border-radius: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); text-align:center; }
    canvas { width: 100% !important; height: 250px !important; }
    .alert { font-size: 1.5em; padding: 15px; border-radius: 10px; text-align:center; }
    .low { background: #90ee90; color: #006400; }
    .moderate { background: #ffd700; color: #8b8000; }
    .high { background: #ff6347; color: white; }
  </style>
</head>
<body>
  <h1>📊 ESP32 Noise Dashboard</h1>

  <!-- 1. Noise Gauge -->
  <div class="block"><h3>Noise Gauge</h3><canvas id="gaugeChart"></canvas></div>

  <!-- 2. VU Meter -->
  <div class="block"><h3>VU Meter</h3><canvas id="vuChart"></canvas></div>

  <!-- 3. Status Alerts -->
  <div class="block"><h3>Status Alerts</h3><div id="alertBox" class="alert">Loading...</div></div>

  <!-- 4. Frequency Spectrum -->
  <div class="block"><h3>Frequency Spectrum</h3><canvas id="spectrumChart"></canvas></div>

<script>
let counter=0;

// Chart.js Configs
const gaugeChart=new Chart(document.getElementById('gaugeChart'),{
  type:'doughnut',
  data:{labels:['Noise','Remaining'],datasets:[{data:[0,1000],backgroundColor:['#ff6347','#e0e0e0']}]},
  options:{circumference:180,rotation:270,cutout:'70%',plugins:{legend:{display:false}}}
});

const vuChart=new Chart(document.getElementById('vuChart'),{
  type:'bar',
  data:{labels:['Level'],datasets:[{data:[0],backgroundColor:['#4caf50']}]},
  options:{indexAxis:'y',scales:{x:{min:0,max:1000}}}
});

const spectrumChart=new Chart(document.getElementById('spectrumChart'),{
  type:'bar',
  data:{labels:['Bass','Mid','Treble'],datasets:[{data:[0,0,0],backgroundColor:['#2196f3','#ff9800','#9c27b0']}]},
  options:{scales:{y:{min:0,max:1000}}}
});

// Fetch Data every 1 second
setInterval(async()=>{
  let res=await fetch('/data');
  let txt=await res.text();
  let val=(txt=="Low")?200:(txt=="Moderate")?600:1000;

  // Gauge
  gaugeChart.data.datasets[0].data=[val,1000-val];
  gaugeChart.update();

  // VU Meter
  vuChart.data.datasets[0].data[0]=val;
  vuChart.data.datasets[0].backgroundColor[0]=(val<300)?"#4caf50":(val<800)?"#ff9800":"#f44336";
  vuChart.update();

  // Alerts
  let alertBox=document.getElementById("alertBox");
  alertBox.textContent=txt;
  alertBox.className="alert "+txt.toLowerCase();

  // Spectrum (fake FFT for demo)
  spectrumChart.data.datasets[0].data=[val*0.5,val*0.7,val*0.3];
  spectrumChart.update();

},1000);
</script>
</body>
</html>
  )rawliteral";

  server.send(200,"text/html",html);
}

void handleData() {
  server.send(200,"text/plain",getNoiseLevel());
}

void loop() {
  server.handleClient();
}
