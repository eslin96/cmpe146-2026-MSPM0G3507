#include <cstdio>
#include <cstdint>

#include "ti_msp_dl_config.h"

#include "../hal/gpio.hpp"
#include "../hal/spi.hpp"

class mspm0_output_pin : public lab1::output_pin
{
private:
    static constexpr std::uintptr_t gpio_b_base_addr = 0x400A'2000u;
    static constexpr std::uintptr_t iomux_base_addr = 0x4042'8000u;

    static constexpr std::uintptr_t gpio_power_enable_offset = 0x800u;
    static constexpr std::uintptr_t gpio_output_set_offset = 0x1290u;
    static constexpr std::uintptr_t gpio_output_clear_offset = 0x12A0u;
    static constexpr std::uintptr_t gpio_output_enable_offset = 0x12C0u;
    static constexpr std::uintptr_t gpio_input_offset = 0x1380u;
    static constexpr std::uintptr_t PINCM52_offset = 0xD0u;

    static constexpr std::uint32_t pin_mask = 1u << 24;

    bool driver_configure(
        lab1::output_pin::settings const& p_settings) override
    {
        if (p_settings.open_drain ||
            p_settings.resistor != lab1::pin_resistor::none)
        {
            return false;
        }

        auto* gpio_power_enable_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_power_enable_offset
                );

        *gpio_power_enable_register =
            (0x26u << 24) | 1u;

        auto* PINCM52_register =
            reinterpret_cast<volatile std::uint32_t*>(
                iomux_base_addr + PINCM52_offset
                );

        *PINCM52_register =
            (1u << 18) |
            (1u << 7) |
            1u;

        auto* gpio_output_set_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_output_set_offset
                );

        auto* gpio_output_enable_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_output_enable_offset
                );

        *gpio_output_set_register = pin_mask;
        *gpio_output_enable_register = pin_mask;

        return true;
    }

    void driver_level(bool p_high) override
    {
        auto* gpio_output_set_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_output_set_offset
                );

        auto* gpio_output_clear_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_output_clear_offset
                );

        if (p_high)
        {
            *gpio_output_set_register = pin_mask;
        }
        else
        {
            *gpio_output_clear_register = pin_mask;
        }
    }

    bool driver_level() override
    {
        auto* gpio_input_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_input_offset
                );

        return (*gpio_input_register & pin_mask) != 0u;
    }
};

class mspm0_spi : public lab3::spi
{
public:
    bool last_transfer_succeeded() const
    {
        return m_last_transfer_succeeded;
    }

private:
    // Base addrs
    static constexpr std::uintptr_t spi0_base_addr = 0x4046'8000u;
    static constexpr std::uintptr_t iomux_base_addr = 0x4042'8000u;
    static constexpr std::uintptr_t gpio_b_base_addr = 0x400A'2000u;

    // Offsets
    static constexpr std::uintptr_t power_enable_offset = 0x800u;
    static constexpr std::uintptr_t reset_control_offset = 0x804u;
    static constexpr std::uintptr_t reset_status_offset = 0x814u;
    static constexpr std::uintptr_t clock_divider_offset = 0x1000u;
    static constexpr std::uintptr_t clock_select_offset = 0x1004u;

    static constexpr std::uintptr_t spi_control_0_offset = 0x1100u;
    static constexpr std::uintptr_t spi_control_1_offset = 0x1104u;
    static constexpr std::uintptr_t clock_prescaler_and_divider_offset =
        0x1108u;
    static constexpr std::uintptr_t spi_status_offset = 0x1110u;
    static constexpr std::uintptr_t spi_rxdata_offset = 0x1130u;
    static constexpr std::uintptr_t spi_txdata_offset = 0x1140u;
    static constexpr std::uintptr_t gpio_output_clear_offset = 0x12A0u;
    static constexpr std::uintptr_t gpio_output_enable_set_offset = 0x12D0u;

