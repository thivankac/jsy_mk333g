#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "jsy_mk333g.h"


void fake_uart_send(const uint8_t *data, uint16_t length)
 {
     printf("Sending %d bytes: ", length);
     for (int i = 0; i < length; i++)
     {
         printf("%02X ", data[i]);
     }
     printf("\n");
 }

uint16_t fake_uart_receive(uint8_t *buffer, uint16_t max_length)
 {
     // Simulate receiving a response from the device (26 registers = 52 data bytes, 57 bytes total)
     uint8_t simulated_response[] = {
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
         // Reg 25 (0x0119): Total power factor (1.00 = 100 = 0x0064)
         0x00, 0x64,
         // CRC16: Low byte, High byte
         0xFB, 0x71
  
     };
     uint16_t len = sizeof(simulated_response);
     if(len > max_length)
     {
         len = max_length;
     }
     memcpy(buffer, simulated_response, len);
     return len;
 }




int main(void)
{


jsy_device_t meter1;
meter1.send = fake_uart_send;
meter1.receive = fake_uart_receive;
meter1.slave_address = 1;

jsy_measurements_t results;
jsy_status_t read_status = jsy_read_measurements(&meter1, &results);

if(read_status == JSY_OK)
{
    printf("Voltage A: %.2f V\n", results.voltage_a);
    printf("Voltage B: %.2f V\n", results.voltage_b);
    printf("Voltage C: %.2f V\n", results.voltage_c);
    printf("Current A: %.2f A\n", results.current_a);
    printf("Current B: %.2f A\n", results.current_b);
    printf("Current C: %.2f A\n", results.current_c);
    printf("Frequency: %.2f Hz\n", results.voltage_frequency);
    printf("Power Factor Total: %.2f\n", results.power_factor_total);
    printf("Total Active Power (W): %u W\n", results.total_active_power_w);
}
else
{
    printf("Failed to read measurements, status code: %d\n", read_status);
}

return 0;

}