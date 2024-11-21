/*
  LoRaWAN®-LED-Christmas-Tree downlink decoder v.1.0 by.: Jan-Ole Giebel
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

function decodeDownlink(input) {
  const bytes = input.bytes;
  let decoded = {
    data: {
      command: '',
    }
  };
  
  if (bytes.length === 0) {
    return {
      warnings: ['Empty payload'],
      data: { }
    };
  }
  
  const command = bytes[0];
  
  try {
    switch (command) {
      case 0x00:
        if (bytes.length !== 3) throw new Error('Invalid length for setInterval command');
        decoded.data.command = 'setInterval';
        decoded.data.interval = (bytes[1] << 8) | bytes[2];
        break;
        
      case 0x01:
        if (bytes.length !== 3) throw new Error('Invalid length for setBrightness command');
        decoded.data.command = 'setBrightness';
        decoded.data.brightness = (bytes[1] << 8) | bytes[2];
        break;
        
      case 0x02:
        if (bytes.length !== 2) throw new Error('Invalid length for setMode command');
        decoded.data.command = 'setMode';
        decoded.data.mode = bytes[1];
        break;
        
      case 0x03:
        if (bytes.length !== 5) throw new Error('Invalid length for setPixel command');
        decoded.data.command = 'setPixel';
        decoded.data.index = bytes[1];
        decoded.data.red = bytes[2];
        decoded.data.green = bytes[3];
        decoded.data.blue = bytes[4];
        break;
        
      case 0x04:
        if (bytes.length !== 4) throw new Error('Invalid length for setAllPixels command');
        decoded.data.command = 'setAllPixels';
        decoded.data.red = bytes[1];
        decoded.data.green = bytes[2];
        decoded.data.blue = bytes[3];
        break;
        
      case 0x05:
        if (bytes.length < 3) throw new Error('Invalid length for animation data');
        decoded.data.command = 'setAnimation';
        const numFrames = bytes[1];
        decoded.data.frames = [];
        
        let offset = 2;
        for (let i = 0; i < numFrames; i++) {
          if (offset + 50 > bytes.length) throw new Error(`Incomplete frame data for frame ${i}`);
          
          const frame = {
            delay: bytes[offset],
            fadeTime: bytes[offset + 1],
            pixels: []
          };
          
          for (let p = 0; p < 16; p++) {
            const pixelOffset = offset + 2 + (p * 3);
            frame.pixels.push({
              red: bytes[pixelOffset],
              green: bytes[pixelOffset + 1],
              blue: bytes[pixelOffset + 2]
            });
          }
          
          decoded.data.frames.push(frame);
          offset += 50;
        }
        break;
        
      default:
        throw new Error(`Unknown command: 0x${command.toString(16)}`);
    }
    
    return decoded;
    
  } catch (error) {
    return {
      warnings: [error.message],
      data: {
        error: error.message,
        command: command
      }
    };
  }
}
