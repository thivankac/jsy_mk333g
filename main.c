#include <stdio.h>
#include <stdint.h>
#include "jsy_mk333g.h"

int main(void)
{
    ////////////////////////////////////////////////////////////
    uint8_t test_data[] = {0x01, 0x03, 0x01, 0x00, 0x00, 0x03};
    uint16_t crc = jsy_crc16(test_data, 6);
    printf("CRC = 0x%04X\n", crc);
    ///////////////////////////////////////////////////////////
    uint8_t frame[8];
    jsy_build_read_request(1, 0x0100, 3, frame);

    for (int i = 0; i < 8; i++)
    {
        printf("%02X", frame[i]);
    }
    printf("\n");
    ////////////////////////////////////////////////////////
    uint8_t response1[] = {0x01, 0x03, 0x06, 0x56, 0x11, 0x56, 0x22, 0x56, 0x33, 0x1F, 0x77};
    uint16_t registers[3];

    jsy_status_t status = jsy_parse_response(response1, sizeof(response1), 3, registers);

    if (status == JSY_OK)
    {
        printf("Parsed Ok\n");
        for (int i = 0; i < 3; i++)
        {
            printf("Register %d raw = %u\n", i, registers[i]);
        }
    }
    else
    {
        printf("Parse failed with status %d\n", status);
    }
    ///////////////////////////////////////////////////
    uint8_t request[8];
    jsy_build_read_request(1, 0x0100, 6, request);

    uint8_t response2[] = {
        0x01, 0x03, 0x0C, // addr, func, byte count (12 bytes = 6 registers)
        0x56, 0x11,       // voltage A = 220.33
        0x56, 0x22,       // voltage B = 220.50
        0x56, 0x33,       // voltage C = 220.67
        0x00, 0x00,       // current A = 0.00
        0x00, 0x00,       // current B = 0.00
        0x00, 0x00,       // current C = 0.00
        0xEE, 0x6E        //  CRC low=0xEE  high=0x6E
    };

    uint16_t crc2 = jsy_crc16(response2, 15);
    printf("CRC low = 0x%02X high = 0x%02X\n", crc2 & 0xFF, crc2 >> 8);

    uint16_t raw_regs[6];
    jsy_status_t status2 = jsy_parse_response(response2, sizeof(response2), 6, raw_regs);

    if (status2 == JSY_OK)
    {
        jsy_measurements_t m;
        jsy_decode_measurements(raw_regs, &m);
        printf("Voltage A: %.2f V\n", m.voltage_a);
        printf("Voltage B: %.2f V\n", m.voltage_b);
        printf("Voltage C: %.2f V\n", m.voltage_c);
    }
    else
    {
        printf("Parse failed: %d\n", status2);
    }

    return 0;
}