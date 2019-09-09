# Software

The software for the midi fader is designed to be cross platform between Windows and Linux. With this in mind, it is
written in [Rust][1] and uses [imgui][2] for the user interface. There were many hiccups during development and it took
far far longer to write this than the entire PCB design and firmware development process. Most of this can be attributed
to my inexperience with Rust.

## Design Choices

### GUI

 - imgui is statically compiled into the application. This removes any dependency on external dynamically linked
   libraries.
 - Of the rather limited GUI options in Rust, I felt that imgui was the most developed since it mirrors the C API almost
   exactly.
 - The standard practice of having a special thread for GUI activity is followed, even though imgui is stateless.

### Device Communication

[1]: https://rust-lang.org
[2]: https://github.com/ocornut/imgui

