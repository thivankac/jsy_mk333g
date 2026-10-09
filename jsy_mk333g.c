#include <stdint.h>
#include <stddef.h>
#include "jsy_mk333g.h"

uint16_t jsy_crc16(const uint8_t *data, uint16_t length)
{

    uint16_t crc_register = 0xFFFF; // 1111 1111 1111 1111

    for (uint16_t i = 0; i < length; i++)
    {
        crc_register ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc_register & 0x0001)
            {
                crc_register >>= 1;
                crc_register ^= 0xA001;
            }
            else
            {
                crc_register >>= 1;
            }
        }
    }
    return crc_register;
}

void jsy_build_read_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *out_frame)
{
    out_frame[0] = slave_addr;
    out_frame[1] = 0x03;
    out_frame[2] = start_reg >> 8;
    out_frame[3] = start_reg & 0x00FF;
    out_frame[4] = reg_count >> 8;
    out_frame[5] = reg_count & 0x00FF;

    uint16_t crc = jsy_crc16(out_frame, 6);

    out_frame[6] = crc & 0x00FF; // CRC Low
    out_frame[7] = crc >> 8;     // CRC High
}

jsy_status_t jsy_parse_response(const uint8_t *response, uint16_t response_len,uint8_t expected_addr, uint16_t expected_reg_count,
                                uint16_t *out_registers)
{
    /*
    Address(1 byte) | Function Code(1 byte) | Byte Count(1 byte) | Data | CRC(2 byte)
    */

    uint16_t expected_len = 5 + (expected_reg_count * 2);
    
    if(response_len < 5) // Minimum length check
    {
        return JSY_ERR_LENGTH;
    }

    
    uint16_t calculated_crc = jsy_crc16(response, response_len - 2);
    uint16_t received_crc = response[response_len - 2] | (response[response_len - 1] << 8);

    if (calculated_crc != received_crc)
    {
        return JSY_ERR_CRC;
    }
    
    
    if(response[0] != expected_addr)
    {
        return JSY_ERR_ADDRESS;
    }

    if(response[1] == 0x83)
    {
        uint8_t error_code = response[2];
        switch(error_code)
        {
            case 0x01:
            case 0x81:
                return JSY_ERR_ILLEGAL_FUNCTION;

            case 0x02:
            case 0x82:
                return JSY_ERR_ILLEGAL_ADDRESS;

            case 0x03:
            case 0x83:
                return JSY_ERR_ILLEGAL_VALUE;
                
            default:
                return JSY_ERR_EXCEPTION;
        } 
    }

    if (response[1] != 0x03)
    {
        return JSY_ERR_FUNCTION_CODE;
    }
    
    if (response_len != expected_len)
    {
        return JSY_ERR_LENGTH;
    }

    

    

    if (response[2] != (expected_reg_count * 2))
    {
        return JSY_ERR_BYTE_COUNT;
    }

    for (int i = 0; i < expected_reg_count; i++)
    {

        out_registers[i] = (response[3 + i * 2] << 8) | response[3 + i * 2 + 1];
    }
    return JSY_OK;
}

void jsy_decode_measurements(const uint16_t *raw_registers, jsy_measurements_t *out)
{
    out->voltage_a = (float)raw_registers[JSY_IDX(JSY_REG_VOLTAGE_A)] / 100.0f;
    out->voltage_b = (float)raw_registers[JSY_IDX(JSY_REG_VOLTAGE_B)] / 100.0f;
    out->voltage_c = (float)raw_registers[JSY_IDX(JSY_REG_VOLTAGE_C)] / 100.0f;
    out->current_a = (float)raw_registers[JSY_IDX(JSY_REG_CURRENT_A)] / 100.0f;
    out->current_b = (float)raw_registers[JSY_IDX(JSY_REG_CURRENT_B)] / 100.0f;
    out->current_c = (float)raw_registers[JSY_IDX(JSY_REG_CURRENT_C)] / 100.0f;

    out->voltage_frequency = (float)raw_registers[JSY_IDX(JSY_REG_VOLTAGE_FREQUENCY)] / 100.0f;
    out->power_factor_total = (float)raw_registers[JSY_IDX(JSY_REG_POWER_FACTOR_TOTAL)] / 1000.0f;

    out->total_active_power_w = jsy_combine_32(raw_registers[JSY_IDX(JSY_REG_TOTAL_ACTIVE_POWER_HIGH)],
                                               raw_registers[JSY_IDX(JSY_REG_TOTAL_ACTIVE_POWER_LOW)]);
}

uint32_t jsy_combine_32(uint16_t high_register, uint16_t low_register)
{
    uint32_t combined_reg = ((uint32_t)high_register) << 16 | low_register;
    return combined_reg;
}

jsy_status_t jsy_read_measurements(jsy_device_t *device, jsy_measurements_t *out_measurements)
{
    if(device == NULL || device->send == NULL || device->receive == NULL || out_measurements == NULL)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    uint8_t request_frame[JSY_READ_REQUEST_LEN];
    jsy_build_read_request(device->slave_address, JSY_REG_BASE, JSY_MEASUREMENTS_REG_COUNT, request_frame);
    
    device->send(request_frame, JSY_READ_REQUEST_LEN);

    uint8_t response_frame[JSY_MEASUREMENTS_RESPONSE_LEN];
    uint16_t bytes_received = device->receive(response_frame, sizeof(response_frame));

    if(bytes_received == 0) 
    {
        return JSY_ERR_TIMEOUT;
    }

    uint16_t raw_registers[JSY_MEASUREMENTS_REG_COUNT];
    jsy_status_t status = jsy_parse_response(response_frame, bytes_received, device->slave_address, JSY_MEASUREMENTS_REG_COUNT, raw_registers);

    if (status != JSY_OK)
    {
        return status;
    }

    jsy_decode_measurements(raw_registers, out_measurements);
    return JSY_OK;
}