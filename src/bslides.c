/*
 * Brian's Slideshow Program
 * Sat Feb 01, 2020 20:26
 *
 * NOTE (brian)
 * All this slideshow program is going to do for now, is render to some images.
 *
 * TODO
 * 1. More color formats than just a Hex String (0xDDDDDD). Like, integers, or
 *    floating point color.
 * 2. Generic function that takes an input buffer, output buffer, dimensions
 *    of both, and desired location on the output. Currently, there are a few
 *    direct-to-image copy functions, and they could probably be baked into
 *    one.
 * 3. Have the concept of a "directive". A directive can be things like
 *    - write line of text, change font, change fontsize, change text color, etc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

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

struct pixel_t {
	u8 r, g, b, a;
};

struct color_t {
	u8 r, g, b, a;
};

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

struct directive_t {
	s32 type;
};

enum {
	  SLIDEJUST_NONE
	, SLIDEJUST_LEFT
	, SLIDEJUST_CENTER
	, SLIDEJUST_RIGHT
	, SLIDEJUST_TOTAL
};

struct slide_t {
	struct image_t *images;
	size_t images_len, images_cap;
	struct string_t *strings;
	size_t strings_len, strings_cap;

	// The array of directives, modifies the (hopefully shallow) state (below)
	// of the slide. The renderer makes a copy of this structure, at render time
	// into the rendering function, and uses that.
	struct directive_t *directives;
	size_t directives_len, directives_cap;

	size_t image_curr;
	size_t string_curr;
	size_t directive_curr;

	struct color_t bg, fg;
	s32 text_just;
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
};

struct show_t {
	struct pixel_t *pixels;
	s32 img_w, img_h; // misleading - w and h of the output framebuffer
	struct slide_t *slides;
	size_t slides_len, slides_cap;
	size_t slide_curr;
	struct font_t *fonts;
	size_t fonts_len, fonts_cap;
	char *name;
	u32 fontsize_curr;
};

/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config);
/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show);

/* f_load : sets up an entry in the font table with these params */
s32 f_load(struct show_t *show, char *path, char *name);
/* f_getcodepoint : retrieves the fchar_t from input font and codepoint */
struct fchar_t *f_getcodepoint(struct show_t *show, char *name, u32 codepoint, u32 fontsize);
/* f_free : frees all fonts associated with the slideshow */
s32 f_free(struct show_t *show);

/* f_vertadvance : returns the desired font vertical advance value */
s32 f_vertadvance(struct font_t *font);

/* slide_render : renders the slide 'idx' into its internal buffer */
int slide_render(struct show_t *show, s32 idx);
/* slide_renderchar : renders a single s32 codepoint to x,y position */
int slide_renderchar(struct slide_t *slide, struct fchar_t *fchar, s32 x, s32 y);
/* slide_renderimage : renders the image to the slide */
int slide_renderimage(struct slide_t *slide, struct image_t *image, s32 x, s32 y);

/* slide_setbg : sets the background (will draw over everything else) */
int slide_setbg(struct slide_t *slide);

/* parse_color : parses a color string into a color structure */
struct color_t parse_color(char *s);

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t);
/* streq : return true if strings are equivalent */
int streq(char *s, char *t);

// NOTE these functions were stolen from Rob Pike
/* regex_match : search for regexp anywhere in text */
int regex_match(char *regexp, char *text);
/* regex_matchhere: search for regexp at beginning of text */
int regex_matchhere(char *regexp, char *text);
/* regex_matchstar: search for c*regexp at beginning of text */
int regex_matchstar(int c, char *regexp, char *text);

/* ltrim : removes whitespace on the "left" (start) of the string */
char *ltrim(char *s);
/* rtrim : removes whitespace on the "right" (end) of the string */
char *rtrim(char *s);

/* c_resize : resizes the ptr should length and capacity be the same */
void c_resize(void *ptr, size_t *len, size_t *cap, size_t bytes);
/* sys_readfile : reads an entire file into a memory buffer */
char *sys_readfile(char *path);

