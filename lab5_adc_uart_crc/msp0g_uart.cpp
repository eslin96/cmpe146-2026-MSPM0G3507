#include <array>
#include <cstdint>
#include <cstdlib>

#include "msp0g_uart.hpp"

namespace {

constexpr std::array<std::uintptr_t, 4> uart_bases = { 0x4010'8000,
                                                       0x4010'0000,
                                                       0x4010'2000,
                                                       0x4050'0000 };

constexpr std::array<std::uint8_t, 4> uart_irqs = { 15, 13, 14, 3 };

std::array<msp0g::msp0g_uart*, 4> uart_instances{};

constexpr std::uintptr_t pwren_offset = 0x800;
constexpr std::uintptr_t rstctl_offset = 0x804;

constexpr std::uintptr_t clksel_offset = 0x1008;

constexpr std::uintptr_t cpuint_imask_offset = 0x1028;
constexpr std::uintptr_t cpuint_iclr_offset = 0x1048;

constexpr std::uintptr_t ctl0_offset = 0x1100;
constexpr std::uintptr_t lcrh_offset = 0x1104;
constexpr std::uintptr_t stat_offset = 0x1108;
constexpr std::uintptr_t ifls_offset = 0x110C;
constexpr std::uintptr_t ibrd_offset = 0x1110;
constexpr std::uintptr_t fbrd_offset = 0x1114;
constexpr std::uintptr_t txdata_offset = 0x1120;
constexpr std::uintptr_t rxdata_offset = 0x1124;

constexpr std::uintptr_t nvic_base = 0xE000'E100;
constexpr std::uintptr_t nvic_iser_offset = 0x000;
constexpr std::uintptr_t nvic_icpr_offset = 0x180;

constexpr std::uint32_t pwren_mask = 0x2600'0001;
constexpr std::uint32_t pwren_enable_mask = 0x1;
constexpr std::uint32_t rstctl_mask = 0xB100'0001;

constexpr std::uint32_t busclk_mask = (0x1 << 3);

constexpr std::uint32_t enable_mask = 0x1;
constexpr std::uint32_t receive_enable_mask = (0x1 << 3);
constexpr std::uint32_t transmit_enable_mask = (0x1 << 4);
constexpr std::uint32_t fifo_enable_mask = (0x1 << 17);

constexpr std::uint32_t parity_enable_mask = (0x1 << 1);
constexpr std::uint32_t even_parity_mask = (0x1 << 2);
constexpr std::uint32_t two_stop_bits_mask = (0x1 << 3);
constexpr std::uint32_t data_size_8_mask = (0b11 << 4);
constexpr std::uint32_t stick_parity_mask = (0x1 << 6);

constexpr std::uint32_t busy_mask = 0x1;
constexpr std::uint32_t rx_fifo_empty_mask = (0x1 << 2);
constexpr std::uint32_t tx_fifo_full_mask = (0x1 << 7);

constexpr std::uint32_t receive_timeout_interrupt_mask = 0x1;
constexpr std::uint32_t receive_interrupt_mask = (0x1 << 10);

constexpr std::uint32_t receive_fifo_level_mask = (0b111 << 4);
constexpr std::uint32_t receive_fifo_quarter_full = (0x1 << 4);
constexpr std::uint32_t receive_timeout_select_mask = (0xF << 8);
constexpr std::uint32_t receive_timeout_four_bits = (0x4 << 8);

constexpr std::uint32_t uart_clock = 32'000'000;
constexpr std::uint32_t min_baud_rate = 31;
constexpr std::uint32_t max_baud_rate = 2'000'000;

volatile std::uint32_t* reg(std::uintptr_t p_base, std::uintptr_t p_offset)
{

  return reinterpret_cast<volatile std::uint32_t*>(p_base + p_offset);
}

bool check_valid_settings(const lab6::serial::settings& p_settings)
{

  bool valid_stop_bits =
    static_cast<std::uint8_t>(p_settings.stop) <=
    static_cast<std::uint8_t>(lab6::serial::settings::stop_bits::two);

  bool valid_parity =
    static_cast<std::uint8_t>(p_settings.parity) <=
    static_cast<std::uint8_t>(lab6::serial::settings::parity::forced0);

  return (p_settings.baud_rate >= min_baud_rate) &&
         (p_settings.baud_rate <= max_baud_rate) && valid_stop_bits &&
         valid_parity;
}

}  // namespace

namespace msp0g {

msp0g_uart::msp0g_uart(const settings& p_settings, uartx p_uartx)
{

  m_uart_index = static_cast<std::uint8_t>(p_uartx);
  m_uart_base = uart_bases[m_uart_index];

  if (!check_valid_settings(p_settings) || !initialize_uart()) {

    std::abort();
  }

  apply_config(p_settings);

  uart_instances[m_uart_index] = this;
  initialize_receive_interrupt();
}

void msp0g_uart::handle_interrupt()
{

  auto* iclr = reg(m_uart_base, cpuint_iclr_offset);
  auto* stat = reg(m_uart_base, stat_offset);
  auto* rxdata = reg(m_uart_base, rxdata_offset);

  *iclr = receive_timeout_interrupt_mask | receive_interrupt_mask;

  std::size_t cursor = m_receive_cursor;

  while ((*stat & rx_fifo_empty_mask) == 0) {

    m_receive_buffer[cursor] = static_cast<std::uint8_t>(*rxdata);

    cursor++;

    if (cursor == m_receive_buffer.size()) {

      cursor = 0;
    }
  }

  m_receive_cursor = cursor;
}

bool msp0g_uart::initialize_uart()
{

  auto* pwren = reg(m_uart_base, pwren_offset);
  auto* rstctl = reg(m_uart_base, rstctl_offset);

  if ((*pwren & pwren_enable_mask) != 0) {

    return false;
  }

  *rstctl = rstctl_mask;
  *pwren = pwren_mask;

  for (volatile int i = 0; i < 16; i++)
    ;

  return true;
}

void msp0g_uart::initialize_receive_interrupt()
{

  auto* imask = reg(m_uart_base, cpuint_imask_offset);
  auto* iclr = reg(m_uart_base, cpuint_iclr_offset);
  auto* ifls = reg(m_uart_base, ifls_offset);

  *ifls &= ~(receive_fifo_level_mask | receive_timeout_select_mask);
  *ifls |= receive_fifo_quarter_full | receive_timeout_four_bits;

  *iclr = receive_timeout_interrupt_mask | receive_interrupt_mask;
  *imask |= receive_timeout_interrupt_mask | receive_interrupt_mask;

  std::uint32_t interrupt_mask = (0x1 << uart_irqs[m_uart_index]);

  auto* nvic_icpr = reg(nvic_base, nvic_icpr_offset);
  auto* nvic_iser = reg(nvic_base, nvic_iser_offset);

  *nvic_icpr = interrupt_mask;
  *nvic_iser = interrupt_mask;
}

bool msp0g_uart::driver_configure(const settings& p_settings)
{

  if (!check_valid_settings(p_settings)) {

    return false;
  }

  apply_config(p_settings);

  return true;
}

void msp0g_uart::driver_write(std::span<const std::uint8_t> p_data)
{

  auto* stat = reg(m_uart_base, stat_offset);
  auto* txdata = reg(m_uart_base, txdata_offset);

  for (std::size_t i = 0; i < p_data.size(); i++) {

    while ((*stat & tx_fifo_full_mask) != 0)
      ;

    *txdata = p_data[i];
  }

  while ((*stat & busy_mask) != 0)
    ;
}

std::span<const std::uint8_t> msp0g_uart::driver_receive_buffer()
{

  return m_receive_buffer;
}

std::size_t msp0g_uart::driver_receive_cursor()
{

  return m_receive_cursor;
}

void msp0g_uart::apply_config(const settings& p_settings)
{

  auto* clksel = reg(m_uart_base, clksel_offset);
  auto* ctl0 = reg(m_uart_base, ctl0_offset);
  auto* lcrh = reg(m_uart_base, lcrh_offset);
  auto* stat = reg(m_uart_base, stat_offset);
  auto* ibrd = reg(m_uart_base, ibrd_offset);
  auto* fbrd = reg(m_uart_base, fbrd_offset);

  while ((*stat & busy_mask) != 0)
    ;

  *ctl0 &= ~enable_mask;

  *clksel = busclk_mask;

  // BRD = uart_clock / (16 * baud_rate). Multiplying BRD by 64
  // combines the integer divisor and 6-bit fractional divisor.
  // adding (p_settings.baud_rate / 2) results in rounded
  // integer rather than truncated 
  std::uint32_t baud_divisor =
    ((uart_clock * 4) + (p_settings.baud_rate / 2)) /
    p_settings.baud_rate;

  *ibrd = baud_divisor >> 6;
  *fbrd = baud_divisor & 0x3F;

  std::uint32_t line_control = data_size_8_mask;

  if (p_settings.stop == settings::stop_bits::two) {

    line_control |= two_stop_bits_mask;
  }

  switch (p_settings.parity) {
    case settings::parity::none:
      break;

    case settings::parity::odd:
      line_control |= parity_enable_mask;
      break;

    case settings::parity::even:
      line_control |= parity_enable_mask | even_parity_mask;
      break;

    case settings::parity::forced1:
      line_control |= parity_enable_mask | stick_parity_mask;
      break;

    case settings::parity::forced0:
      line_control |=
        parity_enable_mask | even_parity_mask | stick_parity_mask;
      break;
  }

  *lcrh = line_control;

  *ctl0 = receive_enable_mask | transmit_enable_mask | fifo_enable_mask |
          enable_mask;
}

}  // namespace msp0g

extern "C" void UART0_IRQHandler()
{

  if (uart_instances[0] != nullptr) {

    uart_instances[0]->handle_interrupt();
  }
}

extern "C" void UART1_IRQHandler()
{

  if (uart_instances[1] != nullptr) {

    uart_instances[1]->handle_interrupt();
  }
}

extern "C" void UART2_IRQHandler()
{

  if (uart_instances[2] != nullptr) {

    uart_instances[2]->handle_interrupt();
  }
}

extern "C" void UART3_IRQHandler()
{

  if (uart_instances[3] != nullptr) {

    uart_instances[3]->handle_interrupt();
  }
}
