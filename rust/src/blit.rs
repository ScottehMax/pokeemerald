#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]

type U8 = u8;
type U16 = u16;
type U32 = u32;

const NO_COLOR_KEY: U8 = 0xff;

#[repr(C)]
pub struct Bitmap {
    pixels: *mut U8,
    dimensions: U32,
}

impl Bitmap {
    #[inline]
    fn width(&self) -> U32 {
        self.dimensions & 0xffff
    }

    #[inline]
    fn height(&self) -> U32 {
        self.dimensions >> 16
    }
}

#[inline]
#[cfg(test)]
fn packed_dimensions(width: U16, height: U16) -> U32 {
    width as U32 | ((height as U32) << 16)
}

#[inline]
fn tile_width_multiplier(width: U32) -> U32 {
    (width + (width & 7)) >> 3
}

#[inline]
fn pixel_offset_4bpp(x: U32, y: U32, width: U32) -> usize {
    let multiplier_y = tile_width_multiplier(width);
    (((x >> 1) & 3)
        + ((x >> 3) << 5)
        + (((y >> 3) * multiplier_y) << 5)
        + ((y & 7) << 2)) as usize
}

#[inline]
fn pixel_offset_8bpp(x: U32, y: U32, width: U32) -> usize {
    let multiplier_y = tile_width_multiplier(width);
    ((x & 7) + ((x >> 3) << 6) + (((y >> 3) * multiplier_y) << 6) + ((y & 7) << 3))
        as usize
}

#[inline]
fn read_4bpp_pixel(pixels: *const U8, x: U32, y: U32, width: U32) -> U8 {
    let offset = pixel_offset_4bpp(x, y, width);
    // SAFETY: Callers pass a valid bitmap pixel buffer large enough for the
    // computed tiled offset, matching the original C pointer arithmetic.
    let byte = unsafe { *pixels.add(offset) };
    (byte >> ((x & 1) << 2)) & 0xf
}

#[inline]
fn write_4bpp_pixel(pixels: *mut U8, x: U32, y: U32, width: U32, value: U8) {
    let offset = pixel_offset_4bpp(x, y, width);
    let shift = (x & 1) << 2;
    let to_orr = (value & 0xf) << shift;
    let to_and = 0xf0 >> shift;
    // SAFETY: Callers pass a valid bitmap pixel buffer large enough for the
    // computed tiled offset, matching the original C pointer arithmetic.
    unsafe {
        let pixel = pixels.add(offset);
        *pixel = to_orr | (*pixel & to_and);
    }
}

#[inline]
fn write_8bpp_pixel(pixels: *mut U8, x: U32, y: U32, width: U32, value: U8) {
    let offset = pixel_offset_8bpp(x, y, width);
    // SAFETY: Callers pass a valid bitmap pixel buffer large enough for the
    // computed tiled offset, matching the original C pointer arithmetic.
    unsafe {
        *pixels.add(offset) = value;
    }
}

fn blit_bitmap_rect_4bit(
    src: &Bitmap,
    dst: &mut Bitmap,
    src_x: U16,
    src_y: U16,
    dst_x: U16,
    dst_y: U16,
    width: U16,
    height: U16,
    color_key: U8,
) {
    let src_x = src_x as U32;
    let src_y = src_y as U32;
    let dst_x = dst_x as U32;
    let dst_y = dst_y as U32;
    let width = width as U32;
    let height = height as U32;
    let x_end = if dst.width().wrapping_sub(dst_x) < width {
        dst.width().wrapping_sub(dst_x).wrapping_add(src_x)
    } else {
        src_x.wrapping_add(width)
    };
    let y_end = if dst.height().wrapping_sub(dst_y) < height {
        dst.height().wrapping_sub(dst_y).wrapping_add(src_y)
    } else {
        src_y.wrapping_add(height)
    };

    let mut loop_src_y = src_y;
    let mut loop_dst_y = dst_y;
    while loop_src_y < y_end {
        let mut loop_src_x = src_x;
        let mut loop_dst_x = dst_x;
        while loop_src_x < x_end {
            let value = read_4bpp_pixel(
                src.pixels as *const U8,
                loop_src_x,
                loop_src_y,
                src.width(),
            );
            if color_key == NO_COLOR_KEY || value != color_key {
                write_4bpp_pixel(dst.pixels, loop_dst_x, loop_dst_y, dst.width(), value);
            }
            loop_src_x += 1;
            loop_dst_x += 1;
        }
        loop_src_y += 1;
        loop_dst_y += 1;
    }
}

