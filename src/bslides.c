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
 * 5. String parser, quoted strings come through as a single string, etc.
 *
 * DOCUMENTATION
 *
 *   This is my slideshow program. Basically, PowerPoint is bad, and just making a PDF from some
 * markup system didn't create the kinds of interactive / animative presentations that really seem
 * to capture audiences. So, here we are.
 *
 *   This is modeled after Jon Blow's slideshow program, where he basically took his 3d game engine,
 * and just made it render text, images, and whatever from a description file. Mine's a little
 * different:
 *
 * - command driven
 * - defers asset loading, keeping everything loaded until close
 * - user hotloadable modules (library)
 *
 * COMMANDS (Completed)
 *
 * COMMANDS (Incompleted)
 *   blank
 *   getdate
 *   font
 *   fontset
 *   fontsize
 *   image
 *   justified
 *   maketemplate
 *   newslide
 *   printline
 *   templateset
 *   usetemplate
 *   libopen
 *   libexec
 *   libclose
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

#define MAX_FUNCTIONS (BUFSMALL)

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

struct template_t {
	char *name;
	s32 justification;
	color_t fg;
	color_t bg;
};

struct settings_t {
	s32 fontidx;
	s32 fontsize;
	s32 template;
	s32 slide;
	s32 pos_x, pos_y;
	s32 img_w, img_h;
};

struct show_t;

typedef int (showfunc_t) (struct show_t *, int, char **);

struct function_t {
	char *name;
	s32 run_once;
	showfunc_t *func;
};

struct show_t {

	// output members
	struct pixel_t *fbuffer; // holds the combined bg + fg
	struct pixel_t *fbuffer_bg;
	struct pixel_t *fbuffer_fg;

	// TODO input members

	// settings
	struct settings_t defaults;
	struct settings_t settings;

	// templates
	struct template_t *templates;
	size_t templates_len, templates_cap;

	// function table
	struct function_t functions[MAX_FUNCTIONS];
	size_t functions_len;

	// command strings
	struct command_t *commands;
	size_t commands_len, commands_cap;

	// font table
	struct font_t *fonts;
	size_t fonts_len, fonts_cap;

	char *name;
};

// Slideshow Init & Free Functions
/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config);
/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show);

// Slideshow Rendering Functions
/* show_render : renders the slide 'idx' into its internal buffer */
int show_render(struct show_t *show, s32 idx);
/* show_renderchar : renders a single s32 codepoint to x,y position */
int show_renderchar(struct show_t *show, struct fchar_t *fchar, color_t color, s32 x, s32 y);
/* show_renderimage : renders the image to the slide */
int show_renderimage(struct show_t *slide, struct image_t *image, s32 x, s32 y);

// User Callable (Default) Slideshow Functions
/* functab_add : adds a callable function into the show */
int functab_add(struct show_t *show, char *name, s32 run_once, showfunc_t func);
/* func_name : user function ; sets the slideshow name, to be run once */
int func_name(struct show_t *show, int argc, char **argv);
/* func_blank : user function ; inserts a blank line at the current cursor position */
int func_blank(struct show_t *show, int argc, char **argv);
/* func_clear : user function ; clears the framebuffer */
int func_clear(struct show_t *show, int argc, char **argv);
/* func_templateadd : user function ; adds / overwrites a template */
int func_templateadd(struct show_t *show, int argc, char **argv);
/* func_templateset : user function ; sets the current template */
int func_templateset(struct show_t *show, int argc, char **argv);
/* func_dimensions : user function ; sets the output width / height */
int func_dimensions(struct show_t *show, int argc, char **argv);
/* func_nop : user(ish) function ; does nothing, but show an error */
int func_nop(struct show_t *show, int argc, char **argv);
/* func_printdate : user function ; prints the date at the current line */
int func_printdate(struct show_t *show, int argc, char **argv);
/* func_printline : user function ; basically, echo */
int func_printline(struct show_t *show, int argc, char **argv);
/* func_fontadd : user function ; loads a font, to be run once */
int func_fontadd(struct show_t *show, int argc, char **argv);
/* func_fontset : user function ; sets the font */
int func_fontset(struct show_t *show, int argc, char **argv);
/* func_fontsizeset : user function ; sets the font size */
int func_fontsizeset(struct show_t *show, int argc, char **argv);
/* func_imageadd : user function ; loads an image, to be run once */
int func_imageadd(struct show_t *show, int argc, char **argv);
/* func_imagedraw : user function ; draws the image in an argument dependent way */
int func_imagedraw(struct show_t *show, int argc, char **argv);

