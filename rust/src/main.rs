use qoi::{decode_to_vec, encode_to_vec};
use rapid_qoi;
use std::fs::read;
use std::time::Instant;
use turbojpeg;

fn test_jpeg(pixels: &Vec<u8>, width: u32, height: u32) {
    let time1 = Instant::now();
    let encoded = turbojpeg::compress(
        turbojpeg::Image {
            pixels: pixels,
            width: width.try_into().unwrap(),
            pitch: 1920* 4,
            height: height.try_into().unwrap(),
            format: turbojpeg::PixelFormat::BGRA,
        },
        100,
        turbojpeg::Subsamp::Sub2x2,
    )
    .unwrap();
    println!(
        "test_jpeg encode time: {}, size: {}",
        time1.elapsed().as_millis(),
        encoded.len()
    );
}

fn test_rapid_qoi(pixels: &Vec<u8>, width: u32, height: u32) {
    let time1 = Instant::now();
    let encoded = rapid_qoi::Qoi {
        width: width,
        height: height,
        colors: rapid_qoi::Colors::SrgbLinA,
    }
    .encode_alloc(&pixels)
    .unwrap();
    println!(
        "test_rapid_qoi encode time: {}, size: {}",
        time1.elapsed().as_millis(),
        encoded.len()
    );

    let time2 = Instant::now();
    rapid_qoi::Qoi::decode_alloc(&encoded).unwrap();
    println!(
        "test_rapid_qoi decode time: {}",
        time2.elapsed().as_millis()
    );
}

fn test_qoi(pixels: &Vec<u8>, width: u32, height: u32) {
    let time1 = Instant::now();
    let encoded = encode_to_vec(&pixels, width, height).unwrap();
    println!(
        "test_qoi encode time: {}, size: {}",
        time1.elapsed().as_millis(),
        encoded.len()
    );

    let time2 = Instant::now();
    decode_to_vec(&encoded);
    println!("test_qoi decode time: {}", time2.elapsed().as_millis());
}

fn main() {
    // load some jpeg data
    let pixels =
        read("/Users/irony/Workspace/QtTest/TestImageEncode/images/raws/1920/0_1920_1080.rgb")
            .unwrap();
    test_jpeg(&pixels, 1920, 1080);
    test_rapid_qoi(&pixels, 1920, 1080);
    test_qoi(&pixels, 1920, 1080);
    println!("Hello, world!");
}