fn fill_bitmap_rect_4bit(
    surface: &mut Bitmap,
    x: U16,
    y: U16,
    width: U16,
    height: U16,
    fill_value: U8,
) {
    let x = x as U32;
    let y = y as U32;
    let mut x_end = x.wrapping_add(width as U32);
    if x_end > surface.width() {
        x_end = surface.width();
    }
    let mut y_end = y.wrapping_add(height as U32);
    if y_end > surface.height() {
        y_end = surface.height();
    }

    let mut loop_y = y;
    while loop_y < y_end {
        let mut loop_x = x;
        while loop_x < x_end {
            write_4bpp_pixel(surface.pixels, loop_x, loop_y, surface.width(), fill_value);
            loop_x += 1;
        }
        loop_y += 1;
    }
}

fn blit_bitmap_rect_4bit_to_8bit(
    src: &Bitmap,
    dst: &mut Bitmap,
    src_x: U16,
    src_y: U16,
    dst_x: U16,
    dst_y: U16,
    width: U16,
    height: U16,
    color_key: U8,
    palette_offset: U8,
) {
    let src_x = src_x as U32;
    let src_y = src_y as U32;
    let dst_x = dst_x as U32;
    let dst_y = dst_y as U32;
    let width = width as U32;
    let height = height as U32;
    let pal_offset_bits = (palette_offset & 0xf) << 4;
    let color_key_bits = (color_key & 0xf) << 4;
    let x_end = if dst.width().wrapping_sub(dst_x) < width {
        dst.width().wrapping_sub(dst_x).wrapping_add(src_x)
    } else {
        src_x.wrapping_add(width)
    };
    let y_end = if dst.height().wrapping_sub(dst_y) < height {
        src_y.wrapping_add(dst.height()).wrapping_sub(dst_y)
    } else {
        src_y.wrapping_add(height)
    };

    let mut loop_src_y = src_y;
    let mut loop_dst_y = dst_y;
    while loop_src_y < y_end {
        let mut loop_src_x = src_x;
        let mut loop_dst_x = dst_x;
        while loop_src_x < x_end {
            let src_offset = pixel_offset_4bpp(loop_src_x, loop_src_y, src.width());
            // SAFETY: Callers pass a valid bitmap pixel buffer large enough
            // for the computed tiled offset, matching the original C pointer
            // arithmetic.
            let src_byte = unsafe { *(src.pixels as *const U8).add(src_offset) };
            let is_odd_pixel = (loop_src_x & 1) != 0;
            let value = if is_odd_pixel {
                src_byte >> 4
            } else {
                src_byte & 0xf
            };
            let should_copy = if color_key == NO_COLOR_KEY {
                true
            } else if is_odd_pixel {
                (src_byte & 0xf0) != color_key_bits
            } else {
                (src_byte & 0xf) != color_key
            };

            if should_copy {
                write_8bpp_pixel(
                    dst.pixels,
                    loop_dst_x,
                    loop_dst_y,
                    dst.width(),
                    pal_offset_bits.wrapping_add(value),
                );
            }
            loop_src_x += 1;
            loop_dst_x += 1;
        }
        loop_src_y += 1;
        loop_dst_y += 1;
    }
}

fn fill_bitmap_rect_8bit(
    surface: &mut Bitmap,
    x: U16,
    y: U16,
    width: U16,
    height: U16,
    fill_value: U8,
) {
    let x = x as U32;
    let y = y as U32;
    let mut x_end = x.wrapping_add(width as U32);
    if x_end > surface.width() {
        x_end = surface.width();
    }
    let mut y_end = y.wrapping_add(height as U32);
    if y_end > surface.height() {
        y_end = surface.height();
    }

    let mut loop_y = y;
    while loop_y < y_end {
        let mut loop_x = x;
        while loop_x < x_end {
            write_8bpp_pixel(surface.pixels, loop_x, loop_y, surface.width(), fill_value);
            loop_x += 1;
        }
        loop_y += 1;
    }
}

#[no_mangle]
pub extern "C" fn BlitBitmapRect4BitWithoutColorKey(
    src: *const Bitmap,
    dst: *mut Bitmap,
    src_x: U16,
    src_y: U16,
    dst_x: U16,
    dst_y: U16,
    width: U16,
    height: U16,
) {
    BlitBitmapRect4Bit(src, dst, src_x, src_y, dst_x, dst_y, width, height, NO_COLOR_KEY);
}

#[no_mangle]
pub extern "C" fn BlitBitmapRect4Bit(
    src: *const Bitmap,
    dst: *mut Bitmap,
    src_x: U16,
    src_y: U16,
    dst_x: U16,
    dst_y: U16,
    width: U16,
    height: U16,
    color_key: U8,
) {
    // SAFETY: The C ABI requires non-null Bitmap pointers whose pixel buffers
    // are valid for the rectangle region, as in the original C implementation.
    unsafe {
        blit_bitmap_rect_4bit(
            &*src, &mut *dst, src_x, src_y, dst_x, dst_y, width, height, color_key,
        );
    }
}