    /*
     * Pin mappings:
     *
     * PB19/MISO -> PINCM45
     * PB17/MOSI -> PINCM43
     * PB18/SCK  -> PINCM44
     */
    static constexpr std::uintptr_t PINCM43_offset = 0xACu;
    static constexpr std::uintptr_t PINCM44_offset = 0xB0u;
    static constexpr std::uintptr_t PINCM45_offset = 0xB4u;

    // offsets for SPI0 FIFO transfer function
    static constexpr std::uint32_t tx_fifo_not_full_mask = 1u << 1;
    static constexpr std::uint32_t rx_fifo_empty_mask = 1u << 2;
    static constexpr std::uint32_t spi_busy_mask = 1u << 4;
    static constexpr std::uint32_t transfer_timeout_limit = 1'000'000u;

    static constexpr std::uint32_t spi_input_clock_rate = 32'000'000u;
    static constexpr std::uint32_t spi_data_size_8 = 7u;
    static constexpr std::uint32_t spi_clock_polarity_mask = 1u << 8;
    static constexpr std::uint32_t spi_clock_phase_mask = 1u << 9;
    static constexpr std::uint32_t spi_controller_mask = 1u << 2;
    static constexpr std::uint32_t spi_msb_first_mask = 1u << 4;
    static constexpr std::uint32_t spi_enable_mask = 1u << 0;
    static constexpr std::uint32_t reset_sticky_mask = 1u << 16;
    static constexpr std::uint32_t spi_sck_pin_mask = 1u << 18;

    bool m_last_transfer_succeeded = false;

    bool driver_configure(
        lab3::spi::settings const& p_settings) override
    {
        if (p_settings.clock_rate == 0u)
        {
            return false;
        }

        std::uint32_t control_value_0 = spi_data_size_8;

        if (p_settings.bus_mode == lab3::spi::mode::m0)
        {
        }
        else if (p_settings.bus_mode == lab3::spi::mode::m1)
        {
            control_value_0 |= spi_clock_phase_mask;
        }
        else if (p_settings.bus_mode == lab3::spi::mode::m2)
        {
            control_value_0 |= spi_clock_polarity_mask;
        }
        else if (p_settings.bus_mode == lab3::spi::mode::m3)
        {
            control_value_0 |=
                spi_clock_polarity_mask |
                spi_clock_phase_mask;
        }
        else
        {
            return false;
        }

        // outputBitRate =
        // spiInputClock / ((1 + SCR) * 2)
        const std::uint64_t denominator =
            2ull *
            static_cast<std::uint64_t>(p_settings.clock_rate);

        const std::uint64_t divider =
            (static_cast<std::uint64_t>(spi_input_clock_rate) +
             denominator -
             1ull) /
            denominator;

        const std::uint64_t scr = divider - 1ull;

        if (scr > 1023ull)
        {
            return false;
        }

        auto* reset_control_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + reset_control_offset
                );

        *reset_control_register =
            (0xB1u << 24) |
            (1u << 1) |
            (1u << 0);

