// The goal of this program is to monitor the audio output of the system.
// If music is playing on the running output, the RMS should get logged
// and send to a socket.
mod audio;
mod connection_manager;

use audio::analyse_samples;
use connection_manager::ConnectionManager;
use itertools::Itertools;
use pipewire::{main_loop::MainLoop, spa::utils::Direction};
use std::iter::Inspect;
use std::sync::{Arc, Mutex};
use std::thread::{self, sleep};
use std::time::{Duration, Instant};

pub const SOCKET_PATH: &str = "/tmp/audio_monitor.sock";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    pipewire::init();

    println!("Init audio monitor. Socket path: {}", SOCKET_PATH);

    let connection_manager = Arc::new(Mutex::new(ConnectionManager::new(SOCKET_PATH)?));
    println!("Socket created, accepting multiple connections...");

    // Optional: spawn a background thread to periodically report client count
    let connection_manager_clone = Arc::clone(&connection_manager);
    thread::spawn(move || {
        let mut last_count = 0;
        loop {
            sleep(Duration::from_secs(10));
            let current_count = connection_manager_clone
                .lock()
                .expect("Failed to lock connection manager")
                .client_count();

            if current_count != last_count {
                println!("Active clients: {}", current_count);
                last_count = current_count;
            }
        }
    });

    let pw_mainloop = MainLoop::new(None)?;
    let pw_context = pipewire::context::Context::new(&pw_mainloop)?;
    let pw_core = pw_context.connect(None)?;

    let pw_stream = pipewire::stream::Stream::new(
        &pw_core,
        "audio-monitor",
        pipewire::properties::properties! {
            "media.class" => "Stream/Input/Audio",
            "node.name" => "AudioMonitor",
            "media.type" => "Audio",
            "media.category" => "Capture",
            "node.target" => "@DEFAULT_SINK@.monitor", // Explicitly target default sink monitor
        },
    )?;

    println!("Setting up stream.process handler");

    let mut last_emit = Instant::now();

    // 🚨 listener must be kept alive
    let _listener = pw_stream
        .add_local_listener()
        .process({
            let connection_manager = Arc::clone(&connection_manager);
            move |stream, _: &mut u16| {
                let now = Instant::now();

                if now - last_emit < Duration::from_millis(16) {
                    return;
                }
                last_emit = now;

                let mut latest_buffer = None;
                while let Some(buffer) = stream.dequeue_buffer() {
                    latest_buffer = Some(buffer);
                }

                if let Some(mut buffer) = latest_buffer {
                    let data = buffer.datas_mut();
                    if data.is_empty() {
                        return;
                    }

                    let valid_size = data[0].chunk().size() as usize;
                    // Grab the first data plane
                    if let Some(bytes) = data[0].data() {
                        // Interpret it as f32 samples
                        let samples = unsafe {
                            std::slice::from_raw_parts(
                                bytes.as_ptr() as *const f32,
                                valid_size / std::mem::size_of::<f32>(),
                            )
                        };

                        if samples.is_empty() {
                            return;
                        }

                        let analyzed = analyse_samples(&samples);
                        let message =
                            format!("{:.1},{}\n", analyzed.rms, analyzed.bands.iter().join(","));

                        println!(
                        "rms: {}| subwoofer {}| subtone {}| kickdrum {}| lowBass {}| bassBody {}| midBass {}| warmth {}| lowMids {}| midsMody {}| upperMids {}| attack {}| highs {} \n",
                        analyzed.rms,
                        analyzed.bands[0],
                        analyzed.bands[1],
                        analyzed.bands[2],
                        analyzed.bands[3],
                        analyzed.bands[4],
                        analyzed.bands[5],
                        analyzed.bands[6],
                        analyzed.bands[7],
                        analyzed.bands[8],
                        analyzed.bands[9],
                        analyzed.bands[10],
                        analyzed.bands[11],
                        );

                        connection_manager
                            .lock()
                            .expect("Failed to unlock connection_manager")
                            .broadcast_message(&message);
                    }
                }
            }
        })
        .register();

    pw_stream.connect(
        Direction::Input,
        None,
        pipewire::stream::StreamFlags::AUTOCONNECT
            | pipewire::stream::StreamFlags::MAP_BUFFERS
            | pipewire::stream::StreamFlags::RT_PROCESS,
        &mut [],
    )?;

    println!("Audio monitor started - monitoring system audio output");
    pw_mainloop.run();
    Ok(())
}
