#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <array>

#include "../hal/i2c.hpp"
#include "../hal/nvic.hpp"

extern "C" void I2C0_IRQHandler(void);
extern "C" void I2C1_IRQHandler(void);

/**
 * @brief I2C driver implementation for the MSPM0G family of microcontrollers
 *
 */
class mspm0g_i2c : public lab4::i2c
{
public:
  enum class port : std::uintptr_t
  {
    i2c0 = 0x400F'0000UL,
    i2c1 = 0x400F'2000UL,
  };

  enum class wait_policy : std::uint8_t
  {
    spin_loop,
    low_power
  };

  mspm0g_i2c(port p_port, wait_policy p_wait_policy = wait_policy::spin_loop)
    : m_port(p_port)
    , m_wait_policy(p_wait_policy)
  {
    // Reset peripheral using rstctl
    auto& rstctl_reg = access(registers::rstctl);
    rstctl_reg = 0xB1000001;

    // Enable power with pwren
    auto& pwren_reg = access(registers::pwren);
    pwren_reg = 0x26000001;

    for (int volatile i = 0; i < 32; i++) {
      continue;
    }

    // Set ccr.active (required and not mentioned in user manual)
    auto& ccr_reg = access(registers::ccr);
    {
      auto ccr_val = ccr_reg;
      ccr_val |= 1;  // set active bit
      ccr_reg = ccr_val;
    }

    // Select clock with clksel
    auto& clksel_reg = access(registers::clksel);
    clksel_reg = 1 << 3;

    // Configure clock with clkdiv [and clkctl]
    // NOTE: The datasheet has some sort of type because clkctl
    // doesn't exist!
    auto& clkdiv_reg = access(registers::clkdiv);
    clkdiv_reg = 0 << 0;  // keep clock at 32MHz

    // The divider (TPR) is calculated with the follow on page 1325:
    //     TPR = (I2C_CLK / (I2C_FREQ * (4 + 6))) - 1
    auto& ctpr_reg = access(registers::ctpr);
    // Set clock rate to the default clock rate defined by the i2c::settings
    // object when default constructed.
    ctpr_reg = (bus_clock / lab4::i2c::settings{}.clock_rate) - 1;

    // Enable interrupts and set the static global variable that the interrupts
    // will use to execute our internal interrupt service routine.
    if (m_port == port::i2c0) {
      i2c0_driver = this;
      cortex_m::nvic_enable_irq(i2c0_irq);
    } else {
      i2c1_driver = this;
      cortex_m::nvic_enable_irq(i2c1_irq);
    }

    // set fifo control to its reset value of 0 to ensure that the
    // the i2c triggers on every byte loaded.
    auto& cfifoctl_reg = access(registers::cfifoctl);
    cfifoctl_reg = 0;

    disable_controller_interrupts();
  }

  ~mspm0g_i2c()
  {
    if (m_port == port::i2c0) {
      cortex_m::nvic_disable_irq(i2c0_irq);
      i2c0_driver = nullptr;
    } else {
      cortex_m::nvic_disable_irq(i2c1_irq);
      i2c1_driver = nullptr;
    }

    access(registers::rstctl) = 0xB1000001;
  }

private:
  friend void I2C0_IRQHandler(void);
  friend void I2C1_IRQHandler(void);

  inline static mspm0g_i2c* i2c0_driver = nullptr;
  inline static mspm0g_i2c* i2c1_driver = nullptr;

  // CCTR bit definitions; see TRM p. 1389.
  struct cctr_bits
  {
    constexpr static std::uint32_t transfer_length_bits = 16;
    constexpr static std::uint32_t burst_run_start_transaction = 1 << 0;
    constexpr static std::uint32_t send_start_condition = 1 << 1;
    constexpr static std::uint32_t stop_at_end = 1 << 2;
    constexpr static std::uint32_t ack_next_byte = 1 << 3;
    constexpr static std::uint32_t controller_ack_override_enable = 1 << 4;
    constexpr static std::uint32_t read_on_tx_empty = 1 << 5;
  };

  // CFIFOCTL bit definitions; see TRM p. 1397.
  struct cfifoctl_bits
  {
    constexpr static std::uint32_t tx_fifo_flush = 1 << 7;
    constexpr static std::uint32_t rx_fifo_flush = 1 << 15;
  };

