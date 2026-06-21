import smtplib
from email.message import EmailMessage
import os
from datetime import datetime
import time
from concurrent.futures import ThreadPoolExecutor

# Simple in-process thread pool for background sending
_executor = ThreadPoolExecutor(max_workers=2)

last_mail_time = 0
MAIL_COOLDOWN = 60


def _build_message(image_bytes, detected_objects):
    obj_string = ""

    for obj in detected_objects:
        obj_string += (
            f"<li>"
            f"{obj['name']} "
            f"({obj['confidence']*100:.1f}%)"
            f"</li>"
        )

    msg = EmailMessage()

    msg["Subject"] = "Objects detected"
    msg["From"] = os.getenv("EMAIL_USER")
    msg["To"] = os.getenv("EMAIL_USER")

    msg.set_content("Detected recognizable objects")

    msg.add_alternative(f"""
    <html>
    <body style="font-family:Arial,sans-serif;background:#f5f5f5;padding:20px;">

    <div style="
        max-width:600px;
        margin:auto;
        background:white;
        border-radius:12px;
        padding:20px;
        box-shadow:0 2px 10px rgba(0,0,0,0.1);
    ">

    <h2 style="color:#d9534f;">
    Image Detected
    </h2>

    <p>
    Objects detected through camera
    <ul>{obj_string}</ul>
    
    </p>

    <p>
    <b>Data:</b> {datetime.now()}
    </p>

    <p>
    The image is inside the attachment
    </p>

    <hr>

    <p style="color:#888;font-size:12px;">
    ESP Camera Manager
    </p>

    </div>

    </body>
    </html>
    """, subtype="html")

    msg.add_attachment(
        image_bytes,
        maintype="image",
        subtype="jpeg",
        filename="alert.jpg"
    )

    return msg


def _send_email_sync(msg):
    with smtplib.SMTP_SSL("smtp.gmail.com", 465) as smtp:
        smtp.login(
            os.getenv("EMAIL_USER"),
            os.getenv("EMAIL_PASS")
        )
        smtp.send_message(msg)


def send_alert(image_bytes, detected_objects):
    global last_mail_time

    msg = _build_message(image_bytes, detected_objects)

    # update cooldown timestamp immediately so caller can rely on it
    last_mail_time = time.time()

    # schedule actual send in background
    _executor.submit(_send_email_sync, msg)

    return None