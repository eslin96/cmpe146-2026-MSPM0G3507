
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "../hal/accelerometer.hpp"
#include "../hal/mspm0g_i2c.hpp"

// Function declarations.
void configure_i2c1_pins();
void delay_ms(std::uint32_t milliseconds);


// Simple delay using the SysTick hardware timer.
// Assumes a 32 MHz processor clock.
void delay_ms(std::uint32_t milliseconds)
{
    constexpr std::uintptr_t systick_base = 0xE000E010UL;

    auto* control =
        reinterpret_cast<volatile std::uint32_t*>(
            systick_base);

    auto* reload =
        reinterpret_cast<volatile std::uint32_t*>(
            systick_base + 0x04);

    auto* current =
        reinterpret_cast<volatile std::uint32_t*>(
            systick_base + 0x08);

    for (std::uint32_t i = 0; i < milliseconds; ++i) {

        *control = 0;
        *reload = 32'000 - 1;
        *current = 0;

        // Start SysTick using the processor clock.
        *control = (1U << 2) | 1U;

        // Wait for one millisecond.
        while ((*control & (1U << 16)) == 0) {
        }
    }

    *control = 0;
}


// BMI160 accelerometer class.
class bmi160 : public lab4::accelerometer
{
public:

    // Constructor: save the I2C bus and sensor address.
    explicit bmi160(lab4::i2c& bus,
                    std::uint8_t address = 0x68)
        : m_bus(bus), m_address(address)
    {
    }

    // Identify and configure the accelerometer.
    bool initialize()
    {
        std::array<std::uint8_t, 1> chip_id{};

        // Register 0x00 contains the BMI160 chip ID.
        if (!read_registers(0x00, chip_id)) {
            std::printf("Failed to read chip ID.\n");
            return false;
        }

        if (chip_id[0] != 0xD1) {
            std::printf("Wrong chip ID: 0x%02X\n",
                        static_cast<unsigned>(chip_id[0]));
            return false;
        }

        // Wake up the accelerometer.
        if (!write_register(0x7E, 0x11)) {
            return false;
        }

        delay_ms(10);

        // Check the accelerometer power mode.
        std::array<std::uint8_t, 1> power_status{};

        if (!read_registers(0x03, power_status)) {
            return false;
        }

        // Bits 5:4 should equal 01 for normal mode.
        if (((power_status[0] >> 4) & 0x03) != 1) {
            std::printf("Accelerometer not in normal mode.\n");
            return false;
        }

        // Set the acceleration range to +/-2 g.
        if (!write_register(0x41, 0x03)) {
            return false;
        }

        delay_ms(20);

        // Display the configuration once.
        std::printf("\n========== BMI160 Accelerometer ==========\n");
        std::printf("I2C Port:       I2C1\n");
        std::printf("I2C Address:    0x%02X\n",
                    static_cast<unsigned>(m_address));
        std::printf("Chip ID:        0x%02X\n",
                    static_cast<unsigned>(chip_id[0]));
        std::printf("Power Mode:     Normal (0x%02X)\n",
                    static_cast<unsigned>(power_status[0]));
        std::printf("Accel Range:    +/-2 g\n");
        std::printf("I2C Speed:      100 kHz\n");
        std::printf("BMI160 initialized successfully!\n");
        std::printf("==========================================\n\n");

        return true;
    }


    // Read acceleration and display roll and pitch.
    void print_orientation()
    {
        auto acceleration = read();

        // Retry once if all three values are zero.
        if (acceleration.x == 0 &&
            acceleration.y == 0 &&
            acceleration.z == 0) {

            delay_ms(1);
            acceleration = read();
        }

        // Check for invalid measurements.
        if (!std::isfinite(acceleration.x) ||
            !std::isfinite(acceleration.y) ||
            !std::isfinite(acceleration.z)) {

            std::printf("Acceleration read failed.\n");
            return;
        }

        if (acceleration.x == 0 &&
            acceleration.y == 0 &&
            acceleration.z == 0) {

            std::printf("Invalid acceleration data.\n");
            return;
        }

        float x = acceleration.x;
        float y = acceleration.y;
        float z = acceleration.z;

        constexpr float degrees = 57.2957795f;

        // Calculate roll and pitch.
        float roll = std::atan2(y, z) * degrees;

        float pitch =
            std::atan2(-x, std::sqrt(y * y + z * z)) * degrees;

        // Display acceleration.
        std::printf("X: %+.3f g | Y: %+.3f g | Z: %+.3f g\n",
                    static_cast<double>(x),
                    static_cast<double>(y),
                    static_cast<double>(z));

        // Display orientation.
        std::printf("Roll: %d deg | Pitch: %d deg\n\n",
                    static_cast<int>(std::round(roll)),
                    static_cast<int>(std::round(pitch)));
    }


private:

    // Variables saved by the constructor.
    lab4::i2c& m_bus;
    std::uint8_t m_address;


