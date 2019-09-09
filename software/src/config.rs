//! Device configuration abstraction

use std::slice;
use std::marker::PhantomData;
use tokio::prelude::*;
use tokio::sync::mpsc as tokio_mpsc;
use std::sync::mpsc as std_mpsc;
use arrayvec;
use device;
use device::{AsyncHidDevice, MidiFader, MidiFaderExtensions, ParameterValue, GetParameter};

// Overall, the code in this module is pretty horrifying. I've tried to reduce the repetitiveness
// through use of macros, but in some cases it was easier to just type it out. In the case where I
// have done things like sequentially read out the parameters from the device, the types of the
// futures get quite lengthy and unwieldy (a syntax error can be mostly typename). Since the new
// method and commit method of the overall device configuration (and underlying collections)
// require iterating all of the parameters, this technique is used multiple places. I'm pretty sure
// it could be turned into a macro someday.

#[derive(Debug, Fail)]
pub enum Error {
    #[fail(display = "Device error: {}", _0)]
    DeviceError(#[cause] device::Error),
    #[fail(display = "Channel receiver error")]
    RecvError,
    #[fail(display = "Channel sender error")]
    SendError,
    #[fail(display = "An error to test stuff")]
    TestError,
}

impl From<device::Error> for Error {
    fn from(e: device::Error) -> Self {
        Error::DeviceError(e)
    }
}

impl From<tokio_mpsc::error::RecvError> for Error {
    fn from(_: tokio_mpsc::error::RecvError) -> Self {
        Error::RecvError
    }
}

impl<T> From<std_mpsc::SendError<T>> for Error {
    fn from(_: std_mpsc::SendError<T>) -> Self {
        Error::SendError
    }
}

macro_rules! parameter_type {
    ($name:ident, $mask:expr, $arg:ident) => {
        /// Generated parameter which encapsulates a parameter identifier and parameter value
        #[derive(Debug, Clone)]
        pub struct $name {
            index: u32,
            value: $arg,
            update: Option<$arg>,
        }

        impl $name {
            /// Creates a new parameter of this type
            ///
            /// index: Channel index of this parameter value
            /// value: Parameter value
            pub fn new(index: u32, value: $arg) -> Self {
                $name { index: index, value: value, update: None }
            }

            pub fn index(&self) -> u32 {
                self.index
            }

            pub fn value(&self) -> $arg {
                match self.update {
                    Some(u) => u,
                    None => self.value,
                }
            }

            pub fn original_value(&self) -> $arg {
                self.value
            }

            pub fn update(&mut self, value: $arg) {
                if self.value != value {
                    self.update = Some(value);
                } else{
                    self.update = None;
                }
            }

            /// Gets the underlying update value
            ///
            /// If there is no update, return sNone. Otherwise it returns Some with the update
            /// value.
            pub fn get_update(&self) -> Option<$arg> {
                self.update
            }

            /// Clears any pending update
            pub fn clear(&mut self) {
                self.update = None
            }

            /// Commits the pending update as the value of this parameter
            fn commit(&mut self) {
                match self.update {
                    Some(u) => self.value = u,
                    _ => {}
                }
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

trait IntoParameterValue : Into<ParameterValue> {
    const SIZE: usize;
}

macro_rules! ranged_type {
    ($name:ident, $of:ident, $min:expr, $max:expr, $size:expr) => {
        #[derive(Debug, Clone, Copy, PartialEq)]
        pub enum $name {
            Valid {
                n: $of,
            },
            Invalid {
                n: $of,
            },
        }

        impl $name {
            pub const MIN: $of = $min;
            pub const MAX: $of = $max;
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

        impl IntoParameterValue for $name {
            const SIZE: usize = $size;
        }

        impl From<i32> for $name {
            fn from(i: i32) -> $name {
                $name::new(i as $of)
            }
        }

        impl From<$name> for i32 {
            fn from(v: $name) -> i32 {
                match v {
                    $name::Valid { n } => n as i32,
                    $name::Invalid { n } => n as i32,
                }
            }
        }
    };
}

macro_rules! flexible_enum {
    ($name:ident => [ $( ($opt:ident, $val:expr) ),+ ] ) => {
        #[derive(Debug, Clone, Copy, PartialEq)]
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

        impl IntoParameterValue for $name {
            const SIZE: usize = 1;
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

        impl From<$name> for i32 {
            fn from(v: $name) -> i32 {
                match v {
                    $(
                        $name::$opt => $val,
                    )+
                        $name::Invalid { n } => n as i32,
                }
            }
        }
    };
}

macro_rules! parameter_collection {
    ($name:ident {
        $( $param:ident: $t:ty, )+
    }) => {
        #[derive(Debug)]
        pub struct $name {
            index: u32,
            $(
                $param: $t,
            )+
        }

        paste::item! {
            struct [<$name Builder>] {
                index : u32,
                $(
                    $param: Option<ParameterValue>,
                )+
            }

            impl [<$name Builder>] {
                // Specify the new function in terms of a future in order to simplify the
                // macro-generation of the multi-step future that reads back device information
                fn new<T: AsyncHidDevice<MidiFader>>(device: T, index: u32) -> impl Future<Item=(T, Self), Error=Error> {
                    let builder = [<$name Builder>] {
                        index: index,
                        $(
                            $param: None,
                        )+
                    };
                    future::result(Ok((device, builder)))
                }

                $(
                    fn [<set_ $param>](mut self, value: ParameterValue) -> Self {
                        self.$param = Some(value);
                        self
                    }
                )+
            }

            impl From<[<$name Builder>]> for $name {
                fn from(builder: [<$name Builder>]) -> $name {
                    $name {
                        index: builder.index,
                        $(
                            $param: $t::new(builder.index, builder.$param.unwrap().value().into()),
                        )+
                    }
                }
            }

            impl $name {
                fn read_from<T: AsyncHidDevice<MidiFader>>(device: T, index: u32) -> impl Future<Item=(T, Self), Error=Error> {
                    [<$name Builder>]::new(device, index)
                    $(
                        .and_then(|res| {
                            res.0.get_parameter($t::index_parameter(res.1.index))
                                .map_err(|e| e.into())
                                .join(Ok(res.1))
                        })
                        .and_then(|(res, builder)| {
                            Ok((res.0, builder.[<set_ $param>](res.1)))
                        })
                    )+
                        .and_then(|(device, builder)| {
                            Ok((device, builder.into()))
                        })
                }

                $(
                    pub fn $param(&self) -> &$t {
                        &self.$param
                    }
                    pub fn [<$param _mut>](&mut self) -> &mut $t {
                        &mut self.$param
                    }

                    fn [<commit_ $param>]<T: AsyncHidDevice<MidiFader>>(self, device: T) -> impl Future<Item=(T, Self), Error=Error> {
                        match self.$param.get_update() {
                            Some(u) => {
                                future::Either::A(
                                    device.set_parameter(self.$param.parameter(), u.into())
                                        .join(Ok(self))
                                        .map_err(|e| e.into())
                                        .and_then(|(device, s)| {
                                            //s.$param.commit();
                                            Ok((device, s))
                                        }))
                            },
                            None => {
                                future::Either::B(future::result(Ok((device, self))))
                            }
                        }
                    }
                )+

                fn commit<T: AsyncHidDevice<MidiFader>>(self, device: T) -> impl Future<Item=(T, Self), Error=Error> {
                    future::result(Ok((device, self)))
                        $(
                            .and_then(|(device, s)| {
                                s.[<commit_ $param>](device)
                            })
                        )+
                }
            }
        }
    }
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
parameter_collection!(Button {
    channel: BtnMidiChannel,
    on: BtnOn,
    off: BtnOff,
    mode: BtnMode,
    control: BtnControl,
    note: BtnNote,
    note_vel: BtnNoteVel,
    style: BtnStyle,
});

/// Settings for a fader on the device
parameter_collection!(Fader {
    channel: FdrMidiChannel,
    mode: FdrMode,
    control: FdrControl,
    control_min: FdrControlMin,
    control_max: FdrControlMax,
    pitch_min: FdrPitchMin,
    pitch_max: FdrPitchMax,
});

#[derive(Debug)]
pub struct GroupConfig {
    index: u32,
    button: Button,
    fader: Fader
}

impl GroupConfig {
    fn get_group_configuration<T: AsyncHidDevice<MidiFader>>(device: T, index: u32) -> impl Future<Item=(T, Self), Error=Error> {
        Fader::read_from(device, index)
            .and_then(move |res| {
                Button::read_from(res.0, index)
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

    fn commit<T: AsyncHidDevice<MidiFader>>(self, device: T) -> impl Future<Item=(T, Self), Error=Error> {
        let index = self.index;
        let fader = self.fader;
        let button = self.button;
        fader.commit(device)
            .and_then(move |(device, fader)| {
                button.commit(device)
                    .join(Ok(fader))
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

    pub fn button(&self) -> &Button {
        &self.button
    }

    pub fn fader(&self) -> &Fader {
        &self.fader
    }

    pub fn button_mut(&mut self) -> &mut Button {
        &mut self.button
    }

    pub fn fader_mut(&mut self) -> &mut Fader {
        &mut self.fader
    }
}

#[derive(Debug)]
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

    pub fn groups_len(&self) -> usize {
        self.groups.len()
    }

    pub fn group_mut<'a>(&'a mut self, index: usize) -> Option<&'a mut GroupConfig> {
        self.groups.get_mut(index)
    }

    pub fn groups_mut<'a>(&'a mut self) -> slice::IterMut<'a, GroupConfig> {
        self.groups.iter_mut()
    }

    /// Discards this configuration and gives back the device
    pub fn discard(self) -> T {
        self.device
    }

    /// Commits this configuration's changes to the device
    pub fn commit(self) -> impl Future<Item=Self, Error=Error> {
        let mut groups = arrayvec::ArrayVec::from(self.groups);
        let group7 = groups.pop().unwrap();
        let group6 = groups.pop().unwrap();
        let group5 = groups.pop().unwrap();
        let group4 = groups.pop().unwrap();
        let group3 = groups.pop().unwrap();
        let group2 = groups.pop().unwrap();
        let group1 = groups.pop().unwrap();
        let group0 = groups.pop().unwrap();
        group0.commit(self.device)
            .and_then(|(device, group)| {
                group1.commit(device)
                    .join(Ok(group))
            })
            .and_then(|(res, group0)| {
                group2.commit(res.0)
                    .join(Ok((group0, res.1)))
            })
            .and_then(|(res, groups)| {
                group3.commit(res.0)
                    .join(Ok((groups.0, groups.1, res.1)))
            })
            .and_then(|(res, groups)| {
                group4.commit(res.0)
                    .join(Ok((groups.0, groups.1, groups.2, res.1)))
            })
            .and_then(|(res, groups)| {
                group5.commit(res.0)
                    .join(Ok((groups.0, groups.1, groups.2, groups.3, res.1)))
            })
            .and_then(|(res, groups)| {
                group6.commit(res.0)
                    .join(Ok((groups.0, groups.1, groups.2, groups.3, groups.4, res.1)))
            })
            .and_then(|(res, groups)| {
                group7.commit(res.0)
                    .join(Ok((groups.0, groups.1, groups.2, groups.3, groups.4, groups.5, res.1)))
            })
            .and_then(|(res, groups)| {
                Ok(DeviceConfig { device: res.0, groups: [groups.0, groups.1, groups.2, groups.3,
                    groups.4, groups.5, groups.6, res.1] })
            })
    }
}

/// Configuration request
#[derive(Debug)]
pub enum Request<T: AsyncHidDevice<MidiFader>> {
    ReadConfiguration(T, std_mpsc::Sender<Response<T>>),
    WriteConfiguration(DeviceConfig<T>, std_mpsc::Sender<Response<T>>),
}

/// Configuration response
#[derive(Debug)]
pub enum Response<T: AsyncHidDevice<MidiFader>> {
    Configured(DeviceConfig<T>),
    Error(Error),
}

/// Captured value that will appear in both Ok and Err results of a future
struct Capture<T> {
    item: T
}

/// Handles configuration requests
pub fn configure<T: AsyncHidDevice<MidiFader>>(
    requests: tokio_mpsc::Receiver<Request<T>>) -> impl Future<Item=(), Error=Error> {

    requests.map_err(|e| e.into())
        .for_each(|r: Request<T>| {
            // Process a request
            //
            // Each request will have the following logic
            //   1. Process the request, possibly through a future
            //   2. Any errors at this point will be send through the response queue
            //   3. Any errors after the point of sending through the queue will result in this
            //      stream finishing.
            match r {
                Request::ReadConfiguration(dev, responses) => {
                    future::Either::A(DeviceConfig::new(dev).join(Ok(responses.clone()))
                        .then(move |res| {
                            future::result(match res {
                                Ok((cfg, responses)) => responses.send(Response::Configured(cfg)),
                                Err(e) => responses.send(Response::Error(e)),
                            })
                        })
                        .map_err(|e| e.into()))
                    },
                Request::WriteConfiguration(c, responses) =>
                    future::Either::B(
                        c.commit()
                        .join(Ok(responses.clone()))
                        .then(move |res| {
                            future::result(match res {
                                Ok((cfg, responses)) => responses.send(Response::Configured(cfg)),
                                Err(e) => responses.send(Response::Error(e)),
                            })
                        })
                        .map_err(|e| e.into()))
            }
        })
}

