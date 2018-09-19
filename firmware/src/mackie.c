/**
 * USB Midi-Fader
 *
 * Mackie control emulation
 *
 * Kevin Cuzner
 */

#include "mackie.h"

#include "atomic.h"
#include "usb_midi.h"
#include "fader.h"
#include "buttons.h"
#include "configuration.h"
#include "systick.h"

#include <string.h>

#define MAX_CTL_COUNT 8

static volatile uint32_t systick_count = 0;
static void mackie_systick(void)
{
    systick_count++;
}

static uint32_t next_event_tick = 0;

void mackie_init()
{
    systick_subscribe(&mackie_systick);
}

static volatile struct {
    uint32_t last_sent_tick;
    uint16_t fader_values[MAX_CTL_COUNT];
    uint8_t fdr_update; // Set to 0xFF to force a complete update
    uint8_t btn_raw_values;
    uint8_t btn_values;
    uint8_t btn_update; // Set to 0xFF to force a complete update
} mackie_status;

/**
 * Scales a fader value to some minimum and maximum
 */
static int16_t mackie_scale_fader(uint8_t index, int16_t min, int16_t max)
{
    int32_t fader = mackie_status.fader_values[index];
    int32_t range = max - min;
    if (range < FADER_MAX)
    {
        // scale down
        fader /= FADER_MAX / range;
    }
    else
    {
        // scale up
        fader *= range / FADER_MAX;
    }
    return fader - min;
}

/**
 * Builds a control change event for a fader
 */
static void mackie_build_fader_cc_event(uint8_t index, uint8_t *buf, Error err)
{
    uint8_t channel = configuration_fdr_channel(index, err);
    uint8_t cc = configuration_fdr_cc(index, err);
    uint8_t cc_min = configuration_fdr_cc_min(index, err);
    uint8_t cc_max = configuration_fdr_cc_max(index, err);
    if (ERROR_IS_FATAL(err))
        return;

    int16_t fader_scaled = mackie_scale_fader(index, cc_min, cc_max);
    if (fader_scaled < 0)
        fader_scaled = 0;
    if (fader_scaled > 127)
        fader_scaled = 127;

    buf[0] = 0xB0 | (channel & 0xF);
    buf[1] = cc & 0x7F;
    buf[2] = fader_scaled & 0x7F;
}

/**
 * Builds a pitch event for a fader
 */
static void mackie_build_fader_pitch_event(uint8_t index, uint8_t *buf, Error err)
{
    uint8_t channel = configuration_fdr_channel(index, err);
    int16_t pitch_min = configuration_fdr_pitch_min(index, err);
    int16_t pitch_max = configuration_fdr_pitch_max(index, err);
    if (ERROR_IS_FATAL(err))
        return;

    int16_t fader_scaled = mackie_scale_fader(index, pitch_min, pitch_max);

    buf[0] = 0xE0 | (channel & 0xF);
    buf[1] = fader_scaled & 0x7F;
    buf[2] = (fader_scaled >> 7) & 0x7F;
}

/**
 * Builds an event for the current fader state
 */
static USBMidiCodeIndex mackie_build_fader_event(uint8_t index, uint8_t *buf, Error err)
{
    uint8_t mode = configuration_fdr_mode(index, err);
    if (ERROR_IS_FATAL(err))
        return MIDI_MISC_FN;

    switch (mode)
    {
        case CFG_MODE_CTL:
            // Issue a control change on the channel
            mackie_build_fader_cc_event(index, buf, err);
            return MIDI_CTRL;
        default:
            // Issue a pitch update on the channel
            mackie_build_fader_pitch_event(index, buf, err);
            return MIDI_PITCH_BEND;
    }
}

/**
 * Builds an event for the current button state
 */