// Utility Functions
/* util_framebuffer : (re)sets the show's internal framebuffer */
int util_framebuffer(struct show_t *show);
/* util_setdefaults : sets default settings */
int util_setdefaults(struct show_t *show);
/* util_slidecount : counts the number of slides in the slideshow */
int util_slidecount(struct show_t *show);
/* util_getfuncidx : gets the function index */
int util_getfuncidx(struct show_t *show, char *function);

// Font Functions
/* font_load : sets up an entry in the font table with these params */
s32 font_load(struct font_t *font, char *name, char *path);
/* font_getfont : returns a pointer to the font structure with the matching name */
struct font_t *font_getfont(struct show_t *show, char *name);
/* font_getcodepoint : retrieves the fchar_t from input font and codepoint */
struct fchar_t *font_getcodepoint(struct font_t *font, u32 codepoint, u32 fontsize);
/* font_free : frees all fonts associated with the slideshow */
s32 font_free(struct show_t *show);
/* font_vertadvance : returns the font's vertical advance */
s32 font_vertadvance(struct font_t *font);


/* util_parsecolor : parses a color string into a color structure */
struct color_t util_parsecolor(char *s);

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t);

int main(int argc, char **argv)
{
	struct show_t show;
	s32 i, len;
	int rc;
	char slidename[BUFSMALL];
	char imagename[BUFSMALL];

	if (argc < 2) {
		fprintf(stderr, "USAGE : %s config\n", argv[0]);
		exit(1);
	}

	memset(slidename, 0, sizeof slidename);
	memset(imagename, 0, sizeof imagename);

	rc = show_load(&show, argv[1]);
	if (rc < 0) {
		fprintf(stderr, "Couldn't load up the show!\n");
		exit(1);
	}

	// hook up the default functions
	functab_add(&show, "blank",        0, func_blank);
	functab_add(&show, "name",         1, func_name);
	functab_add(&show, "clear",        0, func_clear);
	functab_add(&show, "newslide",     0, func_nop);
	functab_add(&show, "templateadd",  1, func_templateadd);
	functab_add(&show, "templateset",  0, func_templateset);
	functab_add(&show, "dimensions",   1, func_dimensions);
	functab_add(&show, "printline",    0, func_printline);
	functab_add(&show, "printdate",    0, func_printdate);
	functab_add(&show, "fontadd",      1, func_fontadd);
	functab_add(&show, "fontset",      0, func_fontset);
	functab_add(&show, "fontsizeset",  0, func_fontsizeset);
	functab_add(&show, "imageadd",     1, func_imageadd);
	functab_add(&show, "imagedraw",    0, func_imagedraw);

	// exec all of the default functions
	for (i = 0; i < show.commands_len; i++) {
		s32 idx = util_getfuncidx(&show, show.commands[i].argv[0]);
		if (idx < 0) {
			ERR("Couldn't find function '%s'\n", show.commands[i].argv[0]);
			continue;
		}
		if (show.functions[idx].run_once) {
			rc = show.functions[idx].func(&show, show.commands[i].argc, show.commands[i].argv);
		}
	}

	// setup the show's framebuffers and whatnot
	util_framebuffer(&show);

	for (i = 0, len = util_slidecount(&show); i < len; i++) {
		snprintf(slidename, sizeof slidename, "%s_%04d", show.name, i);
		snprintf(imagename, sizeof imagename, "%s.png", slidename);

		printf("%s\n", imagename);

		rc = show_render(&show, i);
		if (rc < 0) {
			fprintf(stderr, "Couldn't render '%s' to the image!\n", slidename);
			exit(1);
		}

		// rc = stbi_write_png(imagename, show.settings.img_w, show.settings.img_h,
		// 		sizeof(struct pixel_t), show.fbuffer, sizeof(struct pixel_t) * show.settings.img_w);
		rc = stbi_write_png(imagename, show.settings.img_w, show.settings.img_h,
			sizeof(struct pixel_t), show.fbuffer_bg, sizeof(struct pixel_t) * show.settings.img_w);
		if (!rc) {
			fprintf(stderr, "Couldn't write %s!\n", imagename);
			exit(1);
		}
	}

	rc = show_free(&show);
	if (rc < 0) {
		fprintf(stderr, "Couldn't free the show!\n");
		exit(1);
	}

	return 0;
}

