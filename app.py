from flask import Flask, render_template, jsonify, request
from pymongo import MongoClient
from datetime import datetime
import os

app = Flask(__name__)

MONGO_URI = os.environ.get("MONGO_URI")
client = MongoClient(MONGO_URI)
db = client["tanque_db"]
coleccion = db["registros"]

@app.route('/')
def home():
    return render_template('home.html')


@app.route('/dashboard')
def dashboard():
    return render_template('index.html')


@app.route('/registros')
def registros():
    return render_template('registros.html')


@app.route('/api/registros')
def api_registros():
    try:
        datos = list(
            coleccion.find({}, {"_id": 0}).sort("fecha", -1)
        )
        return jsonify(datos), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/')
def index():
    return render_template('index.html')

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

        ultimo = registros[0]
        registros_cronologicos = registros[::-1]

        return jsonify({
            "nivel_texto": ultimo["nivel"],
            "nivel_distancia": ultimo["nivel_cm"],
            "capacidad_val": ultimo["capacidad"],
            "estado_bomba": "ON" if ultimo["bomba"] == 1 else "OFF",
            "ultima_actualizacion": ultimo["fecha"].strftime("%Y-%m-%d %H:%M:%S"),
            "historial_tiempos": [
                r["fecha"].strftime("%H:%M:%S") for r in registros_cronologicos
            ],
            "historial_niveles": [r["nivel"] for r in registros_cronologicos],
            "tabla": registros
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/update', methods=['POST'])
def update_data():
    try:
        content = request.json

        documento = {
            "fecha": datetime.now(),
            "nivel_cm": content.get("nivel_cm"),
            "nivel": content.get("nivel"),
            "capacidad": content.get("capacidad"),
            "bomba": 1 if content.get("bomba") else 0
        }

        coleccion.insert_one(documento)

        return jsonify({"status": "success"}), 201

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 5000))
