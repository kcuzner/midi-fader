//! GUI layer
//!
//! This interacts with the configuration layer by means of a Tokio MPSC. The GUI is meant to run
//! on its own dedicated thread so that it is always responsive. It owns the device while it is
//! being configured and the passes it over to the tokio layer when the device needs to be read or
//! written, since those operations use nonblocking I/O.

use std::ops;
use tokio::sync::mpsc as tokio_mpsc;
use std::sync::mpsc as std_mpsc;
use std::time::Instant;
use imgui::*;
use device::*;
use config;

const CLEAR_COLOR: [f32; 4] = [1.0, 1.0, 1.0, 1.0];

pub type ConfigRequest = config::Request<Device<MidiFader>>;
pub type ConfigResponse = config::Response<Device<MidiFader>>;

/// Renders a full-window
fn show_window<'a, F: FnOnce((f64, f64))>(ui: &Ui<'a>, title: &'a ImStr, builder: F) {
    let framesize = ui.frame_size().logical_size;
    ui.window(title)
        .size((framesize.0 as f32, framesize.1 as f32), ImGuiCond::FirstUseEver)
        .position((0f32, 0f32), ImGuiCond::FirstUseEver)
        .title_bar(false)
        .resizable(false)
        .movable(false)
        .build(|| {
            builder(framesize)
        })
}

/// Renders a full-window textbox
fn textbox<'a>(ui: &Ui<'a>, title: &'a ImStr, text: &'a ImStr) {
    show_window(ui, title, |framesize| {
        let text_size = ui.calc_text_size(text, false, 0f32);
        let text_pos = ((framesize.0 as f32 - text_size.x) / 2f32, (framesize.1 as f32 - text_size.y) / 2f32);
        ui.set_cursor_pos(text_pos);
        ui.text(text);
    });
}

/// Gui state for when the device is being enumerated.
///
/// This will repeatedly enumerate midi fader devices until one is found
struct FindingDevice {
    timer: f32,
}

impl FindingDevice {
    fn new() -> Self {
        FindingDevice { timer: 0.0 }
    }

    fn render<'a>(mut self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>, delta_s: f32) -> (GuiState, bool) {
        self.timer += delta_s;
        let text = if self.timer < 0.5 {
            im_str!("Finding device.  ")
        } else if self.timer < 1.0 {
            im_str!("Finding device . ")
        } else {
            if self.timer > 1.5 {
                self.timer = 0.0;
            }
            im_str!("Finding device  .")
        };
        textbox(ui, im_str!("Finding Device"), text);
        match Device::<MidiFader>::enumerate().unwrap().take(1).next() {
            Some(dev) => {
                let (tx, rx) = std_mpsc::channel();
                configure_out
                    .try_send(config::Request::ReadConfiguration(dev.unwrap(), tx));
                (WaitingForResponse::new(rx).into(), false)
            },
            None => (self.into(), false),
        }
    }
}

impl From<FindingDevice> for GuiState {
    fn from(s: FindingDevice) -> GuiState {
        GuiState::FindingDevice(s)
    }
}