        auto* power_enable_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + power_enable_offset
                );

        *power_enable_register =
            (0x26u << 24) | 1u;

        for (volatile std::uint32_t delay = 0u;
             delay < 16u;
             ++delay)
        {
        }

        auto* reset_status_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + reset_status_offset
                );

        std::uint32_t timeout = 100'000u;

        while (((*reset_status_register & reset_sticky_mask) == 0u) &&
               (timeout != 0u))
        {
            --timeout;
        }

        if (timeout == 0u)
        {
            return false;
        }

        auto* clock_select_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + clock_select_offset
                );

        *clock_select_register = 1u << 3;

        auto* clock_divider_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + clock_divider_offset
                );

        *clock_divider_register = 0u;

        auto* clock_prescaler_and_divider_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr +
                clock_prescaler_and_divider_offset
                );

        *clock_prescaler_and_divider_register =
            static_cast<std::uint32_t>(scr);

        auto* spi_control_0_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + spi_control_0_offset
                );

        auto* spi_control_1_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + spi_control_1_offset
                );

        *spi_control_0_register = control_value_0;

        const std::uint32_t control_value_1 =
            spi_controller_mask |
            spi_msb_first_mask;

        *spi_control_1_register = control_value_1;

        auto* gpio_output_clear_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_output_clear_offset
                );

        auto* gpio_output_enable_set_register =
            reinterpret_cast<volatile std::uint32_t*>(
                gpio_b_base_addr + gpio_output_enable_set_offset
                );

        auto* PINCM44_register =
            reinterpret_cast<volatile std::uint32_t*>(
                iomux_base_addr + PINCM44_offset
                );

        *spi_control_1_register &= ~spi_enable_mask;

        *PINCM44_register =
            (1u << 7) |
            1u;

        *gpio_output_clear_register = spi_sck_pin_mask;
        *gpio_output_enable_set_register = spi_sck_pin_mask;

        auto* PINCM43_register =
            reinterpret_cast<volatile std::uint32_t*>(
                iomux_base_addr + PINCM43_offset
                );

        *PINCM43_register =
            (1u << 7) |
            3u;

        *PINCM44_register =
            (1u << 18) |
            (1u << 7) |
            3u;

        auto* PINCM45_register =
            reinterpret_cast<volatile std::uint32_t*>(
                iomux_base_addr + PINCM45_offset
                );

        *PINCM45_register =
            (1u << 18) |
            (1u << 7) |
            3u;

        *spi_control_1_register |= spi_enable_mask;

        return true;
    }

    void driver_transfer(
        std::span<std::uint8_t const> p_data_out,
        std::span<std::uint8_t> p_data_in,
        std::uint8_t p_filler) override
    {
        m_last_transfer_succeeded = true;

        auto* tx_data_register =
            reinterpret_cast<volatile std::uint8_t*>(
                spi0_base_addr + spi_txdata_offset
                );

        auto* rx_data_register =
            reinterpret_cast<volatile std::uint8_t*>(
                spi0_base_addr + spi_rxdata_offset
                );

        auto* status_register =
            reinterpret_cast<volatile std::uint32_t*>(
                spi0_base_addr + spi_status_offset
                );

        std::uint32_t timeout = transfer_timeout_limit;

        while (((*status_register & rx_fifo_empty_mask) == 0u) &&
               (timeout != 0u))
        {
            static_cast<void>(*rx_data_register);
            --timeout;
        }

        if (timeout == 0u)
        {
            m_last_transfer_succeeded = false;
            return;
        }

        const std::uint32_t output_length =
            static_cast<std::uint32_t>(p_data_out.size());

        const std::uint32_t input_length =
            static_cast<std::uint32_t>(p_data_in.size());

        std::uint32_t transfer_length = output_length;

        if (input_length > transfer_length)
        {
            transfer_length = input_length;
        }

        for (std::uint32_t index = 0u;
             index < transfer_length;
             ++index)
        {
            timeout = transfer_timeout_limit;

            while (((*status_register & tx_fifo_not_full_mask) == 0u) &&
                   (timeout != 0u))
            {
                --timeout;
            }

            if (timeout == 0u)
            {
                m_last_transfer_succeeded = false;
                return;
            }

            std::uint8_t transmit_value = p_filler;

            if (index < output_length)
            {
                transmit_value = p_data_out[index];
            }

            *tx_data_register = transmit_value;

            timeout = transfer_timeout_limit;

            while (((*status_register & rx_fifo_empty_mask) != 0u) &&
                   (timeout != 0u))
            {
                --timeout;
            }

            if (timeout == 0u)
            {
                m_last_transfer_succeeded = false;
                return;
            }

            const std::uint8_t received_value =
                *rx_data_register;

            if (index < input_length)
            {
                p_data_in[index] = received_value;
            }
        }

        timeout = transfer_timeout_limit;

        while (((*status_register & spi_busy_mask) != 0u) &&
               (timeout != 0u))
        {
            --timeout;
        }

        if (timeout == 0u)
        {
            m_last_transfer_succeeded = false;
        }
    }
};

