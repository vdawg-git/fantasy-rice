use rustfft::{FftPlanner, num_complex::Complex};

const SAMPLE_SIZE: usize = 1024; // Number of audio samples per FFT window
const SAMPLE_RATE: usize = 48000; // Audio sample rate in Hz (samples per second)
const MIN_DB: f32 = -60.0;
const MAX_DB: f32 = 0.0;

/// Upper frequency boundaries for each band (in Hz)
const BANDS_HZ: &[&'static f32; 5] = &[
    &20.0,   // sub-bass
    &250.0,  // Bass
    &500.0,  // low-mids
    &2000.0, // Mids
    &6000.0, // highs
];

/// Result of the audio analysis
/// All values are normalized (0.0 to 1.0)
#[derive(Debug)]
pub struct AnalyzedSamples {
    pub rms: f32, // Root Mean Square (loudness)
    pub sub_bass: f32,
    pub bass: f32,
    pub low_mids: f32,
    pub mids: f32,
    pub highs: f32,
}

/// Main function that takes raw audio samples and extracts energy per band + loudness
pub fn analyse_samples(samples: &[f32]) -> AnalyzedSamples {
    let rms = calcualte_rms(samples); // Perceived loudness (overall energy)
    let bands = compute_bands(samples);

    AnalyzedSamples {
        rms,
        sub_bass: bands[0],
        bass: bands[1],
        low_mids: bands[2],
        mids: bands[3],
        highs: bands[4],
    }
}

/// Root Mean Square (RMS) = sqrt(mean(x²))
/// This is a measure of signal power, roughly matching perceived loudness.
fn calcualte_rms(samples: &[f32]) -> f32 {
    (samples.iter().map(|x| x * x).sum::<f32>() / samples.len() as f32).sqrt()
}

/// Hanning window function smooths edges of signal to reduce spectral leakage
/// Formula: w[n] = 0.5 - 0.5 * cos(2πn/N)
fn hanning_window(size: usize) -> Vec<f32> {
    (0..size)
        .map(|i| 0.5 - 0.5 * ((2.0 * std::f32::consts::PI * i as f32) / (size as f32)).cos())
        .collect()
}

/// Converts FFT bin index to frequency in Hz
/// Formula: freq = bin * (sample_rate / window_size)
fn bin_to_freq(bin: usize, sample_rate: usize, size: usize) -> f32 {
    bin as f32 * sample_rate as f32 / size as f32
}

/// Perform FFT and analyze magnitude spectrum into frequency bands
fn compute_bands(samples: &[f32]) -> Vec<f32> {
    let mut planner = FftPlanner::new();
    let fft = planner.plan_fft_forward(SAMPLE_SIZE);
    let window = hanning_window(SAMPLE_SIZE);

    // Apply window to samples and convert to complex numbers (real + 0i)
    let mut buffer: Vec<Complex<f32>> = samples
        .iter()
        .zip(window.iter())
        .map(|(s, w)| Complex::new(s * w, 0.0))
        .collect();

    // Perform the FFT in-place (mutates buffer)
    fft.process(&mut buffer);

    // Extract magnitude (amplitude) of each frequency bin
    // Only take first half due to symmetry of real-input FFT
    let magnitudes: Vec<f32> = buffer
        .iter()
        .take(SAMPLE_SIZE / 2)
        .map(|c| c.norm())
        .collect();

    // Accumulate magnitudes into the defined frequency bands
    let mut band_vals = vec![0.0; BANDS_HZ.len()];
    let mut band_counts = vec![0; BANDS_HZ.len()];

    for (i, mag) in magnitudes.iter().enumerate() {
        let freq = bin_to_freq(i, SAMPLE_RATE, SAMPLE_SIZE);
        for (band, upper) in BANDS_HZ.iter().enumerate() {
            if freq <= **upper {
                band_vals[band] += mag; // Add magnitude to band
                band_counts[band] += 1; // Track how many bins fell into this band
                break;
            }
        }
    }

    // Compute average magnitude per band
    band_vals
        .iter()
        .zip(band_counts.iter())
        .map(
            |(sum, count)| {
                if *count > 0 { sum / *count as f32 } else { 0.0 }
            },
        )
        .collect()
}

/// Takes the magnitude, converts to db, and normalizes to 0..1
/// 0 means -60db or lower. 1 means 0db or higher.
fn normalize_to_db(value: f32) -> f32 {
    let value = value.max(1e-6);
    let db = 20.0 * value.log10();

    ((db + MIN_DB) / (MAX_DB - MIN_DB)).clamp(0.0, 1.0)
}
