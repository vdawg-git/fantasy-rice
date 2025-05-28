use rustfft::{FftPlanner, num_complex::Complex};

const SAMPLE_SIZE: usize = 1024; // Number of audio samples per FFT window
const SAMPLE_RATE: usize = 48000; // Audio sample rate in Hz (samples per second)
const MIN_DB: f32 = -40.0;
const MAX_DB: f32 = 0.0;

/// Upper frequency boundaries for each band (in Hz)
const BANDS_HZ: &[&'static f32; 12] = &[
    &30.0,    // subwoofer
    &40.0,    // subtone
    &50.0,    // kickdrum
    &70.0,    // lowBass
    &90.0,    // bassBody
    &110.0,   // midBass
    &250.0,   // warmth
    &400.0,   // lowMids
    &700.0,   // midsMoody
    &1200.0,  // upperMids
    &2500.0,  // attack
    &20000.0, // highs
];

/// Result of the audio analysis
/// All values are normalized (0.0 to 1.0)
#[derive(Debug)]
pub struct AnalyzedSamples {
    pub rms: f32, // Root Mean Square (loudness)
    pub bands: Vec<f32>,
}

/// Main function that takes raw audio samples and extracts energy per band + loudness
pub fn analyse_samples(samples: &[f32]) -> AnalyzedSamples {
    let rms = calcualte_rms(samples); // Perceived loudness (overall energy)
    let bands = compute_bands(samples);

    AnalyzedSamples { rms, bands }
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
        .map(|(sum, count)| {
            if *count > 0 {
                normalize_to_db(sum / *count as f32)
            } else {
                0.0
            }
        })
        .collect()
}

/// Takes the magnitude, converts to db, and normalizes to 0..1
fn normalize_to_db(magnitude: f32) -> f32 {
    // It is not uncommon for a band to have magnitudes over 30
    let max_magnitude = 60.0;
    let value = (magnitude / max_magnitude).max(1e-6);
    let db = 20.0 * value.log10();

    ((db - MIN_DB) / (MAX_DB - MIN_DB)).clamp(0.0, 1.0)
}

// Tracks its values and applies dynamic smoothing
// struct BandMeter {
//     min: f32,
//     max: f32,
//     value: f32,
//     smoothing: f32,
// }

// impl BandMeter {
//     pub fn new(smoothnes: f32) -> Self {
//         BandMeter {
//             min: 0.,
//             max: 0.,
//             value: 0.,
//             smoothing: smoothnes,
//         }
//     }

//     pub fn update(&mut self, newValue: f32) -> f32 {
//         self.min = self.min * (1.0 - self.smoothing) + newValue.min(self.min) * self.smoothing;
//         self.max = self.max * (1.0 - self.smoothing) + newValue.max(self.max) * self.smoothing;
//         self.value = self.value * (1.0 - self.smoothing) + newValue * self.smoothing;

//         let range = (self.max - self.min).max(1e-6);
//     }
// }
