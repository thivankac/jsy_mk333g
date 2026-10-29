#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "jsy_mk333g.h"


static uint8_t meter_reply[128];
static uint16_t meter_reply_len = 0;
static uint8_t sent_bytes[64];
static uint16_t sent_bytes_len = 0;

static int failures = 0;

void check_number(const char *name, uint32_t got, uint32_t want)
{
    if (got == want)
    {
        printf("PASS: %s (got %u)\n", name, got);
    }
    else
    {
        printf("FAIL: %s - got %u, want %u\n", name, got, want);
        failures++;
    }
}

void check_float(const char *name, float got, float want)
{
    if (fabsf(got - want) < 0.01f)
    {
        printf("PASS: %s (got %.2f)\n", name, got);
    }
    else
    {
        printf("FAIL: %s - got %.2f, want %.2f\n", name, got, want);
        failures++;
    }
}
void fake_uart_send(const uint8_t *data, uint16_t length)
 {
    memcpy(sent_bytes, data, length);
    sent_bytes_len = length;
 }

uint16_t fake_uart_receive(uint8_t *buffer, uint16_t max_length)
 {
    uint16_t len = meter_reply_len;
    if(len > max_length)
    {
        len = max_length;
    }
    memcpy(buffer, meter_reply, len);
    return len;
 }




int main(void)
{

jsy_device_t meter1 = {0};
meter1.send = fake_uart_send;
meter1.receive = fake_uart_receive;
meter1.slave_address = 1;

const uint8_t good_reply[] = { 
    // Simulate receiving a response from the device (26 registers = 52 data bytes, 57 bytes total)
         0x01, 0x03, 0x34, // addr, func, byte count (52 bytes = 26 registers)
         // Reg 0..2 (0x0100..0x0102): Voltages A, B, C (220.33V, 220.50V, 220.67V)
         0x56, 0x11,
         0x56, 0x22,
         0x56, 0x33,
         // Reg 3..5 (0x0103..0x0105): Currents A, B, C (0.00A, 0.00A, 0.00A)
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         // Reg 6..8 (0x0106..0x0108): intermediate / phase power registers
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         // Reg 9..10 (0x0109..0x010A): Total active power high & low (0x00001234 = 4660 W)
         0x00, 0x00,
         0x12, 0x34,
         // Reg 11..20 (0x010B..0x0114): intermediate registers (10 registers = 20 bytes)
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         // Reg 21 (0x0115): Voltage frequency (50.00 Hz = 5000 = 0x1388)
         0x13, 0x88,
         // Reg 22..24 (0x0116..0x0118): phase power factors (3 registers = 6 bytes)
         0x00, 0x00,
         0x00, 0x00,
         0x00, 0x00,
         // Reg 25 (0x0119): Total power factor 
         0x03, 0xE8, // 1000 = 1.000
         // CRC16: Low byte, High byte
         0xFA, 0x24

};

memcpy(meter_reply, good_reply, sizeof(good_reply));
meter_reply_len = sizeof(good_reply);

jsy_measurements_t results;
jsy_status_t read_status = jsy_read_measurements(&meter1, &results);

if(read_status == JSY_OK)
{
    check_float("Voltage A", results.voltage_a, 220.33f);
    check_float("Voltage B", results.voltage_b, 220.50f);
    check_float("Voltage C", results.voltage_c, 220.67f);
    check_float("Current A", results.current_a, 0.00f);
    check_float("Current B", results.current_b, 0.00f);
    check_float("Current C", results.current_c, 0.00f);
    check_float("Frequency", results.voltage_frequency, 50.00f);
    check_float("Power Factor Total", results.power_factor_total, 1.00f);
    check_number("Total Active Power", results.total_active_power_w, 4660);
}
else
{
    printf("Failed to read measurements, status code: %d\n", read_status);
}

printf("%s (%d failures)\n", failures == 0 ? "All tests passed" : "Some tests failed", failures);
return 0;

}