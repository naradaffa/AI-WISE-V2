// ==========================================
// 1. KONFIGURASI mDNS & GENERATE HTML (10 PERANGKAT)
// ==========================================
const TOTAL_DEVICES = 10;

// Hostname mDNS dari pekerja1.local sampai pekerja10.local
const IP_LIST = Array.from({ length: TOTAL_DEVICES }, (_, i) => `http://pekerja${i + 1}.local/events`);

const MAX_CHART_POINTS = 75;
const CSV_HEADER_STD = "Timestamp(ms),AccX,AccY,AccZ,GyroX,GyroY,GyroZ,Roll,Pitch,Yaw,Suhu(C),Kelembaban(%),DetakJantung(BPM),SpO2(%),Latitude,Longitude";

const container = document.getElementById('dashboard-container');
let htmlBuilder = '';

for (let i = 1; i <= TOTAL_DEVICES; i++) {
    htmlBuilder += `
    <div class="device-section">
        <div class="section-header">
            <h2 class="section-title">Pemantauan Pekerja ${i}</h2>
            <div class="battery-status" style="display: inline-flex; align-items: center; gap: 6px;">
                <svg width="24" height="12" viewBox="0 0 24 12" style="display: block;">
                    <rect x="0.75" y="0.75" width="19.5" height="10.5" rx="2" fill="none" stroke="#555" stroke-width="1.5"/>
                    <rect id="bat-fill-${i}" x="2.5" y="2.5" width="0" height="7" rx="1" fill="#9e9e9e"/>
                    <path d="M22 4v4" stroke="#555" stroke-width="1.5" stroke-linecap="round"/>
                </svg>
                <span><span id="val-bat-${i}">--</span>%</span>
                <span id="val-chg-${i}" style="color: #555; font-size: 0.85em; display: none;">(Charging)</span>
            </div>
            <div class="connection-status disconnected" id="ws-status-${i}">Menghubungkan...</div>
        </div>
        
        <section class="record-panel">
            <button id="btn-start-${i}" onclick="startRecord(${i})">▶ MULAI REKAM ${i}</button>
            <button id="btn-stop-${i}" onclick="stopRecord(${i})" disabled>⏹ BERHENTI</button>
            <a id="btn-download-${i}" class="btn-download">⬇ DOWNLOAD CSV ${i}</a>
            <span id="record-status-${i}" class="status-text">Status: Standby</span>
        </section>

        <section class="cards-container">
            <div class="card"><span class="card-title">Total Akselerasi</span><div class="card-value"><span id="val-total-accel-${i}">0.00</span> m/s&sup2;</div></div>
            <div class="card"><span class="card-title">Pitch (Tundukan)</span><div class="card-value"><span id="val-pitch-${i}">0.0</span>&deg;</div></div>
            <div class="card"><span class="card-title">Roll (Kemiringan)</span><div class="card-value"><span id="val-roll-${i}">0.0</span>&deg;</div></div>
            <div class="card"><span class="card-title">Yaw (Arah)</span><div class="card-value"><span id="val-yaw-${i}">0.0</span>&deg;</div></div>
            <div class="card"><span class="card-title">Suhu Lingkungan</span><div class="card-value"><span id="val-temp-${i}">0.0</span> &deg;C</div></div>
            <div class="card"><span class="card-title">Kelembaban Udara</span><div class="card-value"><span id="val-hum-${i}">0.0</span> %</div></div>
            <div class="card"><span class="card-title">Detak Jantung</span><div class="card-value"><span id="val-bpm-${i}">0</span> BPM</div></div>
            <div class="card"><span class="card-title">Kadar Oksigen (SpO2)</span><div class="card-value"><span id="val-spo2-${i}">0</span> %</div></div>
            <div class="card card-gps">
                <span class="card-title">Koordinat GPS</span>
                <div class="card-value gps-text">
                    Lat: <span id="val-lat-${i}">0.000000</span><br>
                    Lng: <span id="val-lng-${i}">0.000000</span>
                </div>
            </div>
        </section>

        <section class="charts-row">
            <div class="chart-card">
                <h3>Akselerasi 3 Sumbu Pekerja ${i} (m/s&sup2;)</h3>
                <div class="chart-wrapper"><canvas id="chartAccel-${i}"></canvas></div>
            </div>
            <div class="chart-card">
                <h3>Gyroscope 3 Sumbu Pekerja ${i} (&deg;/s)</h3>
                <div class="chart-wrapper"><canvas id="chartGyro-${i}"></canvas></div>
            </div>
        </section>
    </div>`;
}
container.innerHTML = htmlBuilder;

Chart.defaults.animation = false;
Chart.defaults.elements.point.radius = 0; 
Chart.defaults.elements.line.borderWidth = 2; 

const customLegendStyle = {
    display: true, position: 'bottom',
    labels: {
        useBorderRadius: true, borderRadius: 4, boxWidth: 32, boxHeight: 14, padding: 20,          
        font: { size: 14, weight: '500', family: "'Montserrat', sans-serif" }, color: '#333'
    }
};

