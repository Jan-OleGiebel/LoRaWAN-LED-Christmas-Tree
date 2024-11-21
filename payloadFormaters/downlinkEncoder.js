/*
  LoRaWAN®-LED-Christmas-Tree payload encoder v.1.0 by.: Jan-Ole Giebel
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

function encodeDownlink(input) {
  const bytes = [];
  const data = input.data;
  
  switch (data.command) {
    case 'setInterval':
      bytes.push(0x00);
      bytes.push((data.interval >> 8) & 0xFF);
      bytes.push(data.interval & 0xFF);
      break;
      
    case 'setBrightness':
      bytes.push(0x01);
      bytes.push((data.brightness >> 8) & 0xFF);
      bytes.push(data.brightness & 0xFF);
      break;
      
    case 'setMode':
      bytes.push(0x02);
      bytes.push(data.mode & 0xFF);
      break;
      
    case 'setPixel':
      bytes.push(0x03);
      bytes.push(data.index & 0xFF);
      bytes.push(data.red & 0xFF);
      bytes.push(data.green & 0xFF);
      bytes.push(data.blue & 0xFF);
      break;
      
    case 'setAllPixels':
      bytes.push(0x04);
      bytes.push(data.red & 0xFF);
      bytes.push(data.green & 0xFF);
      bytes.push(data.blue & 0xFF);
      break;
      
    case 'setAnimation':
      bytes.push(0x05);
      bytes.push(data.frames.length);
      
      data.frames.forEach(frame => {
        bytes.push(frame.delay & 0xFF);
        bytes.push(frame.fadeTime & 0xFF);
        
        frame.pixels.forEach(pixel => {
          bytes.push(pixel.red & 0xFF);
          bytes.push(pixel.green & 0xFF);
          bytes.push(pixel.blue & 0xFF);
        });
      });
      break;
  }
  
  return {
    bytes: bytes,
    fPort: 1
  };
}
