/*
 * Brian's Slideshow Program
 * Sat Feb 01, 2020 20:26
 *
 * TODO
 * 0. Include the "regular" common single header lib
 * 1. Convert all of the slideshow directives into single C functions, in a DLL
 * 2. Open an SDL-ish Window and render the slideshow in real time
 * 3. Convert rendering into a programmable function situation
 * 4. Create a DLL loader, and read exported functions as viable candidate. Use the real name of the
 *    function as the name in the slideshow file to use
 *
 * COMMANDS (Completed)
 *
 * COMMANDS (Incompleted)
 *   newslide
 *   printline
 *   blank
 *   image
 *   font
 *   fontset
 *   fontsize
 *   maketemplate
 *   templateset
 *   usetemplate
 *   justified
 *   defer
 *
 * BUGS
 * - using a font that isn't aliased crashes the program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

#define COMMON_IMPLEMENTATION
#include "common.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define DEFAULT_WIDTH      (1024)
#define DEFAULT_HEIGHT     (768)
#define DEFAULT_NAME       ("bslides")
#define DEFAULT_FONTSIZE   (15)
#define DEFAULT_COLORBG    ("0x3366cc")
#define DEFAULT_COLORFG    ("0xffcccc")
#define DEFAULT_FONT_SIZE  (32)
#define MAX_LINES_ON_SLIDE (32)

#define MAX_FUNCTIONS (64)

struct pixel_t {
	u8 r, g, b, a;
};

struct color_t {
	u8 r, g, b, a;
};
typedef struct color_t color_t;

// NOTE
// currently, the slideshow program attempts to center the image and will
// overlay images one at a time. Ideally, you'll eventually be able to
// tell the image where it should go
struct image_t {
	struct pixel_t *pixels;
	s32 img_w, img_h;
	char *name;
};

struct string_t {
	char *text;
};

enum {
	  DIRECT_NONE
	, DIRECT_TEXT
	, DIRECT_JUSTIFICATION
	, DIRECT_TOTAL
};

enum {
	  SLIDEJUST_NONE
	, SLIDEJUST_LEFT
	, SLIDEJUST_CENTER
	, SLIDEJUST_RIGHT
	, SLIDEJUST_TOTAL
};

struct fchar_t {
	u8 *bitmap;
	u32 codepoint;
	u32 fontsize;
	s32 f_x; // font size (in pixels)
	s32 f_y;
	s32 b_x; // bearing information
	s32 b_y;
	u32 advance;
};

struct font_t {
	char *name;
	char *path;
	char *ttfbuffer;
	struct fchar_t *ftab;
	size_t ftab_len, ftab_cap;
	f32 scale_x, scale_y;
	s32 ascent;
	s32 descent;
	s32 linegap;
	s32 metricsread;
};

struct command_t {
	int argc;
	char **argv;
};

struct slide_t {
	struct image_t *images;
	size_t images_len, images_cap;
	size_t image_curr;
	struct string_t *strings;
	size_t strings_len, strings_cap;
	size_t string_curr;
	char *fontname;
	s32 fontsize;
};

struct template_t {
	char *id;
	s32 justification;
	color_t fg, bg;
};

struct settings_t {
	s32 fontsize;
	char *fontname;
	s32 slide;
};

struct show_t;

typedef int (showfunc_t) (struct show_t *, int, char **);

struct function_t {
	char *name;
	showfunc_t *func;
};

struct show_t {
	struct pixel_t *fbuffer_bg, *fbuffer_fg;
	struct pixel_t *pixels;
	s32 img_w, img_h; // misleading - w and h of the output framebuffer
	struct slide_t *slides;
	size_t slides_len, slides_cap;
	struct template_t *templates;
	size_t templates_len, templates_cap;
	struct function_t functions[MAX_FUNCTIONS];
	size_t functions_len;
	struct font_t *fonts;
	size_t fonts_len, fonts_cap;
	char *name;
	char *fontname;
	u32 fontsize;
	struct color_t bg, fg;
	struct settings_t defaults;
	struct settings_t settings;
};

// Slideshow Init & Free Functions
/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config);
/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show);

// Slideshow Rendering Functions
/* show_render : renders the slide 'idx' into its internal buffer */
int show_render(struct show_t *show, size_t idx);
/* show_renderchar : renders a single s32 codepoint to x,y position */
int show_renderchar(struct show_t *show, struct fchar_t *fchar, color_t color, s32 x, s32 y);
/* show_renderimage : renders the image to the slide */
int show_renderimage(struct show_t *slide, struct image_t *image, s32 x, s32 y);

