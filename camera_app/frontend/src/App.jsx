import { useState } from "react";

function App() {

  const [devices, setDevices] = useState([]);

  async function scanDevices() {

    await fetch(
      "http://localhost:5000/scan"
    );

    const response =
      await fetch(
        "http://localhost:5000/devices"
      );

    const data =
      await response.json();

    setDevices(data);
  }

  return (
    <div>

      <h1>ESP Camera Manager</h1>

      <button
        onClick={scanDevices}
      >
        Skanuj sieć
      </button>

      <ul>
        {devices.map(device => (
          <li key={device.ip}>
            {device.name} ({device.ip})
          </li>
        ))}
      </ul>

    </div>
  );
}

export default App;
