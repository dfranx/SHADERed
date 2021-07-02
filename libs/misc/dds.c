#include <misc/dds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOURCC(str) (dds_uint)(((str)[3] << 24U) | ((str)[2] << 16U) | ((str)[1] << 8U) | (str)[0])
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define IMAGE_PITCH(width, block_size) MAX(1, ((width + 3) / 4)) * block_size

dds_uint dds_calculate_left_shift(dds_uint* right_shift, dds_uint bit_count)
{
	if (bit_count >= 8) {
		right_shift = right_shift + (bit_count - 8);
		return 0;
	}
	return 8 - bit_count;
}

void dds_parse_uncompressed(dds_image_t image, const char* data, long data_length)
{
	dds_uint img_width = MAX(1, image->header.width);
	dds_uint img_height = MAX(1, image->header.height);
	dds_uint img_depth = MAX(1, image->header.depth);

	// will this even work? have I envisioned this solution correctly?
	dds_uint r_right_shift = 0xFFFFFFFF, g_right_shift = 0xFFFFFFFF, b_right_shift = 0xFFFFFFFF, a_right_shift = 0xFFFFFFFF;
	dds_uint r_left_shift = 0, g_left_shift = 0, b_left_shift = 0, a_left_shift = 0;
	for (dds_byte i = 0; i < 32; i++) {
		if ((image->header.pixel_format.r_bit_mask) >> i & 1) {
			if (r_right_shift == 0xFFFFFFFF)
				r_right_shift = i;
			r_left_shift++;			
		}
		if ((image->header.pixel_format.g_bit_mask) >> i & 1) {
			if (g_right_shift == 0xFFFFFFFF)
				g_right_shift = i;
			g_left_shift++;
		}
		if ((image->header.pixel_format.b_bit_mask) >> i & 1) {
			if (b_right_shift == 0xFFFFFFFF)
				b_right_shift = i;
			b_left_shift++;
		}
		if ((image->header.pixel_format.a_bit_mask) >> i & 1) {
			if (a_right_shift == 0xFFFFFFFF)
				a_right_shift = i;
			a_left_shift++;
		}
	}

	// to avoid undefined behavior:
	if (r_right_shift == 0xFFFFFFFF) r_right_shift = 0;
	if (g_right_shift == 0xFFFFFFFF) g_right_shift = 0;
	if (b_right_shift == 0xFFFFFFFF) b_right_shift = 0;
	if (a_right_shift == 0xFFFFFFFF) a_right_shift = 0;

	// fix left/right shift based on the bit count (currently stored in X_left_shift)
	r_left_shift = dds_calculate_left_shift(&r_right_shift, r_left_shift);
	g_left_shift = dds_calculate_left_shift(&g_right_shift, g_left_shift);
	b_left_shift = dds_calculate_left_shift(&b_right_shift, b_left_shift);
	a_left_shift = dds_calculate_left_shift(&a_right_shift, a_left_shift);

	dds_byte bytes_per_pixel = image->header.pixel_format.rgb_bit_count / 8;

	data += 128; // Skip the header

	// read the actual data
	for (dds_uint z = 0; z < img_depth; z++) 
		for (dds_uint x = 0; x < img_width; x++)
			for (dds_uint y = 0; y < img_height; y++) {
				dds_uint px_index = (z * img_width * img_height + y * img_width + x) * bytes_per_pixel;
				dds_uint data_index = (z * img_width * img_height + (img_height-y-1) * img_width + x) * 4;

				// get the data into uint
				dds_uint px = 0;
				memcpy(&px, data + px_index, bytes_per_pixel);

				// decode
				image->pixels[data_index + 0] = ((px & image->header.pixel_format.r_bit_mask) >> r_right_shift) << r_left_shift;
				image->pixels[data_index + 1] = ((px & image->header.pixel_format.g_bit_mask) >> g_right_shift) << g_left_shift;
				image->pixels[data_index + 2] = ((px & image->header.pixel_format.b_bit_mask) >> b_right_shift) << b_left_shift;
				if (image->header.pixel_format.a_bit_mask == 0)
					image->pixels[data_index + 3] = 0xFF;
				else
					image->pixels[data_index + 3] = ((px & image->header.pixel_format.a_bit_mask) >> a_right_shift) << a_left_shift;
			}
}
void dds_parse_dxt1(dds_image_t image, const char* data, long data_length)
{
	dds_uint img_width = MAX(1, image->header.width);
	dds_uint img_height = MAX(1, image->header.height);
	dds_uint img_depth = MAX(1, image->header.depth);

	dds_uint blocks_x = MAX(1, img_width / 4);
	dds_uint blocks_y = MAX(1, img_height / 4);

	for (dds_uint z = 0; z < img_depth; z++) {
		for (dds_uint x = 0; x < blocks_x; x++)
			for (dds_uint y = 0; y < blocks_y; y++) {
				unsigned short color0 = 0, color1 = 0;
				dds_uint codes = 0;

				// read the block data
				dds_uint block_offset = (y * blocks_x + x) * 8; // * 8 == * 64bits cuz blocks are 64 bits (2x shorts, 1x uint)
				memcpy(&color0, data + block_offset + 0, 2);
				memcpy(&color1, data + block_offset + 2, 2);
				memcpy(&codes, data + block_offset + 4, 4);

				// unpack the color data
				dds_byte r0 = (color0 & 0b1111100000000000) >> 8;
				dds_byte g0 = (color0 & 0b0000011111100000) >> 3;
				dds_byte b0 = (color0 & 0b0000000000011111) << 3;
				dds_byte r1 = (color1 & 0b1111100000000000) >> 8;
				dds_byte g1 = (color1 & 0b0000011111100000) >> 3;
				dds_byte b1 = (color1 & 0b0000000000011111) << 3;

				// process the data
				for (dds_uint b = 0; b < 16; b++) {
					dds_uint px_index = ((z * 4) * img_height * img_width + (img_height - ((y * 4) + b / 4) - 1) * img_width + x * 4 + b % 4) * 4;

					dds_byte code = (codes >> (2 * b)) & 3;
					image->pixels[px_index + 3] = 0xFF;
					if (code == 0) {
						// color0
						image->pixels[px_index + 0] = r0;
						image->pixels[px_index + 1] = g0;
						image->pixels[px_index + 2] = b0;
					} else if (code == 1) {
						// color1
						image->pixels[px_index + 0] = r1;
						image->pixels[px_index + 1] = g1;
						image->pixels[px_index + 2] = b1;
					} else if (code == 2) {
						// (2*color0 + color1) / 3
						if (color0 > color1) {
							image->pixels[px_index + 0] = (2 * r0 + r1) / 3;
							image->pixels[px_index + 1] = (2 * g0 + g1) / 3;
							image->pixels[px_index + 2] = (2 * b0 + b1) / 3;
						}
						// (color0 + color1) / 2
						else {
							image->pixels[px_index + 0] = (r0 + r1) / 2;
							image->pixels[px_index + 1] = (g0 + g1) / 2;
							image->pixels[px_index + 2] = (b0 + b1) / 2;
						}
					} else if (code == 3) {
						// (color0 + 2*color1) / 3
						if (color0 > color1) {
							image->pixels[px_index + 0] = (r0 + 2 * r1) / 3;
							image->pixels[px_index + 1] = (g0 + 2 * g1) / 3;
							image->pixels[px_index + 2] = (b0 + 2 * b1) / 3;
						}
						// black
						else {
							image->pixels[px_index + 0] = 0x00;
							image->pixels[px_index + 1] = 0x00;
							image->pixels[px_index + 2] = 0x00;
						}
					}
				}
			}

		// skip this slice
		data += blocks_x * blocks_y * 16;
	}
}
void dds_parse_dxt3(dds_image_t image, const char* data, long data_length)
{
	dds_uint img_width = MAX(1, image->header.width);
	dds_uint img_height = MAX(1, image->header.height);
	dds_uint img_depth = MAX(1, image->header.depth);

	dds_uint blocks_x = MAX(1, img_width / 4);
	dds_uint blocks_y = MAX(1, img_height / 4);

	for (dds_uint z = 0; z < img_depth; z++) {
		for (dds_uint x = 0; x < blocks_x; x++)
			for (dds_uint y = 0; y < blocks_y; y++) {
				unsigned short color0 = 0, color1 = 0;
				dds_uint codes = 0;
				unsigned long long alpha_data = 0;

				// read the block data
				dds_uint block_offset = (y * blocks_x + x) * 16; // 16 == 128bits
				memcpy(&alpha_data, data + block_offset, 8);
				memcpy(&color0, data + block_offset + 8, 2);
				memcpy(&color1, data + block_offset + 10, 2);
				memcpy(&codes, data + block_offset + 12, 4);

				// unpack the color data
				dds_byte r0 = (color0 & 0b1111100000000000) >> 8;
				dds_byte g0 = (color0 & 0b0000011111100000) >> 3;
				dds_byte b0 = (color0 & 0b0000000000011111) << 3;
				dds_byte r1 = (color1 & 0b1111100000000000) >> 8;
				dds_byte g1 = (color1 & 0b0000011111100000) >> 3;
				dds_byte b1 = (color1 & 0b0000000000011111) << 3;

				// process the data
				for (dds_uint b = 0; b < 16; b++) {
					dds_uint px_index = ((z * 4) * img_height * img_width + (img_height - ((y * 4) + b / 4) - 1) * img_width + x * 4 + b % 4) * 4;
					dds_byte code = (codes >> (2 * b)) & 0b0011;

					dds_byte alpha = (alpha_data >> (4 * b)) & 0b1111;
					image->pixels[px_index + 3] = alpha;

					// color0
					if (code == 0) {
						image->pixels[px_index + 0] = r0;
						image->pixels[px_index + 1] = g0;
						image->pixels[px_index + 2] = b0;
					}
					// color1
					else if (code == 1) {
						image->pixels[px_index + 0] = r1;
						image->pixels[px_index + 1] = g1;
						image->pixels[px_index + 2] = b1;
					}
					// (2*color0 + color1) / 3
					else if (code == 2) {
						image->pixels[px_index + 0] = (2 * r0 + r1) / 3;
						image->pixels[px_index + 1] = (2 * g0 + g1) / 3;
						image->pixels[px_index + 2] = (2 * b0 + b1) / 3;
					}
					// (color0 + 2*color1) / 3
					else if (code == 3) {
						image->pixels[px_index + 0] = (r0 + 2 * r1) / 3;
						image->pixels[px_index + 1] = (g0 + 2 * g1) / 3;
						image->pixels[px_index + 2] = (b0 + 2 * b1) / 3;
					}
				}
			}

		// skip this slice
		data += blocks_x * blocks_y * 16;
	}
}
void dds_parse_dxt5(dds_image_t image, const char* data, long data_length)
{

	dds_uint img_width = MAX(1, image->header.width);
	dds_uint img_height = MAX(1, image->header.height);
	dds_uint img_depth = MAX(1, image->header.depth);

	dds_uint blocks_x = MAX(1, img_width / 4);
	dds_uint blocks_y = MAX(1, img_height / 4);

	for (dds_uint z = 0; z < img_depth; z++) {
		for (dds_uint x = 0; x < blocks_x; x++)
			for (dds_uint y = 0; y < blocks_y; y++) {
				unsigned short color0 = 0, color1 = 0;
				dds_uint codes = 0;
				unsigned long long alpha_codes = 0;
				dds_byte alpha0 = 0, alpha1 = 0;

				// read the block data
				dds_uint block_offset = (y * blocks_x + x) * 16; // 16 == 128bits
				alpha0 = data[block_offset + 0];
				alpha1 = data[block_offset + 1];
				memcpy(&alpha_codes, data + block_offset + 2, 6);
				memcpy(&color0, data + block_offset + 8, 2);
				memcpy(&color1, data + block_offset + 10, 2);
				memcpy(&codes, data + block_offset + 12, 4);

				// unpack the color data
				dds_byte r0 = (color0 & 0b1111100000000000) >> 8;
				dds_byte g0 = (color0 & 0b0000011111100000) >> 3;
				dds_byte b0 = (color0 & 0b0000000000011111) << 3;
				dds_byte r1 = (color1 & 0b1111100000000000) >> 8;
				dds_byte g1 = (color1 & 0b0000011111100000) >> 3;
				dds_byte b1 = (color1 & 0b0000000000011111) << 3;

				// process the data
				for (dds_uint b = 0; b < 16; b++) {
					dds_uint px_index = (z * img_height * img_width + (img_height - ((y * 4) + b / 4) - 1) * img_width + x * 4 + b % 4) * 4;
					dds_byte code = (codes >> (2 * b)) & 0b0011;
					dds_byte alpha_code = (alpha_codes >> (3 * b)) & 0b0111;

					/* COLOR */
					// color0
					if (code == 0) {
						image->pixels[px_index + 0] = r0;
						image->pixels[px_index + 1] = g0;
						image->pixels[px_index + 2] = b0;
					}
					// color1
					else if (code == 1) {
						image->pixels[px_index + 0] = r1;
						image->pixels[px_index + 1] = g1;
						image->pixels[px_index + 2] = b1;
					}
					// (2*color0 + color1) / 3
					else if (code == 2) {
						image->pixels[px_index + 0] = (2 * r0 + r1) / 3;
						image->pixels[px_index + 1] = (2 * g0 + g1) / 3;
						image->pixels[px_index + 2] = (2 * b0 + b1) / 3;
					}
					// (color0 + 2*color1) / 3
					else if (code == 3) {
						image->pixels[px_index + 0] = (r0 + 2 * r1) / 3;
						image->pixels[px_index + 1] = (g0 + 2 * g1) / 3;
						image->pixels[px_index + 2] = (b0 + 2 * b1) / 3;
					}

					/* ALPHA */
					dds_byte alpha = 0xFF;
					if (code == 0)
						alpha = alpha0;
					else if (code == 1)
						alpha = alpha1;
					else {
						if (alpha0 > alpha1)
							alpha = ((8 - alpha_code) * alpha0 + (alpha_code - 1) * alpha1) / 7;
						else {
							if (alpha_code == 6)
								alpha = 0;
							else if (alpha_code == 7)
								alpha = 255;
							else
								alpha = ((6 - alpha_code) * alpha0 + (alpha_code - 1) * alpha1) / 5;
						}
					}
					image->pixels[px_index + 3] = alpha;
				}
			}
	
		// skip this slice
		data += blocks_x * blocks_y * 16;
	}
}