// Font Functions
/* f_load : sets up an entry in the font table with these params */
s32 f_load(struct show_t *show, char *path, char *name);
/* f_getfont : returns a pointer to the font structure with the matching name */
struct font_t *f_getfont(struct show_t *show, char *name);
/* f_getcodepoint : retrieves the fchar_t from input font and codepoint */
struct fchar_t *f_getcodepoint(struct show_t *show, char *name, u32 codepoint, u32 fontsize);
/* f_free : frees all fonts associated with the slideshow */
s32 f_free(struct show_t *show);
/* f_vertadvance : returns the font's vertical advance */
s32 f_vertadvance(struct font_t *font);

/* show_setbg : sets the background (will draw over everything else) */
int show_setbg(struct show_t *show, color_t color);

/* s_getfontname : returns the font name, using the show's as a default */
char *s_getfontname(struct show_t *show, struct slide_t *slide);
/* s_getfontsize : returns the font name, using the show's as a default */
s32 s_getfontsize(struct show_t *show, struct slide_t *slide);

/* parse_color : parses a color string into a color structure */
struct color_t parse_color(char *s);

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t);

int main(int argc, char **argv)
{
	struct show_t slideshow;
	size_t i;
	int rc;
	char slidename[BUFSMALL];
	char imagename[BUFSMALL];

	if (argc < 2) {
		fprintf(stderr, "USAGE : %s config\n", argv[0]);
		exit(1);
	}

	memset(slidename, 0, sizeof slidename);
	memset(imagename, 0, sizeof imagename);

	rc = show_load(&slideshow, argv[1]);
	if (rc < 0) {
		fprintf(stderr, "Couldn't load up the slideshow!\n");
		exit(1);
	}

	// hook up the default functions
	functab_add(&slideshow, "blank", func_blank);

	// print out all of the slideshow images
	for (i = 0; i < slideshow.slides_len; i++) {
		snprintf(slidename, sizeof slidename, "%s_%04ld", slideshow.name, (long)i);
		snprintf(imagename, sizeof imagename, "%s.png", slidename);

		printf("%s\n", imagename);

		rc = show_render(&slideshow, i);
		if (rc < 0) {
			fprintf(stderr, "Couldn't render '%s' to the image!\n", slidename);
			exit(1);
		}

		rc = stbi_write_png(imagename, slideshow.img_w, slideshow.img_h, sizeof(struct pixel_t), slideshow.pixels, sizeof(struct pixel_t) * slideshow.img_w);
		if (!rc) {
			fprintf(stderr, "Couldn't write %s!\n", imagename);
			exit(1);
		}
	}

	rc = show_free(&slideshow);
	if (rc < 0) {
		fprintf(stderr, "Couldn't free the slideshow!\n");
		exit(1);
	}

	return 0;
}

/* func_blank : implements the "blank" function */
int func_blank(struct show_t *show, int argc, char **argv)
{
	assert(show);

	return 0;
}

/* functab_add : adds a callable function into the show */
int functab_add(struct show_t *show, char *name, showfunc_t func)
{
	assert(show);
	assert(show->functions_len < MAX_FUNCTIONS);

	show->functions[show->functions_len++].name = strdup(name);
	show->functions[show->functions_len++].func = func;

	return 0;
}

