#pragma once
/// \file apic.h
/// Classes to control both local and I/O APICs.

#include <stdint.h>
#include <util/bitset.h>

#include "interrupts.h"

enum class isr : uint8_t;

/// Base class for other APIC types
class apic
{
public:
    /// Constructor
    /// \arg base   Physical base address of the APIC's MMIO registers
    apic(uintptr_t base);

protected:
    uint32_t *m_base;
};

/// Controller for processor-local APICs
class lapic :
    public apic
{
public:
    /// Constructor
    /// \arg base      Physicl base address of the APIC's MMIO registers
    lapic(uintptr_t base);

    /// Get the local APIC's ID
    uint8_t get_id();

    enum class ipi_flags
    {
        logical  = 11,
        pending  = 12,
        assert   = 14,
        level    = 15,
    };

    // IPI flags based on delivery mode (bits 8-10)
    static constexpr util::bitset32 ipi_fixed = 0;
    static constexpr util::bitset32 ipi_init  = 0x500;
    static constexpr util::bitset32 ipi_sipi  = 0x600;

    /// Send an inter-processor interrupt.
    /// \arg mode   The sending mode
    /// \arg vector The interrupt vector
    /// \arg dest   The APIC ID of the destination
    void send_ipi(util::bitset32 mode, isr vector, uint8_t dest);

    /// Send an inter-processor broadcast interrupt to all other CPUs
    /// \arg mode   The sending mode
    /// \arg self   If true, include this CPU in the broadcast
    /// \arg vector The interrupt vector
    void send_ipi_broadcast(util::bitset32 mode, bool self, isr vector);

    /// Wait for an IPI to finish sending. This is done automatically
    /// before sending another IPI with send_ipi().
    void ipi_wait();

    /// Enable interrupts for the LAPIC timer.
    /// \arg vector   Interrupt vector the timer should use
    /// \arg repeat   If false, this timer is one-off, otherwise repeating
    void enable_timer(isr vector, bool repeat = true);

    /// Reset the timer countdown.
    /// \arg interval The interval in us before an interrupt, or 0 to stop the timer
    /// \returns      The interval in us that was remaining before reset
    uint32_t reset_timer(uint64_t interval);

    /// Stop the timer.
    /// \returns  The interval in us remaining before an interrupt was to happen
    inline uint32_t stop_timer() { return reset_timer(0); }

    /// Enable interrupts for the LAPIC LINT0 pin.
    /// \arg num      Local interrupt number (0 or 1)
    /// \arg vector   Interrupt vector LINT0 should use
    /// \arg nmi      Whether this interrupt is NMI delivery mode
    /// \arg flags    Flags for mode/polarity (ACPI MPS INTI flags)
    void enable_lint(uint8_t num, isr vector, bool nmi, uint16_t flags);

    void enable();  ///< Enable servicing of interrupts
    void disable(); ///< Disable (temporarily) servicing of interrupts

    /// Calibrate the timer speed against the clock
    void calibrate_timer();

private:
    inline static uint64_t ticks_to_us(uint64_t ticks)    { return ticks / s_ticks_per_us; }
    inline static uint64_t us_to_ticks(uint64_t interval) { return interval * s_ticks_per_us; }

    void set_divisor(uint8_t divisor);
    void set_repeat(bool repeat);

    uint32_t m_divisor;
    static uint64_t s_ticks_per_us;
};


/// Controller for I/O APICs
class ioapic :
    public apic
{
public:
    /// Constructor
    /// \arg base     Physical base address of the APIC's MMIO registers
    /// \arg base_gsi Starting global system interrupt number of this IOAPIC
    ioapic(uintptr_t base, uint32_t base_gsi);

    uint32_t get_base_gsi() const   { return m_base_gsi; }
    uint32_t get_num_gsi() const    { return m_num_gsi; }

    /// Set a redirection entry.
    /// TODO: pick CPU
    /// \arg source   Source interrupt number
    /// \arg vector   Interrupt vector that should be used
    /// \arg flags    Flags for mode/polarity (ACPI MPS INTI flags)
    /// \arg masked   Whether the iterrupt should be suppressed
    void redirect(uint8_t irq, isr vector, uint16_t flags, bool masked);

    /// Mask or unmask an interrupt to stop/start having it sent to the CPU
    /// \arg irq     The IOAPIC-local irq number
    /// \arg masked  Whether to suppress this interrupt
    void mask(uint8_t irq, bool masked);

    /// Mask all interrupts on this IOAPIC.
    void mask_all() { for(int i=0; i<m_num_gsi; ++i) mask(i, true); }

    /// Unmask all interrupts on this IOAPIC.
    void unmask_all() { for(int i=0; i<m_num_gsi; ++i) mask(i, false); }

    void dump_redirs() const;

private:
    uint32_t m_base_gsi;
    uint32_t m_num_gsi;

    uint8_t m_id;
    uint8_t m_version;
};
