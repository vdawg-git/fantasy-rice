use rustfft::num_complex::Complex;

pub struct MelFilterBank {
    filters: Vec<Vec<f32>>, // [mel_band][fft_bin]
}

impl MelFilterBank {
    pub fn new(
        sample_rate: usize,
        fft_size: usize,
        mel_bands: usize,
        min_freq: f32,
        max_freq: f32,
    ) -> Self {
        let min_mel = hz_to_mel(min_freq);
        let max_mel = hz_to_mel(max_freq);

        // Create mel_bands + 2 points to define mel_bands triangular filters
        let mel_points: Vec<f32> = (0..mel_bands + 2)
            .map(|i| {
                let mel = min_mel + (i as f32 / (mel_bands + 1) as f32) * (max_mel - min_mel);
                mel_to_hz(mel)
            })
            .collect();

        // Convert frequencies to FFT bin indices
        let bin = |freq: f32| {
            let bin_idx = ((freq / sample_rate as f32) * fft_size as f32).round() as usize;
            bin_idx.min(fft_size / 2) // Clamp to valid range
        };
        let bin_points: Vec<usize> = mel_points.iter().map(|&f| bin(f)).collect();

        // Initialize filters - only need fft_size/2 + 1 bins for real FFT
        let mut filters = vec![vec![0.0; fft_size / 2 + 1]; mel_bands];

        for i in 0..mel_bands {
            let left = bin_points[i];
            let center = bin_points[i + 1];
            let right = bin_points[i + 2];

            // Skip if points are too close together
            if left >= center || center >= right {
                continue;
            }

            // Rising edge of triangle
            for j in left..center {
                if j < fft_size / 2 + 1 {
                    filters[i][j] = (j - left) as f32 / (center - left) as f32;
                }
            }

            // Falling edge of triangle
            for j in center..right {
                if j < fft_size / 2 + 1 {
                    filters[i][j] = (right - j) as f32 / (right - center) as f32;
                }
            }
        }

        Self { filters }
    }

    pub fn apply(&self, magnitudes: &[f32]) -> Vec<f32> {
        self.filters
            .iter()
            .map(|filter| {
                let energy: f32 = filter
                    .iter()
                    .zip(magnitudes.iter())
                    .map(|(w, m)| w * m)
                    .sum();

                // Convert to log scale (dB) with proper handling of zero/small values
                energy_to_db(energy)
            })
            .collect()
    }
}

/// Convert frequency in Hz to Mel scale
fn hz_to_mel(freq: f32) -> f32 {
    2595.0 * (1.0 + freq / 700.0).log10()
}

/// Convert Mel scale to frequency in Hz
fn mel_to_hz(mel: f32) -> f32 {
    700.0 * (10.0f32.powf(mel / 2595.0) - 1.0)
}

/// Convert energy to decibels with proper normalization
fn energy_to_db(energy: f32) -> f32 {
    // Add small epsilon to avoid log(0)
    let energy_safe = energy.max(1e-10);

    // Convert to dB
    let db = 10.0 * energy_safe.log10();

    // Common range for audio: -80 dB to 0 dB
    let min_db = -80.0;
    let max_db = 0.0;

    // Normalize to [0, 1] range
    ((db - min_db) / (max_db - min_db)).clamp(0.0, 1.0)
}