/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config)
{
	FILE *fp;
	char *s, *t;
	struct slide_t *slide;
	struct image_t *image;
	s32 len;
	int rc, i;
	char *tokens[BUFSMALL];
	char buf[BUFLARGE];

	// TODO
	// this would probably be better with tokenized strings

	memset(show, 0, sizeof(*show));
	memset(tokens, 0, sizeof tokens);

	fp = fopen(config, "r");

	if (!fp) {
		return -1;
	}

	// set some default parameters
	show->fontsize = DEFAULT_FONT_SIZE;
	show->img_w = DEFAULT_WIDTH;
	show->img_h = DEFAULT_HEIGHT;

	while (buf == fgets(buf, sizeof buf, fp)) {
		// trim the string to remove whitespace
		s = rtrim(ltrim(buf));

		len = strlen(s);

		if (len == 0) {
			continue;
		}

		t = strdup(buf);

		for (i = 0, s = strtok(buf, " "); i < ARRSIZE(tokens) && s; i++, s = strtok(NULL, " ")) {
			tokens[i] = s;
		}

		if (streq("//", tokens[0]) || streq("#", tokens[0])) {
			continue;
		}

		if (streq(":", tokens[0])) { // parse a command/directive

			if (streq("newslide", tokens[1])) {
				if (show->slides_cap != 0) {
					show->slides_len++;
				}

				c_resize(&show->slides, &show->slides_len, &show->slides_cap, sizeof(struct slide_t));
				slide = show->slides + show->slides_len;
				c_resize(&slide->strings, &slide->strings_len, &slide->strings_cap, sizeof(*slide->strings));

			} else if (streq("font", tokens[1])) {
				rc = f_load(show, tokens[3], tokens[2]);
				if (rc < 0) {
					fprintf(stderr, "Couldn't load the font!\n");
					return -1;
				}

			} else if (streq("name", tokens[1])) {
				show->name = strdup(tokens[2]);

			} else if (streq("color", tokens[1])) {
				struct color_t *colorptr;

				if (streq("bg", tokens[2])) {
					colorptr = &show->bg;
				} else if (streq("fg", tokens[2])) {
					colorptr = &show->fg;
				} else {
					fprintf(stderr, "Unrecognized Color Type'%s'\n", tokens[2]);
				}

				*colorptr = parse_color(tokens[3]);

			} else if (streq("fontsize", tokens[1])) {
				if (show->slides_len) {
					slide->fontsize = atoi(tokens[2]);
				} else {
					show->fontsize = atoi(tokens[2]);
				}


			} else if (streq("fontname", tokens[1])) {
				if (show->slides_len) {
					if (slide->fontname)
						free(slide->fontname);
					slide->fontname = strdup(tokens[2]);
				} else {
					show->fontname = strdup(tokens[2]);
				}

			} else if (streq("image", tokens[1])) { // attempt to load the image
				slide = show->slides + show->slides_len;

				c_resize(&slide->images, &slide->images_len, &slide->images_cap, sizeof(struct image_t));
				image = slide->images + slide->images_len++;

				image->pixels = (struct pixel_t *)stbi_load(tokens[2], &image->img_w, &image->img_h, NULL, 4);
				if (!image->pixels) {
					fprintf(stderr, "Couldn't load image file '%s'\n", tokens[2]);
					return -1;
				}
				image->name = strdup(tokens[2]);

			} else if (streq("imagesize", tokens[1])) {
				show->img_w = atoi(tokens[2]);
				show->img_h = atoi(tokens[3]);

			} else if (streq("blank", tokens[1])) {
				slide = show->slides + show->slides_len;
				slide->strings_len++;
			}

			free(t);

		} else { // copy a line of text into the slideshow text buffer
			slide = show->slides + show->slides_len;
			assert(slide->strings_len != MAX_LINES_ON_SLIDE);
			slide->strings[slide->strings_len++].text = t;
		}

	}

	fclose(fp);

	// final setup
	show->slides_len++;
	show->pixels = calloc(show->img_w * show->img_h, sizeof(struct pixel_t));

	return 0;
}

