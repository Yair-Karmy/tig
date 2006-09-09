#define VERSION	"tig-0.4.git"
#include <sys/types.h>
#include <regex.h>

#include <locale.h>
#include <langinfo.h>
#include <iconv.h>

#if __GNUC__ >= 3
#define __NORETURN __attribute__((__noreturn__))
#else
#define __NORETURN
#endif

static void __NORETURN die(const char *err, ...);
#define SIZEOF_STR	1024	/* Default string size. */
#define SIZEOF_REVGRAPH	19	/* Size of revision ancestry graphics. */
#define ICONV_NONE	((iconv_t) -1)

#define TIG_LS_REMOTE \
	"git ls-remote . 2>/dev/null"

#define TIG_DIFF_CMD \
	"git show --root --patch-with-stat --find-copies-harder -B -C %s 2>/dev/null"

#define TIG_LOG_CMD	\
	"git log --cc --stat -n100 %s 2>/dev/null"

#define TIG_MAIN_CMD \
	"git log --topo-order --pretty=raw %s 2>/dev/null"

/* XXX: Needs to be defined to the empty string. */
#define TIG_HELP_CMD	""
#define TIG_PAGER_CMD	""

static bool
string_nformat(char *buf, size_t bufsize, int *bufpos, const char *fmt, ...)
{
	va_list args;
	int pos = bufpos ? *bufpos : 0;

	va_start(args, fmt);
	pos += vsnprintf(buf + pos, bufsize - pos, fmt, args);
	va_end(args);

	if (bufpos)
		*bufpos = pos;

	return pos >= bufsize ? FALSE : TRUE;
}

#define string_format(buf, fmt, args...) \
	string_nformat(buf, sizeof(buf), NULL, fmt, args)

#define string_format_from(buf, from, fmt, args...) \
	string_nformat(buf, sizeof(buf), from, fmt, args)

static int
string_enum_compare(const char *str1, const char *str2, int len)
{
	size_t i;

#define string_enum_sep(x) ((x) == '-' || (x) == '_' || (x) == '.')

	/* Diff-Header == DIFF_HEADER */
	for (i = 0; i < len; i++) {
		if (toupper(str1[i]) == toupper(str2[i]))
			continue;

		if (string_enum_sep(str1[i]) &&
		    string_enum_sep(str2[i]))
			continue;

		return str1[i] - str2[i];
	}

	return 0;
}
sq_quote(char buf[SIZEOF_STR], size_t bufsize, const char *src)
#define BUFPUT(x) do { if (bufsize < SIZEOF_STR) buf[bufsize++] = (x); } while (0)
	REQ_GROUP("Searching") \
	REQ_(SEARCH,		"Search the view"), \
	REQ_(SEARCH_BACK,	"Search backwards in the view"), \
	REQ_(FIND_NEXT,		"Find next search match"), \
	REQ_(FIND_PREV,		"Find previous search match"), \
	\
	REQ_(NONE,		"Do nothing"), \
	REQ_(TOGGLE_LINENO,	"Toggle line numbers"), \
	REQ_(TOGGLE_REV_GRAPH,	"Toggle revision graph visualization")
	REQ_INFO,
	REQ_UNKNOWN,
	char *name;
	int namelen;
#define REQ_GROUP(help)	{ 0, NULL, 0, (help) },
#define REQ_(req, help)	{ REQ_##req, (#req), STRING_SIZE(#req), (help) }
static enum request
get_request(const char *name)
{
	int namelen = strlen(name);
	int i;

	for (i = 0; i < ARRAY_SIZE(req_info); i++)
		if (req_info[i].namelen == namelen &&
		    !string_enum_compare(req_info[i].name, name, namelen))
			return req_info[i].request;

	return REQ_UNKNOWN;
}


/*
 * Options
 */
static bool opt_line_number		= FALSE;
static bool opt_rev_graph		= TRUE;
static int opt_num_interval		= NUMBER_INTERVAL;
static int opt_tab_size			= TABSIZE;
static enum request opt_request		= REQ_VIEW_MAIN;
static char opt_cmd[SIZEOF_STR]		= "";
static FILE *opt_pipe			= NULL;
static char opt_encoding[20]		= "UTF-8";
static bool opt_utf8			= TRUE;
static char opt_codeset[20]		= "UTF-8";
static iconv_t opt_iconv		= ICONV_NONE;
static char opt_search[SIZEOF_STR]	= "";

