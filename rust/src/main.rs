// The goal of this program is to monitor the audio output of the system.
// If music is playing on the running output, the RMS should get logged
// and send to a socket.

use anyhow::Context;
use pipewire as pw;
use pipewire::{
    main_loop::MainLoop,
    stream::{Stream, StreamFlags},
};
use pw::spa::utils::Direction;
use std::io::Write;
use std::os::unix::net::UnixStream;
use std::sync::{Arc, Mutex};

const SOCKET_PATH: &str = "/tmp/audio_monitor.sock";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    pipewire::init();

    println!("Init audio monitor. Socket path: {}", SOCKET_PATH);

    let socket = UnixStream::connect(SOCKET_PATH).context(format!(
        "Failed to connect to socket: {}\nHave you started the listener?",
        SOCKET_PATH
    ))?;
    let socket = Arc::new(Mutex::new(socket));
    let mainloop = MainLoop::new(None)?;
    let context = pw::context::Context::new(&mainloop)?;
    let core = context.connect(None)?;

    let stream = Stream::new(
        &core,
        "audio-monitor",
        pw::properties::properties! {
            "media.class" => "Stream/Output/Audio",
            "node.name" => "MyMonitorClient",
        },
    )?;

    println!("Setting up stream.process handler");

    // 🚨 listener must be kept alive
    let _listener = stream
        .add_local_listener()
        .process({
            let socket = Arc::clone(&socket);
            move |stream, _: &mut u32| {
                print!("Process");
                while let Some(mut buffer) = stream.dequeue_buffer() {
                    let data = buffer.datas_mut();
                    if data.is_empty() {
                        continue;
                    }
                    // Grab the first data plane
                    if let Some(bytes) = data[0].data() {
                        // Interpret it as f32 samples
                        let samples = unsafe {
                            std::slice::from_raw_parts(
                                bytes.as_ptr() as *const f32,
                                bytes.len() / std::mem::size_of::<f32>(),
                            )
                        };
                        if samples.is_empty() {
                            continue;
                        }
                        let rms = (samples.iter().map(|x| x * x).sum::<f32>()
                            / samples.len() as f32)
                            .sqrt();
                        let msg = format!("{:.4}", rms);
                        println!("Samples: {}, RMS: {:.4}", samples.len(), rms);
                        let _ = socket.lock().unwrap().write_all(msg.as_bytes());
                    }
                }
            }
        })
        .register();

    stream.connect(
        Direction::Input,
        None,
        StreamFlags::AUTOCONNECT | StreamFlags::MAP_BUFFERS | StreamFlags::RT_PROCESS,
        &mut [],
    )?;

    println!("Audio monitor started");

    mainloop.run();
    Ok(())
}
