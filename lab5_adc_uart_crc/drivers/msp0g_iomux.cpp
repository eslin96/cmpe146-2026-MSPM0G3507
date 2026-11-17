#include <cstdint>
#include <cstdlib>

#include "msp0g_iomux.hpp"

namespace {
constexpr std::uintptr_t iomux_base = 0x4042'8000;

constexpr std::uint32_t pc_mask = 1U << 7;
constexpr std::uint32_t pipd_mask = 1U << 16;
constexpr std::uint32_t pipu_mask = 1U << 17;
constexpr std::uint32_t inena_mask = 1U << 18;
constexpr std::uint32_t hiz1_mask = 1U << 25;

// NOLINTNEXTLINE(performance-no-int-to-ptr): MMIO register access
auto* iomux_registers =
  reinterpret_cast<volatile std::uint32_t*>(iomux_base + 0x4);

/**
 * @brief Locate a PINCM register by its one-based register number.
 * @param p_pincm PINCM register number, from 1 to 60.
 * @return Pointer to the selected register, or nullptr if the number is out of
 *         range.
 */
volatile std::uint32_t* pincm(std::uint8_t p_pincm)
{

  if (p_pincm < 1U || p_pincm > 60U) {
    return nullptr;
  }

  return &iomux_registers[p_pincm - 1U];
}

}  // namespace

namespace msp0g {

msp0g_iomux::msp0g_iomux(const iomux_config& p_iomux_config,
                         const iomux_settings& p_iomux_settings)
{

  m_pincm = p_iomux_config.pincm;
  std::uint8_t pf_sel = p_iomux_config.pf_sel;

  auto* pincm_reg = pincm(m_pincm);

  if (pincm_reg == nullptr || pf_sel > 0x3F) {

    std::abort();
  }

  *pincm_reg = pf_sel | pc_mask;

  if (static_cast<bool>(p_iomux_config.inena)) {

    *pincm_reg |= inena_mask;
  }

  config(p_iomux_settings);
}

void msp0g_iomux::config(const iomux_settings& p_iomux_settings)
{

  auto* pincm_reg = pincm(m_pincm);

  if (p_iomux_settings.resistor == lab1::pin_resistor::pull_down) {

    *pincm_reg &= ~pipu_mask;
    *pincm_reg |= pipd_mask;

  } else if (p_iomux_settings.resistor == lab1::pin_resistor::pull_up) {

    *pincm_reg &= ~pipd_mask;
    *pincm_reg |= pipu_mask;

  } else {

    *pincm_reg = *pincm_reg & ~pipu_mask & ~pipd_mask;
  }

  if (p_iomux_settings.open_drain) {

    *pincm_reg |= hiz1_mask;

  } else {

    *pincm_reg &= ~hiz1_mask;
  }
}

}  // namespace msp0g
