#ifndef JSY_MK333G_H
#define JSY_MK333G_H

#include <stdint.h>

uint16_t jsy_crc16(const uint8_t *data, uint16_t length);
void jsy_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *out_frame);

typedef enum {
    JSY_OK = 0,
    JSY_ERR_LENGTH,
    JSY_ERR_CRC,
    JSY_ERR_FUNCTION_CODE,
    JSY_ERR_BYTE_COUNT
}jsy_status_t;

jsy_status_t jsy_parse_response(const uint8_t *response, uint16_t response_len, uint16_t expected_reg_count,
                                uint16_t *out_registers);

// Starting addresses for the measurement register block (function code 0x03)
#define JSY_REG_VOLTAGE_A      0x0100
#define JSY_REG_VOLTAGE_B      0x0101
#define JSY_REG_VOLTAGE_C      0x0102
#define JSY_REG_CURRENT_A      0x0103
#define JSY_REG_CURRENT_B      0x0104
#define JSY_REG_CURRENT_C      0x0105
#define JSY_REG_POWER_FACTOR_TOTAL  0x0119

typedef struct {
    float voltage_a;
    float voltage_b;
    float voltage_c;
    float current_a;
    float current_b;
    float current_c;
    float power_factor_total;
} jsy_measurements_t;

void jsy_decode_measurements(const uint16_t *raw_registers, jsy_measurements_t *out);


#endif // JSY_MK333G_H