/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show)
{
	size_t i, j;

	if (!show)
		return -1;

	for (i = 0; i < show->slides_len; i++) {
		for (j = 0; j < show->slides[i].strings_len; j++) {
			free(show->slides[i].strings[j].text);
		}
		for (j = 0; j < show->slides[i].images_len; j++) {
			free(show->slides[i].images[j].pixels);
			free(show->slides[i].images[j].name);
		}
		free(show->slides[i].images);
		free(show->slides[i].strings);
	}

	for (i = 0; i < show->fonts_len; i++) {
		for (j = 0; j < show->fonts[i].ftab_len; j++) {
			free(show->fonts[i].ftab[j].bitmap);
		}
		free(show->fonts[i].ftab);
	}
	free(show->fonts);

	free(show->pixels);

	free(show->name);
	free(show->slides);

	return 0;
}

/* show_render : renders the slide 'idx' into its internal buffer */
int show_render(struct show_t *show, size_t idx)
{
	struct slide_t *slide;
	struct fchar_t *fchar;
	s64 i, j;
	s32 w_xpos, w_ypos;
	int rc;

	char *fontname;
	s32 fontsize;

	// NOTE (brian) for every single slide in the slideshow, we have to
	// draw every character for every line
	//
	// TODO
	// 1. Handle Justification
	// 2. Handle out of bounds indexing

	slide = show->slides + idx;

	fontname = s_getfontname(show, slide);
	fontsize = s_getfontsize(show, slide);

	show_setbg(show, show->bg);

	// first, we draw all of our images
	for (i = 0; i < slide->images_len; i++) {
		rc = show_renderimage(show, slide->images + i, -1, -1);
	}

	// get this figure from image dimensions
	w_ypos = show->img_h / 12;

	// then we draw text over it, no text under slides
	for (i = 0; i < slide->strings_len; i++) {
		w_xpos = show->img_w / 12;
		for (j = 0; slide->strings[i].text && j < strlen(slide->strings[i].text); j++) {
			if (slide->strings[i].text[j] != ' ') {
				fchar = f_getcodepoint(show, fontname, slide->strings[i].text[j], fontsize);
				show_renderchar(show, fchar, show->fg, w_xpos, w_ypos);
			}
			w_xpos += fchar->advance;
		}
		s32 t;
		t = f_vertadvance(f_getfont(show, fontname));
		w_ypos += t;
	}

	return 0;
}

/* show_renderchar : renders a single s32 codepoint to x,y position */
int show_renderchar(struct show_t *show, struct fchar_t *fchar, color_t color, s32 x, s32 y)
{
	s32 i, j, img_idx, fchar_idx;
	s32 xpos, ypos;
	struct color_t c;
	f32 alpha;

	for (i = 0; i < fchar->f_x; i++) {
		for (j = 0; j < fchar->f_y; j++) {

			// NOTE we only include the x/y offset from the upper left of the
			// buffer when we go to write into the image buffer. This allows
			// us to use the same index into the font alpha render as into
			// the image.

			xpos = (i + x) + fchar->b_x;
			ypos = (j + y) + fchar->b_y;

			if (xpos < 0 || xpos > show->img_w) {
				continue;
			}
			if (ypos < 0 || ypos > show->img_h) {
				continue;
			}

			img_idx = xpos + ypos * show->img_w;
			fchar_idx = i + j * fchar->f_x;

			alpha = fchar->bitmap[fchar_idx] * 1.0f / 255.0f;
			memcpy(&c, &show->pixels[img_idx], sizeof c);

			show->pixels[img_idx].r = m_lblend_u8(c.r, color.r, alpha);
			show->pixels[img_idx].b = m_lblend_u8(c.b, color.b, alpha);
			show->pixels[img_idx].g = m_lblend_u8(c.g, color.g, alpha);
			show->pixels[img_idx].a = 0xff;
		}
	}

	return 0;
}