  // CFIFOSR bit definitions; see TRM p. 1398.
  struct cfifosr_bits
  {
    constexpr static std::uint32_t count_mask = 0xFU;
    constexpr static std::uint32_t tx_fifo_count_offset = 8;
    constexpr static std::uint32_t fifo_capacity = 8;

    [[nodiscard]] constexpr static std::uint32_t rx_fifo_count(
      std::uint32_t p_status)
    {
      return p_status & count_mask;
    }

    [[nodiscard]] constexpr static std::uint32_t tx_fifo_available_slots(
      std::uint32_t p_status)
    {
      return (p_status >> tx_fifo_count_offset) & count_mask;
    }
  };

  // CSR bit definitions; see TRM p. 1391.
  struct csr_bits
  {
    constexpr static std::uint32_t error = 1 << 1;
    constexpr static std::uint32_t arbitration_lost = 1 << 4;
  };

  enum class registers : std::uint16_t
  {
    pwren = 0x800,      // TRM p. 1352
    rstctl = 0x804,     // TRM p. 1353
    clkcfg = 0x808,     // TRM p. 1354
    stat = 0x814,       // TRM p. 1355
    clkdiv = 0x1000,    // TRM p. 1356
    clksel = 0x1004,    // TRM p. 1357
    iidx = 0x1020,      // TRM p. 1359
    imask = 0x1028,     // TRM p. 1360
    ris = 0x1030,       // TRM p. 1362
    mis = 0x1038,       // TRM p. 1364
    iclr = 0x1048,      // TRM p. 1368
    csa = 0x1210,       // TRM p. 1388
    cctr = 0x1214,      // TRM p. 1389
    csr = 0x1218,       // TRM p. 1391
    crxdata = 0x121C,   // TRM p. 1392
    ctxdata = 0x1220,   // TRM p. 1393
    ctpr = 0x1224,      // TRM p. 1394
    ccr = 0x1228,       // TRM p. 1395
    cbmon = 0x1234,     // TRM p. 1396
    cfifoctl = 0x1238,  // TRM p. 1397
    cfifosr = 0x123C,   // TRM p. 1398
  };

  // Controller interrupt bit positions; see the IMASK register, TRM p. 1361.
  enum interrupt : std::uint8_t
  {
    crxdone = 0,
    ctxdone = 1,
    crxfifotrg = 2,
    ctxfifotrg = 3,
    crxfifofull = 4,
    ctxempty = 5,
    cnack = 7,
    cstop = 9,
  };

  constexpr static std::uint32_t receive_interrupts =
    (1U << interrupt::crxdone) | (1U << interrupt::crxfifotrg);
  constexpr static std::uint32_t transmit_interrupts =
    (1U << interrupt::ctxdone) | (1U << interrupt::ctxempty);
  constexpr static std::uint32_t controller_interrupts =
    receive_interrupts | transmit_interrupts;

  static constexpr int i2c0_irq = 24;
  static constexpr int i2c1_irq = 25;

  static constexpr std::uint32_t bus_clock = 32'000'000;

  /**
   * @brief Direction of the I2C transaction either WRITE or READ
   *
   */
  enum class direction : std::uint8_t
  {
    write = 0,
    read = 1,
  };

  // CSA bit definitions; see TRM p. 1388.
  struct csa_bits
  {
    constexpr static std::uint32_t direction_offset = 0;
    constexpr static std::uint32_t target_address_offset = 1;
    constexpr static std::uint32_t seven_bit_address_mask = 0x7FU;

    [[nodiscard]] constexpr static std::uint32_t seven_bit_address(
      std::uint8_t p_address)
    {
      return (static_cast<std::uint32_t>(p_address) & seven_bit_address_mask)
             << target_address_offset;
    }
  };

  void send_address(std::uint8_t p_address, direction p_direction)
  {
    auto csa = csa_bits::seven_bit_address(p_address);
    csa |= static_cast<std::uint32_t>(p_direction)
           << csa_bits::direction_offset;
    access(registers::csa) = csa;
    wait();  // wait for the address to be sent over the BUS
  }

  auto access(registers p_offset) -> std::uint32_t volatile&
  {
    auto volatile* addr = reinterpret_cast<std::uint32_t volatile*>(
      static_cast<std::uintptr_t>(m_port) +
      static_cast<std::uintptr_t>(p_offset));
    return *addr;
  }

