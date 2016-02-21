#include "OneWireHub.h"
#include "DS18B20.h"


DS18B20::DS18B20(uint8_t ID1, uint8_t ID2, uint8_t ID3, uint8_t ID4, uint8_t ID5, uint8_t ID6, uint8_t ID7) : OneWireItem(ID1, ID2, ID3, ID4, ID5, ID6, ID7)
{
    scratchpad[0] = 0xA0; // TLSB --> 10 degC as std
    scratchpad[1] = 0x00; // TMSB
    scratchpad[2] = 0x4B; // THRE --> Trigger register TH
    scratchpad[3] = 0x46; // TLRE --> TLow
    scratchpad[4] = 0x7F; // Conf
    // = 0 R1 R0 1 1 1 1 1 --> R=0 9bit, R=3 12bit
    scratchpad[5] = 0xFF; // 0xFF
    scratchpad[6] = 0x00; // Rese
    scratchpad[7] = 0x10; // 0x10
    updateCRC(); // update scratchpad[8]
}

void DS18B20::updateCRC()
{
    scratchpad[8] = crc8(scratchpad, 8);
}

bool DS18B20::duty(OneWireHub *hub)
{
    const uint8_t done = hub->recv();

    switch (done)
    {
        case 0x44: // CONVERT T --> start a new measurement conversion
            hub->sendBit(1);
            if (dbg_sensor) Serial.println("DS18B20 : CONVERT T");
            break;

        case 0x4E: // WRITE SCRATCHPAD
            // write 3 byte of data to scratchpad[1:3]
            hub->recvData(&scratchpad[2], 3);
            updateCRC();
            if (dbg_sensor) Serial.println("DS18B20 : WRITE SCRATCHPAD");
            break;

        case 0xBE: // READ SCRATCHPAD
            hub->sendData(scratchpad, 9);
            if (hub->error()) return false;
            if (dbg_sensor)  Serial.println("DS18B20 : READ SCRATCHPAD");
            break;

        case 0x48: // COPY SCRATCHPAD to EPROM
            // send1 if parasite power is used
            if (dbg_sensor) Serial.println("DS18B20 : COPY SCRATCHPAD");
            break;

        case 0xB8: // RECALL E2 (EPROM to 3byte from Scratchpad)
            hub->sendBit(1); // signal that OP is done
            if (dbg_sensor) Serial.println("DS18B20 : RECALL E2");
            break;

        case 0xB4: // READ POWERSUPPLY
            hub->sendBit(1); // 1: say i am external powered, 0: uses parasite power
            if (dbg_sensor) Serial.println("DS18B20 : READ POWERSUPPLY");
            break;

            // READ TIME SLOTS, respond with 1 if conversion is done, not usable with parasite power

            //  write trim2               0x63
            //  copy trim2                0x64
            //  read trim2                0x68
            //  read trim1                0x93
            //  copy trim1                0x94
            //  write trim1               0x95
        case 0xEC:
            // Alarm search command, respond if flag is set
            if (dbg_sensor) Serial.println("DS18B20 : ALARM REQUESTED");
            break;


        default:
            if (dbg_HINT)
            {
                Serial.print("DS18B20=");
                Serial.println(done, HEX);
            }
            break;
    }

    return true;
}


void DS18B20::setTemp(const float temperature_degC)
{
    setTempRaw(static_cast<int16_t>(temperature_degC * 16.0));
};

void DS18B20::setTemp(const int16_t temperature_degC) // TODO: could be int8_t, [-55;+85] degC
{
    setTempRaw(temperature_degC * static_cast<int8_t>(16));
};

// TODO: use allways 12bit mode? also 9,10,11,12 bit possible
void DS18B20::setTempRaw(const int16_t value_raw)
{
    int16_t value = value_raw;

    if (value > 0)
    {
        value &= 0x0FFF;
    }
    else
    {
        value = -value;
        value |= 0xF000;
    }

    scratchpad[0] = uint8_t(value);
    scratchpad[1] = uint8_t(value >> 8);
    // TODO: compare TH,TL with (value>>4 & 0xFF) (bit 11to4)
    // if out of bounds >=TH, <=TL trigger flag

    updateCRC();
};