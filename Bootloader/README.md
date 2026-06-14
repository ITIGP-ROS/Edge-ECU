# STM32 Custom Bootloader

## Some Useful Commands

```sh
sudo apt install stlink-tools

# Read Flash memory: <output_file> <start_address> <length>
st-flash read app_dump.bin 0x08008000 10000
# Convert binary file to hex file: xxd <input_file> > <output_file>
xxd app_dump.bin > flash.hex
# Write Flash memory: <input_file> <start_address>
st-flash write firmware.bin 0x08008000
```