  void irq_handler()
  {
    auto& mis_reg = access(registers::mis);
    auto& imask = access(registers::imask);
    auto& iclr_reg = access(registers::iclr);

    if ((mis_reg >> interrupt::ctxempty) & 1) {
      auto& ctxdata_reg = access(registers::ctxdata);
      if (m_data_out_iter != m_data_out_end) {
        ctxdata_reg = *m_data_out_iter++;
      }
      iclr_reg = (1 << interrupt::ctxempty);
    }

    if ((mis_reg >> interrupt::ctxdone) & 1) {
      // Disable ctxempty so it doesn't trigger again
      imask &= ~(1 << interrupt::ctxempty);
      iclr_reg = (1 << interrupt::ctxdone);
    }

    if ((mis_reg >> interrupt::crxfifotrg) & 1) {
      auto& crxdata_reg = access(registers::crxdata);
      auto& fifo_status = access(registers::cfifosr);
      while (cfifosr_bits::rx_fifo_count(fifo_status) != 0 and
             m_data_in_iter != m_data_in_end) {
        *m_data_in_iter++ = crxdata_reg;
      }

      iclr_reg = (1 << interrupt::crxfifotrg);
    }

    if ((mis_reg >> interrupt::crxdone) & 1) {
      iclr_reg = (1 << interrupt::crxdone);
    }
  }

  bool driver_configure(settings const& p_settings) override
  {
    auto volatile& ctpr_reg = access(registers::ctpr);
    auto const upscaled_clock_rate = p_settings.clock_rate * 10U;
    if (upscaled_clock_rate > bus_clock) {
      return false;
    }

    // The divider (TPR) is calculated with the follow on page 1325:
    //     TPR = (I2C_CLK / (I2C_FREQ * (4 + 6))) - 1
    auto const divider = (bus_clock / upscaled_clock_rate) - 1U;
    ctpr_reg = divider;

    return true;
  }

  bool driver_transaction(std::uint8_t p_address,
                          std::span<std::uint8_t const> p_data_out,
                          std::span<std::uint8_t> p_data_in) override
  {
    // CCTR.CBLEN is a 12-bit field; see the technical reference manual,
    // section 25.3.33, page 1389.
    constexpr std::size_t maximum_transfer_size = 0xFFF;

    m_data_out_iter = p_data_out.begin();
    m_data_out_end = p_data_out.end();
    m_data_in_iter = p_data_in.begin();
    m_data_in_end = p_data_in.end();

    // Set the START bit in I2Cx.CCTR register to generate the start condition
    auto& cctr_reg = access(registers::cctr);

    if (p_data_out.size() > 0) {
      clear_tx_fifo();

      // Load some data into the ctxdatafifo, THEN enable the ctxempty interrupt
      auto& ctxdata_reg = access(registers::ctxdata);
      auto& fifo_status = access(registers::cfifosr);
      while (cfifosr_bits::tx_fifo_available_slots(fifo_status) != 0 and
             m_data_out_iter != m_data_out_end) {
        ctxdata_reg = *m_data_out_iter++;
      }

      send_address(p_address, direction::write);

      access(registers::imask) |= transmit_interrupts;

      while (not p_data_out.empty()) {
        auto const transfer_size =
          std::clamp<std::size_t>(p_data_out.size(), 0u, maximum_transfer_size);
        std::uint32_t cctr_val =
          cctr_bits::send_start_condition |
          cctr_bits::burst_run_start_transaction |
          (transfer_size << cctr_bits::transfer_length_bits);
        if (transfer_size == p_data_out.size() and p_data_in.empty()) {
          cctr_val |= cctr_bits::stop_at_end;
        }
        cctr_reg = cctr_val;

        wait();  // Wait for ISR to finish sending out the data
        if (transaction_failed()) {
          cctr_reg = 0;
          disable_controller_interrupts();
          return false;
        }

        // Move p_data_out ahead by `size` bytes. If the size of the span was
        // below maximum_transfer_size then this will be an empty span. If it
        // were greater, than the loop will continue.
        p_data_out = p_data_out.subspan(transfer_size);
      }
    }

    // Handle read operation
    if (p_data_in.size() > 0) {
      clear_rx_fifo();
      send_address(p_address, direction::read);
      access(registers::imask) |= receive_interrupts;

      while (not p_data_in.empty()) {
        auto const transfer_size =
          std::clamp<std::size_t>(p_data_in.size(), 0, maximum_transfer_size);
        std::uint32_t cctr_val =
          cctr_bits::burst_run_start_transaction |
          cctr_bits::send_start_condition |
          (transfer_size << cctr_bits::transfer_length_bits);
        if (transfer_size == p_data_in.size()) {
          cctr_val |= cctr_bits::stop_at_end;
        }

        cctr_reg = cctr_val;

        wait();  // wait for ISR to finish reading in data
        if (transaction_failed()) {
          cctr_reg = 0;
          disable_controller_interrupts();
          return false;
        }

        p_data_in = p_data_in.subspan(transfer_size);
      }
    }

    // Disable controller enable (burstrun) and the rest
    cctr_reg = 0;
    disable_controller_interrupts();

    return true;
  }