#[no_mangle]
pub extern "C" fn FillBitmapRect4Bit(
    surface: *mut Bitmap,
    x: U16,
    y: U16,
    width: U16,
    height: U16,
    fill_value: U8,
) {
    // SAFETY: The C ABI requires a non-null Bitmap pointer whose pixel buffer
    // is valid for the rectangle region, as in the original C implementation.
    unsafe {
        fill_bitmap_rect_4bit(&mut *surface, x, y, width, height, fill_value);
    }
}

#[no_mangle]
pub extern "C" fn BlitBitmapRect4BitTo8Bit(
    src: *const Bitmap,
    dst: *mut Bitmap,
    src_x: U16,
    src_y: U16,
    dst_x: U16,
    dst_y: U16,
    width: U16,
    height: U16,
    color_key: U8,
    palette_offset: U8,
) {
    // SAFETY: The C ABI requires non-null Bitmap pointers whose pixel buffers
    // are valid for the rectangle region, as in the original C implementation.
    unsafe {
        blit_bitmap_rect_4bit_to_8bit(
            &*src,
            &mut *dst,
            src_x,
            src_y,
            dst_x,
            dst_y,
            width,
            height,
            color_key,
            palette_offset,
        );
    }
}

#[no_mangle]
pub extern "C" fn FillBitmapRect8Bit(
    surface: *mut Bitmap,
    x: U16,
    y: U16,
    width: U16,
    height: U16,
    fill_value: U8,
) {
    // SAFETY: The C ABI requires a non-null Bitmap pointer whose pixel buffer
    // is valid for the rectangle region, as in the original C implementation.
    unsafe {
        fill_bitmap_rect_8bit(&mut *surface, x, y, width, height, fill_value);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bitmap(pixels: &mut [U8], width: U16, height: U16) -> Bitmap {
        Bitmap {
            pixels: pixels.as_mut_ptr(),
            dimensions: packed_dimensions(width, height),
        }
    }

    fn get_4bpp(pixels: &[U8], width: U16, x: U16, y: U16) -> U8 {
        let offset = pixel_offset_4bpp(x as U32, y as U32, width as U32);
        (pixels[offset] >> (((x as U32) & 1) << 2)) & 0xf
    }

    fn set_4bpp(pixels: &mut [U8], width: U16, x: U16, y: U16, value: U8) {
        let offset = pixel_offset_4bpp(x as U32, y as U32, width as U32);
        let shift = ((x as U32) & 1) << 2;
        pixels[offset] = ((value & 0xf) << shift) | (pixels[offset] & (0xf0 >> shift));
    }

    fn get_8bpp(pixels: &[U8], width: U16, x: U16, y: U16) -> U8 {
        pixels[pixel_offset_8bpp(x as U32, y as U32, width as U32)]
    }

    #[test]
    fn bitmap_dimensions_match_c_bitfield_storage() {
        let mut pixels = [0; 1];
        let surface = bitmap(&mut pixels, 0x1234, 0xabcd);

        assert_eq!(surface.width(), 0x1234);
        assert_eq!(surface.height(), 0xabcd);
        let expected_size = {
            let unaligned = core::mem::size_of::<*mut U8>() + core::mem::size_of::<U32>();
            let align = core::mem::align_of::<Bitmap>();
            (unaligned + align - 1) & !(align - 1)
        };
        assert_eq!(core::mem::size_of::<Bitmap>(), expected_size);
    }

    #[test]
    fn tile_offset_helpers_match_4bpp_and_8bpp_layouts() {
        assert_eq!(pixel_offset_4bpp(0, 0, 8), 0);
        assert_eq!(pixel_offset_4bpp(1, 0, 8), 0);
        assert_eq!(pixel_offset_4bpp(7, 0, 8), 3);
        assert_eq!(pixel_offset_4bpp(0, 1, 8), 4);
        assert_eq!(pixel_offset_4bpp(8, 0, 16), 32);
        assert_eq!(pixel_offset_8bpp(0, 0, 8), 0);
        assert_eq!(pixel_offset_8bpp(7, 0, 8), 7);
        assert_eq!(pixel_offset_8bpp(0, 1, 8), 8);
        assert_eq!(pixel_offset_8bpp(8, 0, 16), 64);
    }

    #[test]
    fn fill_bitmap_rect_4bit_sets_only_requested_nibbles_and_clips() {
        let mut pixels = [0xaa; 32];
        let mut surface = bitmap(&mut pixels, 8, 8);

        FillBitmapRect4Bit(&mut surface, 1, 1, 10, 2, 3);

        assert_eq!(get_4bpp(&pixels, 8, 0, 1), 0xa);
        for y in 1..3 {
            for x in 1..8 {
                assert_eq!(get_4bpp(&pixels, 8, x, y), 3);
            }
        }
        assert_eq!(get_4bpp(&pixels, 8, 0, 3), 0xa);
    }

    #[test]
    fn blit_bitmap_rect_4bit_copies_nibbles_without_color_key() {
        let mut src_pixels = [0; 32];
        let mut dst_pixels = [0xee; 32];
        for y in 0..8 {
            for x in 0..8 {
                set_4bpp(&mut src_pixels, 8, x, y, ((y * 8 + x) & 0xf) as U8);
            }
        }
        let src = bitmap(&mut src_pixels, 8, 8);
        let mut dst = bitmap(&mut dst_pixels, 8, 8);

        BlitBitmapRect4BitWithoutColorKey(&src, &mut dst, 2, 1, 3, 4, 3, 2);

        for y in 0..2 {
            for x in 0..3 {
                assert_eq!(
                    get_4bpp(&dst_pixels, 8, 3 + x, 4 + y),
                    get_4bpp(&src_pixels, 8, 2 + x, 1 + y)
                );
            }
        }
        assert_eq!(get_4bpp(&dst_pixels, 8, 2, 4), 0xe);
        assert_eq!(get_4bpp(&dst_pixels, 8, 6, 5), 0xe);
    }

    #[test]
    fn blit_bitmap_rect_4bit_respects_color_key() {
        let mut src_pixels = [0; 32];
        let mut dst_pixels = [0xdd; 32];
        set_4bpp(&mut src_pixels, 8, 0, 0, 1);
        set_4bpp(&mut src_pixels, 8, 1, 0, 2);
        set_4bpp(&mut src_pixels, 8, 2, 0, 1);
        let src = bitmap(&mut src_pixels, 8, 8);
        let mut dst = bitmap(&mut dst_pixels, 8, 8);

        BlitBitmapRect4Bit(&src, &mut dst, 0, 0, 0, 0, 3, 1, 1);

        assert_eq!(get_4bpp(&dst_pixels, 8, 0, 0), 0xd);
        assert_eq!(get_4bpp(&dst_pixels, 8, 1, 0), 2);
        assert_eq!(get_4bpp(&dst_pixels, 8, 2, 0), 0xd);
    }

    #[test]
    fn blit_bitmap_rect_4bit_to_8bit_applies_palette_offset_and_key() {
        let mut src_pixels = [0; 32];
        let mut dst_pixels = [0xcc; 64];
        set_4bpp(&mut src_pixels, 8, 0, 0, 1);
        set_4bpp(&mut src_pixels, 8, 1, 0, 2);
        set_4bpp(&mut src_pixels, 8, 2, 0, 3);
        let src = bitmap(&mut src_pixels, 8, 8);
        let mut dst = bitmap(&mut dst_pixels, 8, 8);

        BlitBitmapRect4BitTo8Bit(&src, &mut dst, 0, 0, 2, 1, 3, 1, 2, 4);

        assert_eq!(get_8bpp(&dst_pixels, 8, 2, 1), 0x41);
        assert_eq!(get_8bpp(&dst_pixels, 8, 3, 1), 0xcc);
        assert_eq!(get_8bpp(&dst_pixels, 8, 4, 1), 0x43);
    }

    #[test]
    fn blit_bitmap_rect_4bit_to_8bit_matches_odd_nibble_color_key_rule() {
        let mut src_pixels = [0; 32];
        let mut dst_pixels = [0xcc; 64];
        set_4bpp(&mut src_pixels, 8, 0, 0, 2);
        set_4bpp(&mut src_pixels, 8, 1, 0, 2);
        let src = bitmap(&mut src_pixels, 8, 8);
        let mut dst = bitmap(&mut dst_pixels, 8, 8);

        BlitBitmapRect4BitTo8Bit(&src, &mut dst, 0, 0, 0, 0, 2, 1, 0x12, 0);

        assert_eq!(get_8bpp(&dst_pixels, 8, 0, 0), 2);
        assert_eq!(get_8bpp(&dst_pixels, 8, 1, 0), 0xcc);
    }

    #[test]
    fn fill_bitmap_rect_8bit_sets_requested_pixels_and_clips() {
        let mut pixels = [0x11; 64];
        let mut surface = bitmap(&mut pixels, 8, 8);

        FillBitmapRect8Bit(&mut surface, 6, 6, 5, 5, 0x7e);

        assert_eq!(get_8bpp(&pixels, 8, 5, 6), 0x11);
        for y in 6..8 {
            for x in 6..8 {
                assert_eq!(get_8bpp(&pixels, 8, x, y), 0x7e);
            }
        }
    }
}
