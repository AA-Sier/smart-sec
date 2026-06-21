from flask import Flask
from flask import render_template
from flask import jsonify
from flask import Response
from flask import stream_with_context
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
    print(devices)
    return jsonify(devices)


@app.route("/devices")
def get_devices():
    return jsonify(devices)

def generate_detected_frames(ip):
    while True:
        image = get_detected_frame(ip)

        if image is None:
            continue

        yield (b"--frame\r\n"
               b"Content-Type: image/jpeg\r\n\r\n"
               + image + b"\r\n")


@app.route("/detect")
def detect():

    if current_ip is None:
        return "No camera selected", 400

    return Response(
        stream_with_context(
            generate_detected_frames(current_ip)
        ),
        mimetype="multipart/x-mixed-replace; boundary=frame"
    )

if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )