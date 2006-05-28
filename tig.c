 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
static int read_properties(const char *cmd, int separator, int (*read)(char *, int, char *, int));
	REQ_NEXT,
	REQ_PREVIOUS,
static struct ref **get_refs(char *id);
static const char usage[] =
VERSION " (" __DATE__ ")\n"
"\n"
"Usage: tig [options]\n"
"   or: tig [options] [--] [git log options]\n"
"   or: tig [options] log  [git log options]\n"
"   or: tig [options] diff [git diff options]\n"
"   or: tig [options] show [git show options]\n"
"   or: tig [options] <    [git command output]\n"
"\n"
"Options:\n"
"  -l                          Start up in log view\n"
"  -d                          Start up in diff view\n"
"  -n[I], --line-number[=I]    Show line numbers with given interval\n"
"  -t[N], --tab-size[=N]       Set number of spaces for tab expansion\n"
"  --                          Mark end of tig options\n"
"  -v, --version               Show version and exit\n"
"  -h, --help                  Show help message and exit\n";

static char opt_encoding[20]	= "";
static bool opt_utf8		= TRUE;
		 * -b[NSPACES], --tab-size[=NSPACES]::
		if (!strncmp(opt, "-b", 2) ||
			if (opt[1] == 'b') {
		/**
		 * -h, --help::
		 *	Show help message and exit.
		 **/
		if (!strcmp(opt, "-h") ||
		    !strcmp(opt, "--help")) {
			printf(usage);
			return FALSE;
		}

	if (*opt_encoding && strcasecmp(opt_encoding, "UTF-8"))
		opt_utf8 = FALSE;

/**
 * ENVIRONMENT VARIABLES
 * ---------------------
 * Several options related to the interface with git can be configured
 * via environment options.
 *
 * Repository references
 * ~~~~~~~~~~~~~~~~~~~~~
 * Commits that are referenced by tags and branch heads will be marked
 * by the reference name surrounded by '[' and ']':
 *
 *	2006-03-26 19:42 Petr Baudis         | [cogito-0.17.1] Cogito 0.17.1
 *
 * If you want to filter out certain directories under `.git/refs/`, say
 * `tmp` you can do it by setting the following variable:
 *
 *	$ TIG_LS_REMOTE="git ls-remote . | sed /\/tmp\//d" tig
 *
 * Or set the variable permanently in your environment.
 *
 * TIG_LS_REMOTE::
 *	Set command for retrieving all repository references. The command
 *	should output data in the same format as git-ls-remote(1).
 **/

#define TIG_LS_REMOTE \
	"git ls-remote . 2>/dev/null"

/**
 * [[history-commands]]
 * History commands
 * ~~~~~~~~~~~~~~~~
 * It is possible to alter which commands are used for the different views.
 * If for example you prefer commits in the main view to be sorted by date
 * and only show 500 commits, use:
 *
 *	$ TIG_MAIN_CMD="git log --date-order -n500 --pretty=raw %s" tig
 *
 * Or set the variable permanently in your environment.
 *
 * Notice, how `%s` is used to specify the commit reference. There can
 * be a maximum of 5 `%s` ref specifications.
 *
 * TIG_DIFF_CMD::
 *	The command used for the diff view. By default, git show is used
 *	as a backend.
 *
 * TIG_LOG_CMD::
 *	The command used for the log view. If you prefer to have both
 *	author and committer shown in the log view be sure to pass
 *	`--pretty=fuller` to git log.
 *
 * TIG_MAIN_CMD::
 *	The command used for the main view. Note, you must always specify
 *	the option: `--pretty=raw` since the main view parser expects to
 *	read that format.
 **/

#define TIG_DIFF_CMD \
	"git show --patch-with-stat --find-copies-harder -B -C %s"

#define TIG_LOG_CMD	\
	"git log --cc --stat -n100 %s"

#define TIG_MAIN_CMD \
	"git log --topo-order --stat --pretty=raw %s"

/* ... silently ignore that the following are also exported. */

#define TIG_HELP_CMD \
	"man tig 2>/dev/null"

#define TIG_PAGER_CMD \
	""


struct line {
	enum line_type type;
	void *data;		/* User data */
};
struct view_ops;
#define displayed_views()	(display[1] != NULL ? 2 : 1)

	struct view_ops *ops;	/* View operations */
	/* If non-NULL, points to the view that opened this view. If this view
	 * is closed tig will switch back to the parent view. */
	struct view *parent;

	struct line *line;	/* Line index */
struct view_ops {
	/* What type of content being displayed. Used in the title bar. */
	const char *type;
	/* Draw one line; @lineno must be < view->height. */
	bool (*draw)(struct view *view, struct line *line, unsigned int lineno);
	/* Read one line; updates view->line. */
	bool (*read)(struct view *view, struct line *prev, char *data);
	/* Depending on view, change display based on current line. */
	bool (*enter)(struct view *view, struct line *line);
};

#define VIEW_STR(name, cmd, env, ref, ops) \
	{ name, cmd, #env, ref, ops }
#define VIEW_(id, name, ops, ref) \
	VIEW_STR(name, TIG_##id##_CMD,  TIG_##id##_CMD, ref, ops)
	VIEW_(MAIN,  "main",  &main_ops,  ref_head),
	VIEW_(DIFF,  "diff",  &pager_ops, ref_commit),
	VIEW_(LOG,   "log",   &pager_ops, ref_head),
	VIEW_(HELP,  "help",  &pager_ops, "static"),
	VIEW_(PAGER, "pager", &pager_ops, "static"),
static bool
draw_view_line(struct view *view, unsigned int lineno)
{
	if (view->offset + lineno >= view->lines)
		return FALSE;

	return view->ops->draw(view, &view->line[view->offset + lineno], lineno);
}

		if (!draw_view_line(view, lineno))
	if (view->lines || view->pipe) {
		unsigned int lines = view->lines
				   ? (view->lineno + 1) * 100 / view->lines
				   : 0;

			lines);
	if (view->pipe) {
		time_t secs = time(NULL) - view->start_time;

		/* Three git seconds are a long time ... */
		if (secs > 2)
			wprintw(view->title, " %lds", secs);
	}

	wmove(view->title, 0, view->width - 1);
			view->win = newwin(view->height, 0, offset, 0);
			wresize(view->win, view->height, view->width);
static void
update_display_cursor(void)
{
	struct view *view = display[current_view];

	/* Move the cursor to the right-most column of the cursor line.
	 *
	 * XXX: This could turn out to be a bit expensive, but it ensures that
	 * the cursor does not jump around. */
	if (view->lines) {
		wmove(view->win, view->lineno - view->offset, view->width - 1);
		wrefresh(view->win);
	}
}
do_scroll_view(struct view *view, int lines, bool redraw)
			if (!draw_view_line(view, line))
		draw_view_line(view, 0);
		draw_view_line(view, view->lineno - view->offset);
	if (!redraw)
		return;

	do_scroll_view(view, lines, TRUE);
move_view(struct view *view, enum request request, bool redraw)
		draw_view_line(view,  prev_lineno);
		do_scroll_view(view, steps, redraw);
	draw_view_line(view, view->lineno - view->offset);

	if (!redraw)
		return;
static void
end_update(struct view *view)
{
	if (!view->pipe)
		return;
	set_nonblocking_input(FALSE);
	if (view->pipe == stdin)
		fclose(view->pipe);
	else
		pclose(view->pipe);
	view->pipe = NULL;
}

	if (view->pipe)
		end_update(view);

			if (view->line[i].data)
				free(view->line[i].data);
	struct line *tmp;
		struct line *prev = view->lines
				  ? &view->line[view->lines - 1]
				  : NULL;

		if (!view->ops->read(view, prev, line))
		report("");
	int nviews = displayed_views();
	struct view *base_view = display[0];
	/* Resize the view when switching between split- and full-screen,
	 * or when switching between two different full-screen views. */
	if (nviews != displayed_views() ||
	    (nviews == 1 && base_view != display[0]))
		resize_display();
		do_scroll_view(prev, lines, TRUE);
		if (split && !backgrounded) {
			/* "Blur" the previous view. */
		}
		view->parent = prev;
	if (view->pipe && view->lines == 0) {
		report("");
		move_view(view, request, TRUE);
	case REQ_NEXT:
	case REQ_PREVIOUS:
		request = request == REQ_NEXT ? REQ_MOVE_DOWN : REQ_MOVE_UP;

		if (view == VIEW(REQ_VIEW_DIFF) &&
		    view->parent == VIEW(REQ_VIEW_MAIN)) {
			bool redraw = display[1] == view;

			view = view->parent;
			move_view(view, request, redraw);
			if (redraw)
				update_view_title(view);
		} else {
			move_view(view, request, TRUE);
			break;
		}
		return view->ops->enter(view, &view->line[view->lineno]);
		int nviews = displayed_views();
		for (i = 0; i < ARRAY_SIZE(views); i++) {
			view = &views[i];
				report("Stopped loading the %s view", view->name),
		report("%s (built %s)", VERSION, __DATE__);
		/* XXX: Mark closed views by letting view->parent point to the
		 * view itself. Parents to closed view should never be
		 * followed. */
		if (view->parent &&
		    view->parent->parent != view->parent) {
			display[current_view] = view->parent;
			view->parent = view;
 * Pager backend
pager_draw(struct view *view, struct line *line, unsigned int lineno)
	char *text = line->data;
	enum line_type type = line->type;
	int textlen = strlen(text);
			string_copy(view->ref, text + 7);
		while (text && col_offset + col < view->width) {
			char *pos = text;
			if (*text == '\t') {
				text++;
				pos = spaces;
				text = strchr(text, '\t');
				cols = line ? text - pos : strlen(pos);
			waddnstr(view->win, pos, MIN(cols, cols_max));
		for (; pos < textlen && col < view->width; pos++, col++)
			if (text[pos] == '\t')
		waddnstr(view->win, text, pos);
pager_read(struct view *view, struct line *prev, char *line)
	    !*line && prev && !*((char *) prev->data))
	view->line[view->lines].data = strdup(line);
	if (!view->line[view->lines].data)
	view->line[view->lines].type = get_line_type(line);

pager_enter(struct view *view, struct line *line)
	int split = 0;
	if (line->type == LINE_COMMIT &&
	   (view == VIEW(REQ_VIEW_LOG) ||
	    view == VIEW(REQ_VIEW_PAGER))) {
		open_view(view, REQ_VIEW_DIFF, OPEN_SPLIT);
		split = 1;
	/* Always scroll the view even if it was split. That way
	 * you can use Enter to scroll through the log view and
	 * split open each commit diff. */
	scroll_view(view, REQ_SCROLL_LINE_DOWN);

	/* FIXME: A minor workaround. Scrolling the view will call report("")
	 * but if we are scrolling a non-current view this won't properly
	 * update the view title. */
	if (split)
		update_view_title(view);
/*
 * Main view backend
 */

struct commit {
	char id[41];		/* SHA1 ID. */
	char title[75];		/* The first line of the commit message. */
	char author[75];	/* The author of the commit. */
	struct tm time;		/* Date from the author ident. */
	struct ref **refs;	/* Repository references; tags & branch heads. */
};
main_draw(struct view *view, struct line *line, unsigned int lineno)
	struct commit *commit = line->data;
	int trimmed = 1;
	if (opt_utf8) {
		authorlen = utf8_length(commit->author, AUTHOR_COLS - 2, &col, &trimmed);
	} else {
		authorlen = strlen(commit->author);
		if (authorlen > AUTHOR_COLS - 2) {
			authorlen = AUTHOR_COLS - 2;
			trimmed = 1;
		}
	}
main_read(struct view *view, struct line *prev, char *line)
		view->line[view->lines++].data = commit;
		if (!prev)
			break;

		commit = prev->data;

		if (!prev)
		commit = prev->data;

main_enter(struct view *view, struct line *line)
	enum open_flags flags = display[0] == view ? OPEN_SPLIT : OPEN_DEFAULT;

	open_view(view, REQ_VIEW_DIFF, flags);
	 **/
	{ 'm',		REQ_VIEW_MAIN },
	{ 'd',		REQ_VIEW_DIFF },
	{ 'l',		REQ_VIEW_LOG },
	{ 'p',		REQ_VIEW_PAGER },
	{ 'h',		REQ_VIEW_HELP },

	/**
	 * View manipulation
	 * ~~~~~~~~~~~~~~~~~
	 *	Close view, if multiple views are open it will jump back to the
	 *	previous view in the view stack. If it is the last open view it
	 *	will quit. Use 'Q' to quit all views at once.
	 * Up::
	 *	This key is "context sensitive" and will move the cursor one
	 *	line up. However, uf you opened a diff view from the main view
	 *	(split- or full-screen) it will change the cursor to point to
	 *	the previous commit in the main view and update the diff view
	 *	to display it.
	 * Down::
	 *	Similar to 'Up' but will move down.
	{ KEY_UP,	REQ_PREVIOUS },
	{ KEY_DOWN,	REQ_NEXT },
	 * j::
	 *	Move cursor one line down.
	 * -::
	{ 'k',		REQ_MOVE_UP },
	{ 'j',		REQ_MOVE_DOWN },
	{ '-',		REQ_MOVE_PAGE_UP },
						/* CJK ... Yi */
	case 6:
static bool cursed = FALSE;
	update_display_cursor();
/* Id <-> ref store */
static struct ref ***id_refs;
static size_t id_refs_size;

	struct ref ***tmp_id_refs;
	struct ref **ref_list = NULL;
	size_t ref_list_size = 0;
	for (i = 0; i < id_refs_size; i++)
		if (!strcmp(id, id_refs[i][0]->id))
			return id_refs[i];

	tmp_id_refs = realloc(id_refs, (id_refs_size + 1) * sizeof(*id_refs));
	if (!tmp_id_refs)
		return NULL;

	id_refs = tmp_id_refs;

		tmp = realloc(ref_list, (ref_list_size + 1) * sizeof(*ref_list));
			if (ref_list)
				free(ref_list);
		ref_list = tmp;
		if (ref_list_size > 0)
			ref_list[ref_list_size - 1]->next = 1;
		ref_list[ref_list_size] = &refs[i];
		ref_list[ref_list_size]->next = 0;
		ref_list_size++;
	if (ref_list)
		id_refs[id_refs_size++] = ref_list;

	return ref_list;
}

static int
read_ref(char *id, int idlen, char *name, int namelen)
{
	struct ref *ref;
	bool tag = FALSE;
	bool tag_commit = FALSE;

	/* Commits referenced by tags has "^{}" appended. */
	if (name[namelen - 1] == '}') {
		while (namelen > 0 && name[namelen] != '^')
			namelen--;
		if (namelen > 0)
			tag_commit = TRUE;
		name[namelen] = 0;
	}

	if (!strncmp(name, "refs/tags/", STRING_SIZE("refs/tags/"))) {
		if (!tag_commit)
			return OK;
		name += STRING_SIZE("refs/tags/");
		tag = TRUE;

	} else if (!strncmp(name, "refs/heads/", STRING_SIZE("refs/heads/"))) {
		name += STRING_SIZE("refs/heads/");

	} else if (!strcmp(name, "HEAD")) {
		return OK;
	}

	refs = realloc(refs, sizeof(*refs) * (refs_size + 1));
	if (!refs)
		return ERR;

	ref = &refs[refs_size++];
	ref->name = strdup(name);
	if (!ref->name)
		return ERR;

	ref->tag = tag;
	string_copy(ref->id, id);

	return OK;
	return read_properties(cmd, '\t', read_ref);
}
static int
read_config_option(char *name, int namelen, char *value, int valuelen)
{
	if (!strcmp(name, "i18n.commitencoding")) {
		string_copy(opt_encoding, value);
	}
	return OK;
}
static int
load_config(void)
{
	return read_properties("git repo-config --list", '=',
			       read_config_option);
}
static int
read_properties(const char *cmd, int separator,
		int (*read_property)(char *, int, char *, int))
{
	FILE *pipe = popen(cmd, "r");
	char buffer[BUFSIZ];
	char *name;
	int state = OK;
	if (!pipe)
		return ERR;
	while (state == OK && (name = fgets(buffer, sizeof(buffer), pipe))) {
		char *value = strchr(name, separator);
		int namelen;
		int valuelen;

		if (value) {
			namelen = value - name;
			*value++ = 0;
			valuelen = strlen(value);
			if (valuelen > 0) {
				valuelen--;
				value[valuelen] = 0;
			}
		} else {
			namelen = strlen(name);
			value = "";
			valuelen = 0;
		if (namelen)
			state = read_property(name, namelen, value, valuelen);
	if (state != ERR && ferror(pipe))
		state = ERR;
	return state;

	/* Load the repo config file first so options can be overwritten from
	 * the command line.  */
	if (load_config() == ERR)
		die("Failed to load repo config.");

	/* Require a git repository unless when running in pager mode. */
	if (refs_size == 0 && opt_request != REQ_VIEW_PAGER)
		die("Not a git repository");

				report("Prompt interrupted by loading view, "
				       "press 'z' to stop loading views");
				request = REQ_SCREEN_UPDATE;
 * variables described in the  <<history-commands, "History commands">>
 * section.
 * - The cursor can wrap-around on the last line and cause the
 *   window to scroll.
 *
 * - The prompt doesn't work while loading.
 *
 * Copyright (c) 2006 Jonas Fonseca <fonseca@diku.dk>
 * - link:http://www.kernel.org/pub/software/scm/git/docs/[git(7)],
 * - link:http://www.kernel.org/pub/software/scm/cogito/docs/[cogito(7)]
 *
 * Other git repository browsers:
 *
 *  - gitk(1)
 *  - qgit(1)
 *  - gitview(1)