#!/usr/bin/env python3
import os
import json
from protocol_synthesizer import BitstreamBuilder, I2CGenerator

def generate_ds1307_testdata():
    sample_count = 20000
    samplerate = 1000000
    bb = BitstreamBuilder(num_channels=2, sample_count=sample_count, samplerate=samplerate)
    
    # scl=0, sda=1
    i2c = I2CGenerator(bb, scl_ch=0, sda_ch=1, speed=100000)
    
    # DS1307 I2C address is 0x68
    addr = 0x68
    
    # Sequence 1: Write register pointer to 0x00 (Seconds)
    i2c.start()
    i2c.write_byte(addr << 1) # Write
    i2c.write_byte(0x00)      # Reg pointer
    i2c.stop()
    
    bb.pos += 1000 # Idle
    
    # Sequence 2: Read 7 bytes (Time/Date)
    # 0x00: Sec, 0x01: Min, 0x02: Hour, 0x03: Day, 0x04: Date, 0x05: Month, 0x06: Year
    # Values: 59s, 59m, 23h, Day 7, 31st, Dec, 2026 (approx)
    i2c.start()
    i2c.write_byte((addr << 1) | 1) # Read
    i2c.write_byte(0x59) # Sec (BCD)
    i2c.write_byte(0x59) # Min
    i2c.write_byte(0x23) # Hour
    i2c.write_byte(0x07) # Day
    i2c.write_byte(0x31) # Date
    i2c.write_byte(0x12) # Month
    i2c.write_byte(0x26) # Year
    i2c.stop()
    
    # Save files
    test_dir = "testdata/ds1307_c/default"
    os.makedirs(test_dir, exist_ok=True)
    
    with open(os.path.join(test_dir, "input.bin"), "wb") as f:
        f.write(bb.get_bitpacked())
        
    config = {
        "decoder": "ds1307_c",
        "samplerate": samplerate,
        "num_channels": 2,
        "sample_count": sample_count,
        "channels": { "scl": 0, "sda": 1 },
        "stack": [
            {
                "id": "i2c_c",
                "channels": { "scl": 0, "sda": 1 }
            }
        ]
    }
    
    with open(os.path.join(test_dir, "config.json"), "w") as f:
        json.dump(config, f, indent=2)
        
    print(f"Generated DS1307 test data in {test_dir}")

if __name__ == "__main__":
    generate_ds1307_testdata()
