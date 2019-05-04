//! GUI layer
//!
//! This interacts with the configuration layer by means of a Tokio MPSC. The GUI is meant to run
//! on its own dedicated thread so that it is always responsive. It owns the device while it is
//! being configured and the passes it over to the tokio layer when the device needs to be read or
//! written, since those operations use nonblocking I/O.

use tokio::sync::mpsc as tokio_mpsc;
use std::sync::mpsc as std_mpsc;
use std::time::Instant;
use imgui::*;
use device::*;
use config;

const CLEAR_COLOR: [f32; 4] = [1.0, 1.0, 1.0, 1.0];

pub type ConfigRequest = config::Request<Device<MidiFader>>;
pub type ConfigResponse = config::Response<Device<MidiFader>>;

/// Renders a full-window textbox
fn textbox<'a>(ui: &Ui<'a>, title: &'a ImStr, text: &'a ImStr) {
    let framesize = ui.frame_size().logical_size;
    ui.window(title)
        .size((framesize.0 as f32, framesize.1 as f32), ImGuiCond::FirstUseEver)
        .position((0f32, 0f32), ImGuiCond::FirstUseEver)
        .title_bar(false)
        .resizable(false)
        .movable(false)
        .build(|| {
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
    _0: (),
}

impl FindingDevice {
    fn new() -> Self {
        FindingDevice { _0: () }
    }

    fn render<'a>(self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>) -> (GuiState, bool) {
        textbox(ui, im_str!("Finding Device"), im_str!("Finding Device..."));
        match Device::<MidiFader>::enumerate().unwrap().take(1).next() {
            Some(dev) => {
                let (tx, rx) = std_mpsc::channel();
                configure_out
                    .try_send(config::Request::ReadConfiguration(dev.unwrap(), tx));
                (GuiState::WaitingForResponse(WaitingForResponse::new(rx)), false)
            },
            None => (GuiState::FindingDevice(self), false),
        }
    }
}

/// Primary GUI state for when the device is being configured
///
/// This will display configuration controls whose state is cached until the user clicks a save
/// button. At that point, this state will be exited and the WaitingForResponse state will be
/// entered.
struct Configuring {
    dev: config::DeviceConfig<Device<MidiFader>>
}

impl Configuring {
    fn new(device: config::DeviceConfig<Device<MidiFader>>) -> Self {
        Configuring { dev: device }
    }

    fn render<'a>(self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>) -> (GuiState, bool) {
        (GuiState::Configuring(self), false)
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
}

impl WaitingForResponse {
    fn new(receiver: std_mpsc::Receiver<ConfigResponse>) -> Self {
        WaitingForResponse { receiver: receiver }
    }

    fn render<'a>(self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>) -> (GuiState, bool) {
        textbox(ui, im_str!("Waiting"), im_str!("Waiting for Device..."));
        match self.receiver.try_recv() {
            Ok(r) => {
                match r {
                    config::Response::Configured(d) => (GuiState::Configuring(Configuring::new(d)), false),
                    config::Response::Error(e) => unimplemented!(),
                    config::Response::Easy => unimplemented!(),
                }
            },
            Err(std_mpsc::TryRecvError::Empty) => (GuiState::WaitingForResponse(self), false),
            Err(std_mpsc::TryRecvError::Disconnected) => {
                // We have crashed
                unimplemented!()
            }
        }
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
}

impl GuiState {
    fn new() -> Self {
        GuiState::FindingDevice(FindingDevice::new())
    }

    fn render<'a>(self, ui: &Ui<'a>, configure_out: &mut tokio_mpsc::Sender<ConfigRequest>) -> (GuiState, bool) {
        match self {
            GuiState::FindingDevice(s) => s.render(ui, configure_out),
            GuiState::Configuring(s) => s.render(ui, configure_out),
            GuiState::WaitingForResponse(s) => s.render(ui, configure_out),
            _ => (self, false),
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

    let mut last_frame = Instant::now();
    let mut quit = false;

    let mut state = GuiState::new();

    loop {
        events_loop.poll_events(|event| {
            use glium::glutin::WindowEvent::*;
            use glium::glutin::Event;

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
        let (next_state, inner_quit) = state.render(&ui, &mut configure_out);
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

