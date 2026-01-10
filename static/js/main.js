let chart;

function initChart() {
    const ctx = document.getElementById('lineChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Nivel de agua (cm)',
                data: [],
                borderColor: '#007bff',
                backgroundColor: 'rgba(0, 123, 255, 0.1)',
                fill: true,
                tension: 0.3
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: true,
                    max: 12,
                    title: { display: true, text: "Altura (cm)" },
                    ticks: { callback: v => v + " cm" }
                },
                x: {
                    title: { display: true, text: "Tiempo" }
                }
            }
        }
    });
}

async function fetchData() {
    try {
        const response = await fetch('/api/data');
        if (!response.ok) throw new Error("HTTP error");
        const data = await response.json();

        const errorBanner = document.querySelector('.alert-error-conexion');
        if (errorBanner) errorBanner.style.display = 'none';

        document.getElementById('nivel-texto').innerText = data.nivel_texto;
        document.getElementById('nivel-cm').innerText = data.nivel_distancia + " cm";
        document.getElementById('bomba-status').innerText = data.estado_bomba;

        if (data.ultima_actualizacion) {
            const [fecha, hora] = data.ultima_actualizacion.split(" ");
            document.getElementById('ultima-fecha').innerText = fecha;
            document.getElementById('ultima-hora').innerText = hora;
        }

        const cap = data.capacidad_val || 0;
        document.getElementById('water-level').style.height = cap + "%";
        document.getElementById('porcentaje-visual').innerText = cap + "%";

        const registros = data.tabla.slice().reverse();
        chart.data.labels = registros.map(r => r.fecha.split(" ")[1]);
        chart.data.datasets[0].data = registros.map(r => r.nivel_cm);
        chart.update();

        const tbody = document.getElementById('tabla-registros');
        tbody.innerHTML = registros.map(r => `
            <tr>
                <td>${r.fecha}</td>
                <td>${r.nivel_cm} cm</td>
                <td>${r.nivel}</td>
                <td>
                    <span class="badge ${r.bomba ? 'bg-danger' : 'bg-success'}">
                        ${r.bomba ? 'ON' : 'OFF'}
                    </span>
                </td>
            </tr>
        `).join('');

    } catch (e) {
        console.error("Error de conexión:", e);
        const errorBanner = document.querySelector('.alert-error-conexion');
        if (errorBanner) errorBanner.style.display = 'block';
    }
}

document.addEventListener('DOMContentLoaded', () => {
    initChart();
    fetchData();
    setInterval(fetchData, 2000);
});