class flash_memory
{
public:
    flash_memory(
        lab3::spi* p_spi,
        lab1::output_pin* p_chip_select)
        : m_spi(p_spi)
        , m_chip_select(p_chip_select)
    {
    }

    bool configure()
    {
        if (m_chip_select == nullptr)
        {
            return false;
        }

        lab1::output_pin::settings pin_settings;

        if (!m_chip_select->configure(pin_settings))
        {
            return false;
        }

        m_chip_select->level(true);
        return true;
    }

    bool read_jedec_id(
        std::uint8_t& p_manufacturer_id,
        std::uint8_t& p_memory_type,
        std::uint8_t& p_capacity_code)
    {
        if (!is_ready())
        {
            return false;
        }

        std::uint8_t command[4] = {
            command_read_jedec_id,
            0u,
            0u,
            0u
        };

        std::uint8_t response[4] = {};

        select();
        m_spi->transfer(
            std::span<std::uint8_t const>(command, 4u),
            std::span<std::uint8_t>(response, 4u)
            );
        deselect();

        p_manufacturer_id = response[1];
        p_memory_type = response[2];
        p_capacity_code = response[3];

        return true;
    }

    static bool capacity_from_code(
        std::uint8_t p_capacity_code,
        std::uint32_t& p_capacity_bytes)
    {
        if (p_capacity_code >= 32u)
        {
            return false;
        }

        p_capacity_bytes = 1u << p_capacity_code;
        return true;
    }

    bool read_first_block(std::span<std::uint8_t> p_data)
    {
        return read_bytes(0u, p_data);
    }

    bool write_first_block(std::span<std::uint8_t const> p_data)
    {
        if (p_data.size() > first_block_size)
        {
            return false;
        }

        if (p_data.size() == 0u)
        {
            return true;
        }

        if (!erase_sector(0u))
        {
            return false;
        }

        return page_program(0u, p_data);
    }

private:
    static constexpr std::uint8_t command_write_enable = 0x06u;
    static constexpr std::uint8_t command_read_status = 0x05u;
    static constexpr std::uint8_t command_read_data = 0x03u;
    static constexpr std::uint8_t command_page_program = 0x02u;
    static constexpr std::uint8_t command_sector_erase_4k = 0x20u;
    static constexpr std::uint8_t command_read_jedec_id = 0x9Fu;

    static constexpr std::uint8_t status_write_enable_latch_mask = 1u << 1;
    static constexpr std::uint8_t status_write_in_progress_mask = 1u << 0;

    static constexpr std::uint32_t first_block_size = 32u;
    static constexpr std::uint32_t max_wait_iterations = 100'000u;
    static constexpr std::uint32_t chip_select_delay_iterations = 32u;

    bool is_ready() const
    {
        return (m_spi != nullptr) &&
               (m_chip_select != nullptr);
    }

    void select()
    {
        m_chip_select->level(true);

        for (volatile std::uint32_t delay = 0u;
             delay < chip_select_delay_iterations;
             ++delay)
        {
        }

        m_chip_select->level(false);

        for (volatile std::uint32_t delay = 0u;
             delay < chip_select_delay_iterations;
             ++delay)
        {
        }
    }

    void deselect()
    {
        m_chip_select->level(true);

        for (volatile std::uint32_t delay = 0u;
             delay < chip_select_delay_iterations;
             ++delay)
        {
        }
    }

    bool read_status(std::uint8_t& p_status)
    {
        if (!is_ready())
        {
            return false;
        }

        std::uint8_t command[2] = {
            command_read_status,
            0u
        };

        std::uint8_t response[2] = {};

        select();
        m_spi->transfer(
            std::span<std::uint8_t const>(command, 2u),
            std::span<std::uint8_t>(response, 2u)
            );
        deselect();

        p_status = response[1];
        return true;
    }

