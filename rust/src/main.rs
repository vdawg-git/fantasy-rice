// The goal of this program is to monitor the audio output of the system.
// If music is playing on the running output, the RMS should get logged
// and send to a socket.
mod audio;

use anyhow::Context;
use audio::analyse_samples;
use pipewire::{main_loop::MainLoop, spa::utils::Direction};
use std::fs::remove_file;
use std::io::Write;
use std::os::unix::net::UnixListener;
use std::sync::{Arc, Mutex};
use std::thread::sleep;
use std::time::Duration;

const SOCKET_PATH: &str = "/tmp/audio_monitor.sock";
const SAMPLE_RATE: u32 = 48000;
const CHANNELS: u32 = 2; // Stereo

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let _ = remove_file(SOCKET_PATH);
    pipewire::init();

    println!("Init audio monitor. Socket path: {}", SOCKET_PATH);

    let unix_socket = UnixListener::bind(SOCKET_PATH)
        .context(format!("Failed to create socket: {}", SOCKET_PATH))?;
    println!("Awaiting Unix connections");
    let unix_stream = Arc::new(Mutex::new(unix_socket.accept()?.0));

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
            let unix_stream = Arc::clone(&unix_stream);
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
                            "{},{},{},{},{},{}\n",
                            analyzed.rms,
                            analyzed.sub_bass,
                            analyzed.bass,
                            analyzed.low_mids,
                            analyzed.mids,
                            analyzed.highs
                        );

                        // println!("Analysis: {:?}", analyzed);

                        let _ = unix_stream
                            .lock()
                            .expect("Failed to unlock unix_stream")
                            .write_all(message.as_bytes());

                        sleep(Duration::from_millis(105));
                    }
                }
            }
        })
        .register();

    // For now, let's try without explicit format parameters
    // PipeWire should negotiate a suitable format automatically

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