/* show_renderimage : renders the image to the slide */
int show_renderimage(struct show_t *show, struct image_t *image, s32 x, s32 y)
{
	struct pixel_t *iscaled; // image scaled
	s32 bound_x, bound_y, bound_w, bound_h;
	s32 img_w, img_h, win_w, win_h;
	s32 i, j, dst, src;
	f32 scale;
	int rc;

	// NOTE (brian)
	// Before we even attempt to render the image to the window, we have to
	// determine where in X and Y we can actually fit the image onto the screen.
	//
	// We do this by determining the dimensions it has to fit in, then calling
	// stbir_resize_uint8 on those dimensions, then copying it to the image.

	img_w = image->img_w;
	img_h = image->img_h;
	win_w = show->img_w;
	win_h = show->img_h;

	scale = 1.0f;

	if (win_w < img_w) {
		scale = (f32)win_w / img_w;
	}

	if (win_h < img_h) {
		scale = (f32)win_h / img_h;
	}

	img_w *= scale;
	img_h *= scale;

	if (img_w <= win_w && img_h <= win_h) {
		// copy over parameters if we can fit in the window
		bound_w = img_w;
		bound_h = img_h;
		bound_x = (win_w - img_w) / 2;
		bound_y = (win_h - img_h) / 2;
	} else if (win_h < img_h) {
		// move the picture to be centered in x
		bound_w = (int)roundf(((f32)img_w * win_h) / img_h);
		bound_h = win_h;
		bound_x = (win_w - bound_w) / 2;
		bound_y = 0;
	} else if (win_w < img_w) {
		// move the picture to be centered in y
		bound_w = win_w;
		bound_h = (int)roundf(((f32)win_h * img_w) / win_w);
		bound_x = 0;
		bound_y = (win_w - bound_h) / 2;
	}

	// before we can put images on the framebuffer, we have to scale them
	iscaled = calloc(bound_w * bound_h, sizeof(struct pixel_t));
	rc = stbir_resize_uint8((const u8 *)image->pixels, image->img_w, image->img_h, 0, (u8 *)iscaled, bound_w, bound_h, 0, 4);
	if (!rc) { // success
		fprintf(stderr, "Couldn't scale image '%s'\n", image->name);
		return -1;
	}

	// now we can put the image onto the canvas
	for (i = 0; i < bound_w; i++) {
		for (j = 0; j < bound_h; j++) {
			dst = (i + bound_x) + (j + bound_y) * show->img_w;
			src = i + j * bound_w;
			show->pixels[dst].r = iscaled[src].r;
			show->pixels[dst].g = iscaled[src].g;
			show->pixels[dst].b = iscaled[src].b;
			show->pixels[dst].a = iscaled[src].a;
		}
	}

	free(iscaled);

	return 0;
}

/* show_setbg : sets the background (will draw over everything else) */
int show_setbg(struct show_t *show, color_t color)
{
	s32 i, j;

	for (j = 0; j < show->img_h; j++) {
		for (i = 0; i < show->img_w; i++) {
			show->pixels[i + j * show->img_w].r = color.r;
			show->pixels[i + j * show->img_w].g = color.g;
			show->pixels[i + j * show->img_w].b = color.b;
			show->pixels[i + j * show->img_w].a = 0xff;
		}
	}

	return 0;
}

/* parse_color : parses a color string into a color structure */
struct color_t parse_color(char *s)
{
	struct color_t color;
	u32 r, g, b;

	memset(&color, 0, sizeof color);

	sscanf(s, "0x%2x%2x%2x", &r, &g, &b);

	color.r = r;
	color.g = g;
	color.b = b;

	return color;
}

/* f_load : sets up an entry in the font table with these params */
s32 f_load(struct show_t *show, char *path, char *name)
{
	c_resize(&show->fonts, &show->fonts_len, &show->fonts_cap, sizeof(*show->fonts));

	show->fonts[show->fonts_len].ttfbuffer = sys_readfile(path);

	if (!show->fonts[show->fonts_len].ttfbuffer) {
		return -1;
	}

	show->fonts[show->fonts_len].name = strdup(name);
	show->fonts[show->fonts_len].path = strdup(path);

	show->fonts_len++;

	return 0;
}

