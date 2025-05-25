// The goal of this program is to monitor the audio output of the system.
// If music is playing on the running output, the RMS should get logged
// and send to a socket.
mod audio;
mod connection_manager;

use audio::analyse_samples;
use connection_manager::ConnectionManager;
use pipewire::{main_loop::MainLoop, spa::utils::Direction};
use std::sync::{Arc, Mutex};
use std::thread::{self, sleep};
use std::time::Duration;

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

    // 🚨 listener must be kept alive
    let _listener = pw_stream
        .add_local_listener()
        .process({
            let connection_manager = Arc::clone(&connection_manager);
            move |stream, _: &mut u16| {
                while let Some(mut buffer) = stream.dequeue_buffer() {
                    let data = buffer.datas_mut();
                    if data.is_empty() {
                        continue;
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
                            continue;
                        }

                        // Debug: Check if we're getting actual audio data
                        let non_zero_count = samples.iter().filter(|&&x| x.abs() > 0.0001).count();
                        let max_sample = samples.iter().fold(0.0f32, |a, &b| a.max(b.abs()));

                        if non_zero_count > 0 {
                            println!(
                                "Got {} samples, {} non-zero, max: {:.2}",
                                samples.len(),
                                non_zero_count,
                                max_sample
                            );
                            // println!("{:?}", samples);
                        } else {
                            println!(
                                "All samples are effectively zero (got {} samples)",
                                samples.len()
                            );
                            continue; // Skip processing if all samples are zero
                        }

                        let analyzed = analyse_samples(&samples);
                        let message = format!(
                            "{:.1},{:.1},{:.1},{:.1},{:.1},{:.1}\n",
                            analyzed.rms,
                            analyzed.sub_bass,
                            analyzed.bass,
                            analyzed.low_mids,
                            analyzed.mids,
                            analyzed.highs
                        );

                        // println!("Analysis: {:?}", analyzed);

                        connection_manager
                            .lock()
                            .expect("Failed to unlock connection_manager")
                            .broadcast_message(&message);

                        sleep(Duration::from_millis(15));
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
