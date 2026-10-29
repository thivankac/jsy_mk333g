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

static jsy_status_t jsy_map_exception(uint8_t error_code)
{
    switch (error_code)
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


jsy_status_t jsy_parse_response(const uint8_t *response, uint16_t response_len, uint8_t expected_addr, uint16_t expected_reg_count,
                                uint16_t *out_registers)
{
    /*
    Address(1 byte) | Function Code(1 byte) | Byte Count(1 byte) | Data | CRC(2 byte)
    */

    uint16_t expected_len = 5 + (expected_reg_count * 2);

    if (response_len < 5) // Minimum length check
    {
        return JSY_ERR_LENGTH;
    }

    uint16_t calculated_crc = jsy_crc16(response, response_len - 2);
    uint16_t received_crc = response[response_len - 2] | (response[response_len - 1] << 8);

    if (calculated_crc != received_crc)
    {
        return JSY_ERR_CRC;
    }

    if (response[0] != expected_addr)
    {
        return JSY_ERR_ADDRESS;
    }

    if (response[1] == 0x83)
    {
        return jsy_map_exception(response[2]);
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
    if (device == NULL || device->send == NULL || device->receive == NULL || out_measurements == NULL)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    uint8_t request_frame[JSY_READ_REQUEST_LEN];
    jsy_build_read_request(device->slave_address, JSY_REG_BASE, JSY_MEASUREMENTS_REG_COUNT, request_frame);

    device->send(request_frame, JSY_READ_REQUEST_LEN);

    uint8_t response_frame[JSY_MEASUREMENTS_RESPONSE_LEN];
    uint16_t bytes_received = device->receive(response_frame, sizeof(response_frame));

    if (bytes_received == 0)
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

jsy_status_t jsy_read_registers(jsy_device_t *device, uint16_t start_reg, uint16_t reg_count, uint16_t *out_registers)
{
    if (device == NULL || device->send == NULL || device->receive == NULL || out_registers == NULL)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    if (reg_count == 0 || reg_count > JSY_MAX_READ_REGISTERS)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    uint8_t request_frame[JSY_READ_REQUEST_LEN];
    jsy_build_read_request(device->slave_address, start_reg, reg_count, request_frame);

    device->send(request_frame, JSY_READ_REQUEST_LEN);

    uint8_t response_frame[JSY_MAX_RESPONSE_LEN];
    uint16_t expected_len = 5 + (reg_count * 2);
    uint16_t bytes_received = device->receive(response_frame, expected_len);

    if (bytes_received == 0)
    {
        return JSY_ERR_TIMEOUT;
    }

    jsy_status_t status = jsy_parse_response(response_frame, bytes_received, device->slave_address, reg_count, out_registers);

    return status;
}

uint16_t jsy_build_write_request(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, const uint16_t *values, uint8_t *out_frame)
{
    /*
    Address(1 byte) | Function Code(1 byte) | Start Register(2 bytes) | Register Count(2 bytes) | Byte Count(1 byte) | Data | CRC(2 byte)
    */

    if (reg_count == 0 || reg_count > JSY_MAXWRITE_REGISTERS || values == NULL || out_frame == NULL)
    {
        return 0; // Invalid argument
    }

    out_frame[0] = slave_addr;
    out_frame[1] = 0x10;                    // Function code for write multiple registers
    out_frame[2] = (start_reg >> 8) & 0xFF; // Start register high byte
    out_frame[3] = start_reg & 0xFF;        // Start register low byte
    out_frame[4] = (reg_count >> 8) & 0xFF; // Register count high byte
    out_frame[5] = reg_count & 0xFF;        // Register count low byte
    out_frame[6] = reg_count * 2;           // Byte count

    for (uint16_t i = 0; i < reg_count; i++)
    {
        out_frame[7 + i * 2] = (values[i] >> 8) & 0xFF; // Value high byte
        out_frame[8 + i * 2] = values[i] & 0xFF;        // Value low byte
    }

    uint16_t data_end = 7 + reg_count * 2;         // Position after the last data byte
    uint16_t crc = jsy_crc16(out_frame, data_end); // Calculate CRC for the frame excluding the CRC bytes
    out_frame[data_end] = crc & 0xFF;              // CRC low byte
    out_frame[data_end + 1] = (crc >> 8) & 0xFF;   // CRC high byte

    return data_end + 2; // Return the total length of the request frame
}

jsy_status_t jsy_parse_write_response(const uint8_t *response, uint16_t response_len, uint8_t expected_addr, uint16_t expected_start_reg, uint16_t expected_reg_count)
{
   uint16_t expected_len = 8; // Address(1) + Function Code(1) + Start Register(2) + Register Count(2) + CRC(2)

    if (response_len < 5) // Minimum length check
    {
        return JSY_ERR_LENGTH;
    }

    uint16_t calculated_crc = jsy_crc16(response, response_len - 2);
    uint16_t received_crc = response[response_len - 2] | (response[response_len - 1] << 8);

    if (calculated_crc != received_crc)
    {
        return JSY_ERR_CRC;
    }

    if (response[0] != expected_addr)
    {
        return JSY_ERR_ADDRESS;
    }

    if (response[1] == 0x90)
    {
        return jsy_map_exception(response[2]);
    }

   if (response[1] != 0x10)
    {
        return JSY_ERR_FUNCTION_CODE;
    }
   if(response_len != expected_len)
    {
        return JSY_ERR_LENGTH;
    }
    uint16_t start_reg = (response[2] << 8) | response[3];
    uint16_t reg_count = (response[4] << 8) | response[5];

    if (start_reg != expected_start_reg || reg_count != expected_reg_count)
    {
        return JSY_ERR_ECHO_MISMATCH;
    }

    

    return JSY_OK;
}

jsy_status_t jsy_write_registers(jsy_device_t *device, uint16_t start_reg, uint16_t reg_count, const uint16_t *values)
{
    if(device == NULL || device->send == NULL || device->receive == NULL || values == NULL)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    uint8_t request_frame[JSY_WRITE_REQUEST_LEN];
    uint16_t request_len = jsy_build_write_request(device->slave_address, start_reg, reg_count, values, request_frame);

    if(request_len == 0)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    device->send(request_frame, request_len);

    uint8_t response_frame[JSY_WRITE_RESPONSE_LEN];
    uint16_t bytes_received = device->receive(response_frame, sizeof(response_frame));

    if(bytes_received == 0)
    {
        return JSY_ERR_TIMEOUT;
    }

    jsy_status_t status = jsy_parse_write_response(response_frame, bytes_received, device->slave_address, start_reg, reg_count);

    return status;
}

jsy_status_t jsy_read_model(jsy_device_t *device, uint16_t *out_model)
{
    if (out_model == NULL)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    uint16_t model_registers[1];
    jsy_status_t status = jsy_read_registers(device, JSY_REG_MODEL, 1, model_registers);

    if (status != JSY_OK)
    {
        return status;
    }

    *out_model = model_registers[0];
    return JSY_OK;
}

jsy_status_t jsy_set_alarm_limits(jsy_device_t *device, uint16_t voltage_limit, float current_limit)
{
   if(voltage_limit == 0 || !(current_limit >= 0.0f && current_limit <= 6553.0f))
   {
       return JSY_ERR_INVALID_ARGUMENT;
   }

   uint16_t current_reg = (uint16_t)(current_limit * 10.0f + 0.5f); // Round to nearest integer

   uint16_t values[2] ;
   values[0] = voltage_limit;
   values[1] = current_reg;

   return jsy_write_registers(device, JSY_REG_VOLTAGE_LIMIT, 2, values);

}

uint16_t jsy_pack_addr_baud(uint8_t address, uint8_t format ,uint8_t baud)
{
    uint16_t high_byte = (address << 8) & 0xFFFF;
    uint16_t low_byte = ((format & 0x03) << 6) | (baud & 0x0F);
    return (high_byte | low_byte);
}

jsy_status_t jsy_set_address_baud(jsy_device_t *device, uint8_t new_address, uint8_t format, uint8_t baud)
{
    if(new_address == 0 || format > JSY_FORMAT_8N2 || baud < JSY_BAUD_4800 || baud > JSY_BAUD_19200)
    {
        return JSY_ERR_INVALID_ARGUMENT;
    }

    uint16_t packed_value = jsy_pack_addr_baud(new_address, format, baud);

    return jsy_write_registers(device, JSY_REG_ADDR_BAUD, 1, &packed_value);
}