/* f_getcodepoint : retrieves the fchar_t from input font and codepoint */
struct fchar_t *f_getcodepoint(struct show_t *show, char *name, u32 codepoint, u32 fontsize)
{
	struct font_t *font;
	size_t i;

	font = f_getfont(show, name);
	if (!font) {
		return NULL;
	}

	// TODO (brian) make searching the font in the table, not terribly slow
	// for large amounts

	for (i = 0; i < font->ftab_len; i++) {
		if (font->ftab[i].codepoint == codepoint && font->ftab[i].fontsize == fontsize) {
			return font->ftab + i;
		}
	}

	// NOTE (brian) if we get here, we didn't find the codepoint, so we
	// have to render a new one

	c_resize(&font->ftab, &font->ftab_len, &font->ftab_cap, sizeof(*font->ftab));

	stbtt_fontinfo fontinfo;

	f32 scale_x, scale_y;
	s32 w, h, xoff, yoff, advance, lsb;
	u8 *bitmap;

	stbtt_InitFont(&fontinfo, (unsigned char *)font->ttfbuffer, stbtt_GetFontOffsetForIndex((unsigned char *)font->ttfbuffer, 0));
	scale_y = stbtt_ScaleForPixelHeight(&fontinfo, fontsize);
	scale_x = scale_y;

	bitmap = stbtt_GetCodepointBitmap(&fontinfo, scale_x, scale_y, codepoint, &w, &h, &xoff, &yoff);

	stbtt_GetCodepointHMetrics(&fontinfo, (int)codepoint, &advance, &lsb);

	// if this is the first time through here, we can get our 
	if (!font->metricsread) {
		stbtt_GetFontVMetrics(&fontinfo, &font->ascent, &font->descent, &font->linegap);
		font->scale_x = scale_x;
		font->scale_y = scale_y;
		font->ascent *= scale_y;
		font->descent *= scale_y;
		font->linegap *= scale_y;
		font->metricsread = true;
	}

	font->ftab[font->ftab_len].bitmap  = bitmap;
	font->ftab[font->ftab_len].f_x     = w;
	font->ftab[font->ftab_len].f_y     = h;
	font->ftab[font->ftab_len].b_x     = xoff;
	font->ftab[font->ftab_len].b_y     = yoff;
	font->ftab[font->ftab_len].advance = advance * scale_x;

	font->ftab_len++;

	return font->ftab + i;
}

/* f_getfont : returns a pointer to the font structure with the matching name */
struct font_t *f_getfont(struct show_t *show, char *name)
{
	struct font_t *font;
	size_t i;

	// NOTE (brian) don't use this without the font table being initialized

	for (i = 0, font = NULL; i < show->fonts_len; i++) {
		if (streq(show->fonts[i].name, name)) {
			font = show->fonts + i;
		}
	}

	return font;
}

/* f_vertadvance : returns the font's vertical advance */
s32 f_vertadvance(struct font_t *font)
{
	return font->ascent - font->descent + font->linegap;
}

/* f_fontfree : frees all resources associated with the font */
int f_fontfree(struct font_t *font)
{
	size_t i;

	if (font) {
		for (i = 0; i < font->ftab_len; i++) {
			if (font->ftab[i].bitmap) {
				free(font->ftab[i].bitmap);
			}
		}
	}

	return 0;
}

/* s_getfontname : returns the font name, using the show's as a default */
char *s_getfontname(struct show_t *show, struct slide_t *slide)
{
	if (slide && slide->fontname) {
		return slide->fontname;
	}

	return show->fontname;
}

/* s_getfontsize : returns the font name, using the show's as a default */
s32 s_getfontsize(struct show_t *show, struct slide_t *slide)
{
	if (slide && slide->fontsize) {
		return slide->fontsize;
	}

	return show->fontsize;
}

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t)
{
	return (u8)((a + t * (b - a)) + 0.5f);
}

