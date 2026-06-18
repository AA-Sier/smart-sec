from flask import Flask
from flask import render_template
from flask import jsonify
from flask import Response
from scanner import scan_network
from face_detect import get_detected_frame
from flask_cors import CORS
app = Flask(__name__)
CORS(app)
current_ip = None
devices = []


@app.route("/")
def index():
    return render_template(
        "index.html"
    )

@app.route("/select/<ip>")
def select_camera(ip):

    global current_ip

    current_ip = ip

    return "OK"

@app.route("/scan")
def scan():

    global devices

    devices = scan_network(
        "192.168.1"
    )

    return jsonify(devices)


@app.route("/devices")
def get_devices():
    return jsonify(devices)

@app.route("/detect")
def detect():

    if current_ip is None:
        return "No camera selected", 400

    image = get_detected_frame(
        current_ip
    )

    return Response(
        image,
        mimetype="image/jpeg"
    )

if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )