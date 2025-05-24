// The goal of this program is to monitor the audio output of the system.
// If music is playing on the running output, the RMS should get logged
// and send to a socket.

mod audio;

use anyhow::Context;
use audio::analyse_samples;
use pipewire as pw;
use pipewire::{
    main_loop::MainLoop,
    stream::{Stream, StreamFlags},
};
use pw::spa::utils::Direction;
use std::fs::remove_file;
use std::io::Write;
use std::os::unix::net::UnixListener;
use std::sync::{Arc, Mutex};

const SOCKET_PATH: &str = "/tmp/audio_monitor.sock";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let _ = remove_file(SOCKET_PATH);

    pipewire::init();

    println!("Init audio monitor. Socket path: {}", SOCKET_PATH);

    let unix_socket = UnixListener::bind(SOCKET_PATH)
        .context(format!("Failed to create  socket: {}", SOCKET_PATH))?;

    println!("Awaiting Unix connections");

    let unix_stream = Arc::new(Mutex::new(unix_socket.accept()?.0));

    let pw_mainloop = MainLoop::new(None)?;
    let pw_context = pw::context::Context::new(&pw_mainloop)?;
    let pw_core = pw_context.connect(None)?;

    let pw_stream = Stream::new(
        &pw_core,
        "audio-monitor",
        pw::properties::properties! {
            "media.class" => "Stream/Output/Audio",
            "node.name" => "MyMonitorClient",
        },
    )?;

    println!("Setting up stream.process handler");

    // 🚨 listener must be kept alive
    let _listener = pw_stream
        .add_local_listener()
        .process({
            let unix_stream = Arc::clone(&unix_stream);
            move |stream, _: &mut u32| {
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
                        };

                        let analyzed = analyse_samples(samples);
                        let message = format!(
                            "{},{},{},{},{},{}\n",
                            analyzed.rms,
                            analyzed.sub_bass,
                            analyzed.bass,
                            analyzed.low_mids,
                            analyzed.mids,
                            analyzed.highs
                        );

                        println!("Samples: {}, RMS: {:?}", samples.len(), analyzed);

                        let _ = unix_stream
                            .lock()
                            .expect("Failed to unlock unix_stream")
                            .write_all(message.as_bytes());
                    }
                }
            }
        })
        .register();

    pw_stream.connect(
        Direction::Input,
        None,
        StreamFlags::AUTOCONNECT | StreamFlags::MAP_BUFFERS | StreamFlags::RT_PROCESS,
        &mut [],
    )?;

    println!("Audio monitor started");
    pw_mainloop.run();

    Ok(())
}
