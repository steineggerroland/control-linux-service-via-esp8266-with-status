from flask import Flask, request, jsonify, abort
import subprocess
import os

SERVICE_NAME = "the.service"
API_TOKEN = os.environ.get("SERVICE_API_TOKEN")

app = Flask(__name__)

def require_token():
    token = request.args.get("token")
    if not token or token != API_TOKEN:
        abort(401)

def get_status():
    result = subprocess.run(["systemctl", "is-active", SERVICE_NAME], capture_output=True, text=True)
    return result.stdout.strip()

@app.route("/start", methods=["GET"])
def start_service():
    require_token()
    subprocess.run(["sudo", "systemctl", "start", SERVICE_NAME])
    return jsonify({"status": "starting", "service": SERVICE_NAME})

@app.route("/stop", methods=["GET"])
def stop_service():
    require_token()
    subprocess.run(["sudo", "systemctl", "stop", SERVICE_NAME])
    return jsonify({"status": "stopping", "service": SERVICE_NAME})

@app.route("/status", methods=["GET"])
def status():
    require_token()
    status = get_status()
    return jsonify({"status": status, "service": SERVICE_NAME})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
