import cv2
from ultralytics import YOLO
import requests
import numpy as np
import time
import mailing
from datetime import datetime
face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades +
    "haarcascade_frontalface_default.xml"
)

model = YOLO("models/yolo26n.pt")

def get_detected_frame(ip):

    response = requests.get(
        f"http://{ip}/capture",
        timeout=3
    )

    img_array = np.frombuffer(
        response.content,
        dtype=np.uint8
    )

    frame = cv2.imdecode(
        img_array,
        cv2.IMREAD_COLOR
    )

    results = model(frame)[0]
    annotated_image = results.plot()

    _, jpeg = cv2.imencode(
        ".jpg",
        annotated_image
    )

    detected_objects = []
    
    for box in results.boxes:

        cls = int(box.cls[0])

        confidence = float(box.conf[0])

        name = model.names[cls]

        print(f"{name}: {confidence:.2f}")

        if confidence > 0.7:

            detected_objects.append({
                "name": name,
                "confidence": confidence
            })

    if len(detected_objects) > 0:            
        now = time.time()

        print(
            f"Cooldown left: "
            f"{mailing.MAIL_COOLDOWN - (now - mailing.last_mail_time):.1f}s"
            )
        if now - mailing.last_mail_time > mailing.MAIL_COOLDOWN:
            
            _, jpeg = cv2.imencode(
                ".jpg",
                annotated_image
                    )

            # schedule background send (non-blocking)
            mailing.send_alert(image_bytes=jpeg.tobytes(),
                               detected_objects=detected_objects)

            timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
            
            cv2.imwrite(
            f"captures/{timestamp}.jpg",
            frame
            )

            # mailing.send_alert already updated `mailing.last_mail_time`
    return jpeg.tobytes()