/// Renders a button onto a UI and handles user changes
fn render_button<'a>(ui: &Ui<'a>, button: &mut config::Button) {
    ui.text(im_str!("Button"));
    // Channel
    let mut channel = button.channel().value().into();
    ui.slider_int(im_str!("MIDI Channel"), &mut channel, config::MidiChannel::MIN as i32,
        config::MidiChannel::MAX as i32).build();
    button.channel_mut().update(channel.into());
    // Button mode
    let mut mode = button.mode().value().into();
    ui.text(im_str!("Button Mode"));
    ui.radio_button(im_str!("Control Code"), &mut mode, config::ButtonMode::Control.into());
    ui.same_line(0f32);
    ui.radio_button(im_str!("Note"), &mut mode, config::ButtonMode::Note.into());
    button.mode_mut().update(mode.into());
    match button.mode().value() {
        config::ButtonMode::Control => {
            // Control Code
            let mut control = button.control().value().into();
            ui.slider_int(im_str!("CC Number"), &mut control, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            button.control_mut().update(control.into());
            // On CC value
            let mut on = button.on().value().into();
            ui.slider_int(im_str!("Active CC Value"), &mut on, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            button.on_mut().update(on.into());
            // Off CC value
            let mut off = button.off().value().into();
            ui.slider_int(im_str!("Inactive CC Value"), &mut off, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            button.off_mut().update(off.into());
        },
        config::ButtonMode::Note => {
            // Note
            let mut note = button.note().value().into();
            ui.slider_int(im_str!("MIDI Note"), &mut note, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            button.note_mut().update(note.into());
            // Note velocity
            let mut note_vel = button.note_vel().value().into();
            ui.slider_int(im_str!("Note Velocity"), &mut note_vel, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            button.note_vel_mut().update(note_vel.into());
        },
        config::ButtonMode::Invalid { n } => {
            ui.text(im_str!("-- Invalid Button Mode Selected -- "));
        },
    }
    // Button style
    let mut style = button.style().value().into();
    ui.text(im_str!("Button Style"));
    ui.radio_button(im_str!("Momentary"), &mut style, config::ButtonStyle::Momentary.into());
    ui.same_line(0f32);
    ui.radio_button(im_str!("Toggle"), &mut style, config::ButtonStyle::Toggle.into());
    button.style_mut().update(style.into());
}

fn render_fader<'a>(ui: &Ui<'a>, fader: &mut config::Fader) {
    ui.text(im_str!("Fader"));
    // Channel
    let mut channel = fader.channel().value().into();
    ui.slider_int(im_str!("MIDI Channel"), &mut channel, config::MidiChannel::MIN as i32,
        config::MidiChannel::MAX as i32).build();
    fader.channel_mut().update(channel.into());
    // Fader mode
    let mut mode = fader.mode().value().into();
    ui.text(im_str!("Fader Mode"));
    ui.radio_button(im_str!("Control Code"), &mut mode, config::FaderMode::Control.into());
    ui.same_line(0f32);
    ui.radio_button(im_str!("Pitch"), &mut mode, config::FaderMode::Pitch.into());
    fader.mode_mut().update(mode.into());
    match fader.mode().value() {
        config::FaderMode::Control => {
            // Control code
            let mut control = fader.control().value().into();
            ui.slider_int(im_str!("CC Number"), &mut control, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            fader.control_mut().update(control.into());
            // Control minimum
            let mut control_min = fader.control_min().value().into();
            ui.slider_int(im_str!("Min CC Value"), &mut control_min, config::MidiValue::MIN as i32,
                config::MidiValue::MAX as i32).build();
            fader.control_min_mut().update(control_min.into());
            // Control maximum
            let mut control_max = fader.control_max().value().into();
            ui.slider_int(im_str!("Max CC Value"), &mut control_max, config::MidiValue::MAX as i32,
                config::MidiValue::MAX as i32).build();
            fader.control_max_mut().update(control_max.into());
        },
        config::FaderMode::Pitch => {
            // Pitch minimum
            let mut pitch_min = fader.pitch_min().value().into();
            ui.slider_int(im_str!("Min Pitch Value"), &mut pitch_min, config::MidiPitch::MIN as i32,
                config::MidiPitch::MAX as i32).build();
            fader.pitch_min_mut().update(pitch_min.into());
            // Pitch maximum
            let mut pitch_max = fader.pitch_max().value().into();
            ui.slider_int(im_str!("Max Pitch Value"), &mut pitch_max, config::MidiPitch::MIN as i32,
                config::MidiPitch::MAX as i32).build();
            fader.pitch_max_mut().update(pitch_max.into());
        },
        config::FaderMode::Invalid { n } => {
            ui.text(im_str!("-- Invalid Fader Mode Selected --"))
        },
    }
}

/// Function which tells the borrow checker how long to borrow the elements
/// of a slice for
fn borrow_all<'a>(source: &'a[ImString]) -> Vec<&'a ImStr> {
    let mut vec = Vec::with_capacity(source.len());
    for t in source {
        vec.push(t.as_ref())
    }
    vec
}

/// Primary GUI state for when the device is being configured
///
/// This will display configuration controls whose state is cached until the user clicks a save
/// button. At that point, this state will be exited and the WaitingForResponse state will be
/// entered.
struct Configuring {
    dev: config::DeviceConfig<Device<MidiFader>>,
    group_index: i32,
}

impl Configuring {
    fn new(device: config::DeviceConfig<Device<MidiFader>>) -> Self {
        Configuring { dev: device, group_index: 0 }
    }

    fn render<'a>(mut self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>, delta_s: f32) -> (GuiState, bool) {
        #[derive(Debug)]
        enum UiResult {
            Save,
            Discard,
            Waiting,
        }
        let mut result = UiResult::Waiting;
        show_window(ui, im_str!("Configuring"), |framesize| {
            let menu_items = ops::Range { start: 0, end: self.dev.groups_len() }
                .into_iter().map(|x| ImString::new(format!("Channel {}", x))).collect::<Vec<ImString>>();
            let menu_strs = borrow_all(&menu_items);
            if self.group_index >= menu_strs.len() as i32 {
                self.group_index = menu_strs.len() as i32 - 1;
            }
            if self.group_index < 0 {
                self.group_index = 0
            }
            ui.combo(im_str!("Configure Channel"), &mut self.group_index, &menu_strs, self.dev.groups_len() as i32);
            ui.separator();
            let group = self.dev.group_mut(self.group_index as usize).expect("Invalid group selected");
            ui.columns(2, im_str!("columns"), true);
            render_button(ui, group.button_mut());
            ui.next_column();
            render_fader(ui, group.fader_mut());
            ui.next_column();
            ui.separator();
            if ui.small_button(im_str!("Save changes to device")) {
                result = UiResult::Save;
            }
            if ui.small_button(im_str!("Discard changes")) {
                result = UiResult::Discard;
            }
        });
        match result {
            UiResult::Waiting => (self.into(), false),
            UiResult::Save => {
                let (tx, rx) = std_mpsc::channel();
                configure_out
                    .try_send(config::Request::WriteConfiguration(self.dev, tx));
                (WaitingForResponse::new(rx).into(), false)
            }
            UiResult::Discard => {
                let (tx, rx) = std_mpsc::channel();
                configure_out
                    .try_send(config::Request::ReadConfiguration(self.dev.discard(), tx));
                (WaitingForResponse::new(rx).into(), false)
            }
        }
    }
}

impl From<Configuring> for GuiState {
    fn from(s: Configuring) -> GuiState {
        GuiState::Configuring(s)
    }
}

/// Generic state for when the device is being configured in some way.
///
/// Whenever a configuration request is performed, the device itself is passed to the configuration
/// thread(s) which are tokio-based for async io. Once the configurator has finished doing its
/// thing, it will send the device back as a configuration response through the mpsc sender that
/// was passed with the configuration request. This gui state waits for something to appear on that
/// queue before moving back to the configuring GUI state.
struct WaitingForResponse {
    receiver: std_mpsc::Receiver<ConfigResponse>,
    timer: f32,
}

impl WaitingForResponse {
    fn new(receiver: std_mpsc::Receiver<ConfigResponse>) -> Self {
        WaitingForResponse { receiver: receiver, timer: 0.0 }
    }

    fn render<'a>(mut self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>, delta_s: f32) -> (GuiState, bool) {
        self.timer += delta_s;
        let text = if self.timer < 0.5 {
            im_str!("Waiting for device.  ")
        } else if self.timer < 1.0 {
            im_str!("Waiting for device . ")
        } else {
            if self.timer > 1.5 {
                self.timer = 0.0;
            }
            im_str!("Waiting for device  .")
        };
        textbox(ui, im_str!("Waiting"), text);
        match self.receiver.try_recv() {
            Ok(r) => {
                match r {
                    config::Response::Configured(d) => (Configuring::new(d).into(), false),
                    config::Response::Error(e) => (ShowError::new(e).into(), false),
                }
            },
            Err(std_mpsc::TryRecvError::Empty) => (self.into(), false),
            Err(std_mpsc::TryRecvError::Disconnected) => {
                // We have crashed
                unimplemented!()
            }
        }
    }
}

impl From<WaitingForResponse> for GuiState {
    fn from(s: WaitingForResponse) -> GuiState {
        GuiState::WaitingForResponse(s)
    }
}

struct ShowError {
    error: config::Error,
}

impl ShowError {
    fn new(error: config::Error) -> Self {
        ShowError { error: error }
    }

    fn render<'a>(self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>, delta_s: f32) -> (GuiState, bool) {
        enum UiResult {
            Quit,
            FindDevice,
            Waiting,
        };
        let mut result = UiResult::Waiting;
        show_window(ui, im_str!("Device Error"), |framesize| {
            let text = format!("{}", self.error);
            let imtext = ImString::new(text);
            let text_size = ui.calc_text_size(&imtext, false, 0f32);
            let text_pos = ((framesize.0 as f32 - text_size.x) / 2f32, (framesize.1 as f32 - text_size.y) / 2f32);
            ui.set_cursor_pos(text_pos);
            ui.text(imtext);
            let button_pos = (text_pos.0 / 2f32, text_pos.1 + (framesize.1 as f32 - text_pos.1) / 2f32);
            ui.set_cursor_pos(button_pos);
            if ui.small_button(im_str!("Quit")) {
                result = UiResult::Quit;
            }
            let button_pos = (text_pos.0 + text_pos.0 / 2f32, button_pos.1);
            ui.set_cursor_pos(button_pos);
            if ui.small_button(im_str!("Continue")) {
                result = UiResult::FindDevice;
            }
        });
        match result {
            UiResult::Waiting => (self.into(), false),
            UiResult::Quit => (self.into(), true),
            UiResult::FindDevice => (FindingDevice::new().into(), false),
        }
    }
}

