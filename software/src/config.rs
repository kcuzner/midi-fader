//! Device configuration abstraction

use std::marker::PhantomData;
use tokio::prelude::*;
use tokio::sync::mpsc::*;
use device::{AsyncHidDevice, MidiFader, MidiFaderExtensions, ParameterValue, GetParameter, Error};

macro_rules! parameter_type {
    ($name:ident, $mask:expr, $arg:ident) => {
        #[derive(Debug, Clone)]
        pub struct $name {
            index: u32,
            value: $arg,
        }

        impl $name {
            pub fn new(index: u32, value: $arg) -> Self {
                $name { index: index, value: value }
            }

            pub fn index(&self) -> u32 {
                self.index
            }

            pub fn value(&self) -> $arg {
                self.value
            }

            pub fn parameter(&self) -> u16 {
                $name::index_parameter(self.index)
            }

            fn index_parameter(index: u32) -> u16 {
                (((index & 0xF) as u16) << 8) | $mask
            }
        }
    };
}

macro_rules! ranged_type {
    ($name:ident, $of:ident, $min:expr, $max:expr, $size:expr) => {
        #[derive(Debug, Clone, Copy)]
        pub enum $name {
            Valid {
                n: $of,
            },
            Invalid {
                n: $of,
            },
        }

        impl $name {
            const SIZE: usize = 1;

            /// Creates a new ranged type from a raw value
            pub fn new(raw: $of) -> Self {
                match raw {
                    n @ $min...$max => $name::Valid { n: n },
                    n => $name::Invalid { n: n },
                }
            }
        }

        impl From<$name> for ParameterValue {
            fn from(v: $name) -> ParameterValue {
                match v {
                    $name::Valid { n } => ParameterValue::new(n as i32, $name::SIZE),
                    $name::Invalid { n } => ParameterValue::new(n as i32, $name::SIZE),
                }
            }
        }

        impl From<i32> for $name {
            fn from(i: i32) -> $name {
                $name::new(i as $of)
            }
        }
    };
}

macro_rules! flexible_enum {
    ($name:ident => [ $( ($opt:ident, $val:expr) ),+ ] ) => {
        #[derive(Debug, Clone, Copy)]
        pub enum $name {
            $(
                $opt,
            )+
                Invalid { n: u32 },
        }

        impl From<$name> for ParameterValue {
            fn from(v: $name) -> ParameterValue {
                let n = match v {
                    $(
                        $name::$opt => $val,
                    )+
                        $name::Invalid { n } => n as i32,
                };
                ParameterValue::new(n, 1)
            }
        }

        impl From<i32> for $name {
            fn from(i: i32) -> $name {
                match i {
                    $(
                        $val => $name::$opt,
                    )+
                        n => $name::Invalid { n: n as u32 },
                }
            }
        }
    };
}

/// Enumeration for the possible midi channels
///
/// In reality, this is a hacky attempt at a ranged numeric. Only certain numbers are valid midi
/// channels and this enum will express that.
ranged_type!(MidiChannel, u32, 0, 15, 1);

impl MidiChannel {
    /// Creates a new MidiChannel and returns Some if the value is within the valid channel range.
    pub fn try_from_channel(channel: u32) -> Option<MidiChannel> {
        // In reality, the channel is off by one
        match Self::new(channel - 1) {
            v @ MidiChannel::Valid { .. } => Some(v),
            _ => None,
        }
    }

    /// Gets the midi channel number if this is a valid channel
    ///
    /// Note that the MIDI Channel Number is one more than the raw channel number.
    pub fn channel(&self) -> Option<u32> {
        match self {
            &MidiChannel::Valid { n } => Some(n + 1),
            _ => None,
        }
    }
}

/// General valid midi value
///
/// This is a hacky attempt at a ranged numeric which works for valid midi values
ranged_type!(MidiValue, u32, 0, 127, 1);

impl MidiValue {
    pub fn try_from_value(value: u32) -> Option<MidiValue> {
        match Self::new(value) {
            v @ MidiValue::Valid { .. } => Some(v),
            _ => None,
        }
    }

    pub fn value(&self) -> Option<u32> {
        match self {
            &MidiValue::Valid { n } => Some(n),
            _ => None,
        }
    }
}

/// General valid midi pitch
///
/// This is a hacky attempt at a ranged numeric which works for valid pitch values
ranged_type!(MidiPitch, i32, -8192, 8191, 2);

impl MidiPitch {
    pub fn try_from_value(value: i32) -> Option<MidiPitch> {
        match Self::new(value) {
            v @ MidiPitch::Valid { .. } => Some(v),
            _ => None,
        }
    }

    pub fn value(&self) -> Option<i32> {
        match self {
            &MidiPitch::Valid { n } => Some(n),
            _ => None,
        }
    }
}

/// Mode for a button
flexible_enum!(ButtonMode => [ (Control, 0), (Note, 1) ]);

/// Style setting for a button
flexible_enum!(ButtonStyle => [ (Momentary, 0), (Toggle, 1) ]);

