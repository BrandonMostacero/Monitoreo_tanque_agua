from flask import Flask, render_template, jsonify, request, send_file
from pymongo import MongoClient
from datetime import datetime
from zoneinfo import ZoneInfo
import os
import pandas as pd
from io import BytesIO

app = Flask(__name__)

MONGO_URI = os.environ.get("MONGO_URI")
client = MongoClient(MONGO_URI)
db = client["tanque_db"]
coleccion = db["registros"]

estado_control = {
    "calibrar": False,
    "modo": "auto",
    "bomba": False
}


TZ_PE = ZoneInfo("America/Lima")

@app.route('/')
def home():
    return render_template('home.html')

@app.route('/dashboard')
def dashboard():
    return render_template('index.html')

@app.route('/registros')
def registros():
    return render_template('registros.html')

@app.route('/api/data')
def get_data():
    try:
        registros = list(
            coleccion.find({}, {"_id": 0})
            .sort("fecha", -1)
            .limit(15)
        )

        if not registros:
            return jsonify({"error": "No hay datos"}), 404

        # Convertir fechas a hora Perú
        for r in registros:
            r["fecha"] = (
                r["fecha"]
                .replace(tzinfo=ZoneInfo("UTC"))
                .astimezone(TZ_PE)
                .strftime("%Y-%m-%d %H:%M:%S")
            )

        ultimo = registros[0]
        registros_cronologicos = registros[::-1]

        return jsonify({
            "nivel_texto": ultimo["nivel"],
            "nivel_distancia": ultimo["nivel_cm"],
            "capacidad_val": ultimo["capacidad"],
            "estado_bomba": "ON" if ultimo["bomba"] == 1 else "OFF",
            "h_tanque": ultimo.get("h_tanque"),
            "ultima_actualizacion": ultimo["fecha"],
            "historial_tiempos": [
                r["fecha"].split(" ")[1] for r in registros_cronologicos
            ],
            "historial_niveles": [
                r["nivel"] for r in registros_cronologicos
            ],
            "tabla": registros
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/registros')
def get_all_registros():
    try:
        registros = list(
            coleccion.find({}, {"_id": 0}).sort("fecha", -1)
        )
        return jsonify(registros)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/update', methods=['POST'])
def update_data():
    try:
        content = request.json

        documento = {
            "fecha": datetime.now(TZ_PE),
            "nivel_cm": content.get("nivel_cm"),
            "nivel": content.get("nivel"),
            "capacidad": content.get("capacidad"),
            "bomba": 1 if content.get("bomba") else 0,
            "h_tanque": content.get("h_tanque")
        }

        coleccion.insert_one(documento)
        return jsonify({"status": "success"}), 201

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/api/control')
def control():
    respuesta = {
        "calibrar": estado_control["calibrar"],
        "modo": estado_control["modo"],
        "bomba": estado_control["bomba"]
    }

    estado_control["calibrar"] = False

    return jsonify(respuesta)

@app.route('/api/control/auto', methods=['POST'])
def control_auto():
    estado_control["modo"] = "auto"
    return jsonify({"status": "ok", "modo": "auto"})

@app.route('/api/control/manual', methods=['POST'])
def control_manual():
    data = request.json

    estado_control["modo"] = "manual"
    estado_control["bomba"] = bool(data.get("bomba", False))

    return jsonify({
        "status": "ok",
        "modo": "manual",
        "bomba": estado_control["bomba"]
    })

@app.route('/api/calibrar', methods=['POST'])
def solicitar_calibracion():
    estado_control["calibrar"] = True
    return jsonify({"status": "ok"})

@app.route('/api/export/excel')
def export_excel():
    registros = list(coleccion.find({}, {"_id": 0}))

    if not registros:
        return jsonify({"error": "No hay datos"}), 404

    df = pd.DataFrame(registros)
    df["fecha"] = pd.to_datetime(df["fecha"])

    output = BytesIO()
    with pd.ExcelWriter(output, engine="openpyxl") as writer:
        df.to_excel(writer, index=False, sheet_name="Registros")

        ws = writer.sheets["Registros"]
        ws.column_dimensions["A"].width = 22
        ws.column_dimensions["B"].width = 12
        ws.column_dimensions["C"].width = 12
        ws.column_dimensions["D"].width = 14
        ws.column_dimensions["E"].width = 10

    output.seek(0)

    return send_file(
        output,
        as_attachment=True,
        download_name="registros_tanque.xlsx",
        mimetype="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
    )

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 5000)))