    // Read data from one or more BMI160 registers.
    bool read_registers(std::uint8_t reg,
                        std::span<std::uint8_t> data)
    {
        // Tell the sensor which register we want.
        std::array<std::uint8_t, 1> request{reg};

        // Ask the professor's driver to read the data.
        if (!m_bus.transaction(m_address, request, data)) {
            return false;
        }

        // Temporary timing workaround from our tests.
        delay_ms(2);

        // I2C1 hardware register addresses.
        constexpr std::uintptr_t i2c1_base = 0x400F'2000UL;

        auto* fifo_status =
            reinterpret_cast<volatile std::uint32_t*>(
                i2c1_base + 0x123C);

        auto* rx_data =
            reinterpret_cast<volatile std::uint32_t*>(
                i2c1_base + 0x121C);

        // Count unread bytes in the receive FIFO.
        std::size_t remaining = *fifo_status & 0x0F;

        if (remaining > data.size()) {
            return false;
        }

        // Copy any remaining bytes into the data array.
        for (std::size_t i = data.size() - remaining;
             i < data.size(); ++i) {

            data[i] = static_cast<std::uint8_t>(*rx_data);
        }

        return true;
    }


    // Write a value to a BMI160 register.
    bool write_register(std::uint8_t reg,
                        std::uint8_t value)
    {
        std::array<std::uint8_t, 2> message{reg, value};

        return m_bus.transaction(m_address, message, {});
    }


    // Convert two raw bytes into acceleration in g.
    static float convert_to_g(std::uint8_t low,
                              std::uint8_t high)
    {
        int value = (static_cast<int>(high) << 8) | low;

        // Convert the measurement to a signed value.
        if (value >= 32768) {
            value -= 65536;
        }

        // At +/-2 g, 16384 counts represents 1 g.
        return static_cast<float>(value) / 16384.0f;
    }


    // Read X, Y, and Z acceleration.
    read_t driver_read() override
    {
        // Two bytes per axis, six bytes total.
        std::array<std::uint8_t, 6> data{};

        if (!read_registers(0x12, data)) {

            float invalid =
                std::numeric_limits<float>::quiet_NaN();

            return {invalid, invalid, invalid};
        }

        read_t acceleration;

        acceleration.x = convert_to_g(data[0], data[1]);
        acceleration.y = convert_to_g(data[2], data[3]);
        acceleration.z = convert_to_g(data[4], data[5]);

        return acceleration;
    }
};


int main()
{
    // Configure PB2 and PB3 for I2C1.
    configure_i2c1_pins();

    // Create the professor's I2C driver.
    mspm0g_i2c bus(mspm0g_i2c::port::i2c1);

    // Enable interrupts.
    asm volatile("cpsie i");

    // Set I2C speed to 100 kHz.
    lab4::i2c::settings settings;
    settings.clock_rate = 100'000;

    if (!bus.configure(settings)) {
        std::printf("I2C configuration failed.\n");
        return 1;
    }

    // Create the BMI160 using its constructor.
    bmi160 sensor(bus);

    delay_ms(50);

    // Initialize the accelerometer.
    if (!sensor.initialize()) {
        std::printf("BMI160 initialization failed.\n");
        return 1;
    }

    // Continuously read the sensor.
    while (true) {
        sensor.print_orientation();
        delay_ms(200);
    }
}


// Interrupt handlers required by the professor's I2C driver.
extern "C" void I2C0_IRQHandler(void)
{
    if (mspm0g_i2c::i2c0_driver != nullptr) {
        mspm0g_i2c::i2c0_driver->irq_handler();
    }
}

extern "C" void I2C1_IRQHandler(void)
{
    if (mspm0g_i2c::i2c1_driver != nullptr) {
        mspm0g_i2c::i2c1_driver->irq_handler();
    }
}


// Configure PB2 and PB3 using the Lab 1 register-access style.
void configure_i2c1_pins()
{
    constexpr std::uintptr_t io_mux_addr = 0x4042'8000UL;

    std::uint32_t volatile* io_mux_array =
        reinterpret_cast<std::uint32_t volatile*>(io_mux_addr);

    // PINCM bit positions from the technical reference manual.
    constexpr std::uint32_t periph_conn_bit = 7;
    constexpr std::uint32_t pull_up_bit = 17;
    constexpr std::uint32_t input_enable_bit = 18;
    constexpr std::uint32_t open_drain_bit = 25;

    // Peripheral function 4 connects PB2/PB3 to I2C1.
    constexpr std::uint32_t i2c_function = 4;

    // Both pins need the same configuration.
    std::uint32_t pin_config =
        (1U << periph_conn_bit) |
        (1U << pull_up_bit) |
        (1U << input_enable_bit) |
        (1U << open_drain_bit) |
        i2c_function;

    // PB2 = PINCM15 = I2C1 SCL.
    io_mux_array[15] = pin_config;

    // PB3 = PINCM16 = I2C1 SDA.
    io_mux_array[16] = pin_config;
}