/// Mode setting for a fader
flexible_enum!(FaderMode => [ (Control, 0), (Pitch, 2) ]);

parameter_type!(BtnMidiChannel, 0x4001, MidiChannel);
parameter_type!(BtnOn, 0x4002, MidiValue);
parameter_type!(BtnOff, 0x4003, MidiValue);
parameter_type!(BtnMode, 0x4004, ButtonMode);
parameter_type!(BtnControl, 0x4005, MidiValue);
parameter_type!(BtnNote, 0x4006, MidiValue);
parameter_type!(BtnNoteVel, 0x4007, MidiValue);
parameter_type!(BtnStyle, 0x4008, ButtonStyle);
parameter_type!(FdrMidiChannel, 0x2001, MidiChannel);
parameter_type!(FdrMode, 0x2002, FaderMode);
parameter_type!(FdrControl, 0x2003, MidiValue);
parameter_type!(FdrControlMin, 0x2004, MidiValue);
parameter_type!(FdrControlMax, 0x2005, MidiValue);
parameter_type!(FdrPitchMin, 0x2006, MidiPitch);
parameter_type!(FdrPitchMax, 0x2007, MidiPitch);

/// Future which gets a particular parameter value
pub struct GetParameterValue<T: AsyncHidDevice<MidiFader>, U: Into<ParameterValue>> {
    _0: PhantomData<U>,
    underlying: GetParameter<T>,
}

impl<T: AsyncHidDevice<MidiFader>, U: Into<ParameterValue>> GetParameterValue<T, U> {
    fn new(device: T, parameter: U) -> Self {
        unimplemented!()
    }
}

/// Settings for a button on the device
pub struct Button {
    index: u32,
    channel: BtnMidiChannel,
    on: BtnOn,
    off: BtnOff,
    mode: BtnMode,
    control: BtnControl,
    note: BtnNote,
    note_vel: BtnNoteVel,
    style: BtnStyle,
}

impl Button {
    /// Builds a button configuration using the passed device and index
    ///
    /// This returns a future for the device and the associated button configuration
    fn get_button_configuration<T: AsyncHidDevice<MidiFader>>(device: T, index: u32) -> impl Future<Item=(T, Self), Error=Error> {
        // This might be the textbook definition of un-ergonomic...
        // Just watch as the tuple size gets longer and longer.
        // Maybe this could be a macro?
        device.get_parameter(BtnMidiChannel::index_parameter(index))
            .and_then(move |res| {
                let ch = res.1;
                res.0.get_parameter(BtnOn::index_parameter(index))
                    .join(Ok(ch))
            })
            .and_then(move |(res, ch)| {
                let on = res.1;
                res.0.get_parameter(BtnOff::index_parameter(index))
                    .join(Ok((ch, on)))
            })
            .and_then(move |(res, (ch, on))| {
                let off = res.1;
                res.0.get_parameter(BtnMode::index_parameter(index))
                    .join(Ok((ch, on, off)))
            })
            .and_then(move |(res, (ch, on, off))| {
                let mode = res.1;
                res.0.get_parameter(BtnControl::index_parameter(index))
                    .join(Ok((ch, on, off, mode)))
            })
            .and_then(move |(res, (ch, on, off, mode))| {
                let control = res.1;
                res.0.get_parameter(BtnNote::index_parameter(index))
                    .join(Ok((ch, on, off, mode, control)))
            })
            .and_then(move |(res, (ch, on, off, mode, control))| {
                let note = res.1;
                res.0.get_parameter(BtnNoteVel::index_parameter(index))
                    .join(Ok((ch, on, off, mode, control, note)))
            })
            .and_then(move |(res, (ch, on, off, mode, control, note))| {
                let note_vel = res.1;
                res.0.get_parameter(BtnStyle::index_parameter(index))
                    .join(Ok((ch, on, off, mode, control, note, note_vel)))
            })
            .and_then(move |(res, (ch, on, off, mode, control, note, note_vel))| {
                let style = res.1;
                let button = Button {
                    index: index,
                    channel: BtnMidiChannel::new(index, ch.value().into()),
                    on: BtnOn::new(index, on.value().into()),
                    off: BtnOff::new(index, off.value().into()),
                    mode: BtnMode::new(index, mode.value().into()),
                    control: BtnControl::new(index, control.value().into()),
                    note: BtnNote::new(index, note.value().into()),
                    note_vel: BtnNoteVel::new(index, note_vel.value().into()),
                    style: BtnStyle::new(index, style.value().into()),
                };
                Ok((res.0, button))
            })
    }
}

/// Settings for a fader on the device
pub struct Fader {
    index: u32,
    channel: FdrMidiChannel,
    mode: FdrMode,
    control: FdrControl,
    control_min: FdrControlMin,
    control_max: FdrControlMax,
    pitch_min: FdrPitchMin,
    pitch_max: FdrPitchMax,
}