impl From<ShowError> for GuiState {
    fn from(s: ShowError) -> GuiState {
        GuiState::ShowError(s)
    }
}

/// Captures all possible states for the GUI
///
/// The GUI operates as a state machine which controls the immediate-mode GUI (which pretends it is
/// stateless, even though it isn't...because it's a gui. Nonetheless, it's a very convenient
/// interface). This enumeration captures the possible states and delegates a single `render`
/// method out to the individual render methods of each state.
///
/// The main purpose of this state machine is to implement the modalness of the GUI so I can do
/// things like stop user input while finding, reading, or writing to the Midi-Fader device.
enum GuiState {
    FindingDevice(FindingDevice),
    Configuring(Configuring),
    WaitingForResponse(WaitingForResponse),
    ShowError(ShowError),
}

impl GuiState {
    fn new() -> Self {
        FindingDevice::new().into()
    }

    fn render<'a>(self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>, delta_s: f32) -> (GuiState, bool) {
        match self {
            GuiState::FindingDevice(s) => s.render(ui, configure_out, delta_s),
            GuiState::Configuring(s) => s.render(ui, configure_out, delta_s),
            GuiState::WaitingForResponse(s) => s.render(ui, configure_out, delta_s),
            GuiState::ShowError(s) => s.render(ui, configure_out, delta_s),
        }
    }
}

