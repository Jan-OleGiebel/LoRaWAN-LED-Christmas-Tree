/*
  LoRaWAN®-LED-Christmas-Tree payload decoder v.1.0 by.: Jan-Ole Giebel
  This code is released under the Apache 2.0 License.

  Copyright 2024 Jan-Ole Giebel

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

function decodeUplink(input) {
    const bytes = input.bytes;
    let decoded = {
        type: bytes[0] === 0x01 ? 'ok' : 'error',
        firmware: {
            version: (bytes[2] << 8) | bytes[3]
        }
    };

    // Define initial offset based on packet type
    let offset = bytes[0] === 0x01 ? 4 : 3;

    while (offset < bytes.length) {
        const fieldId = bytes[offset];
        
        switch (fieldId) {
            case 0x03: // Temperature
                const temp = (bytes[offset + 1] << 8) | bytes[offset + 2];
                decoded.temperature = (temp / 100).toFixed(2);
                offset += 3;
                break;

            case 0x04: // Humidity
                const hum = (bytes[offset + 1] << 8) | bytes[offset + 2];
                decoded.humidity = (hum / 100).toFixed(2);
                offset += 3;
                break;

            case 0x05: // Battery voltage
                const batteryBytes = bytes.slice(offset + 1, offset + 5);
                const batteryFloat = new Float32Array(new Uint8Array(batteryBytes).buffer)[0];
                decoded.batteryVoltage = batteryFloat.toFixed(2);
                offset += 5;
                break;

            case 0x06: // TX Interval
                decoded.txInterval = (bytes[offset + 1] << 24) |
                                   (bytes[offset + 2] << 16) |
                                   (bytes[offset + 3] << 8) |
                                   bytes[offset + 4];
                offset += 5;
                break;

            case 0x07: // Mode
                decoded.mode = bytes[offset + 1];
                offset += 2;
                break;

            case 0x08: // Max Brightness
                decoded.maxBrightness = (bytes[offset + 1] << 8) | bytes[offset + 2];
                offset += 3;
                break;

            default:
                return {
                    errors: [`Unknown field ID: ${fieldId} at offset ${offset}`]
                };
        }
    }

    return {
        data: decoded
    };
}
