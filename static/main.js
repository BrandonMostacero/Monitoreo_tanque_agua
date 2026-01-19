let chart;
let alturaTanque = null;

function initChart() {
    const ctx = document.getElementById('lineChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Nivel de agua (cm)',
                data: [],
                borderColor: '#38bdf8',
                backgroundColor: 'rgba(56, 189, 248, 0.1)',
                fill: true,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: {
                        color: '#94a3b8'
                    }
                }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    max: 15,
                    title: {
                        display: true,
                        text: "Altura (cm)",
                        color: '#94a3b8'
                    },
                    grid: {
                        color: 'rgba(255, 255, 255, 0.1)'
                    },
                    ticks: {
                        color: '#94a3b8',
                        callback: v => v + " cm"
                    }
                },
                x: {
                    title: {
                        display: true,
                        text: "Tiempo",
                        color: '#94a3b8'
                    },
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: '#94a3b8'
                    }
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

        if (data.h_tanque && data.h_tanque !== alturaTanque) {
            alturaTanque = data.h_tanque;
            chart.options.scales.y.max = alturaTanque;
            chart.update();
        }

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

async function calibrarTanque() {
    try {
        const response = await fetch('/api/calibrar', {
            method: 'POST'
        });

        if (!response.ok) throw new Error("Error calibrando");

        alert("Calibración solicitada. Asegura que el tanque esté vacío.");

    } catch (e) {
        alert("No se pudo enviar la orden de calibración");
    }
}

document.addEventListener('DOMContentLoaded', () => {
    initChart();
    fetchData();
    setInterval(fetchData, 2000);

    const btn = document.getElementById('btn-calibrar');
    if (btn) {
        btn.addEventListener('click', calibrarTanque);
    }
});

if (data.h_tanque !== null && data.h_tanque !== undefined) {
    document.getElementById('altura-tanque').innerText =
        data.h_tanque.toFixed(2) + " cm";
}