/* show_load : load up the slideshow from the config file */
int show_load(struct show_t *show, char *config)
{
	FILE *fp;
	char *s;
	s32 i;
	s32 argc;
	s32 len;
	char *tokens[BUFSMALL];
	char **argv;
	char buf[BUFLARGE];

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

		memset(tokens, 0, sizeof tokens);

		for (i = 0, s = strtok(buf, " "); i < ARRSIZE(tokens) && s; i++, s = strtok(NULL, " ")) {
			tokens[i] = s;
		}

		if (streq("//", tokens[0]) || streq("#", tokens[0])) {
			continue;
		}

		if (!streq(":", tokens[0])) {
			for (i = len - 1; i > -1; i--) {
				tokens[i + 1] = tokens[i];
			}
			tokens[0] = "printline";
			argv = tokens;
		} else {
			argv = tokens + 1;
		}

		for (argc = 0; argv[argc]; argc++)
			;

		C_RESIZE(&show->commands, &show->commands, sizeof(*show->commands));

		show->commands[show->commands_len].argc = argc;
		show->commands[show->commands_len].argv = calloc(argc, sizeof(char *));
		for (i = 0; i < argc; i++) {
			show->commands[show->commands_len].argv[i] = strdup(argv[i]);
		}

		show->commands_len++;
	}

	fclose(fp);

	return 0;
}

/* show_free : frees everything related to the slideshow */
int show_free(struct show_t *show)
{
	return 0;
}

/* show_render : renders the slide 'idx' into its internal buffer */
int show_render(struct show_t *show, s32 idx)
{
	s32 cmdidx;
	s32 i, slide;
	s32 j;
	s32 rc;
	color_t fg, bg;
	color_t outcolor;

	// NOTE (brian) runs commands from `newslide` to `newslide`, combines framebuffers, then returns

	assert(show);

	for (i = 0, slide = -1; i < show->commands_len && slide < idx; i++) {
		if (streq(show->commands[i].argv[0], "newslide")) {
			slide++;
		}
	}

	cmdidx = i;

	for (i = cmdidx; i < show->commands_len && !streq(show->commands[i].argv[0], "newslide"); i++) {
		j = util_getfuncidx(show, show->commands[i].argv[0]);
		if (j < 0) {
			ERR("Function '%s' doesn't exist!\n", show->commands[i].argv[0]);
			continue;
		}

		printf("Execing '%s'\n", show->functions[j].name);

		rc = show->functions[j].func(show, show->commands[i].argc, show->commands[i].argv);
		if (rc < 0) {
			ERR("Function '%s' returned with an error!\n", show->commands[i].argv[0]);
		}
	}

	for (i = 0; i < show->settings.img_w; i++) {
		for (j = 0; j < show->settings.img_h; j++) {
			// idx = i * show->settings.img_h + j;
			idx = i + j * show->settings.img_w;

			fg = *(struct color_t *)&show->fbuffer_fg[idx];
			bg = *(struct color_t *)&show->fbuffer_bg[idx];

			outcolor.r = m_lblend_u8(fg.r, bg.r, fg.a);
			outcolor.g = m_lblend_u8(fg.g, bg.g, fg.a);
			outcolor.b = m_lblend_u8(fg.b, bg.b, fg.a);
			outcolor.a = 0xff;

			show->fbuffer[idx] = *(struct pixel_t *)&outcolor;
		}
	}

#if 0 // legacy
	// then we draw text over it, no text under slides
	for (i = 0; i < slide->strings_len; i++) {
		w_xpos = show->settings.img_w / 12;
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
#endif

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
	win_w = show->settings.img_w;
	win_h = show->settings.img_h;

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
			dst = (i + bound_x) + (j + bound_y) * show->settings.img_w;
			src = i + j * bound_w;
			show->fbuffer_fg[dst].r = iscaled[src].r;
			show->fbuffer_fg[dst].g = iscaled[src].g;
			show->fbuffer_fg[dst].b = iscaled[src].b;
			show->fbuffer_fg[dst].a = iscaled[src].a;
		}
	}

	free(iscaled);

	return 0;
}