    bool wait_until_ready()
    {
        for (std::uint32_t attempt = 0u;
             attempt < max_wait_iterations;
             ++attempt)
        {
            std::uint8_t status = 0u;

            if (!read_status(status))
            {
                return false;
            }

            if ((status & status_write_in_progress_mask) == 0u)
            {
                return true;
            }
        }

        return false;
    }

    bool write_enable()
    {
        if (!is_ready())
        {
            return false;
        }

        std::uint8_t command = command_write_enable;

        select();
        m_spi->transfer(
            std::span<std::uint8_t const>(&command, 1u),
            std::span<std::uint8_t>(nullptr, 0u)
            );
        deselect();

        std::uint8_t status = 0u;

        if (!read_status(status))
        {
            return false;
        }

        return (status & status_write_enable_latch_mask) != 0u;
    }

    bool erase_sector(std::uint32_t p_address)
    {
        if (!write_enable())
        {
            return false;
        }

        std::uint8_t command[4] = {
            command_sector_erase_4k,
            static_cast<std::uint8_t>(p_address >> 16),
            static_cast<std::uint8_t>(p_address >> 8),
            static_cast<std::uint8_t>(p_address)
        };

        select();
        m_spi->transfer(
            std::span<std::uint8_t const>(command, 4u),
            std::span<std::uint8_t>(nullptr, 0u)
            );
        deselect();

        return wait_until_ready();
    }

    bool page_program(
        std::uint32_t p_address,
        std::span<std::uint8_t const> p_data)
    {
        if (!is_ready() || p_data.size() > first_block_size)
        {
            return false;
        }

        if (!write_enable())
        {
            return false;
        }

        const std::uint32_t data_length =
            static_cast<std::uint32_t>(p_data.size());

        std::uint8_t command_and_data[4u + first_block_size] = {};

        command_and_data[0] = command_page_program;
        command_and_data[1] =
            static_cast<std::uint8_t>(p_address >> 16);
        command_and_data[2] =
            static_cast<std::uint8_t>(p_address >> 8);
        command_and_data[3] =
            static_cast<std::uint8_t>(p_address);

        for (std::uint32_t index = 0u;
             index < data_length;
             ++index)
        {
            command_and_data[4u + index] = p_data[index];
        }

        select();
        m_spi->transfer(
            std::span<std::uint8_t const>(
                command_and_data,
                4u + data_length
                ),
            std::span<std::uint8_t>(nullptr, 0u)
            );
        deselect();

        return wait_until_ready();
    }

    bool read_bytes(
        std::uint32_t p_address,
        std::span<std::uint8_t> p_data)
    {
        if (!is_ready() || p_data.size() > first_block_size)
        {
            return false;
        }

        if (p_data.size() == 0u)
        {
            return true;
        }

        const std::uint32_t data_length =
            static_cast<std::uint32_t>(p_data.size());

        std::uint8_t command[4] = {
            command_read_data,
            static_cast<std::uint8_t>(p_address >> 16),
            static_cast<std::uint8_t>(p_address >> 8),
            static_cast<std::uint8_t>(p_address)
        };

        std::uint8_t response[4u + first_block_size] = {};

        select();
        m_spi->transfer(
            std::span<std::uint8_t const>(command, 4u),
            std::span<std::uint8_t>(response, 4u + data_length)
            );
        deselect();

        for (std::uint32_t index = 0u;
             index < data_length;
             ++index)
        {
            p_data[index] = response[4u + index];
        }

        return true;
    }

    lab3::spi* m_spi = nullptr;
    lab1::output_pin* m_chip_select = nullptr;
};

static void print_block(std::span<std::uint8_t const> p_data)
{
    for (std::uint32_t index = 0u;
         index < p_data.size();
         ++index)
    {
        std::printf(
            "%02X ",
            static_cast<unsigned int>(p_data[index])
            );
    }

    std::printf("\n");
}