  void clear_rx_fifo()
  {
    auto& fifo_control = access(registers::cfifoctl);
    fifo_control |= cfifoctl_bits::rx_fifo_flush;

    auto& fifo_status = access(registers::cfifosr);
    while (cfifosr_bits::rx_fifo_count(fifo_status) != 0) {
      continue;
    }

    fifo_control &= ~cfifoctl_bits::rx_fifo_flush;
  }

  [[nodiscard]] bool transaction_failed()
  {
    auto const status = access(registers::csr);
    return (status & (csr_bits::error | csr_bits::arbitration_lost)) != 0;
  }

  void disable_controller_interrupts()
  {
    access(registers::imask) &= ~controller_interrupts;
    access(registers::iclr) = controller_interrupts;
  }

  void clear_tx_fifo()
  {
    auto& fifo_control = access(registers::cfifoctl);
    fifo_control |= cfifoctl_bits::tx_fifo_flush;

    auto& fifo_status = access(registers::cfifosr);
    while (cfifosr_bits::tx_fifo_available_slots(fifo_status) !=
           cfifosr_bits::fifo_capacity) {
      continue;
    }

    fifo_control &= ~cfifoctl_bits::tx_fifo_flush;
  }

  void wait()
  {
    // [[maybe_unused]] is used here so that developers can see each of these
    // registers while debugging as local variables.
    [[maybe_unused]] auto& cctr_reg = access(registers::cctr);
    [[maybe_unused]] auto& fifo_status = access(registers::cfifosr);
    [[maybe_unused]] auto& imask = access(registers::imask);
    constexpr std::size_t busy_bit = 0;
    auto& status = access(registers::csr);
    while ((status >> busy_bit) & 1) {
      execute_wait_policy();
    }
  }

  void wait_bus()
  {
    constexpr std::size_t bus_busy_bit = 6;
    auto& status = access(registers::csr);
    while ((status >> bus_busy_bit) & 1) {
      execute_wait_policy();
    }
  }

  void execute_wait_policy()
  {
    if (m_wait_policy == wait_policy::low_power) {
      asm volatile("wfi");  // enter low power Wait For Interrupt instruction
    }
  }

  port m_port;
  std::span<std::uint8_t const>::iterator m_data_out_iter{};
  std::span<std::uint8_t const>::iterator m_data_out_end{};
  std::span<std::uint8_t>::iterator m_data_in_iter{};
  std::span<std::uint8_t>::iterator m_data_in_end{};
  wait_policy m_wait_policy = wait_policy::spin_loop;
};

// NOTE: Put the code below within your .cpp file without the MACRO part, so it
// gets picked by the compiler Strong overrides for the I2C0 and I2C1 IRQ
// handlers
#ifdef MSPM0G_I2C_IMPLEMENTATION
extern "C"
{
  void I2C0_IRQHandler(void)
  {
    if (mspm0g_i2c::i2c0_driver != nullptr) {
      mspm0g_i2c::i2c0_driver->irq_handler();
    }
  }
  void I2C1_IRQHandler(void)
  {
    if (mspm0g_i2c::i2c1_driver != nullptr) {
      mspm0g_i2c::i2c1_driver->irq_handler();
    }
  }
}
#endif
