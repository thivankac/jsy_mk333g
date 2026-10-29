#ifndef JSY_MK333G_H
#define JSY_MK333G_H

#include <stdint.h>


#define JSY_MEASUREMENTS_REG_COUNT 26
#define JSY_MEASUREMENTS_RESPONSE_LEN (5 + (JSY_MEASUREMENTS_REG_COUNT * 2)) // 5 bytes header + 2 bytes per register
/*
    Address(1 byte) | Function Code(1 byte) | Byte Count(1 byte) | Data | CRC(2 byte)
*/

#define JSY_MAX_READ_REGISTERS 64
#define JSY_MAX_RESPONSE_LEN (5 + (JSY_MAX_READ_REGISTERS * 2)) // 5 bytes header + 2 bytes per register

#define JSY_READ_REQUEST_LEN 8 // 1 byte slave addr + 1 byte function code + 2 bytes start reg + 2 bytes reg count + 2 bytes CRC

#define JSY_MAXWRITE_REGISTERS 16
#define JSY_WRITE_REQUEST_LEN (9 + (JSY_MAXWRITE_REGISTERS * 2)) // 9 bytes header + 2 bytes per register 

#define JSY_WRITE_RESPONSE_LEN 8 // 1 byte slave addr + 1 byte function code + 2 bytes start reg + 2 bytes reg count + 2 bytes CRC

/*
    Address(1 byte) | Function Code(1 byte) | Start Register(2 bytes) | Register Count(2 bytes) | Byte Count(1 byte) | Data | CRC(2 byte)
*/

#define JSY_REG_BASE 0x0100
#define JSY_IDX(reg) ((reg) - JSY_REG_BASE)

#define JSY_REG_VOLTAGE_A      (0x0100)
#define JSY_REG_VOLTAGE_B      (0x0101)
#define JSY_REG_VOLTAGE_C      (0x0102)
#define JSY_REG_CURRENT_A      (0x0103)
#define JSY_REG_CURRENT_B      (0x0104)
#define JSY_REG_CURRENT_C      (0x0105)

#define JSY_REG_TOTAL_ACTIVE_POWER_HIGH  (0x0109)
#define JSY_REG_TOTAL_ACTIVE_POWER_LOW   (0x010A)

#define JSY_REG_VOLTAGE_FREQUENCY  (0x0115)
#define JSY_REG_POWER_FACTOR_TOTAL  (0x0119)

#define JSY_REG_MODEL (0x0000)
#define JSY_REG_ADDR_BAUD (0x0004)
#define JSY_REG_VOLTAGE_LIMIT (0x0020)
#define JSY_REG_CURRENT_LIMIT (0x0021)   

#define JSY_MODEL_ID (0x0333)

#define JSY_BAUD_4800  (5)
#define JSY_BAUD_9600  (6)
#define JSY_BAUD_19200 (7)

#define JSY_FORMAT_8N1 (0)
#define JSY_FORMAT_8E1 (1)
#define JSY_FORMAT_8O1 (2)
#define JSY_FORMAT_8N2 (3)

uint16_t jsy_crc16(const uint8_t *data, uint16_t length);
void jsy_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *out_frame);

typedef enum {
    JSY_OK = 0,
    JSY_ERR_LENGTH,
    JSY_ERR_CRC,
    JSY_ERR_FUNCTION_CODE,
    JSY_ERR_BYTE_COUNT,
    JSY_ERR_TIMEOUT,
    JSY_ERR_INVALID_ARGUMENT,
    JSY_ERR_ADDRESS,
    JSY_ERR_EXCEPTION,          // slave addr | function code + 0x80 | error code (01/02/03) | CRC low | CRC high
    JSY_ERR_ILLEGAL_FUNCTION,   // meter doesn't support that function code
    JSY_ERR_ILLEGAL_ADDRESS,    // register address doesn't exist
    JSY_ERR_ILLEGAL_VALUE,      // value out of range
    JSY_ERR_ECHO_MISMATCH        // meter's reply doesn't match the request
}jsy_status_t;   

jsy_status_t jsy_parse_response(const uint8_t *response, uint16_t response_len, uint8_t expected_addr, uint16_t expected_reg_count,
                                uint16_t *out_registers);



typedef struct {
    float voltage_a;
    float voltage_b;
    float voltage_c;
    float current_a;
    float current_b;
    float current_c;
    float voltage_frequency;
    float power_factor_total;
    uint32_t total_active_power_w;

} jsy_measurements_t;

void jsy_decode_measurements(const uint16_t *raw_registers, jsy_measurements_t *out);
uint32_t jsy_combine_32(uint16_t high_register, uint16_t low_register);

typedef void (*jsy_uart_send_fn)(const uint8_t *data, uint16_t length);
typedef uint16_t (*jsy_uart_receive_fn)(uint8_t *buffer, uint16_t max_length);


typedef struct {
    jsy_uart_send_fn send;
    jsy_uart_receive_fn receive;
    uint8_t slave_address;
} jsy_device_t;

jsy_status_t jsy_read_measurements(jsy_device_t *device, jsy_measurements_t *out_measurements);

jsy_status_t jsy_read_registers(jsy_device_t *device, uint16_t start_reg, uint16_t reg_count, uint16_t *out_registers);

uint16_t jsy_build_write_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, const uint16_t *values, uint8_t *out_frame);

jsy_status_t jsy_parse_write_response(const uint8_t *response, uint16_t response_len, uint8_t expected_addr, uint16_t expected_start_reg, uint16_t expected_reg_count);

jsy_status_t jsy_write_registers(jsy_device_t *device, uint16_t start_reg, uint16_t reg_count, const uint16_t *values);

jsy_status_t jsy_read_model(jsy_device_t *device, uint16_t *out_model);

jsy_status_t jsy_set_alarm_limits(jsy_device_t *device, uint16_t voltage_limit, float current_limit);

uint16_t jsy_pack_addr_baud(uint8_t address, uint8_t format ,uint8_t baud);

jsy_status_t jsy_set_address_baud(jsy_device_t *device, uint8_t new_address, uint8_t format, uint8_t baud);

#endif // JSY_MK333G_H