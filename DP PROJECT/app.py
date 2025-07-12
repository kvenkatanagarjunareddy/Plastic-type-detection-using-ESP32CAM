from flask import Flask, request, render_template, jsonify
import cv2
import torch
import os
import datetime
import uuid
import numpy as np
import json
from ultralytics import YOLO

app = Flask(__name__)

UPLOAD_FOLDER = "static/uploads"
DETECTION_FOLDER = "static/detections"
LOG_FILE = "static/detection_log.json"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(DETECTION_FOLDER, exist_ok=True)

# Initialize log file if it doesn't exist
if not os.path.exists(LOG_FILE):
    with open(LOG_FILE, 'w') as f:
        json.dump([], f)

yolo_model = YOLO("best.pt")  # Load YOLOv8 model

def detect_objects(image_path):
    img = cv2.imread(image_path) 
    results = yolo_model.predict(img)

    detected_objects = []
    for result in results:
        for box in result.boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0].cpu().numpy())
            class_id = int(box.cls.cpu().numpy()[0])
            label = yolo_model.names[class_id]
            detected_objects.append(label)

            # Draw bounding box and label
            cv2.rectangle(img, (x1, y1), (x2, y2), (0, 255, 0), 2)
            cv2.putText(img, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

    # Generate timestamp
    now = datetime.datetime.now()
    timestamp = now.strftime("%Y-%m-%d_%H-%M-%S")
    time_display = now.strftime("%I:%M %p")  # Format as 10:00 PM
    date_display = now.strftime("%Y-%m-%d")

    # Create filename
    output_filename = f"{timestamp}_{os.path.basename(image_path)}"
    output_path = os.path.join(DETECTION_FOLDER, output_filename)
    cv2.imwrite(output_path, img)

    # Save detection info to log
    detection_info = {
        "filename": output_filename,
        "objects": detected_objects,
        "timestamp": timestamp,
        "time_display": time_display,
        "date_display": date_display
    }

    # Update log file
    try:
        with open(LOG_FILE, 'r') as f:
            logs = json.load(f)
    except:
        logs = []
    
    logs.append(detection_info)
    
    with open(LOG_FILE, 'w') as f:
        json.dump(logs, f)

    return output_filename, detected_objects, timestamp, time_display, date_display

@app.route('/')
def index():
    try:
        with open(LOG_FILE, 'r') as f:
            logs = json.load(f)
        # Sort by timestamp (newest first)
        logs = sorted(logs, key=lambda x: x.get("timestamp", ""), reverse=True)
    except:
        logs = []
    
    # Format for display
    for log in logs:
        # Join detected objects with commas or show "Unknown"
        if log.get("objects") and len(log["objects"]) > 0:
            log["detection_name"] = ", ".join(log["objects"])
        else:
            log["detection_name"] = "Unknown"
    
    return render_template('dashboard.html', logs=logs)

@app.route('/upload', methods=['POST'])
def upload():
    if 'image' not in request.files:
        return jsonify({"error": "No image uploaded"}), 400
    file = request.files['image']
    filename = f"{uuid.uuid4().hex}.jpg"
    filepath = os.path.join(UPLOAD_FOLDER, filename)
    file.save(filepath)

    detected_filename, detected_objects, timestamp, time_display, date_display = detect_objects(filepath)

    return jsonify({
        "status": "success",
        "detected_image": f"/static/detections/{detected_filename}",
        "objects": detected_objects,
        "timestamp": timestamp,
        "time": time_display,
        "date": date_display
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)