int main(int argc, char **argv)
{
	struct show_t slideshow;
	struct slide_t *slide;
	size_t i;
	int rc;
	char slidename[BUFSMALL];
	char imagename[BUFSMALL];

	memset(slidename, 0, sizeof slidename);
	memset(imagename, 0, sizeof imagename);

	rc = show_load(&slideshow, "show.cfg");
	if (rc < 0) {
		fprintf(stderr, "Couldn't load up the slideshow!\n");
		exit(1);
	}

	// print out all of the slideshow images
	for (i = 0; i < slideshow.slides_len; i++) {
		snprintf(slidename, sizeof slidename, "%s_%04ld", slideshow.name, (long)i);
		snprintf(imagename, sizeof imagename, "%s.png", slidename);

		slide = slideshow.slides + i;

		rc = slide_render(&slideshow, i);
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

/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config)
{
	FILE *fp;
	char *s;
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
	show->fontsize_curr = DEFAULT_FONT_SIZE;

	while (buf == fgets(buf, sizeof buf, fp)) {
		// trim the string to remove whitespace
		s = rtrim(ltrim(buf));

		len = strlen(s);

		if (len == 0) {
			continue;
		}

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

			} else if (streq("font ", tokens[1])) {
				// TODO fix fonts!
				// NOTE we assume the file path is a single
				rc = f_load(show, tokens[3], tokens[2]);
				if (rc < 0) {
					fprintf(stderr, "Couldn't load the font!\n");
					return -1;
				}

			} else if (streq("name", tokens[1])) {
				show->name = strdup(tokens[2]);

			} else if (streq("color", tokens[1])) {
				slide = show->slides + show->slides_len;
				struct color_t *colorptr;

				if (streq("bg", tokens[2])) {
					colorptr = &slide->bg;
				} else if (streq("fg", tokens[2])) {
					colorptr = &slide->fg;
				} else {
					fprintf(stderr, "Unrecognized Color Type'%s'\n", tokens[2]);
				}

				*colorptr = parse_color(tokens[3]);

			} else if (streq("fontsize", tokens[1])) {
				show->fontsize = atoi(tokens[2]);

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

			} else if (streq("justification", tokens[1])) {
				slide = show->slides + show->slides_len;

				if (strcmp(tokens[2], "left") == 0) {
					slide->justification = SLIDEJUST_LEFT;
				} else if (strcmp(tokens[2], "center") == 0) {
					slide->justification = SLIDEJUST_CENTER;
				} else if (strcmp(tokens[2], "right") == 0) {
					slide->justification = SLIDEJUST_RIGHT;
				} else {
					slide->justification = SLIDEJUST_NONE;
				}

			} else if (streq("blank", tokens[1])) {
				slide = show->slides = show->slides_len;
				slide->text_len++;
			}

		} else { // copy a line of text into the slideshow text buffer
			slide = show->slides + show->slides_len;
			assert(slide->text_len != MAX_LINES_ON_SLIDE);
			slide->text[slide->text_len++] = strdup(s);
		}
	}

	show->slides_len++;

	fclose(fp);

	return 0;
}

/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show)
{
	size_t i, j;

	if (!show)
		return -1;

	for (i = 0; i < show->slides_len; i++) {
		for (j = 0; j < show->slides[i].text_len; j++) {
			free(show->slides[i].text[j]);
		}
		for (j = 0; j < show->slides[i].images_len; j++) {
			free(show->slides[i].images[j].pixels);
			free(show->slides[i].images[j].name);
		}
		free(show->slides[i].images);
		free(show->slides[i].pixels);
		free(show->slides[i].text);
	}

	for (i = 0; i < show->font.ftab_len; i++) {
		free(show->font.ftab[i].bitmap);
	}
	free(show->font.ftab);

	free(show->name);
	free(show->slides);

	return 0;
}