/* m_lblend_u8 : linear blend on u8s */
u8 m_lblend_u8(u8 a, u8 b, f32 t)
{
	return (u8)((a + t * (b - a)) + 0.5f);
}

/* functab_add : adds a callable function into the show */
int functab_add(struct show_t *show, char *name, s32 run_once, showfunc_t func)
{
	assert(show);
	assert(show->functions_len < MAX_FUNCTIONS);

	show->functions[show->functions_len].name = strdup(name);
	show->functions[show->functions_len].run_once = run_once;
	show->functions[show->functions_len].func = func;

	show->functions_len++;

	return 0;
}

//
// User Callable Functions
//

/* func_name : user function ; sets the slideshow name, to be run once */
int func_name(struct show_t *show, int argc, char **argv)
{
	assert(show);

	if (argc < 2) {
		return -1;
	}

	show->name = strdup(argv[1]);

	return 0;
}

/* func_blank : user function ; inserts a blank line at the current cursor position */
int func_blank(struct show_t *show, int argc, char **argv)
{
	assert(show);

	return 0;
}

/* func_clear : user function ; clears framebuffers */
int func_clear(struct show_t *show, int argc, char **argv)
{
	struct pixel_t bg, fg, nil;
	s32 i, j;
	s32 idx;

	assert(show);

	bg = *(struct pixel_t *)&show->templates[show->settings.template].bg;
	fg = *(struct pixel_t *)&show->templates[show->settings.template].fg;
	memset(&nil, 0, sizeof nil);

	for (j = 0; j < show->settings.img_h; j++) {
		for (i = 0; i < show->settings.img_w; i++) {
			idx = i + j * show->settings.img_w;
			show->fbuffer_bg[idx] = bg;
			show->fbuffer_fg[idx] = fg;
			show->fbuffer[idx] = nil;
		}
	}

	return 0;
}

/* func_templateadd : user function ; adds / overwrites a template */
int func_templateadd(struct show_t *show, int argc, char **argv)
{
	char *name;
	char *justification;
	color_t bg, fg;

	assert(show);

	C_RESIZE(&show->templates, &show->templates, sizeof(*show->templates));

	if (argc < 5) {
		ERR("[%s] : not enough arguments, 5 required, found %d\n", argv[0], argc);
		return -1;
	}

	name = argv[1];
	bg = util_parsecolor(argv[2]);
	fg = util_parsecolor(argv[3]);
	justification = argv[4];

	show->templates[show->templates_len].name = strdup(name);
	show->templates[show->templates_len].bg = bg;
	show->templates[show->templates_len].fg = fg;

	// TODO make nicer
	if (streq(justification, "left")) {
		show->templates[show->templates_len].justification = SLIDEJUST_LEFT;
	} else if (streq(justification, "center")) {
		show->templates[show->templates_len].justification = SLIDEJUST_CENTER;
	} else if (streq(justification, "right")) {
		show->templates[show->templates_len].justification = SLIDEJUST_RIGHT;
	} else { // default to left
		show->templates[show->templates_len].justification = SLIDEJUST_LEFT;
	}

	show->templates_len++;

	return 0;
}

/* func_templateset : user function ; sets the current template */
int func_templateset(struct show_t *show, int argc, char **argv)
{
	s32 i;

	assert(show);

	for (i = 0; i < show->templates_len; i++) {
		if (streq(show->templates[i].name, argv[1])) {
			show->settings.template = i;
			return 0;
		}
	}

	return -1;
}

/* func_printdate : user function ; prints the date at the current line */
int func_printdate(struct show_t *show, int argc, char **argv)
{
	return 0;
}

/* func_printline : user function ; basically, echo */
int func_printline(struct show_t *show, int argc, char **argv)
{
	s32 i, j;
	struct font_t *font;
	struct fchar_t *fchar;
	char buf[BUFLARGE];

	// NOTE (brian): string join on spaces, then print from 0 to len

	assert(show);
	assert(show->settings.fontidx >= 0);

	memset(buf, 0, sizeof buf);

	font = show->fonts + show->settings.fontidx;

	for (i = 1; i < argc; i++) {
		snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "%s", argv[i]);
		if (i != argc - 1)
			snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "%s", " ");
	}

	for (i = 0; i < strlen(buf); i++) {
		fchar = font_getcodepoint(font, buf[i], show->settings.fontsize);

		draw_rect(show->fbuffer_fg,
				  fchar->bitmap, // src buf
				  show->settings.img_w,
				  show->settings.img_h,
				  fchar->f_x, // src w
				  fchar->f_y, // src h
				  show->settings.pos_x + fchar->b_x,
				  show->settings.pos_y + fchar->b_y
				  );
	}

	return 0;
}

