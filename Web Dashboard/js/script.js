// ==========================================
// 1. SETUP GRAFIK CHART.JS (4 CHART)
// ==========================================
Chart.defaults.animation = false;
Chart.defaults.elements.point.radius = 0; 
Chart.defaults.elements.line.borderWidth = 2; 

const customLegendStyle = {
    display: true,
    position: 'bottom',
    labels: {
        useBorderRadius: true, borderRadius: 4, boxWidth: 32, boxHeight: 14, padding: 20,          
        font: { size: 14, weight: '500', family: "'Montserrat', sans-serif" }, color: '#333'
    }
};

const commonOptions = {
    responsive: true, maintainAspectRatio: false, animation: false, normalized: true, spanGaps: true,
    scales: { x: { display: false } }, plugins: { legend: customLegendStyle }
};

const MAX_CHART_POINTS = 75;

// ---- CHART PEKERJA 1 ----
const ctxAccel1 = document.getElementById('chartAccel-1').getContext('2d');
const chartAccel1 = new Chart(ctxAccel1, {
    type: 'line',
    data: { labels: [], datasets: [
            { label: 'Acc X', data: [], borderColor: '#ffc107', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Acc Y', data: [], borderColor: '#ff5722', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Acc Z', data: [], borderColor: '#e91e63', backgroundColor: 'transparent', tension: 0.1 }
    ]},
    options: { ...commonOptions, scales: { ...commonOptions.scales, y: { suggestedMin: -20, suggestedMax: 20 } } }
});

const ctxGyro1 = document.getElementById('chartGyro-1').getContext('2d');
const chartGyro1 = new Chart(ctxGyro1, {
    type: 'line',
    data: { labels: [], datasets: [
            { label: 'Gyro X', data: [], borderColor: '#00bcd4', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Gyro Y', data: [], borderColor: '#3f51b5', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Gyro Z', data: [], borderColor: '#4caf50', backgroundColor: 'transparent', tension: 0.1 }
    ]},
    options: { ...commonOptions, scales: { ...commonOptions.scales, y: { suggestedMin: -100, suggestedMax: 100 } } }
});

// ---- CHART PEKERJA 2 ----
const ctxAccel2 = document.getElementById('chartAccel-2').getContext('2d');
const chartAccel2 = new Chart(ctxAccel2, {
    type: 'line',
    data: { labels: [], datasets: [
            { label: 'Acc X', data: [], borderColor: '#ffc107', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Acc Y', data: [], borderColor: '#ff5722', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Acc Z', data: [], borderColor: '#e91e63', backgroundColor: 'transparent', tension: 0.1 }
    ]},
    options: { ...commonOptions, scales: { ...commonOptions.scales, y: { suggestedMin: -20, suggestedMax: 20 } } }
});

const ctxGyro2 = document.getElementById('chartGyro-2').getContext('2d');
const chartGyro2 = new Chart(ctxGyro2, {
    type: 'line',
    data: { labels: [], datasets: [
            { label: 'Gyro X', data: [], borderColor: '#00bcd4', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Gyro Y', data: [], borderColor: '#3f51b5', backgroundColor: 'transparent', tension: 0.1 },
            { label: 'Gyro Z', data: [], borderColor: '#4caf50', backgroundColor: 'transparent', tension: 0.1 }
    ]},
    options: { ...commonOptions, scales: { ...commonOptions.scales, y: { suggestedMin: -100, suggestedMax: 100 } } }
});


// ==========================================
// 2. LOGIKA KONTROL CSV (DIPISAH PER DEVICE)
// ==========================================
let isRecording_1 = false, isRecording_2 = false;
let csvDataArray_1 = [], csvDataArray_2 = [];
let dataCount_1 = 0, dataCount_2 = 0;
// UPDATE V2: Header CSV ditambah parameter sensor baru
const CSV_HEADER = "Timestamp(ms),AccX,AccY,AccZ,GyroX,GyroY,GyroZ,Roll,Pitch,Yaw,Suhu(C),Kelembaban(%),DetakJantung(BPM),SpO2(%),Latitude,Longitude";

function startRecord(device) {
    if (device === 1) {
        isRecording_1 = true;
        csvDataArray_1 = [CSV_HEADER];
        dataCount_1 = 0;
        
        document.getElementById('btn-start-1').disabled = true;
        document.getElementById('btn-stop-1').disabled = false;
        document.getElementById('btn-download-1').style.display = "none";
        
        const recordStatus = document.getElementById('record-status-1');
        recordStatus.innerText = "Merekam (100Hz)... 0 baris";
        recordStatus.style.color = "#d32f2f";
    } 
    else if (device === 2) {
        isRecording_2 = true;
        csvDataArray_2 = [CSV_HEADER];
        dataCount_2 = 0;
        
        document.getElementById('btn-start-2').disabled = true;
        document.getElementById('btn-stop-2').disabled = false;
        document.getElementById('btn-download-2').style.display = "none";
        
        const recordStatus = document.getElementById('record-status-2');
        recordStatus.innerText = "Merekam (100Hz)... 0 baris";
        recordStatus.style.color = "#d32f2f";
    }
}

function stopRecord(device) {
    if (device === 1) {
        isRecording_1 = false;
        document.getElementById('btn-start-1').disabled = false;
        document.getElementById('btn-stop-1').disabled = true;
        
        const recordStatus = document.getElementById('record-status-1');
        recordStatus.innerText = `Selesai! Total: ${dataCount_1} baris.`;
        recordStatus.style.color = "#2e7d32";

        const blob = new Blob([csvDataArray_1.join("\n")], { type: 'text/csv' });
        const url = window.URL.createObjectURL(blob);
        
        const btnDownload = document.getElementById('btn-download-1');
        btnDownload.href = url;
        btnDownload.download = `Log_Pekerja1_${Date.now()}.csv`;
        btnDownload.style.display = "inline-block";
    } 
    else if (device === 2) {
        isRecording_2 = false;
        document.getElementById('btn-start-2').disabled = false;
        document.getElementById('btn-stop-2').disabled = true;
        
        const recordStatus = document.getElementById('record-status-2');
        recordStatus.innerText = `Selesai! Total: ${dataCount_2} baris.`;
        recordStatus.style.color = "#2e7d32";

        const blob = new Blob([csvDataArray_2.join("\n")], { type: 'text/csv' });
        const url = window.URL.createObjectURL(blob);
        
        const btnDownload = document.getElementById('btn-download-2');
        btnDownload.href = url;
        btnDownload.download = `Log_Pekerja2_${Date.now()}.csv`;
        btnDownload.style.display = "inline-block";
    }
}


// ==========================================
// 3. PENERIMAAN DATA DARI 2 ESP32 (SSE)
// ==========================================
const IP_PEKERJA_1 = "http://192.168.100.105/events"; 
const IP_PEKERJA_2 = "http://192.168.100.106/events"; 

let latestData_1 = null;
let latestData_2 = null;

if (!!window.EventSource) {
    
    // --- KONEKSI DEVICE 1 ---
    const source1 = new EventSource(IP_PEKERJA_1);
    const wsStatus1 = document.getElementById('ws-status-1');

    source1.addEventListener('open', () => {
        wsStatus1.innerText = 'TERHUBUNG';
        wsStatus1.className = 'connection-status connected';
    });
    source1.addEventListener('error', (e) => {
        if (e.target.readyState != EventSource.OPEN) {
            wsStatus1.innerText = 'TERPUTUS';
            wsStatus1.className = 'connection-status disconnected';
        }
    });
    source1.addEventListener('sensor_data', (e) => {
        try {
            latestData_1 = JSON.parse(e.data);
            if (isRecording_1) {
                // UPDATE V2: Baris data rekaman menyertakan data sensor sekunder
                const row = `${latestData_1.time || 0},${latestData_1.x},${latestData_1.y},${latestData_1.z},${latestData_1.gx || 0},${latestData_1.gy || 0},${latestData_1.gz || 0},${latestData_1.roll},${latestData_1.pitch},${latestData_1.yaw},${latestData_1.temp || 0},${latestData_1.hum || 0},${latestData_1.bpm || 0},${latestData_1.spo2 || 0},${latestData_1.lat || 0},${latestData_1.lng || 0}`;
                csvDataArray_1.push(row);
                dataCount_1++;
            }
        } catch (err) {}
    });

    // --- KONEKSI DEVICE 2 ---
    const source2 = new EventSource(IP_PEKERJA_2);
    const wsStatus2 = document.getElementById('ws-status-2');

    source2.addEventListener('open', () => {
        wsStatus2.innerText = 'TERHUBUNG';
        wsStatus2.className = 'connection-status connected';
    });
    source2.addEventListener('error', (e) => {
        if (e.target.readyState != EventSource.OPEN) {
            wsStatus2.innerText = 'TERPUTUS';
            wsStatus2.className = 'connection-status disconnected';
        }
    });
    source2.addEventListener('sensor_data', (e) => {
        try {
            latestData_2 = JSON.parse(e.data);
            if (isRecording_2) {
                // UPDATE V2: Baris data rekaman menyertakan data sensor sekunder
                const row = `${latestData_2.time || 0},${latestData_2.x},${latestData_2.y},${latestData_2.z},${latestData_2.gx || 0},${latestData_2.gy || 0},${latestData_2.gz || 0},${latestData_2.roll},${latestData_2.pitch},${latestData_2.yaw},${latestData_2.temp || 0},${latestData_2.hum || 0},${latestData_2.bpm || 0},${latestData_2.spo2 || 0},${latestData_2.lat || 0},${latestData_2.lng || 0}`;
                csvDataArray_2.push(row);
                dataCount_2++;
            }
        } catch (err) {}
    });

} else {
    console.error("Browser Anda tidak mendukung Server-Sent Events (SSE).");
}


// ==========================================
// 4. PEMBARUAN LAYAR DOM & GRAFIK (INTERVAL)
// ==========================================
setInterval(() => {
    
    // --- UPDATE UI PEKERJA 1 ---
    if (latestData_1) {
        document.getElementById('val-total-accel-1').innerText = parseFloat(latestData_1.total || 0).toFixed(2);
        document.getElementById('val-roll-1').innerText = parseFloat(latestData_1.roll).toFixed(1);
        document.getElementById('val-pitch-1').innerText = parseFloat(latestData_1.pitch).toFixed(1);
        document.getElementById('val-yaw-1').innerText = parseFloat(latestData_1.yaw).toFixed(1);
        
        // UPDATE V2: Print data sensor baru Pekerja 1 ke DOM kartu halaman web
        document.getElementById('val-temp-1').innerText = parseFloat(latestData_1.temp || 0).toFixed(1);
        document.getElementById('val-hum-1').innerText = parseFloat(latestData_1.hum || 0).toFixed(1);
        document.getElementById('val-bpm-1').innerText = parseInt(latestData_1.bpm || 0);
        document.getElementById('val-spo2-1').innerText = parseInt(latestData_1.spo2 || 0);
        document.getElementById('val-lat-1').innerText = parseFloat(latestData_1.lat || 0).toFixed(6);
        document.getElementById('val-lng-1').innerText = parseFloat(latestData_1.lng || 0).toFixed(6);

        const timeString = latestData_1.time.toString();
        
        chartAccel1.data.labels.push(timeString);
        chartAccel1.data.datasets[0].data.push(latestData_1.x);
        chartAccel1.data.datasets[1].data.push(latestData_1.y);
        chartAccel1.data.datasets[2].data.push(latestData_1.z);

        chartGyro1.data.labels.push(timeString);
        chartGyro1.data.datasets[0].data.push(latestData_1.gx || 0);
        chartGyro1.data.datasets[1].data.push(latestData_1.gy || 0);
        chartGyro1.data.datasets[2].data.push(latestData_1.gz || 0);

        if (chartAccel1.data.labels.length > MAX_CHART_POINTS) {
            chartAccel1.data.labels.shift();
            chartAccel1.data.datasets.forEach(ds => ds.data.shift());
            
            chartGyro1.data.labels.shift();
            chartGyro1.data.datasets.forEach(ds => ds.data.shift());
        }

        chartAccel1.update();
        chartGyro1.update();

        if (isRecording_1) {
            document.getElementById('record-status-1').innerText = `Merekam (100Hz)... ${dataCount_1} baris`;
        }
    }

    // --- UPDATE UI PEKERJA 2 ---
    if (latestData_2) {
        document.getElementById('val-total-accel-2').innerText = parseFloat(latestData_2.total || 0).toFixed(2);
        document.getElementById('val-roll-2').innerText = parseFloat(latestData_2.roll).toFixed(1);
        document.getElementById('val-pitch-2').innerText = parseFloat(latestData_2.pitch).toFixed(1);
        document.getElementById('val-yaw-2').innerText = parseFloat(latestData_2.yaw).toFixed(1);
        
        // UPDATE V2: Print data sensor baru Pekerja 2 ke DOM kartu halaman web
        document.getElementById('val-temp-2').innerText = parseFloat(latestData_2.temp || 0).toFixed(1);
        document.getElementById('val-hum-2').innerText = parseFloat(latestData_2.hum || 0).toFixed(1);
        document.getElementById('val-bpm-2').innerText = parseInt(latestData_2.bpm || 0);
        document.getElementById('val-spo2-2').innerText = parseInt(latestData_2.spo2 || 0);
        document.getElementById('val-lat-2').innerText = parseFloat(latestData_2.lat || 0).toFixed(6);
        document.getElementById('val-lng-2').innerText = parseFloat(latestData_2.lng || 0).toFixed(6);

        const timeString = latestData_2.time.toString();
        
        chartAccel2.data.labels.push(timeString);
        chartAccel2.data.datasets[0].data.push(latestData_2.x);
        chartAccel2.data.datasets[1].data.push(latestData_2.y);
        chartAccel2.data.datasets[2].data.push(latestData_2.z);

        chartGyro2.data.labels.push(timeString);
        chartGyro2.data.datasets[0].data.push(latestData_2.gx || 0);
        chartGyro2.data.datasets[1].data.push(latestData_2.gy || 0);
        chartGyro2.data.datasets[2].data.push(latestData_2.gz || 0);

        if (chartAccel2.data.labels.length > MAX_CHART_POINTS) {
            chartAccel2.data.labels.shift();
            chartAccel2.data.datasets.forEach(ds => ds.data.shift());
            
            chartGyro2.data.labels.shift();
            chartGyro2.data.datasets.forEach(ds => ds.data.shift());
        }

        chartAccel2.update();
        chartGyro2.update();

        if (isRecording_2) {
            document.getElementById('record-status-2').innerText = `Merekam (100Hz)... ${dataCount_2} baris`;
        }
    }

}, 66);