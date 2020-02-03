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
#define MAX_LINES_ON_SLIDE (32)

struct pixel_t {
	u8 r, g, b, a;
};

struct color_t {
	u8 r, g, b, a;
};

struct slide_t {
	struct pixel_t *pixels;
	s32 img_w, img_h;
	char **text;
	size_t text_len, text_cap;
	s32 justification;
	struct color_t bg, fg;
};

enum {
	  FONTT_NONE
	, FONTT_REG
	, FONTT_ITAL
	, FONTT_BOLD
	, FONTT_TOTAL
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
	s32 f_x; // font size (in pixels)
	s32 f_y;
	s32 b_x; // bearing information
	s32 b_y;
	u32 advance;
};

struct font_t {
	int type;
	char *path;
	struct fchar_t *ftab;
	size_t ftab_len, ftab_cap;
};

struct show_t {
	struct slide_t *slides;
	size_t slides_len, slides_cap;
	size_t slide_curr;
	struct font_t font;
	s32 fontsize;
	char *name;
};

/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config);
/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show);
/* f_fontload : load as many ascii characters into the font table as possible */
int f_fontload(struct font_t *font, char *path, s32 fontsize);
/* f_fontfree : frees all resources associated with the font */
int f_fontfree(struct font_t *font);
/* f_vertadvance : returns the desired font vertical advance value */
s32 f_vertadvance(struct font_t *font);

/* slide_render : renders the slide 'idx' into its internal buffer */
int slide_render(struct show_t *show, s32 idx);
/* slide_renderchar : renders a single s32 codepoint to x,y position */
int slide_renderchar(struct slide_t *slide, struct fchar_t *fchar, s32 x, s32 y);

/* slide_setbg : sets the background (will draw over everything else) */
int slide_setbg(struct slide_t *slide);

/* parse_color : parses a color string into a color structure */
struct color_t parse_color(char *s);

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t);

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
	size_t i, j;
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
		snprintf(slidename, sizeof slidename, "%s_%04I64d", slideshow.name, i);
		snprintf(imagename, sizeof imagename, "%s.png", slidename);

		slide = slideshow.slides + i;

		rc = slide_render(&slideshow, i);
		if (rc < 0) {
			fprintf(stderr, "Couldn't render '%s' to the image!\n", slidename);
			exit(1);
		}

		rc = stbi_write_png(imagename, slide->img_w, slide->img_h, sizeof(struct pixel_t), slide->pixels, sizeof(struct pixel_t) * slide->img_w);
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
	s32 i, j, len;
	struct slide_t *slide;
	struct color_t color;
	char buf[BUFLARGE];
	int rc;

	// TODO
	// this would probably be better with tokenized strings

	memset(show, 0, sizeof(*show));

	fp = fopen(config, "r");

	if (!fp) {
		return -1;
	}

	while (buf == fgets(buf, sizeof buf, fp)) {
		// trim the string to remove whitespace
		s = rtrim(ltrim(buf));

		len = strlen(s);

		if (len == 0) {
			continue;
		}

		if (regex_match("^//", buf) || regex_match("^#", buf)) {
			continue;
		}

		// now, we check to see if the string that we have matches
		// a regular expression
		if (regex_match("^: newslide$", s)) {
			if (show->slides_cap != 0) {
				show->slides_len++;
			}

			c_resize(&show->slides, &show->slides_len, &show->slides_cap, sizeof(struct slide_t));

			slide = show->slides + show->slides_len;

			slide->img_w = DEFAULT_WIDTH;
			slide->img_h = DEFAULT_HEIGHT;
			slide->text_cap = MAX_LINES_ON_SLIDE;
			slide->text_len = 0;
			slide->text = calloc(slide->text_cap, sizeof(char *));
			slide->pixels = calloc(slide->img_w * slide->img_h, sizeof(*slide->pixels));

		} else if (regex_match("^: font ", s)) {
			// NOTE we assume the file path is a single
			s = ltrim(s + strlen(": font"));
			rc = f_fontload(&show->font, s, show->fontsize ? show->fontsize : DEFAULT_FONTSIZE);
			if (rc < 0) {
				fprintf(stderr, "Couldn't load the font!\n");
				return -1;
			}

		} else if (regex_match("^: name", s)) {
			s = ltrim(s + strlen(": name"));
			show->name = strdup(s);

		} else if (regex_match("^: fontsize", s)) {
			s = ltrim(s + strlen(": fontsize"));
			show->fontsize = atoi(s);

		} else if (regex_match("^: justified", s)) {
			s = ltrim(s + strlen(": justified"));

			slide = show->slides + show->slides_len;

			if (strcmp(s, "left") == 0) {
				slide->justification = SLIDEJUST_LEFT;
			} else if (strcmp(s, "center") == 0) {
				slide->justification = SLIDEJUST_CENTER;
			} else if (strcmp(s, "right") == 0) {
				slide->justification = SLIDEJUST_RIGHT;
			} else {
				slide->justification = SLIDEJUST_NONE;
			}

		} else if (regex_match("^: blank", s)) {
			slide->text_len++;

		} else if (regex_match("^: color", s)) {
			slide = show->slides + show->slides_len;
			s = ltrim(s + strlen(": color"));

			if (s[0] == 'b' && s[1] == 'g') {
				s += 2;
				s = ltrim(strchr(s, ' '));
				slide->bg = parse_color(s);

			} else if (s[0] == 'f' && s[1] == 'g') {
				s += 2;
				s = ltrim(strchr(s, ' '));
				slide->fg = parse_color(s);

			} else {
				fprintf(stderr, "Unrecognized Color '%s'\n", s);
			}

		} else {
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
			free(show->slides[i].pixels);
		}
		free(show->slides[i].pixels);
		free(show->slides[i].text);
	}

	free(show->name);
	free(show->slides);

	return 0;
}

