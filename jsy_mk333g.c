#include <stdint.h>
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

jsy_status_t jsy_parse_response(const uint8_t *response, uint16_t response_len, uint16_t expected_reg_count,
                                uint16_t *out_registers)
{
    /*
    Address(1 byte) | Function Code(1 byte) | Byte Count(1 byte) | Data | CRC(2 byte)
    */
    
    uint16_t expected_len = 5 + (expected_reg_count*2);

    if(response_len != expected_len){
        return JSY_ERR_LENGTH;
    }
    uint16_t calculated_crc = jsy_crc16(response,expected_len-2);
    uint16_t received_crc = response[expected_len-2] | (response[expected_len-1]<<8);

    if(calculated_crc != received_crc){
        return JSY_ERR_CRC;
    }

    if(response[1]!= 0x03){
        return JSY_ERR_FUNCTION_CODE;
    }

    if(response[2] != (expected_reg_count*2)){
        return JSY_ERR_BYTE_COUNT;
    }

    for(int i=0; i< expected_reg_count;i++){
       
        out_registers[i]= (response[3 + i*2]<<8)|response[3 + i*2 + 1]; 
    }
    return JSY_OK;
}

void jsy_decode_measurements(const uint16_t *raw_registers, jsy_measurements_t *out){
out->voltage_a = (float)raw_registers[0]/100.0f;
out->voltage_b = (float)raw_registers[1]/100.0f;
out->voltage_c = (float)raw_registers[2]/100.0f;
out->current_a = (float)raw_registers[3]/100.0f;
out->current_b = (float)raw_registers[4]/100.0f;
out->current_c = (float)raw_registers[5]/100.0f;
}