/* slide_render : renders the slide 'idx' into its internal buffer */
int slide_render(struct show_t *show, s32 idx)
{
	struct slide_t *slide;
	struct fchar_t *fchar;
	struct image_t *image;
	s64 i, j;
	s32 w_xpos, w_ypos;
	s32 yadv;
	int rc;

	// NOTE (brian) for every single slide in the slideshow, we have to
	// draw every character for every line
	//
	// TODO
	// 1. Handle Justification
	// 2. Handle out of bounds indexing

	slide = show->slides + idx;

	slide_setbg(slide);

	// first, we draw all of our images
	for (i = 0; i < slide->images_len; i++) {
		image = slide->images + i;
		rc = slide_renderimage(slide, image, -1, -1);
	}

	// get this figure from image dimensions
	w_ypos = 64;
	yadv = f_vertadvance(&show->font);

	// then we draw text over it
	// (who, if anyone ever, wants text UNDER their slide? Not me)
	for (i = 0; i < slide->text_len; i++) {
		w_xpos = 32;
		for (j = 0; slide->text[i] && j < strlen(slide->text[i]); j++) {
			if (slide->text[i][j] != ' ') {
				fchar = show->font.ftab + slide->text[i][j];
				slide_renderchar(slide, fchar, w_xpos, w_ypos);
			}

			w_xpos += fchar->advance;
		}
		w_ypos += yadv;
	}

	return 0;
}

/* slide_renderchar : renders a single s32 codepoint to x,y position */
int slide_renderchar(struct slide_t *slide, struct fchar_t *fchar, s32 x, s32 y)
{
	s32 i, j, img_idx, fchar_idx;
	s32 xpos, ypos;
	struct color_t fg, bg;
	f32 alpha;

	// TODO
	// 1. Linear Alpha Blending

	memcpy(&fg, &slide->fg, sizeof fg);

	for (i = 0; i < fchar->f_x; i++) {
		for (j = 0; j < fchar->f_y; j++) {

			// NOTE we only include the x/y offset from the upper left of the
			// buffer when we go to write into the image buffer. This allows
			// us to use the same index into the font alpha render as into
			// the image.

			xpos = (i + x) + fchar->b_x;
			ypos = (j + y) + fchar->b_y;

			if (xpos < 0 || xpos > slide->img_w) {
				continue;
			}
			if (ypos < 0 || ypos > slide->img_h) {
				continue;
			}

			img_idx = xpos + ypos * slide->img_w;
			fchar_idx = i + j * fchar->f_x;

			alpha = fchar->bitmap[fchar_idx] * 1.0f / 255.0f;
			memcpy(&bg, &slide->pixels[img_idx], sizeof bg);

			slide->pixels[img_idx].r = m_lblend_u8(bg.r, fg.r, alpha);
			slide->pixels[img_idx].b = m_lblend_u8(bg.b, fg.b, alpha);
			slide->pixels[img_idx].g = m_lblend_u8(bg.g, fg.g, alpha);
			slide->pixels[img_idx].a = 0xff;
		}
	}

	return 0;
}

/* slide_renderimage : renders the image to the slide */
int slide_renderimage(struct slide_t *slide, struct image_t *image, s32 x, s32 y)
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
	win_w = slide->img_w;
	win_h = slide->img_h;

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
			dst = (i + bound_x) + (j + bound_y) * slide->img_w;
			src = i + j * bound_w;
			slide->pixels[dst].r = iscaled[src].r;
			slide->pixels[dst].g = iscaled[src].g;
			slide->pixels[dst].b = iscaled[src].b;
			slide->pixels[dst].a = iscaled[src].a;
		}
	}

	free(iscaled);

	return 0;
}