/* func_dimensions : user function ; sets the output width / height */
int func_dimensions(struct show_t *show, int argc, char **argv)
{
	assert(show);

	if (argc < 2) {
		return -1;
	}

	show->settings.img_w = atoi(argv[1]);
	show->settings.img_h = atoi(argv[2]);

	return 0;
}

/* func_fontadd : user function ; loads a font, to be run once */
int func_fontadd(struct show_t *show, int argc, char **argv)
{
	char *name;
	char *path;
	int rc;

	assert(show);

	switch (argc) {
		case 0:
		case 1:
		{
			return -1;
		}

		case 3:
		{
			name = argv[1];
			path = argv[2];
			break;
		}

		case 2:
		{
			name = argv[1];
			path = argv[1];
			break;
		}
	}

	C_RESIZE(&show->fonts, &show->fonts, sizeof(*show->fonts));

	rc = font_load(show->fonts + show->fonts_len, name, path);
	if (rc < 0) {
		return -1;
	}

	show->fonts_len++;

	return 0;
}

/* func_fontset : user function ; sets the font */
int func_fontset(struct show_t *show, int argc, char **argv)
{
	s32 i;

	assert(show);

	if (argc < 2) {
		return -1;
	}

	for (i = 0; i < show->fonts_len; i++) {
		if (streq(show->fonts[i].name, argv[1])) {
			show->settings.fontidx = i;
			return 0;
		}
	}

	ERR("Couldn't find font '%s'\n", argv[1]);

	return -1;
}

/* func_fontsizeset : user function ; sets the font size */
int func_fontsizeset(struct show_t *show, int argc, char **argv)
{
	assert(show);

	if (argc < 2) {
		return -1;
	}

	show->settings.fontsize = atoi(argv[1]);

	return 0;
}

/* func_imageadd : user function ; loads an image, to be run once */
int func_imageadd(struct show_t *show, int argc, char **argv)
{
	return 0;
}

/* func_imagedraw : user function ; draws the image in an argument dependent way */
int func_imagedraw(struct show_t *show, int argc, char **argv)
{
	return 0;
}

/* func_nop : user(ish) function ; does nothing */
int func_nop(struct show_t *show, int argc, char **argv)
{
	return 0;
}

//
// Utility Functions
//

/* util_framebuffer : (re)sets the show's internal framebuffer */
int util_framebuffer(struct show_t *show)
{
	s32 pixels;

	assert(show);

	free(show->fbuffer);
	free(show->fbuffer_fg);
	free(show->fbuffer_bg);

	pixels = show->settings.img_w * show->settings.img_h;

	show->fbuffer    = calloc(pixels, sizeof(struct pixel_t));
	show->fbuffer_bg = calloc(pixels, sizeof(struct pixel_t));
	show->fbuffer_fg = calloc(pixels, sizeof(struct pixel_t));

	return 0;
}

/* util_setdefaults : sets default settings */
int util_setdefaults(struct show_t *show)
{
	assert(show);

	memset(&show->defaults, 0, sizeof show->defaults);

	return 0;
}

/* util_slidecount : counts the number of slides in the slideshow */
int util_slidecount(struct show_t *show)
{
	s32 i, rc;

	assert(show);

	for (i = 0, rc = 0; i < show->commands_len; i++) {
		if (streq(show->commands[i].argv[0], "newslide")) {
			rc++;
		}
	}

	return rc;
}

/* util_getfuncidx : gets the function index */
int util_getfuncidx(struct show_t *show, char *function)
{
	s32 i;

	assert(show);
	assert(function);

	for (i = 0; i < show->functions_len; i++) {
		if (streq(show->functions[i].name, function)) {
			return i;
		}
	}

	return -1;
}