int main()
{
    SYSCFG_DL_init();

    mspm0_spi spi0;

    mspm0_output_pin chip_select;
    flash_memory flash(&spi0, &chip_select);

    if (!flash.configure())
    {
        std::printf("Flash chip-select configuration failed\n");

        while (true)
        {
        }
    }

    lab3::spi::settings spi_settings;
    spi_settings.clock_rate = 1'000'000u;
    spi_settings.bus_mode = lab3::spi::mode::m0;

    if (!spi0.configure(spi_settings))
    {
        std::printf("SPI configuration failed\n");

        while (true)
        {
        }
    }

    std::uint8_t manufacturer_id = 0u;
    std::uint8_t memory_type = 0u;
    std::uint8_t capacity_code = 0u;

    if (!flash.read_jedec_id(
            manufacturer_id,
            memory_type,
            capacity_code
            ) ||
        !spi0.last_transfer_succeeded())
    {
        std::printf("JEDEC ID read failed\n");

        while (true)
        {
        }
    }

    std::printf(
        "JEDEC ID: %02X %02X %02X\n",
        static_cast<unsigned int>(manufacturer_id),
        static_cast<unsigned int>(memory_type),
        static_cast<unsigned int>(capacity_code)
        );

    std::uint32_t capacity_bytes = 0u;

    if (!flash_memory::capacity_from_code(
            capacity_code,
            capacity_bytes
            ))
    {
        std::printf("Unknown flash capacity code\n");

        while (true)
        {
        }
    }

    std::printf(
        "Capacity: %u bytes (%u MiB)\n",
        static_cast<unsigned int>(capacity_bytes),
        static_cast<unsigned int>(
            capacity_bytes / (1024u * 1024u)
            )
        );

    std::uint8_t first_block[32] = {};

    if (!flash.read_first_block(
            std::span<std::uint8_t>(first_block, 32u)
            ) ||
        !spi0.last_transfer_succeeded())
    {
        std::printf("Initial 32-byte read failed\n");

        while (true)
        {
        }
    }

    std::printf("First 32 bytes before write:\n");
    print_block(
        std::span<std::uint8_t const>(first_block, 32u)
        );

    std::printf(
        "Enter up to 32 ASCII characters and press Enter:\n"
        );
    std::fflush(stdout);

    char input_line[33] = {};

    if (std::fgets(input_line, sizeof(input_line), stdin) == nullptr)
    {
        std::printf("No input received\n");

        while (true)
        {
        }
    }

    std::uint8_t input_data[32] = {};
    std::uint32_t input_length = 0u;
    bool line_ended = false;

    for (std::uint32_t index = 0u;
         index < 32u && input_line[index] != '\0';
         ++index)
    {
        if (input_line[index] == '\n' ||
            input_line[index] == '\r')
        {
            line_ended = true;
            break;
        }

        input_data[input_length] =
            static_cast<std::uint8_t>(input_line[index]);
        ++input_length;
    }

    if (!line_ended)
    {
        int character = 0;

        do
        {
            character = std::getchar();
        }
        while (character != '\n' && character != EOF);
    }

    if (input_length == 0u)
    {
        while (true)
        {
        }
    }

    if (!flash.write_first_block(
            std::span<std::uint8_t const>(input_data, input_length)
            ) ||
        !spi0.last_transfer_succeeded())
    {
        std::printf("Flash write failed\n");

        while (true)
        {
        }
    }

    std::uint8_t read_back[32] = {};

    if (!flash.read_first_block(
            std::span<std::uint8_t>(read_back, 32u)
            ) ||
        !spi0.last_transfer_succeeded())
    {
        std::printf("Read-back failed\n");

        while (true)
        {
        }
    }

    std::printf("First 32 bytes after write:\n");
    print_block(
        std::span<std::uint8_t const>(read_back, 32u)
        );

    bool data_matches = true;

    for (std::uint32_t index = 0u;
         index < input_length;
         ++index)
    {
        if (read_back[index] != input_data[index])
        {
            data_matches = false;
        }
    }

    std::printf(
        "Read-back result: %s\n",
        data_matches ? "MATCH" : "MISMATCH"
        );

    while (true)
    {
    }
}