static USBMidiCodeIndex mackie_build_button_event(uint8_t index, uint8_t *buf, Error err)
{
    uint8_t mode = configuration_btn_mode(index, err);
    uint8_t channel = configuration_fdr_channel(index, err);
    if (ERROR_IS_FATAL(err))
        return MIDI_MISC_FN;

    uint8_t btn = mackie_status.btn_values & (0x1 << index);
    if (mode == CFG_MODE_CTL)
    {
        uint8_t cc = configuration_btn_cc(index, err);
        uint8_t value = btn ? configuration_btn_cc_on(index, err) :
            configuration_btn_cc_off(index, err);
        
        buf[0] = 0xB0 | (channel & 0xF);
        buf[1] = cc & 0x7F;
        buf[2] = value & 0x7F;
        return MIDI_CTRL;
    }
    else
    {
        uint8_t note = configuration_btn_note(index, err);
        uint8_t vel = configuration_btn_note_vel(index, err);
        
        buf[0] = (btn ? 0x90 : 0x80) | (channel & 0xF);
        buf[1] = note & 0x7F;
        buf[2] = vel & 0x7F;
        return btn ? MIDI_NOTE_ON : MIDI_NOTE_OFF;
    }
}

void mackie_tick()
{
    ERROR_INST(err);

    // Determine if its time to do stuff yet
    if (systick_count < next_event_tick)
        return;

    // Determine when next to do stuff
    uint32_t dly = configuration_event_tick_delay(&err);
    if (ERROR_IS_FATAL(&err))
        return;
    next_event_tick = systick_count + dly;

    // Read the number of controls
    //
    // There will always be the same number of buttons and faders
    uint8_t ctl_count = buttons_get_count();

    // Check for updates
    uint8_t new_buttons = buttons_read();
    uint8_t i, mask;
    for (i = 0, mask = 0x01; i < ctl_count; i++, mask <<= 1)
    {
        // Check for fader updates
        uint16_t new_fader = fader_get_value(i) & 0xFF0; //chop the last bit for noise
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if (new_fader != mackie_status.fader_values[i])
                mackie_status.fdr_update |= mask;
        }
        mackie_status.fader_values[i] = new_fader;

        // Check for button updates
        uint8_t new_btn_masked = new_buttons & mask;
        uint8_t old_btn_masked = mackie_status.btn_raw_values & mask;
        uint8_t btn_mode = configuration_btn_style(i, &err);
        if (ERROR_IS_FATAL(&err))
            return;
        if ((btn_mode == CFG_BTN_MOM && new_btn_masked != old_btn_masked) ||
                (btn_mode == CFG_BTN_TOG && new_btn_masked && !old_btn_masked))
        {
            mackie_status.btn_values ^= mask;
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                mackie_status.btn_update |= mask;
            }
        }

        // Generate an update for this channel if requested
        //
        // NOTE: The update flag is stored in its very own bit in the persistent
        // state so that we can trigger a mass update from elsewhere without
        // needing to have separate logic.
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            uint8_t buffer[3];
            if (mackie_status.fdr_update & mask)
            {
                memset(buffer, 0, sizeof(buffer));
                USBMidiCodeIndex code = mackie_build_fader_event(i, buffer, &err);
                if (ERROR_IS_FATAL(&err))
                    return;
                usb_midi_send(code, buffer, sizeof(buffer), USB_MIDI_NOBLOCK);
                    mackie_status.fdr_update &= ~mask;
            }
            if (mackie_status.btn_update & mask)
            {
                memset(buffer, 0, sizeof(buffer));
                USBMidiCodeIndex code = mackie_build_button_event(i, buffer, &err);
                if (ERROR_IS_FATAL(&err))
                    return;
                usb_midi_send(code, buffer, sizeof(buffer), USB_MIDI_NOBLOCK);
                    mackie_status.btn_update &= ~mask;
            }
        }
    }

    mackie_status.btn_raw_values = new_buttons;
    buttons_write_leds(mackie_status.btn_values);

    if (mackie_status.last_sent_tick + USB_MIDI_TX_INTERVAL_MS > systick_count)
    {
        // Only flush if we have managed to actually send data within the last
        // TX interval. Hopefully this will prevent stale data from being
        // queued.
        usb_midi_flush();
    }
}

void hook_usb_send_complete()
{
    mackie_status.last_sent_tick = systick_count;
}