/// Runs the GUI
///
/// This takes a Tokio MPSC Sender which is used to make configuration requests to the
/// configuration process (which runs on tokio).
pub fn gui_main(mut configure_out: tokio_mpsc::Sender<ConfigRequest>) {
    use glium::glutin;
    use glium::{Display, Surface};
    use imgui_glium_renderer::Renderer;
    use imgui_winit_support;

    let mut events_loop = glutin::EventsLoop::new();
    let context = glutin::ContextBuilder::new().with_vsync(true);
    let builder = glutin::WindowBuilder::new()
        .with_title("Midi-Fader Control")
        .with_resizable(false)
        .with_dimensions(glutin::dpi::LogicalSize::new(768f64, 320f64));
    let display = Display::new(builder, context, &events_loop).unwrap();
    let window = display.gl_window();

    let mut imgui = ImGui::init();
    imgui.set_ini_filename(None);

    /*{
        let mut style = imgui.style_mut();
        let original_rounding = style.window_rounding;
        style.window_rounding = 0.0;
        style.child_rounding = original_rounding;
    }*/

    let hidpi_factor = window.get_hidpi_factor().round();
    let font_size = (13.0 * hidpi_factor) as f32;

    imgui.fonts().add_default_font_with_config(
        ImFontConfig::new()
            .oversample_h(1)
            .pixel_snap_h(true)
            .size_pixels(font_size)
    );

    imgui.set_font_global_scale((1.0 / hidpi_factor) as f32);

    let mut renderer = Renderer::init(&mut imgui, &display).expect("Failed to initialize renderer");

    imgui_winit_support::configure_keys(&mut imgui);

    let mut last_frame = Instant::now();
    let mut quit = false;

    let mut state = GuiState::new();

    loop {
        events_loop.poll_events(|event| {
            use glium::glutin::WindowEvent::*;
            use glium::glutin::Event;

            imgui_winit_support::handle_event(
                &mut imgui,
                &event,
                window.get_hidpi_factor(),
                hidpi_factor,
            );

            if let Event::WindowEvent { event, .. } = event {
                match event {
                    CloseRequested => quit = true,
                    _ => (),
                }
            }
        });

        let physical_size = window
            .get_inner_size()
            .unwrap()
            .to_physical(window.get_hidpi_factor());
        let logical_size = physical_size.to_logical(hidpi_factor);

        let frame_size = FrameSize {
            logical_size: logical_size.into(),
            hidpi_factor,
        };

        let now = Instant::now();
        let delta = now - last_frame;
        let delta_s = delta.as_secs() as f32 + delta.subsec_nanos() as f32 / 1_000_000_000.0;
        last_frame = now;

        let ui = imgui.frame(frame_size, delta_s);
        let (next_state, inner_quit) = state.render(&ui, &mut configure_out, delta_s);
        state = next_state;
        quit |= inner_quit;

        let mut target = display.draw();
        target.clear_color(
            CLEAR_COLOR[0],
            CLEAR_COLOR[1],
            CLEAR_COLOR[2],
            CLEAR_COLOR[3],
        );
        renderer.render(&mut target, ui).expect("Rendering failed");
        target.finish().unwrap();

        if quit {
            break;
        }
    }
}

