import requests
from concurrent.futures import ThreadPoolExecutor

def check_ip(ip):
    try:
        r = requests.get(
            f"http://{ip}/info",
            timeout=0.3
        )

        data = r.json()

        if data.get("device") == "ESP32_CAMERA":
            return {
                "ip": ip,
                "name": data.get("name", "Unknown")
            }

    except:
        pass

    return None


def scan_network(subnet):
    found = []

    with ThreadPoolExecutor(max_workers=50) as pool:

        futures = []

        for i in range(1, 255):

            ip = f"{subnet}.{i}"

            futures.append(
                pool.submit(check_ip, ip)
            )

        for future in futures:

            result = future.result()

            if result:
                found.append(result)

    return found