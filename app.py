from flask import Flask, render_template, jsonify, request
import sqlite3
import os

base_dir = os.path.dirname(os.path.abspath(__file__))
app = Flask(__name__)

DATABASE = os.path.join(base_dir, 'tanque.db')

def init_db():
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS registros (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                fecha TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                nivel_cm REAL,
                nivel TEXT,
                capacidad INTEGER,
                bomba INTEGER
            )
        ''')
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"Error inicializando DB: {e}")

init_db()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/data')
def get_data():
    try:
        conn = sqlite3.connect(DATABASE)
        conn.row_factory = sqlite3.Row
        query = "SELECT fecha, nivel_cm, nivel, capacidad, bomba FROM registros ORDER BY id DESC LIMIT 15"
        registros = conn.execute(query).fetchall()
        conn.close()

        if not registros:
            return jsonify({"error": "No hay datos"}), 404

        ultimo = registros[0]
        registros_cronologicos = registros[::-1]
        
        return jsonify({
            "nivel_texto": ultimo['nivel'],
            "nivel_distancia": ultimo['nivel_cm'],
            "capacidad_val": ultimo['capacidad'],
            "estado_bomba": "ON" if ultimo['bomba'] == 1 else "OFF",
            "ultima_actualizacion": ultimo['fecha'],
            "historial_tiempos": [r['fecha'].split()[-1] for r in registros_cronologicos],
            "historial_niveles": [r['nivel'] for r in registros_cronologicos],
            "tabla": [dict(r) for r in registros]
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/update', methods=['POST'])
def update_data():
    try:
        content = request.json
        nivel_cm = content.get('nivel_cm')
        nivel = content.get('nivel')
        capacidad = content.get('capacidad')
        bomba = 1 if content.get('bomba') else 0
        
        conn = sqlite3.connect(DATABASE)
        conn.execute("""
            INSERT INTO registros (fecha, nivel_cm, nivel, capacidad, bomba) 
            VALUES (datetime('now', 'localtime'), ?, ?, ?, ?)
        """, (nivel_cm, nivel, capacidad, bomba))
        conn.commit()
        conn.close()
        return jsonify({"status": "success"}), 201
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400
    
if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