const commonOptions = {
    responsive: true, maintainAspectRatio: false, animation: false, normalized: true, spanGaps: true,
    scales: { x: { display: false } }, plugins: { legend: customLegendStyle }
};

const isRecording = {};
const csvDataArray = {};
const dataCount = {};
const latestData = {};
const chartAccels = {};
const chartGyros = {};

// ==========================================
// 2. INISIALISASI CHART & SSE (10 PERANGKAT)
// ==========================================
for (let i = 1; i <= TOTAL_DEVICES; i++) {
    isRecording[i] = false;
    csvDataArray[i] = [];
    dataCount[i] = 0;
    latestData[i] = null;

    const ctxAccel = document.getElementById(`chartAccel-${i}`).getContext('2d');
    chartAccels[i] = new Chart(ctxAccel, {
        type: 'line',
        data: { labels: [], datasets: [
            { label: 'Acc X', data: [], borderColor: '#ffc107', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Acc Y', data: [], borderColor: '#ff5722', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Acc Z', data: [], borderColor: '#e91e63', backgroundColor: 'transparent', tension: 0.1 }
        ]},
        options: { ...commonOptions, scales: { ...commonOptions.scales, y: { suggestedMin: -20, suggestedMax: 20 } } }
    });

    const ctxGyro = document.getElementById(`chartGyro-${i}`).getContext('2d');
    chartGyros[i] = new Chart(ctxGyro, {
        type: 'line',
        data: { labels: [], datasets: [
            { label: 'Gyro X', data: [], borderColor: '#00bcd4', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Gyro Y', data: [], borderColor: '#3f51b5', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Gyro Z', data: [], borderColor: '#4caf50', backgroundColor: 'transparent', tension: 0.1 }
        ]},
        options: { ...commonOptions, scales: { ...commonOptions.scales, y: { suggestedMin: -100, suggestedMax: 100 } } }
    });

    if (!!window.EventSource) {
        const source = new EventSource(IP_LIST[i - 1]);
        const wsStatus = document.getElementById(`ws-status-${i}`);

        source.addEventListener('open', () => {
            wsStatus.innerText = 'TERHUBUNG';
            wsStatus.className = 'connection-status connected';
        });
        source.addEventListener('error', (e) => {
            if (e.target.readyState != EventSource.OPEN) {
                wsStatus.innerText = 'TERPUTUS';
                wsStatus.className = 'connection-status disconnected';
            }
        });
        source.addEventListener('sensor_data', (e) => {
            try {
                const parsed = JSON.parse(e.data);
                latestData[i] = parsed;
                if (isRecording[i]) {
                    const row = `${parsed.time || 0},${parsed.x},${parsed.y},${parsed.z},${parsed.gx || 0},${parsed.gy || 0},${parsed.gz || 0},${parsed.roll},${parsed.pitch},${parsed.yaw},${parsed.temp || 0},${parsed.hum || 0},${parsed.bpm || 0},${parsed.spo2 || 0},${parsed.lat || 0},${parsed.lng || 0}`;
                    csvDataArray[i].push(row);
                    dataCount[i]++;
                }
            } catch (err) {}
        });
    }
}

// ==========================================
// 3. LOGIKA KONTROL CSV REKAMAN (INDIVIDUAL & GLOBAL)
// ==========================================
function startRecord(device) {
    isRecording[device] = true;
    csvDataArray[device] = [CSV_HEADER_STD];
    dataCount[device] = 0;
    
    document.getElementById(`btn-start-${device}`).disabled = true;
    document.getElementById(`btn-stop-${device}`).disabled = false;
    document.getElementById(`btn-download-${device}`).style.display = "none";
    
    const recordStatus = document.getElementById(`record-status-${device}`);
    recordStatus.innerText = "Merekam (100Hz)... 0 baris";
    recordStatus.style.color = "#d32f2f";
}

function stopRecord(device) {
    isRecording[device] = false;
    document.getElementById(`btn-start-${device}`).disabled = false;
    document.getElementById(`btn-stop-${device}`).disabled = true;
    
    const recordStatus = document.getElementById(`record-status-${device}`);
    recordStatus.innerText = `Selesai! Total: ${dataCount[device]} baris.`;
    recordStatus.style.color = "#2e7d32";

    const blob = new Blob([csvDataArray[device].join("\n")], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    
    const btnDownload = document.getElementById(`btn-download-${device}`);
    btnDownload.href = url;
    btnDownload.download = `Log_Pekerja${device}_${Date.now()}.csv`;
    btnDownload.style.display = "inline-block";
}

// --- FUNGSI KONTROL REKAM SEMUA PEKERJA ---
function startRecordAll() {
    for (let i = 1; i <= TOTAL_DEVICES; i++) {
        startRecord(i);
    }
    document.getElementById('btn-start-all').disabled = true;
    document.getElementById('btn-stop-all').disabled = false;
    document.getElementById('btn-download-all').style.display = "none";
    
    const globalStatus = document.getElementById('global-record-status');
    globalStatus.innerText = "Merekam Semua Perangkat...";
    globalStatus.style.color = "#d32f2f";
}

function stopRecordAll() {
    for (let i = 1; i <= TOTAL_DEVICES; i++) {
        stopRecord(i);
    }
    document.getElementById('btn-start-all').disabled = false;
    document.getElementById('btn-stop-all').disabled = true;

    // Gabungkan seluruh data CSV dari Pekerja 1 sampai 10
    const masterHeader = "Device_ID," + CSV_HEADER_STD;
    let masterRows = [masterHeader];
    let totalAllRows = 0;

    for (let i = 1; i <= TOTAL_DEVICES; i++) {
        if (csvDataArray[i] && csvDataArray[i].length > 1) {
            // Lewati baris header individu [0], ambil baris data [1..n]
            for (let j = 1; j < csvDataArray[i].length; j++) {
                masterRows.push(`Pekerja_${i},${csvDataArray[i][j]}`);
                totalAllRows++;
            }
        }
    }

    const blob = new Blob([masterRows.join("\n")], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);

    const btnDownloadAll = document.getElementById('btn-download-all');
    btnDownloadAll.href = url;
    btnDownloadAll.download = `Log_Master_Semua_Pekerja_${Date.now()}.csv`;
    btnDownloadAll.style.display = "inline-block";

    const globalStatus = document.getElementById('global-record-status');
    globalStatus.innerText = `Selesai! Total Gabungan: ${totalAllRows} baris.`;
    globalStatus.style.color = "#2e7d32";
}

// ==========================================
// 4. PEMBARUAN LAYAR DOM & GRAFIK (INTERVAL)
// ==========================================
setInterval(() => {
    for (let i = 1; i <= TOTAL_DEVICES; i++) {
        const data = latestData[i];
        if (data) {
            // Mengupdate persentase, bar isi, dan warna ikon baterai
            const batFill = document.getElementById(`bat-fill-${i}`);
            const batText = document.getElementById(`val-bat-${i}`);

            if (data.bat !== undefined) {
                const batVal = Math.max(0, Math.min(100, parseInt(data.bat)));
                batText.innerText = batVal;
                batFill.setAttribute('width', ((batVal / 100) * 16).toFixed(1));

                if (batVal <= 20) {
                    batFill.setAttribute('fill', '#e53935'); // Merah
                } else if (batVal <= 50) {
                    batFill.setAttribute('fill', '#fb8c00'); // Oranye
                } else {
                    batFill.setAttribute('fill', '#43a047'); // Hijau
                }
            } else {
                batText.innerText = '--';
                batFill.setAttribute('width', '0');
                batFill.setAttribute('fill', '#9e9e9e');
            }

            // Menampilkan atau menyembunyikan ikon petir charging
            if (data.chg === true) {
                document.getElementById(`val-chg-${i}`).style.display = "inline";
            } else {
                document.getElementById(`val-chg-${i}`).style.display = "none";
            }

            document.getElementById(`val-total-accel-${i}`).innerText = parseFloat(data.total || 0).toFixed(2);
            document.getElementById(`val-roll-${i}`).innerText = parseFloat(data.roll || 0).toFixed(1);
            document.getElementById(`val-pitch-${i}`).innerText = parseFloat(data.pitch || 0).toFixed(1);
            document.getElementById(`val-yaw-${i}`).innerText = parseFloat(data.yaw || 0).toFixed(1);
            
            document.getElementById(`val-temp-${i}`).innerText = parseFloat(data.temp || 0).toFixed(1);
            document.getElementById(`val-hum-${i}`).innerText = parseFloat(data.hum || 0).toFixed(1);
            document.getElementById(`val-bpm-${i}`).innerText = parseInt(data.bpm || 0);
            document.getElementById(`val-spo2-${i}`).innerText = parseInt(data.spo2 || 0);
            document.getElementById(`val-lat-${i}`).innerText = parseFloat(data.lat || 0).toFixed(6);
            document.getElementById(`val-lng-${i}`).innerText = parseFloat(data.lng || 0).toFixed(6);

            const timeString = data.time ? data.time.toString() : '';

            chartAccels[i].data.labels.push(timeString);
            chartAccels[i].data.datasets[0].data.push(data.x || 0);
            chartAccels[i].data.datasets[1].data.push(data.y || 0);
            chartAccels[i].data.datasets[2].data.push(data.z || 0);

            chartGyros[i].data.labels.push(timeString);
            chartGyros[i].data.datasets[0].data.push(data.gx || 0);
            chartGyros[i].data.datasets[1].data.push(data.gy || 0);
            chartGyros[i].data.datasets[2].data.push(data.gz || 0);

            if (chartAccels[i].data.labels.length > MAX_CHART_POINTS) {
                chartAccels[i].data.labels.shift();
                chartAccels[i].data.datasets.forEach(ds => ds.data.shift());
                
                chartGyros[i].data.labels.shift();
                chartGyros[i].data.datasets.forEach(ds => ds.data.shift());
            }

            chartAccels[i].update();
            chartGyros[i].update();

            if (isRecording[i]) {
                document.getElementById(`record-status-${i}`).innerText = `Merekam (100Hz)... ${dataCount[i]} baris`;
            }
        }
    }
}, 66);
