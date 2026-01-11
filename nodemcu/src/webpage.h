#ifndef WEBPAGE_H
#define WEBPAGE_H

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BME280 Live-Daten</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
<h2>BME280 Live-Daten</h2>
<p id="values">Lade...</p>
<canvas id="chart"></canvas>

<script>
const ctx = document.getElementById('chart').getContext('2d');

const data = {
  labels: [],
  datasets: [
    {
      label: 'Temperatur (°C)',
      data: [],
      borderColor: 'red',
      fill: false,
      tension: 0.2,
      yAxisID: 'y'
    },
    {
      label: 'Luftfeuchte (%)',
      data: [],
      borderColor: 'blue',
      fill: false,
      tension: 0.2,
      yAxisID: 'y'
    },
    {
      label: 'Luftdruck (hPa)',
      data: [],
      borderColor: 'green',
      fill: false,
      tension: 0.2,
      yAxisID: 'y1'
    }
  ]
};

const chart = new Chart(ctx, {
  type: 'line',
  data: data,
  options: {
    responsive: true,
    interaction: {
      mode: 'index',
      intersect: false
    },
    stacked: false,
    plugins: {
      legend: { display: true }
    },
    scales: {
      y: {
        type: 'linear',
        position: 'left',
        title: { display: true, text: '°C / %' },
        suggestedMin: 0,
        suggestedMax: 100
      },
      y1: {
        type: 'linear',
        position: 'right',
        title: { display: true, text: 'hPa' },
        suggestedMin: 900,
        suggestedMax: 1100,
        grid: { drawOnChartArea: false }
      }
    }
  }
});

function update() {
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      document.getElementById('values').innerHTML =
        `Temp: ${d.temperature} °C<br>
         Feuchte: ${d.humidity} %<br>
         Druck: ${d.pressure} hPa`;

      const t = new Date().toLocaleTimeString();
      data.labels.push(t);
      data.datasets[0].data.push(d.temperature);
      data.datasets[1].data.push(d.humidity);
      data.datasets[2].data.push(d.pressure);

      if (data.labels.length > 20) {
        data.labels.shift();
        data.datasets.forEach(ds => ds.data.shift());
      }
      chart.update();
    });
}

setInterval(update, 2000);
update();
</script>
</body>
</html>
)rawliteral";

#endif