/* slide_setbg : sets the background (will draw over everything else) */
int slide_setbg(struct slide_t *slide)
{
	s32 i, j;

	for (j = 0; j < slide->img_h; j++) {
		for (i = 0; i < slide->img_w; i++) {
			slide->pixels[i + j * slide->img_w].r = slide->bg.r;
			slide->pixels[i + j * slide->img_w].g = slide->bg.g;
			slide->pixels[i + j * slide->img_w].b = slide->bg.b;
			slide->pixels[i + j * slide->img_w].a = 0xff;
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

/* f_vertadvance : returns the desired font vertical advance value */
s32 f_vertadvance(struct font_t *font)
{
	s32 i, ret;

	for (i = 0, ret = 0; i < font->ftab_len; i++) {
		if (ret < font->ftab[i].f_y) {
			ret = font->ftab[i].f_y;
		}
	}

	return ret;
}

/* f_getcodepoint : retrieves the fchar_t from input font and codepoint */
struct fchar_t *f_getcodepoint(struct show_t *show, char *name, u32 codepoint, u32 fontsize)
{
	struct font_t *font;
	size_t i;

	// NOTE (brian) don't use this without the font table being initialized

	for (i = 0; i < show->fonts_len; i++) {
		if (streq(show->fonts[i].name, name)) {
			font = show->fonts + i;
		}
	}

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

	bitmap = stbtt_GetCodepointBitmap(&fontinfo, scale_x, scale_y, i, &w, &h, &xoff, &yoff);

	stbtt_GetCodepointHMetrics(&fontinfo, (int)codepoint, &advance, &lsb);

	font->ftab[font->ftab_len].bitmap  = bitmap;
	font->ftab[font->ftab_len].f_x     = w;
	font->ftab[font->ftab_len].f_y     = h;
	font->ftab[font->ftab_len].b_x     = xoff;
	font->ftab[font->ftab_len].b_y     = yoff;
	font->ftab[font->ftab_len].advance = advance * scale_x;

	font->ftab_len++;

	return font->ftab + i;
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

/* regex_match : search for regexp anywhere in text */
int regex_match(char *regexp, char *text)
{
	if (regexp[0] == '^')
		return regex_matchhere(regexp+1, text);
	do {    /* must look even if string is empty */
		if (regex_matchhere(regexp, text))
			return 1;
	} while (*text++ != '\0');
	return 0;
}

/* regex_matchhere: search for regexp at beginning of text */
int regex_matchhere(char *regexp, char *text)
{
	if (regexp[0] == '\0')
		return 1;
	if (regexp[1] == '*')
		return regex_matchstar(regexp[0], regexp+2, text);
	if (regexp[0] == '$' && regexp[1] == '\0')
		return *text == '\0';
	if (*text!='\0' && (regexp[0]=='.' || regexp[0]==*text))
		return regex_matchhere(regexp+1, text+1);
	return 0;
}

/* regex_matchstar: search for c*regexp at beginning of text */
int regex_matchstar(int c, char *regexp, char *text)
{
	do {    /* a * matches zero or more instances */
		if (regex_matchhere(regexp, text))
			return 1;
	} while (*text != '\0' && (*text++ == c || c == '.'));
	return 0;
}

/* ltrim : removes whitespace on the "left" (start) of the string */
char *ltrim(char *s)
{
	while (isspace(*s))
		s++;

	return s;
}

/* rtrim : removes whitespace on the "right" (end) of the string */
char *rtrim(char *s)
{
	char *e;

	for (e = s + strlen(s) - 1; isspace(*e); e--)
		*e = 0;

	return s;
}

/* c_resize : resizes the ptr, should length and capacity be the same */
void c_resize(void *ptr, size_t *len, size_t *cap, size_t bytes)
{
	void **p;

	if (*len == *cap) {
		if (*cap) {
			*cap *= 2;
		} else {
			*cap = BUFSMALL;
		}
		p = (void **)ptr;
		*p = realloc(*p, bytes * *cap);

		// set the rest of the elements to zero
#if 0
		u8 *b;
		size_t i;
		for (b = ((u8 *)*p) + *len * bytes, i = *len; i < *cap; b += bytes, i++) {
			memset(b, 0, bytes);
		}
#else
		memset(((u8 *)*p) + *len * bytes, 0, (*cap - *len) * bytes);
#endif
	}
}

/* sys_readfile : reads an entire file into a memory buffer */
char *sys_readfile(char *path)
{
	FILE *fp;
	s64 size;
	char *buf;

	fp = fopen(path, "rb");
	if (!fp) {
		return NULL;
	}

	// get the file's size
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf = malloc(size + 1);
	memset(buf, 0, size + 1);

	fread(buf, 1, size, fp);
	fclose(fp);

	return buf;
}

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t)
{
	return (u8)((a + t * (b - a)) + 0.5f);
}

/* streq : return true if strings are equivalent */
int streq(char *s, char *t)
{
	return s && t && strcmp(s, t) == 0;
}