/* util_parsecolor : parses a color string into a color structure */
struct color_t util_parsecolor(char *s)
{
	struct color_t color;
	u32 r, g, b, a;
	s32 rc;

	memset(&color, 0, sizeof color);

	rc = sscanf(s, "0x%2x%2x%2x%2x", &r, &g, &b, &a);

	if (rc != 3 && rc != 4) {
		ERR("Color Parse Error! '%s'\n", s);
		return color;
	}

	if (rc == 3) {
		color.r = r;
		color.g = g;
		color.b = b;
		color.a = 0xff;
	}

	if (rc == 4) {
		color.a = a;
	}

	return color;
}

//
// Framebuffer Functions
//

/* draw_rect : blits a rectangle */
int draw_rect(struct pixel_t *dst, struct pixel_t *src, s32 dst_w, s32 dst_h,
		s32 src_w, s32 src_h, s32 pos_x, s32 pos_y)
{
	s32 src_x, src_y;
	s32 dst_x, dst_y;
	s32 src_idx, dst_idx;
	struct pixel_t dst_pix, src_pix;

	// NOTE (brian): draws a rectangle on top of another. iterates over the source pixels, picks the
	// place in the output, performs linear blending, and puts into the output framebuffer.

	for (src_x = 0; src_x < src_w; src_x++) {
		for (src_y = 0; src_y < src_h; src_y++) {

			dst_x = pos_x + src_x;
			dst_y = pos_y + src_y;

			// check for errors first, then determine indexes
			if (dst_x < 0 || dst_x > dst_w) {
				continue;
			}

			if (dst_y < 0 || dst_y > dst_h) {
				continue;
			}

			src_idx = src_x + src_y * src_w;
			src_pix = src[src_idx];

			dst_idx = dst_x + dst_y * dst_w;
			dst_pix = dst[dst_idx];

			dst_pix.r = m_lblend_u8(dst_pix.r, src_pix.r, src_pix.a);
			dst_pix.g = m_lblend_u8(dst_pix.g, src_pix.g, src_pix.a);
			dst_pix.b = m_lblend_u8(dst_pix.b, src_pix.b, src_pix.a);
			dst_pix.a = 0xff;

			dst[dst_idx] = dst_pix;
		}
	}

	return 0;
}


//
// Font Functions
//

/* font_load : sets up an entry in the font table with these params */
s32 font_load(struct font_t *font, char *name, char *path)
{
	assert(font);

	font->name = strdup(name);
	font->path = strdup(path);
	font->ttfbuffer = sys_readfile(path);

	return 0;
}

/* font_getcodepoint : retrieves the fchar_t from input font and codepoint */
struct fchar_t *font_getcodepoint(struct font_t *font, u32 codepoint, u32 fontsize)
{
	s32 i;

	// NOTE (brian): search for the codepoint in the fonttable. if it's there and rendered for the
	// given size, return it. Otherwise, render the character for the required fontsize, insert it
	// into the table, then return it.

	for (i = 0; i < font->ftab_len; i++) {
		if (font->ftab[i].codepoint == codepoint && font->ftab[i].fontsize == fontsize) {
			return font->ftab + i;
		}
	}

	// NOTE (brian) if we get here, we didn't find the codepoint, so we
	// have to render a new one

	C_RESIZE(&font->ftab, &font->ftab, sizeof(*font->ftab));

	stbtt_fontinfo fontinfo;

	f32 scale_x, scale_y;
	s32 w, h, xoff, yoff, advance, lsb;
	u8 *alpha_bitmap;

	stbtt_InitFont(&fontinfo, (unsigned char *)font->ttfbuffer, stbtt_GetFontOffsetForIndex((unsigned char *)font->ttfbuffer, 0));
	scale_y = stbtt_ScaleForPixelHeight(&fontinfo, fontsize);
	scale_x = scale_y;

	alpha_bitmap = stbtt_GetCodepointBitmap(&fontinfo, scale_x, scale_y, codepoint, &w, &h, &xoff, &yoff);

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

	struct pixels_t *rgba_bitmap;

	for (i = 0; i < w * h; i++) {
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

/* font_vertadvance : returns the font's vertical advance */
s32 font_vertadvance(struct font_t *font)
{
	return font->ascent - font->descent + font->linegap;
}

/* font_fontfree : frees all resources associated with the font */
int font_fontfree(struct font_t *font)
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

