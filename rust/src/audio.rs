use rustfft::{FftPlanner, num_complex::Complex};

const SAMPLE_SIZE: usize = 1024;
const SAMPLE_RATE: usize = 48000;
const BANDS_HZ: &[&'static f32; 5] = &[
    &20.0,   // bass
    &250.0,  // low-mids
    &500.0,  // mids
    &2000.0, // Upper mids
    &6000.0, // Highs
];

/// All range from 0-1
pub struct AnalyzedSamples {
    rms: f32,
    bass: f32,
    low_mids: f32,
    mids: f32,
    high_mids: f32,
    highs: f32,
}

pub fn analyseSamples(samples: &[f32]) -> AnalyzedSamples {
    let rms = calcualte_rms(samples);

    AnalyzedSamples {
        rms,
        bass: todo!(),
        low_mids: todo!(),
        mids: todo!(),
        high_mids: todo!(),
        highs: todo!(),
    }
}

fn calcualte_rms(samples: &[f32]) -> f32 {
    (samples.iter().map(|x| x * x).sum::<f32>() / samples.len() as f32).sqrt()
}

fn hanning_window(size: usize) -> Vec<f32> {
    (0..size)
        .map(|i| 0.5 - 0.5 * ((2.0 * std::f32::consts::PI * i as f32) / (size as f32)).cos())
        .collect()
}

fn bin_to_freq(bin: usize, sample_rate: usize, size: usize) -> f32 {
    bin as f32 * sample_rate as f32 / size as f32
}

fn compute_bands(samples: &[f32]) -> Vec<f32> {
    let mut planner = FftPlanner::new();
    let fft = planner.plan_fft_forward(SAMPLE_SIZE);
    let window = hanning_window(SAMPLE_SIZE);

    let mut buffer: Vec<Complex<f32>> = samples
        .iter()
        .zip(window.iter())
        .map(|(s, w)| Complex::new(s * w, 0.0))
        .collect();

    fft.process(&mut buffer);

    let magnitudes: Vec<f32> = buffer
        .iter()
        .take(SAMPLE_SIZE / 2)
        .map(|c| c.norm())
        .collect();

    let mut band_vals = vec![0.0; BANDS_HZ.len()];
    let mut band_counts = vec![0; BANDS_HZ.len()];

    for (i, mag) in magnitudes.iter().enumerate() {
        let freq = bin_to_freq(i, SAMPLE_RATE, SAMPLE_SIZE);
        for (b, upper) in BANDS_HZ.iter().enumerate() {
            if freq <= **upper {
                band_vals[b] += mag;
                band_counts[b] += 1;
                break;
            }
        }
    }

    band_vals
        .iter()
        .zip(band_counts.iter())
        .map(|(sum, count)| if *count > 0 { sum / *count as f32 } else { 0.0 })
        .collect()
}

fn normalize_db(db: f32) -> f32 {
    ((db + 60.0) / 60.0).clamp(0.0, 1.0)
}
