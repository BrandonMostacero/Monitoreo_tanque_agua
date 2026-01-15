from flask import Flask, render_template, jsonify, request
from flask_pymongo import PyMongo
from datetime import datetime
import os

app = Flask(__name__)

app.config["MONGO_URI"] = os.environ.get("MONGO_URI", "mongodb://localhost:27017/tanque_db")
mongo = PyMongo(app)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/data')
def get_data():
    try:
        cursor = mongo.db.registros.find().sort("fecha", -1).limit(15)
        registros = list(cursor)

        if not registros:
            return jsonify({"error": "No hay datos"}), 404

        ultimo = registros[0]
        registros_cronologicos = registros[::-1]
        
        return jsonify({
            "nivel_texto": ultimo.get('nivel'),
            "nivel_distancia": ultimo.get('nivel_cm'),
            "capacidad_val": ultimo.get('capacidad'),
            "estado_bomba": "ON" if ultimo.get('bomba') == 1 else "OFF",
            "ultima_actualizacion": ultimo.get('fecha').strftime('%Y-%m-%d %H:%M:%S'),
            "historial_tiempos": [r['fecha'].strftime('%H:%M:%S') for r in registros_cronologicos],
            "historial_niveles": [r['nivel'] for r in registros_cronologicos],
            "tabla": [{
                "fecha": r['fecha'].strftime('%Y-%m-%d %H:%M:%S'),
                "nivel_cm": r['nivel_cm'],
                "nivel": r['nivel'],
                "capacidad": r['capacidad'],
                "bomba": r['bomba']
            } for r in registros]
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/update', methods=['POST'])
def update_data():
    try:
        content = request.json
        
        nuevo_registro = {
            "fecha": datetime.now(),
            "nivel_cm": content.get('nivel_cm'),
            "nivel": content.get('nivel'),
            "capacidad": content.get('capacidad'),
            "bomba": 1 if content.get('bomba') else 0
        }
        
        mongo.db.registros.insert_one(nuevo_registro)
        return jsonify({"status": "success"}), 201
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400
    
if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    app.run(host='0.0.0.0', port=port)
