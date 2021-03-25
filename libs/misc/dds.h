#ifndef __DFRANX_DDS_H__
#define __DFRANX_DDS_H__

typedef unsigned int dds_uint;
typedef unsigned char dds_byte;

// dds_header::flags
enum {
	DDSD_CAPS			= 0x1,
	DDSD_HEIGHT			= 0x2,
	DDSD_WIDTH			= 0x4,
	DDSD_PITCH			= 0x8,
	DDSD_PIXELFORMAT	= 0x1000,
	DDSD_MIPMAPCOUNT	= 0x20000,
	DDSD_LINEARSIZE		= 0x80000,
	DDSD_DEPTH			= 0x800000
};

// dds_header::caps
enum {
	DDSCAPS_COMPLEX		= 0x8,
	DDSCAPS_MIPMAP		= 0x400000,
	DDSCAPS_TEXTURE		= 0x1000
};

// dds_header::caps2
enum {
	DDSCAPS2_CUBEMAP			= 0x200,
	DDSCAPS2_CUBEMAP_POSITIVEX	= 0x400,
	DDSCAPS2_CUBEMAP_NEGATIVEX	= 0x800,
	DDSCAPS2_CUBEMAP_POSITIVEY	= 0x1000,
	DDSCAPS2_CUBEMAP_NEGATIVEY	= 0x2000,
	DDSCAPS2_CUBEMAP_POSITIVEZ	= 0x4000,
	DDSCAPS2_CUBEMAP_NEGATIVEZ	= 0x8000,
	DDSCAPS2_VOLUME				= 0x200000
};

// dds_pixelformat::flags
enum {
	DDPF_ALPHAPIXELS	= 0x1,
	DDPF_ALPHA			= 0x2,
	DDPF_FOURCC			= 0x4,
	DDPF_RGB			= 0x40,
	DDPF_YUV			= 0x200,
	DDPF_LUMINANCE		= 0x20000
};

struct dds_pixelformat {
	dds_uint size;
	dds_uint flags;
	dds_uint four_cc;
	dds_uint rgb_bit_count;
	dds_uint r_bit_mask;
	dds_uint g_bit_mask;
	dds_uint b_bit_mask;
	dds_uint a_bit_mask;
};

struct dds_header {
	dds_uint size;
	dds_uint flags;
	dds_uint height;
	dds_uint width;
	dds_uint pitch_linear_size;
	dds_uint depth;
	dds_uint mipmap_count;
	dds_uint reserved1[11];
	struct dds_pixelformat pixel_format;
	dds_uint caps;
	dds_uint caps2;
	dds_uint caps3;
	dds_uint caps4;
	dds_uint reserved2;
};

struct dds_header_dxt10
{
	dds_uint dxgi_format;
	dds_uint resource_dimension;
	dds_uint misc_flag;
	dds_uint array_size;
	dds_uint misc_flags2;
};

struct dds_image {
	struct dds_header header;
	struct dds_header_dxt10 header10;

	dds_byte* pixels; // always RGBA; currently no support for mipmaps
};
typedef struct dds_image* dds_image_t;

dds_image_t dds_load_from_memory(const char* data, long data_length);
dds_image_t dds_load_from_file(const char* filename);
void dds_image_free(dds_image_t image);

#endif // __DFRANX_DDS_H__