dds_image_t dds_load_from_memory(const char* data, long data_length)
{
	const char* data_loc = data;

	dds_uint magic = 0x00;
	memcpy(&magic, data_loc, sizeof(magic));
	data_loc += 4;

	if (magic != 0x20534444) // 'DDS '
		return NULL;

	dds_image_t ret = (dds_image_t)malloc(sizeof(struct dds_image));


	// read the header
	memcpy(&ret->header, data_loc, sizeof(struct dds_header));
	data_loc += sizeof(struct dds_header);

	// check if the dds_header::dwSize (must be equal to 124)
	if (ret->header.size != 124) {
		dds_image_free(ret);
		return NULL;
	}

	// check the dds_header::flags (DDSD_CAPS, DDSD_HEIGHT, DDSD_WIDTH, DDSD_PIXELFORMAT must be set)
	if (!((ret->header.flags & DDSD_CAPS) && (ret->header.flags & DDSD_HEIGHT) && (ret->header.flags & DDSD_WIDTH) && (ret->header.flags & DDSD_PIXELFORMAT))) {
		dds_image_free(ret);
		return NULL;
	}

	// check the dds_header::caps
	if ((ret->header.caps & DDSCAPS_TEXTURE) == 0) {
		dds_image_free(ret);
		return NULL;
	}

	// check if we need to load dds_header_dxt10
	if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DX10")) {
		// read the header10
		memcpy(&ret->header10, data_loc, sizeof(struct dds_header_dxt10));
		data_loc += sizeof(struct dds_header_dxt10);
	}

	// allocate pixel data
	dds_uint img_width = MAX(1, ret->header.width);
	dds_uint img_height = MAX(1, ret->header.height);
	dds_uint img_depth = MAX(1, ret->header.depth);
	ret->pixels = (dds_byte*)malloc(img_width * img_height * img_depth * 4);

	// parse/decompress the pixel data
	if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DXT1"))
		dds_parse_dxt1(ret, data_loc, data_length - (data_loc - data));
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DXT2"))
		dds_parse_dxt3(ret, data_loc, data_length - (data_loc - data));
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DXT3"))
		dds_parse_dxt3(ret, data_loc, data_length - (data_loc - data));
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DXT4"))
		dds_parse_dxt5(ret, data_loc, data_length - (data_loc - data));
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DXT5"))
		dds_parse_dxt5(ret, data_loc, data_length - (data_loc - data));
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("DX10")) {
		// dds_parse_dxt10
	}
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("ATI1")) {
		// dds_parse_ati1
	}
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("ATI2")) {
		// dds_parse_ati2
	}
	else if ((ret->header.pixel_format.flags & DDPF_FOURCC) && ret->header.pixel_format.four_cc == FOURCC("A2XY")) {
		// dds_parse_a2xy
	}
	else
		dds_parse_uncompressed(ret, data, data_length - (data_loc - data));

	return ret;
}
dds_image_t dds_load_from_file(const char* filename)
{
	// open the file
	FILE* f = fopen(filename, "rb");
	if (f == NULL)
		return NULL;

	// get file size
	fseek(f, 0, SEEK_END);
	long file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (file_size < 128) {
		fclose(f);
		return NULL;
	}

	// read the whole file
	char* dds_data = (char*)malloc(file_size);
	fread(dds_data, 1, file_size, f);

	// parse the data
	dds_image_t ret = dds_load_from_memory(dds_data, file_size);

	// clean up
	free(dds_data);
	fclose(f);

	return ret;
}

void dds_image_free(dds_image_t image)
{
	free(image->pixels);
	free(image);
}
