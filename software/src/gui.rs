//! GUI layer
//!

use std::sync::mpsc::{Sender, Receiver};
use std::time::Instant;
use imgui::*;
use device::*;
use config;

const CLEAR_COLOR: [f32; 4] = [1.0, 1.0, 1.0, 1.0];

struct FindingDevice {
    _0: (),
}

impl FindingDevice {
    fn new() -> Self {
        FindingDevice { _0: () }
    }

    fn render<'a>(self, ui: &Ui<'a>) -> (GuiState, bool) {
        let framesize = ui.frame_size().logical_size;
        ui.window(im_str!("Finding Device"))
            .size((framesize.0 as f32, framesize.1 as f32), ImGuiCond::FirstUseEver)
            .position((0f32, 0f32), ImGuiCond::FirstUseEver)
            .title_bar(false)
            .resizable(false)
            .movable(false)
            .build(|| {
                let text = im_str!("Finding Device...");
                let text_size = ui.calc_text_size(text, false, 0f32);
                let text_pos = ((framesize.0 as f32 - text_size.x) / 2f32, (framesize.1 as f32 - text_size.y) / 2f32);
                ui.set_cursor_pos(text_pos);
                ui.text(text);
            });
        match Device::<MidiFader>::enumerate().unwrap().take(1).next() {
            Some(dev) => (GuiState::Configuring(Configuring::new(dev.unwrap())), false),
            None => (GuiState::FindingDevice(self), false),
        }
    }
}

struct Configuring {
    dev: Option<Device<MidiFader>>,
}

impl Configuring {
    fn new(device: Device<MidiFader>) -> Self {
        Configuring { dev: Some(device) }
    }

    fn render<'a>(self, ui: &Ui<'a>) -> (GuiState, bool) {
        (GuiState::Configuring(self), false)
    }
}

enum GuiState {
    FindingDevice(FindingDevice),
    Configuring(Configuring),
}

impl GuiState {
    fn new() -> Self {
        GuiState::FindingDevice(FindingDevice::new())
    }

    fn render<'a>(self, ui: &Ui<'a>) -> (GuiState, bool) {
        match self {
            GuiState::FindingDevice(d) => d.render(ui),
            _ => (self, false),
        }
    }
}

pub type ConfigRequest = config::Request<Device<MidiFader>>;
pub type ConfigResponse = config::Response<Device<MidiFader>>;

pub fn gui_main(configure_out: Sender<ConfigRequest>, configure_in: Receiver<ConfigResponse>) {
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
        let (next_state, inner_quit) = state.render(&ui);
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

