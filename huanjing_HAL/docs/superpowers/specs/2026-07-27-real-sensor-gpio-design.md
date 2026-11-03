# Real Sensor GPIO Design

## Goal

Replace generated environmental sensor values with production HAL drivers for the board's DHT11, BH1750, and MQ-135 interfaces, while retaining a focused `.c/.h` pair for every application-level module.

## Hardware mapping

- DHT11 DATA: PC3, bidirectional GPIO with an external pull-up.
- BH1750: I2C1 on PB6 (SCL) and PB7 (SDA), 100 kHz, address `0x23` by default.
- MQ-135 AO: ADC1 channel 10 on PC0 in analog mode.
- KEY1: PF8 input with pull-up.
- ESP32-C3 EN: PE4 push-pull output; USART2 remains PA2/PA3.

## Architecture

`dht11.c/.h`, `bh1750.c/.h`, and `mq135.c/.h` own their respective device protocols. `adc.c/.h` and `i2c.c/.h` own HAL peripheral initialization and export their handles. `env_sensor.c/.h` only schedules sensor reads and combines successful values into `EnvData_t`; it contains no simulation code.

The application initializes GPIO, ADC1, I2C1, FSMC, and UARTs before calling `EnvSensor_Init`. Sensor read failures preserve the last successful value and are represented by a per-field validity bit in `EnvData_t`, so a disconnected module cannot turn a transient bus error into fabricated data.

## Data handling

- DHT11 supplies temperature and humidity in tenths.
- BH1750 returns whole lux after a one-time continuous high-resolution mode command.
- MQ-135 returns the raw 12-bit ADC code and a documented linear display estimate in ppm. The estimate is explicitly an uncalibrated indication until the sensor's R0 calibration is supplied.

## Testing and limits

Host-side tests will validate the stable public data structure and the absence of simulation code. Static checks will verify that PC0, PC3, PB6, and PB7 are configured and that ADC/I2C HAL modules and source files are included in the Keil project. Functional validation on physical sensors remains outside scope because no hardware is connected.