/* slide_render : renders the slide 'idx' into its internal buffer */
int slide_render(struct show_t *show, s32 idx)
{
	struct slide_t *slide;
	struct fchar_t *fchar;
	s64 i, j;
	s32 w_xpos, w_ypos;
	s32 yadv;

	// NOTE (brian) for every single slide in the slideshow, we have to
	// draw every character for every line
	//
	// TODO
	// 1. Handle Justification
	// 2. Handle out of bounds indexing

	slide = show->slides + idx;

	slide_setbg(slide);

	// get this figure from image dimensions
	w_ypos = 64;
	yadv = f_vertadvance(&show->font);

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
	memcpy(&bg, &slide->bg, sizeof bg);

	for (i = 0; i < fchar->f_x; i++) {
		for (j = 0; j < fchar->f_y; j++) {

			// NOTE we only include the x/y offset from the upper left of the
			// buffer when we go to write into the image buffer. This allows
			// us to use the same index into the font alpha render as into
			// the image.

			xpos = (i + x) + fchar->b_x;
			ypos = (j + y) + fchar->b_y;

			img_idx = xpos + ypos * slide->img_w;
			fchar_idx = i + j * fchar->f_x;

			alpha = fchar->bitmap[fchar_idx] * 1.0f / 255.0f;

			slide->pixels[img_idx].r = m_lblend_u8(bg.r, fg.r, alpha);
			slide->pixels[img_idx].b = m_lblend_u8(bg.b, fg.b, alpha);
			slide->pixels[img_idx].g = m_lblend_u8(bg.g, fg.g, alpha);
			slide->pixels[img_idx].a = 0xff;
		}
	}

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

/* f_fontload : load as many ascii characters into the font table as possible */
int f_fontload(struct font_t *font, char *path, s32 fontsize)
{
	stbtt_fontinfo fontinfo;
	f32 scale_x, scale_y;
	s32 i, w, h, xoff, yoff, advance, lsb;
	unsigned char *ttf_buffer, *bitmap;
	int rc;

	// TODO (brian)
	// Come back through here and if you want anything other than ascii,
	// make a hot-loading glyph situation. When doing this, the rasterizing
	// 

	ttf_buffer = (unsigned char *)sys_readfile(path);
	if (!ttf_buffer) {
		fprintf(stderr, "Couldn't read fontfile [%s]\n", path);
		exit(1);
	}

	stbtt_InitFont(&fontinfo, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
	scale_y = stbtt_ScaleForPixelHeight(&fontinfo, fontsize);
	scale_x = scale_y;

	for (i = 0; i < 128; i++) {
		bitmap = stbtt_GetCodepointBitmap(&fontinfo, scale_x, scale_y, i, &w, &h, &xoff, &yoff);

		stbtt_GetCodepointHMetrics(&fontinfo, i, &advance, &lsb);

		c_resize(&font->ftab, &font->ftab_len, &font->ftab_cap, sizeof(struct fchar_t));

		font->ftab[font->ftab_len].bitmap  = bitmap;
		font->ftab[font->ftab_len].f_x     = w;
		font->ftab[font->ftab_len].f_y     = h;
		font->ftab[font->ftab_len].b_x     = xoff;
		font->ftab[font->ftab_len].b_y     = yoff;
		font->ftab[font->ftab_len].advance = advance * scale_x;

		font->ftab_len++;
	}

	free(ttf_buffer);

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
	size_t i;
	u8 *b;
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