enum option_type {
	OPT_NONE,
	OPT_INT,
};

static bool
check_option(char *opt, char short_name, char *name, enum option_type type, ...)
{
	va_list args;
	char *value = "";
	int *number;

	if (opt[0] != '-')
		return FALSE;

	if (opt[1] == '-') {
		int namelen = strlen(name);

		opt += 2;

		if (strncmp(opt, name, namelen))
			return FALSE;

		if (opt[namelen] == '=')
			value = opt + namelen + 1;

	} else {
		if (!short_name || opt[1] != short_name)
			return FALSE;
		value = opt + 2;
	}

	va_start(args, type);
	if (type == OPT_INT) {
		number = va_arg(args, int *);
		if (isdigit(*value))
			*number = atoi(value);
	}
	va_end(args);

	return TRUE;
}
		if (check_option(opt, 'n', "line-number", OPT_INT, &opt_num_interval)) {
		if (check_option(opt, 'b', "tab-size", OPT_INT, &opt_tab_size)) {
			opt_tab_size = MIN(opt_tab_size, TABSIZE);
		if (check_option(opt, 'v', "version", OPT_NONE)) {
		if (check_option(opt, 'h', "help", OPT_NONE)) {
		die("unknown option '%s'\n\n%s", opt, usage);
/*
 * Line-oriented content detection.
 */
LINE(PP_REFS,	   "Refs: ",		COLOR_RED,	COLOR_DEFAULT,	0), \
	enum line_type type;

	for (type = 0; type < ARRAY_SIZE(line_info); type++)
		if (namelen == line_info[type].namelen &&
		    !string_enum_compare(line_info[type].name, name, namelen))
			return &line_info[type];

	return NULL;
}

static void
init_colors(void)
{
	int default_bg = COLOR_BLACK;
	int default_fg = COLOR_WHITE;
	enum line_type type;

	start_color();

	if (use_default_colors() != ERR) {
		default_bg = -1;
		default_fg = -1;
	}

	for (type = 0; type < ARRAY_SIZE(line_info); type++) {
		struct line_info *info = &line_info[type];
		int bg = info->bg == COLOR_DEFAULT ? default_bg : info->bg;
		int fg = info->fg == COLOR_DEFAULT ? default_fg : info->fg;

		init_pair(type, fg, bg);
	}
}

struct line {
	enum line_type type;
	void *data;		/* User data */
};


/*
 * Keys
 */

struct keybinding {
	int alias;
	enum request request;
	struct keybinding *next;
};

static struct keybinding default_keybindings[] = {
	/* View switching */
	{ 'm',		REQ_VIEW_MAIN },
	{ 'd',		REQ_VIEW_DIFF },
	{ 'l',		REQ_VIEW_LOG },
	{ 'p',		REQ_VIEW_PAGER },
	{ 'h',		REQ_VIEW_HELP },

	/* View manipulation */
	{ 'q',		REQ_VIEW_CLOSE },
	{ KEY_TAB,	REQ_VIEW_NEXT },
	{ KEY_RETURN,	REQ_ENTER },
	{ KEY_UP,	REQ_PREVIOUS },
	{ KEY_DOWN,	REQ_NEXT },

	/* Cursor navigation */
	{ 'k',		REQ_MOVE_UP },
	{ 'j',		REQ_MOVE_DOWN },
	{ KEY_HOME,	REQ_MOVE_FIRST_LINE },
	{ KEY_END,	REQ_MOVE_LAST_LINE },
	{ KEY_NPAGE,	REQ_MOVE_PAGE_DOWN },
	{ ' ',		REQ_MOVE_PAGE_DOWN },
	{ KEY_PPAGE,	REQ_MOVE_PAGE_UP },
	{ 'b',		REQ_MOVE_PAGE_UP },
	{ '-',		REQ_MOVE_PAGE_UP },

	/* Scrolling */
	{ KEY_IC,	REQ_SCROLL_LINE_UP },
	{ KEY_DC,	REQ_SCROLL_LINE_DOWN },
	{ 'w',		REQ_SCROLL_PAGE_UP },
	{ 's',		REQ_SCROLL_PAGE_DOWN },

	/* Searching */
	{ '/',		REQ_SEARCH },
	{ '?',		REQ_SEARCH_BACK },
	{ 'n',		REQ_FIND_NEXT },
	{ 'N',		REQ_FIND_PREV },

	/* Misc */
	{ 'Q',		REQ_QUIT },
	{ 'z',		REQ_STOP_LOADING },
	{ 'v',		REQ_SHOW_VERSION },
	{ 'r',		REQ_SCREEN_REDRAW },
	{ 'n',		REQ_TOGGLE_LINENO },
	{ 'g',		REQ_TOGGLE_REV_GRAPH },
	{ ':',		REQ_PROMPT },

	/* wgetch() with nodelay() enabled returns ERR when there's no input. */
	{ ERR,		REQ_NONE },

	/* Using the ncurses SIGWINCH handler. */
	{ KEY_RESIZE,	REQ_SCREEN_RESIZE },
};

#define KEYMAP_INFO \
	KEYMAP_(GENERIC), \
	KEYMAP_(MAIN), \
	KEYMAP_(DIFF), \
	KEYMAP_(LOG), \
	KEYMAP_(PAGER), \
	KEYMAP_(HELP) \

enum keymap {
#define KEYMAP_(name) KEYMAP_##name
	KEYMAP_INFO
#undef	KEYMAP_
};

static struct int_map keymap_table[] = {
#define KEYMAP_(name) { #name, STRING_SIZE(#name), KEYMAP_##name }
	KEYMAP_INFO
#undef	KEYMAP_
};

#define set_keymap(map, name) \
	set_from_int_map(keymap_table, ARRAY_SIZE(keymap_table), map, name, strlen(name))

static struct keybinding *keybindings[ARRAY_SIZE(keymap_table)];

static void
add_keybinding(enum keymap keymap, enum request request, int key)
{
	struct keybinding *keybinding;

	keybinding = calloc(1, sizeof(*keybinding));
	if (!keybinding)
		die("Failed to allocate keybinding");

	keybinding->alias = key;
	keybinding->request = request;
	keybinding->next = keybindings[keymap];
	keybindings[keymap] = keybinding;
}

/* Looks for a key binding first in the given map, then in the generic map, and
 * lastly in the default keybindings. */
static enum request
get_keybinding(enum keymap keymap, int key)
{
	struct keybinding *kbd;
	int i;

	for (kbd = keybindings[keymap]; kbd; kbd = kbd->next)
		if (kbd->alias == key)
			return kbd->request;

	for (kbd = keybindings[KEYMAP_GENERIC]; kbd; kbd = kbd->next)
		if (kbd->alias == key)
			return kbd->request;

	for (i = 0; i < ARRAY_SIZE(default_keybindings); i++)
		if (default_keybindings[i].alias == key)
			return default_keybindings[i].request;

	return (enum request) key;
}


struct key {
	char *name;
	int value;
};

static struct key key_table[] = {
	{ "Enter",	KEY_RETURN },
	{ "Space",	' ' },
	{ "Backspace",	KEY_BACKSPACE },
	{ "Tab",	KEY_TAB },
	{ "Escape",	KEY_ESC },
	{ "Left",	KEY_LEFT },
	{ "Right",	KEY_RIGHT },
	{ "Up",		KEY_UP },
	{ "Down",	KEY_DOWN },
	{ "Insert",	KEY_IC },
	{ "Delete",	KEY_DC },
	{ "Hash",	'#' },
	{ "Home",	KEY_HOME },
	{ "End",	KEY_END },
	{ "PageUp",	KEY_PPAGE },
	{ "PageDown",	KEY_NPAGE },
	{ "F1",		KEY_F(1) },
	{ "F2",		KEY_F(2) },
	{ "F3",		KEY_F(3) },
	{ "F4",		KEY_F(4) },
	{ "F5",		KEY_F(5) },
	{ "F6",		KEY_F(6) },
	{ "F7",		KEY_F(7) },
	{ "F8",		KEY_F(8) },
	{ "F9",		KEY_F(9) },
	{ "F10",	KEY_F(10) },
	{ "F11",	KEY_F(11) },
	{ "F12",	KEY_F(12) },
};

static int
get_key_value(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_table); i++)
		if (!strcasecmp(key_table[i].name, name))
			return key_table[i].value;

	if (strlen(name) == 1 && isprint(*name))
		return (int) *name;

	return ERR;
}

static char *
get_key(enum request request)
{
	static char buf[BUFSIZ];
	static char key_char[] = "'X'";
	int pos = 0;
	char *sep = "    ";
	int i;

	buf[pos] = 0;

	for (i = 0; i < ARRAY_SIZE(default_keybindings); i++) {
		struct keybinding *keybinding = &default_keybindings[i];
		char *seq = NULL;
		int key;

		if (keybinding->request != request)
			continue;

		for (key = 0; key < ARRAY_SIZE(key_table); key++)
			if (key_table[key].value == keybinding->alias)
				seq = key_table[key].name;

		if (seq == NULL &&
		    keybinding->alias < 127 &&
		    isprint(keybinding->alias)) {
			key_char[1] = (char) keybinding->alias;
			seq = key_char;
		}

		if (!seq)
			seq = "'?'";

		if (!string_format_from(buf, &pos, "%s%s", sep, seq))
			return "Too many keybindings!";
		sep = ", ";
	}

	return buf;
}


/*
 * User config file handling.
 */

static struct int_map color_map[] = {
#define COLOR_MAP(name) { #name, STRING_SIZE(#name), COLOR_##name }
	COLOR_MAP(DEFAULT),
	COLOR_MAP(BLACK),
	COLOR_MAP(BLUE),
	COLOR_MAP(CYAN),
	COLOR_MAP(GREEN),
	COLOR_MAP(MAGENTA),
	COLOR_MAP(RED),
	COLOR_MAP(WHITE),
	COLOR_MAP(YELLOW),
};

#define set_color(color, name) \
	set_from_int_map(color_map, ARRAY_SIZE(color_map), color, name, strlen(name))

static struct int_map attr_map[] = {
#define ATTR_MAP(name) { #name, STRING_SIZE(#name), A_##name }
	ATTR_MAP(NORMAL),
	ATTR_MAP(BLINK),
	ATTR_MAP(BOLD),
	ATTR_MAP(DIM),
	ATTR_MAP(REVERSE),
	ATTR_MAP(STANDOUT),
	ATTR_MAP(UNDERLINE),
};

#define set_attribute(attr, name) \
	set_from_int_map(attr_map, ARRAY_SIZE(attr_map), attr, name, strlen(name))

static int   config_lineno;
static bool  config_errors;
static char *config_msg;

/* Wants: object fgcolor bgcolor [attr] */
static int
option_color_command(int argc, char *argv[])
{
	struct line_info *info;
	if (argc != 3 && argc != 4) {
		config_msg = "Wrong number of arguments given to color command";
		return ERR;
	info = get_line_info(argv[0], strlen(argv[0]));
	if (!info) {
		config_msg = "Unknown color name";
		return ERR;
	}
	if (set_color(&info->fg, argv[1]) == ERR ||
	    set_color(&info->bg, argv[2]) == ERR) {
		config_msg = "Unknown color";
		return ERR;
	}

	if (argc == 4 && set_attribute(&info->attr, argv[3]) == ERR) {
		config_msg = "Unknown attribute";
		return ERR;
	}

	return OK;
/* Wants: name = value */
static int
option_set_command(int argc, char *argv[])
	if (argc != 3) {
		config_msg = "Wrong number of arguments given to set command";
		return ERR;
	}
	if (strcmp(argv[1], "=")) {
		config_msg = "No value assigned";
		return ERR;
	}
	if (!strcmp(argv[0], "show-rev-graph")) {
		opt_rev_graph = (!strcmp(argv[2], "1") ||
				 !strcmp(argv[2], "true") ||
				 !strcmp(argv[2], "yes"));
		return OK;
	if (!strcmp(argv[0], "line-number-interval")) {
		opt_num_interval = atoi(argv[2]);
		return OK;
	}
	if (!strcmp(argv[0], "tab-size")) {
		opt_tab_size = atoi(argv[2]);
		return OK;
	}

	if (!strcmp(argv[0], "commit-encoding")) {
		char *arg = argv[2];
		int delimiter = *arg;
		int i;

		switch (delimiter) {
		case '"':
		case '\'':
			for (arg++, i = 0; arg[i]; i++)
				if (arg[i] == delimiter) {
					arg[i] = 0;
					break;
				}
		default:
			string_copy(opt_encoding, arg);
			return OK;
		}

	config_msg = "Unknown variable name";
	return ERR;
/* Wants: mode request key */
static int
option_bind_command(int argc, char *argv[])
{
	enum request request;
	int keymap;
	int key;

	if (argc != 3) {
		config_msg = "Wrong number of arguments given to bind command";
		return ERR;
	}
	if (set_keymap(&keymap, argv[0]) == ERR) {
		config_msg = "Unknown key map";
		return ERR;
	}
	key = get_key_value(argv[1]);
	if (key == ERR) {
		config_msg = "Unknown key";
		return ERR;
	}
	request = get_request(argv[2]);
	if (request == REQ_UNKNOWN) {
		config_msg = "Unknown request name";
		return ERR;
	}
	add_keybinding(keymap, request, key);
	return OK;
}
set_option(char *opt, char *value)
{
	char *argv[16];
	int valuelen;
	int argc = 0;
	/* Tokenize */
	while (argc < ARRAY_SIZE(argv) && (valuelen = strcspn(value, " \t"))) {
		argv[argc++] = value;
		value += valuelen;
		if (!*value)
			break;
		*value++ = 0;
		while (isspace(*value))
			value++;
	if (!strcmp(opt, "color"))
		return option_color_command(argc, argv);

	if (!strcmp(opt, "set"))
		return option_set_command(argc, argv);

	if (!strcmp(opt, "bind"))
		return option_bind_command(argc, argv);

	config_msg = "Unknown option command";
	int status = OK;

	/* Check for comment markers, since read_properties() will
	 * only ensure opt and value are split at first " \t". */
	optlen = strcspn(opt, "#");
	if (optlen == 0)
	if (opt[optlen] != 0) {
		config_msg = "No option value";
		status = ERR;

	}  else {
		/* Look for comment endings in the value. */
		int len = strcspn(value, "#");

		if (len < valuelen) {
			valuelen = len;
			value[valuelen] = 0;
		}

		status = set_option(opt, value);
	if (status == ERR) {
		fprintf(stderr, "Error on line %d, near '%.*s': %s\n",
	char buf[SIZEOF_STR];
	if (!home || !string_format(buf, "%s/.tigrc", home))
	enum keymap keymap;	/* What keymap does this view have */

	char cmd[SIZEOF_STR];	/* Command buffer */
	/* Searching */
	char grep[SIZEOF_STR];	/* Search string */
	regex_t regex;		/* Pre-compiled regex */

	bool (*read)(struct view *view, char *data);
	/* Search for regex in a line. */
	bool (*grep)(struct view *view, struct line *line);
#define VIEW_STR(name, cmd, env, ref, ops, map) \
	{ name, cmd, #env, ref, ops, map}
	VIEW_STR(name, TIG_##id##_CMD,  TIG_##id##_CMD, ref, ops, KEYMAP_##id)
		unsigned int view_lines = view->offset + view->height;
				   ? MIN(view_lines, view->lines) * 100 / view->lines
/*
 * Searching
 */

static void search_view(struct view *view, enum request request, const char *search);

static bool
find_next_line(struct view *view, unsigned long lineno, struct line *line)
{
	if (!view->ops->grep(view, line))
		return FALSE;

	if (lineno - view->offset >= view->height) {
		view->offset = lineno;
		view->lineno = lineno;
		redraw_view(view);

	} else {
		unsigned long old_lineno = view->lineno - view->offset;

		view->lineno = lineno;

		wmove(view->win, old_lineno, 0);
		wclrtoeol(view->win);
		draw_view_line(view, old_lineno);

		draw_view_line(view, view->lineno - view->offset);
		redrawwin(view->win);
		wrefresh(view->win);
	}

	report("Line %ld matches '%s'", lineno + 1, view->grep);
	return TRUE;
}

static void
find_next(struct view *view, enum request request)
{
	unsigned long lineno = view->lineno;
	int direction;

	if (!*view->grep) {
		if (!*opt_search)
			report("No previous search");
		else
			search_view(view, request, opt_search);
		return;
	}

	switch (request) {
	case REQ_SEARCH:
	case REQ_FIND_NEXT:
		direction = 1;
		break;

	case REQ_SEARCH_BACK:
	case REQ_FIND_PREV:
		direction = -1;
		break;

	default:
		return;
	}

	if (request == REQ_FIND_NEXT || request == REQ_FIND_PREV)
		lineno += direction;

	/* Note, lineno is unsigned long so will wrap around in which case it
	 * will become bigger than view->lines. */
	for (; lineno < view->lines; lineno += direction) {
		struct line *line = &view->line[lineno];

		if (find_next_line(view, lineno, line))
			return;
	}

	report("No match found for '%s'", view->grep);
}

static void
search_view(struct view *view, enum request request, const char *search)
{
	int regex_err;

	if (*view->grep) {
		regfree(&view->regex);
		*view->grep = 0;
	}

	regex_err = regcomp(&view->regex, search, REG_EXTENDED);
	if (regex_err != 0) {
		char buf[SIZEOF_STR] = "unknown error";

		regerror(regex_err, &view->regex, buf, sizeof(buf));
		report("Search failed: %s", buf);;
		return;
	}

	string_copy(view->grep, search);

	find_next(view, request);
}

		if (!string_format(view->cmd, format, id, id, id, id, id))
	char in_buffer[BUFSIZ];
	char out_buffer[BUFSIZ * 2];
	while ((line = fgets(in_buffer, sizeof(in_buffer), view->pipe))) {
		size_t linelen = strlen(line);
		if (opt_iconv != ICONV_NONE) {
			char *inbuf = line;
			size_t inlen = linelen;

			char *outbuf = out_buffer;
			size_t outlen = sizeof(out_buffer);

			size_t ret;

			ret = iconv(opt_iconv, &inbuf, &inlen, &outbuf, &outlen);
			if (ret != (size_t) -1) {
				line = out_buffer;
				linelen = strlen(out_buffer);
			}
		}

		if (!view->ops->read(view, line))

/*
 * View opening
 */

static void open_help_view(struct view *view)
{
	char buf[BUFSIZ];
	int lines = ARRAY_SIZE(req_info) + 2;
	int i;

	if (view->lines > 0)
		return;

	for (i = 0; i < ARRAY_SIZE(req_info); i++)
		if (!req_info[i].request)
			lines++;

	view->line = calloc(lines, sizeof(*view->line));
	if (!view->line) {
		report("Allocation failure");
		return;
	}

	view->ops->read(view, "Quick reference for tig keybindings:");

	for (i = 0; i < ARRAY_SIZE(req_info); i++) {
		char *key;

		if (!req_info[i].request) {
			view->ops->read(view, "");
			view->ops->read(view, req_info[i].help);
			continue;
		}

		key = get_key(req_info[i].request);
		if (!string_format(buf, "%-25s %s", key, req_info[i].help))
			continue;

		view->ops->read(view, buf);
	}
}

	if (view == VIEW(REQ_VIEW_HELP)) {
		open_help_view(view);

	} else if ((reload || strcmp(view->vid, view->id)) &&
		   !begin_update(view)) {
	case REQ_TOGGLE_REV_GRAPH:
		opt_rev_graph = !opt_rev_graph;
		redraw_display();
		break;

	case REQ_SEARCH:
	case REQ_SEARCH_BACK:
		search_view(view, request, opt_search);
		break;

	case REQ_FIND_NEXT:
	case REQ_FIND_PREV:
		find_next(view, request);
		break;

	case REQ_NONE:
add_describe_ref(char *buf, int *bufpos, char *commit_id, const char *sep)
	char refbuf[SIZEOF_STR];
	char *ref = NULL;
	FILE *pipe;

	if (!string_format(refbuf, "git describe %s", commit_id))
		return TRUE;

	pipe = popen(refbuf, "r");
	if (!pipe)
		return TRUE;

	if ((ref = fgets(refbuf, sizeof(refbuf), pipe)))
		ref = chomp_string(ref);
	pclose(pipe);

	if (!ref || !*ref)
		return TRUE;

	/* This is the only fatal call, since it can "corrupt" the buffer. */
	if (!string_nformat(buf, SIZEOF_STR, bufpos, "%s%s", sep, ref))
	return TRUE;
}

static void
add_pager_refs(struct view *view, struct line *line)
{
	char buf[SIZEOF_STR];
	char *commit_id = line->data + STRING_SIZE("commit ");
	struct ref **refs;
	int bufpos = 0, refpos = 0;
	const char *sep = "Refs: ";
	bool is_tag = FALSE;

	assert(line->type == LINE_COMMIT);

	refs = get_refs(commit_id);
	if (!refs) {
		if (view == VIEW(REQ_VIEW_DIFF))
			goto try_add_describe_ref;
		return;
	}

	do {
		struct ref *ref = refs[refpos];
		char *fmt = ref->tag ? "%s[%s]" : "%s%s";

		if (!string_format_from(buf, &bufpos, fmt, sep, ref->name))
			return;
		sep = ", ";
		if (ref->tag)
			is_tag = TRUE;
	} while (refs[refpos++]->next);

	if (!is_tag && view == VIEW(REQ_VIEW_DIFF)) {
try_add_describe_ref:
		if (!add_describe_ref(buf, &bufpos, commit_id, sep))
			return;
	}

	if (!realloc_lines(view, view->line_size + 1))
		return;

	line = &view->line[view->lines];
	line->data = strdup(buf);
	if (!line->data)
		return;

	line->type = LINE_PP_REFS;
	view->lines++;
}

static bool
pager_read(struct view *view, char *data)
{
	struct line *line = &view->line[view->lines];
	line->data = strdup(data);
	if (!line->data)
		return FALSE;

	line->type = get_line_type(line->data);

	if (line->type == LINE_COMMIT &&
	    (view == VIEW(REQ_VIEW_DIFF) ||
	     view == VIEW(REQ_VIEW_LOG)))
		add_pager_refs(view, line);

	/* FIXME: A minor workaround. Scrolling the view will call report("")
	 * but if we are scrolling a non-current view this won't properly
	 * update the view title. */
	if (split)
		update_view_title(view);

	return TRUE;
}

static bool
pager_grep(struct view *view, struct line *line)
{
	regmatch_t pmatch;
	char *text = line->data;

	if (!*text)
		return FALSE;

	if (regexec(&view->regex, text, 1, &pmatch, 0) == REG_NOMATCH)
		return FALSE;
	pager_grep,
	char id[41];			/* SHA1 ID. */
	char title[75];			/* First line of the commit message. */
	char author[75];		/* Author of the commit. */
	struct tm time;			/* Date from the author ident. */
	struct ref **refs;		/* Repository references. */
	chtype graph[SIZEOF_REVGRAPH];	/* Ancestry chain graphics. */
	size_t graph_size;		/* The width of the graph array. */
	if (opt_rev_graph && commit->graph_size) {
		size_t i;

		wmove(view->win, lineno, col);
		/* Using waddch() instead of waddnstr() ensures that
		 * they'll be rendered correctly for the cursor line. */
		for (i = 0; i < commit->graph_size; i++)
			waddch(view->win, commit->graph[i]);

		col += commit->graph_size + 1;
	}

	wmove(view->win, lineno, col);
main_read(struct view *view, char *line)
	struct commit *commit = view->lines
			      ? view->line[view->lines - 1].data : NULL;
		commit->graph[commit->graph_size++] = ACS_LTEE;
		if (!commit)
			char *email = end + 1;


			if (end == ident && *email) {
				ident = email;
				end = strchr(ident, '>');
				for (; end > ident && isspace(end[-1]); end--) ;
			}
		/* End is NULL or ident meaning there's no author. */
		if (end <= ident)
			ident = "Unknown";

		if (!commit)
static bool
main_grep(struct view *view, struct line *line)
	struct commit *commit = line->data;
	enum { S_TITLE, S_AUTHOR, S_DATE, S_END } state;
	char buf[DATE_COLS + 1];
	regmatch_t pmatch;

	for (state = S_TITLE; state < S_END; state++) {
		char *text;

		switch (state) {
		case S_TITLE:	text = commit->title;	break;
		case S_AUTHOR:	text = commit->author;	break;
		case S_DATE:
			if (!strftime(buf, sizeof(buf), DATE_FORMAT, &commit->time))
				continue;
			text = buf;
			break;
		default:
			return FALSE;
		if (regexec(&view->regex, text, 1, &pmatch, 0) != REG_NOMATCH)
			return TRUE;
	return FALSE;
static struct view_ops main_ops = {
	"commit",
	main_draw,
	main_read,
	main_enter,
	main_grep,
};
		if (!io)
			die("Failed to open /dev/tty");
static char *
read_prompt(const char *prompt)
{
	enum { READING, STOP, CANCEL } status = READING;
	static char buf[sizeof(opt_cmd) - STRING_SIZE("git \0")];
	int pos = 0;

	while (status == READING) {
		struct view *view;
		int i, key;

		foreach_view (view, i)
			update_view(view);

		report("%s%.*s", prompt, pos, buf);
		/* Refresh, accept single keystroke of input */
		key = wgetch(status_win);
		switch (key) {
		case KEY_RETURN:
		case KEY_ENTER:
		case '\n':
			status = pos ? STOP : CANCEL;
			break;

		case KEY_BACKSPACE:
			if (pos > 0)
				pos--;
			else
				status = CANCEL;
			break;

		case KEY_ESC:
			status = CANCEL;
			break;

		case ERR:
			break;

		default:
			if (pos >= sizeof(buf)) {
				report("Input string too long");
				return NULL;
			}

			if (isprint(key))
				buf[pos++] = (char) key;
		}
	}

	if (status == CANCEL) {
		/* Clear the status window */
		report("");
		return NULL;
	}

	buf[pos++] = 0;

	return buf;
}
	if (!strncmp(name, "refs/tags/", STRING_SIZE("refs/tags/"))) {
		/* Commits referenced by tags has "^{}" appended. */
		if (name[namelen - 1] != '}')
			return OK;

		namelen -= STRING_SIZE("refs/tags/");
		name	+= STRING_SIZE("refs/tags/");
		namelen -= STRING_SIZE("refs/heads/");
		name	+= STRING_SIZE("refs/heads/");
	ref->name = malloc(namelen + 1);
	strncpy(ref->name, name, namelen);
	ref->name[namelen] = 0;
	if (!strcmp(name, "i18n.commitencoding"))
	if (setlocale(LC_ALL, "")) {
		string_copy(opt_codeset, nl_langinfo(CODESET));
	}

	if (*opt_codeset && strcmp(opt_codeset, opt_encoding)) {
		opt_iconv = iconv_open(opt_codeset, opt_encoding);
		if (opt_iconv == (iconv_t) -1)
			die("Failed to initialize character set conversion");
	}


		request = get_keybinding(display[current_view]->keymap, key);
		{
			char *cmd = read_prompt(":");

			if (cmd && string_format(opt_cmd, "git %s", cmd)) {
				if (strncmp(cmd, "show", 4) && isspace(cmd[4])) {
					opt_request = REQ_VIEW_DIFF;
				} else {
					opt_request = REQ_VIEW_PAGER;
				}
				break;
			request = REQ_NONE;
		}
		case REQ_SEARCH:
		case REQ_SEARCH_BACK:
		{
			const char *prompt = request == REQ_SEARCH
					   ? "/" : "?";
			char *search = read_prompt(prompt);
			if (search)
				string_copy(opt_search, search);
			else
				request = REQ_NONE;
			break;
		}