impl Fader {
    /// Builds a fader configuration using the passed device and index
    ///
    /// This returns a future for the device and the associated fader configuration
    fn get_fader_configuration<T: AsyncHidDevice<MidiFader>>(device: T, index: u32) -> impl Future<Item=(T, Self), Error=Error> {
        device.get_parameter(FdrMidiChannel::index_parameter(index))
            .and_then(move |res| {
                let ch = res.1;
                res.0.get_parameter(FdrMode::index_parameter(index))
                    .join(Ok(ch))
            })
            .and_then(move |(res, ch)| {
                let mode = res.1;
                res.0.get_parameter(FdrControl::index_parameter(index))
                    .join(Ok((ch, mode)))
            })
            .and_then(move |(res, (ch, mode))| {
                let control = res.1;
                res.0.get_parameter(FdrControlMin::index_parameter(index))
                    .join(Ok((ch, mode, control)))
            })
            .and_then(move |(res, (ch, mode, control))| {
                let control_min = res.1;
                res.0.get_parameter(FdrControlMax::index_parameter(index))
                    .join(Ok((ch, mode, control, control_min)))
            })
            .and_then(move |(res, (ch, mode, control, control_min))| {
                let control_max = res.1;
                res.0.get_parameter(FdrPitchMin::index_parameter(index))
                    .join(Ok((ch, mode, control, control_min, control_max)))
            })
            .and_then(move |(res, (ch, mode, control, control_min, control_max))| {
                let pitch_min = res.1;
                res.0.get_parameter(FdrPitchMax::index_parameter(index))
                    .join(Ok((ch, mode, control, control_min, control_max, pitch_min)))
            })
            .and_then(move |(res, (ch, mode, control, control_min, control_max, pitch_min))| {
                let pitch_max = res.1;
                let fader = Fader {
                    index: index,
                    channel: FdrMidiChannel::new(index, ch.value().into()),
                    mode: FdrMode::new(index, mode.value().into()),
                    control: FdrControl::new(index, control.value().into()),
                    control_min: FdrControlMin::new(index, control_min.value().into()),
                    control_max: FdrControlMax::new(index, control_max.value().into()),
                    pitch_min: FdrPitchMin::new(index, pitch_min.value().into()),
                    pitch_max: FdrPitchMax::new(index, pitch_max.value().into()),
                };
                Ok((res.0, fader))
            })
    }
}

pub struct GroupConfig {
    index: u32,
    button: Button,
    fader: Fader
}

impl GroupConfig {
    fn get_group_configuration<T: AsyncHidDevice<MidiFader>>(device: T, index: u32) -> impl Future<Item=(T, Self), Error=Error> {
        Fader::get_fader_configuration(device, index)
            .and_then(move |res| {
                Button::get_button_configuration(res.0, index)
                    .join(Ok(res.1))
            })
            .and_then(move |(res, fader)| {
                let group = GroupConfig {
                    index: index,
                    button: res.1,
                    fader: fader
                };
                Ok((res.0, group))
            })
    }
}

pub struct DeviceConfig<T: AsyncHidDevice<MidiFader>> {
    device: T,
    groups: [GroupConfig; 8],
}

impl<T: AsyncHidDevice<MidiFader>> DeviceConfig<T> {
    pub fn new(device: T) -> impl Future<Item=Self, Error=Error> {
        GroupConfig::get_group_configuration(device, 0)
            .and_then(|res| {
                GroupConfig::get_group_configuration(res.0, 1)
                    .join(Ok(res.1))
            })
            .and_then(|(res, group0)| {
                GroupConfig::get_group_configuration(res.0, 2)
                    .join(Ok((group0, res.1)))
            })
            .and_then(|(res, groups)| {
                GroupConfig::get_group_configuration(res.0, 3)
                    .join(Ok((groups.0, groups.1, res.1)))
            })
            .and_then(|(res, groups)| {
                GroupConfig::get_group_configuration(res.0, 4)
                    .join(Ok((groups.0, groups.1, groups.2, res.1)))
            })
            .and_then(|(res, groups)| {
                GroupConfig::get_group_configuration(res.0, 5)
                    .join(Ok((groups.0, groups.1, groups.2, groups.3, res.1)))
            })
            .and_then(|(res, groups)| {
                GroupConfig::get_group_configuration(res.0, 6)
                    .join(Ok((groups.0, groups.1, groups.2, groups.3, groups.4, res.1)))
            })
            .and_then(|(res, groups)| {
                GroupConfig::get_group_configuration(res.0, 7)
                    .join(Ok((groups.0, groups.1, groups.2, groups.3, groups.4, groups.5,
                              res.1)))
            })
            .and_then(|(res, groups)| {
                Ok(DeviceConfig { device: res.0, groups: [groups.0, groups.1, groups.2,
                    groups.3, groups.4, groups.5, groups.6, res.1] })
            })
    }
}

/// Configuration request
pub enum Request<T: AsyncHidDevice<MidiFader>> {
    ReadConfiguration(T),
}

pub enum Response<T: AsyncHidDevice<MidiFader>> {
    Configured(DeviceConfig<T>)
}

pub fn configure<T: AsyncHidDevice<MidiFader>>(requests: Receiver<Request<T>>, responses:
                                               Sender<Response<T>>) {
}

