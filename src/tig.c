/* Copyright (c) 2006-2014 Jonas Fonseca <jonas.fonseca@gmail.com>
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

#define WARN_MISSING_CURSES_CONFIGURATION

#include "tig/tig.h"
#include "tig/types.h"
#include "tig/util.h"
#include "tig/parse.h"
#include "tig/io.h"
#include "tig/argv.h"
#include "tig/refs.h"
#include "tig/graph.h"
#include "tig/git.h"
#include "tig/request.h"
#include "tig/line.h"
#include "tig/keys.h"
#include "tig/view.h"
#include "tig/repo.h"
#include "tig/options.h"
#include "tig/draw.h"
#include "tig/display.h"
#include "tig/grep.h"

static bool
forward_request_to_child(struct view *child, enum request request)
{
	return displayed_views() == 2 && view_is_displayed(child) &&
		!strcmp(child->vid, child->ops->id);
}

static enum request
view_request(struct view *view, enum request request)
{
	if (!view || !view->lines)
		return request;

	if (request == REQ_ENTER && !opt_focus_child &&
	    view_has_flags(view, VIEW_SEND_CHILD_ENTER)) {
		struct view *child = display[1];

	    	if (forward_request_to_child(child, request)) {
			view_request(child, request);
			return REQ_NONE;
		}
	}

	if (request == REQ_REFRESH && view->unrefreshable) {
		report("This view can not be refreshed");
		return REQ_NONE;
	}

	return view->ops->request(view, request, &view->line[view->pos.lineno]);
}

/*
 * Option management
 */

#define VIEW_FLAG_RESET_DISPLAY	((enum view_flag) -1)

#define TOGGLE_MENU_INFO(_) \
	_('.', "line numbers",      &opt_show_line_numbers, NULL, VIEW_NO_FLAGS), \
	_('D', "dates",             &opt_show_date, date_map, VIEW_NO_FLAGS), \
	_('A', "author",            &opt_show_author, author_map, VIEW_NO_FLAGS), \
	_('~', "graphics",          &opt_line_graphics, graphic_map, VIEW_NO_FLAGS), \
	_('g', "revision graph",    &opt_show_rev_graph, NULL, VIEW_LOG_LIKE), \
	_('#', "file names",        &opt_show_filename, filename_map, VIEW_NO_FLAGS), \
	_('*', "file sizes",        &opt_show_file_size, file_size_map, VIEW_NO_FLAGS), \
	_('W', "space changes",  &opt_ignore_space, ignore_space_map, VIEW_DIFF_LIKE), \
	_('l', "commit order",   &opt_commit_order, commit_order_map, VIEW_LOG_LIKE), \
	_('F', "reference display", &opt_show_refs, NULL, VIEW_NO_FLAGS), \
	_('C', "local change display", &opt_show_changes, NULL, VIEW_NO_FLAGS), \
	_('X', "commit ID display", &opt_show_id, NULL, VIEW_NO_FLAGS), \
	_('%', "file filtering",    &opt_file_filter, NULL, VIEW_DIFF_LIKE | VIEW_LOG_LIKE), \
	_('$', "commit title overflow display", &opt_title_overflow, NULL, VIEW_NO_FLAGS), \
	_('d', "untracked directory info", &opt_status_untracked_dirs, NULL, VIEW_STATUS_LIKE), \
	_('|', "view split",   &opt_vertical_split, vertical_split_map, VIEW_FLAG_RESET_DISPLAY), \

static enum view_flag
toggle_option(struct view *view, enum request request, char msg[SIZEOF_STR])
{
	const struct {
		const struct enum_map *map;
		enum view_flag reload_flags;
	} data[] = {
#define DEFINE_TOGGLE_DATA(key, help, value, map, vflags) { map, vflags }
		TOGGLE_MENU_INFO(DEFINE_TOGGLE_DATA)
	};
	const struct menu_item menu[] = {
#define DEFINE_TOGGLE_MENU(key, help, value, map, vflags) { key, help, value }
		TOGGLE_MENU_INFO(DEFINE_TOGGLE_MENU)
		{ 0 }
	};
	int i = 0;

	if (request == REQ_OPTIONS) {
		if (!prompt_menu("Toggle option", menu, &i))
			return VIEW_NO_FLAGS;
	} else {
		while (i < ARRAY_SIZE(data) && menu[i].data == &opt_file_filter)
			i++;
		if (i >= ARRAY_SIZE(data))
			die("Invalid request (%d)", request);
	}

	if (data[i].map != NULL) {
		unsigned int *opt = menu[i].data;

		*opt = (*opt + 1) % data[i].map->size;
		if (data[i].map == ignore_space_map) {
			string_format_size(msg, SIZEOF_STR,
				"Ignoring %s %s", enum_name(data[i].map->entries[*opt]), menu[i].text);

		} else if (data[i].map == commit_order_map) {
			string_format_size(msg, SIZEOF_STR,
				"Using %s %s", enum_name(data[i].map->entries[*opt]), menu[i].text);

		} else {
			string_format_size(msg, SIZEOF_STR,
				"Displaying %s %s", enum_name(data[i].map->entries[*opt]), menu[i].text);
		}

	} else if (menu[i].data == &opt_title_overflow) {
		int *option = menu[i].data;

		*option = *option ? -*option : 50;
		string_format_size(msg, SIZEOF_STR,
			"%sabling %s", *option > 0 ? "En" : "Dis", menu[i].text);

	} else {
		bool *option = menu[i].data;

		*option = !*option;
		string_format_size(msg, SIZEOF_STR,
			"%sabling %s", *option ? "En" : "Dis", menu[i].text);
	}

	return data[i].reload_flags;
}


/*
 * View opening
 */

static enum request run_prompt_command(struct view *view, const char *argv[]);

static enum request
open_run_request(struct view *view, enum request request)
{
	struct run_request *req = get_run_request(request);
	const char **argv = NULL;
	bool confirmed = FALSE;

	request = REQ_NONE;

	if (!req) {
		report("Unknown run request");
		return request;
	}

	if (argv_format(view->env, &argv, req->argv, FALSE, TRUE)) {
		if (req->flags.internal) {
			request = run_prompt_command(view, argv);
		}
		else {
			confirmed = !req->flags.confirm;

			if (req->flags.confirm) {
				char cmd[SIZEOF_STR], prompt[SIZEOF_STR];
				const char *and_exit = req->flags.exit ? " and exit" : "";

				if (argv_to_string(argv, cmd, sizeof(cmd), " ") &&
				    string_format(prompt, "Run `%s`%s?", cmd, and_exit) &&
				    prompt_yesno(prompt)) {
					confirmed = TRUE;
				}
			}

			if (confirmed && argv_remove_quotes(argv)) {
				if (req->flags.silent)
					io_run_bg(argv);
				else
					open_external_viewer(argv, NULL, !req->flags.exit, "");
			}
		}
	}

	if (argv)
		argv_free(argv);
	free(argv);

	if (request == REQ_NONE) {
		if (req->flags.confirm && !confirmed)
			request = REQ_NONE;

		else if (req->flags.exit)
			request = REQ_QUIT;

		else if (!req->flags.internal && view_has_flags(view, VIEW_REFRESH) && !view->unrefreshable)
			request = REQ_REFRESH;
	}
	return request;
}

/*
 * User request switch noodle
 */

static int
view_driver(struct view *view, enum request request)
{
	int i;

	if (request == REQ_NONE)
		return TRUE;

	if (request >= REQ_RUN_REQUESTS) {
		request = open_run_request(view, request);

		// exit quickly rather than going through view_request and back
		if (request == REQ_QUIT)
			return FALSE;
	}

	request = view_request(view, request);
	if (request == REQ_NONE)
		return TRUE;

	switch (request) {
	case REQ_MOVE_UP:
	case REQ_MOVE_DOWN:
	case REQ_MOVE_PAGE_UP:
	case REQ_MOVE_PAGE_DOWN:
	case REQ_MOVE_FIRST_LINE:
	case REQ_MOVE_LAST_LINE:
		move_view(view, request);
		break;

	case REQ_SCROLL_FIRST_COL:
	case REQ_SCROLL_LEFT:
	case REQ_SCROLL_RIGHT:
	case REQ_SCROLL_LINE_DOWN:
	case REQ_SCROLL_LINE_UP:
	case REQ_SCROLL_PAGE_DOWN:
	case REQ_SCROLL_PAGE_UP:
	case REQ_SCROLL_WHEEL_DOWN:
	case REQ_SCROLL_WHEEL_UP:
		scroll_view(view, request);
		break;

	case REQ_VIEW_GREP:
		open_grep_view(view);
		break;

	case REQ_VIEW_MAIN:
	case REQ_VIEW_DIFF:
	case REQ_VIEW_LOG:
	case REQ_VIEW_TREE:
	case REQ_VIEW_HELP:
	case REQ_VIEW_BRANCH:
	case REQ_VIEW_BLAME:
	case REQ_VIEW_BLOB:
	case REQ_VIEW_STATUS:
	case REQ_VIEW_STAGE:
	case REQ_VIEW_PAGER:
	case REQ_VIEW_STASH:
		open_view(view, request, OPEN_DEFAULT);
		break;

	case REQ_NEXT:
	case REQ_PREVIOUS:
		if (view->parent) {
			int line;

			view = view->parent;
			line = view->pos.lineno;
			view_request(view, request);
			move_view(view, request);
			if (view_is_displayed(view))
				update_view_title(view);
			if (line != view->pos.lineno)
				view_request(view, REQ_ENTER);
		} else {
			move_view(view, request);
		}
		break;

	case REQ_VIEW_NEXT:
	{
		int nviews = displayed_views();
		int next_view = (current_view + 1) % nviews;

		if (next_view == current_view) {
			report("Only one view is displayed");
			break;
		}

		current_view = next_view;
		/* Blur out the title of the previous view. */
		update_view_title(view);
		report_clear();
		break;
	}
	case REQ_REFRESH:
		report("Refreshing is not supported by the %s view", view->name);
		break;

	case REQ_PARENT:
		report("Moving to parent is not supported by the the %s view", view->name);
		break;

	case REQ_BACK:
		report("Going back is not supported for by %s view", view->name);
		break;

	case REQ_MAXIMIZE:
		if (displayed_views() == 2)
			maximize_view(view, TRUE);
		break;

	case REQ_OPTIONS:
		{
			char action[SIZEOF_STR] = "";
			enum view_flag flags = toggle_option(view, request, action);
	
			if (flags == VIEW_FLAG_RESET_DISPLAY) {
				resize_display();
				redraw_display(TRUE);
			} else {
				foreach_displayed_view(view, i) {
					if (view_has_flags(view, flags) && !view->unrefreshable)
						reload_view(view);
					else
						redraw_view(view);
				}
			}

			if (*action)
				report("%s", action);
		}
		break;

	case REQ_TOGGLE_SORT_FIELD:
	case REQ_TOGGLE_SORT_ORDER:
		report("Sorting is not yet supported for the %s view", view->name);
		break;

	case REQ_SEARCH:
	case REQ_SEARCH_BACK:
		search_view(view, request);
		break;

	case REQ_FIND_NEXT:
	case REQ_FIND_PREV:
		find_next(view, request);
		break;

	case REQ_STOP_LOADING:
		foreach_view(view, i) {
			if (view->pipe)
				report("Stopped loading the %s view", view->name),
			end_update(view, TRUE);
		}
		break;

	case REQ_SHOW_VERSION:
		report("tig-%s (built %s)", TIG_VERSION, __DATE__);
		return TRUE;

	case REQ_SCREEN_REDRAW:
		redraw_display(TRUE);
		break;

	case REQ_EDIT:
		report("Nothing to edit");
		break;

	case REQ_ENTER:
		report("Nothing to enter");
		break;

	case REQ_VIEW_CLOSE:
		/* XXX: Mark closed views by letting view->prev point to the
		 * view itself. Parents to closed view should never be
		 * followed. */
		if (view->prev && view->prev != view) {
			maximize_view(view->prev, TRUE);
			view->prev = view;
			break;
		}
		/* Fall-through */
	case REQ_QUIT:
		return FALSE;

	default:
		report("Unknown key, press %s for help",
		       get_view_key(view, REQ_VIEW_HELP));
		return TRUE;
	}

	return TRUE;
}

/*
 * Main
 */

static const char usage_string[] =
"tig " TIG_VERSION " (" __DATE__ ")\n"
"\n"
"Usage: tig        [options] [revs] [--] [paths]\n"
"   or: tig log    [options] [revs] [--] [paths]\n"
"   or: tig show   [options] [revs] [--] [paths]\n"
"   or: tig blame  [options] [rev] [--] path\n"
"   or: tig grep   [options] [pattern]\n"
"   or: tig stash\n"
"   or: tig status\n"
"   or: tig <      [git command output]\n"
"\n"
"Options:\n"
"  +<number>       Select line <number> in the first view\n"
"  -v, --version   Show version and exit\n"
"  -h, --help      Show help message and exit";

void
usage(const char *message)
{
	die("%s\n\n%s", message, usage_string);
}

static int
read_filter_args(char *name, size_t namelen, char *value, size_t valuelen, void *data)
{
	const char ***filter_args = data;

	return argv_append(filter_args, name) ? OK : ERR;
}

static void
filter_rev_parse(const char ***args, const char *arg1, const char *arg2, const char *argv[])
{
	const char *rev_parse_argv[SIZEOF_ARG] = { "git", "rev-parse", arg1, arg2 };
	const char **all_argv = NULL;

	if (!argv_append_array(&all_argv, rev_parse_argv) ||
	    !argv_append_array(&all_argv, argv) ||
	    io_run_load(all_argv, "\n", read_filter_args, args) == ERR)
		die("Failed to split arguments");
	argv_free(all_argv);
	free(all_argv);
}

static bool
is_rev_flag(const char *flag)
{
	static const char *rev_flags[] = { GIT_REV_FLAGS };
	int i;

	for (i = 0; i < ARRAY_SIZE(rev_flags); i++)
		if (!strcmp(flag, rev_flags[i]))
			return TRUE;

	return FALSE;
}

static void
filter_options(const char *argv[], bool rev_parse)
{
	const char **flags = NULL;
	int next, flags_pos;

	update_options_from_argv(argv);

	if (!rev_parse) {
		opt_cmdline_argv = argv;
		return;
	}

	filter_rev_parse(&opt_file_argv, "--no-revs", "--no-flags", argv);
	filter_rev_parse(&flags, "--flags", "--no-revs", argv);

	if (flags) {
		for (next = flags_pos = 0; flags && flags[next]; next++) {
			const char *flag = flags[next];

			if (is_rev_flag(flag))
				argv_append(&opt_rev_argv, flag);
			else
				flags[flags_pos++] = flag;
		}

		flags[flags_pos] = NULL;

		opt_cmdline_argv = flags;
	}

	filter_rev_parse(&opt_rev_argv, "--symbolic", "--revs-only", argv);
}

static enum request
parse_options(int argc, const char *argv[], bool pager_mode)
{
	enum request request;
	const char *subcommand;
	bool seen_dashdash = FALSE;
	bool rev_parse = TRUE;
	const char **filter_argv = NULL;
	int i;

	request = pager_mode ? REQ_VIEW_PAGER : REQ_VIEW_MAIN;

	if (argc <= 1)
		return request;

	subcommand = argv[1];
	if (!strcmp(subcommand, "status")) {
		request = REQ_VIEW_STATUS;

	} else if (!strcmp(subcommand, "blame")) {
		request = REQ_VIEW_BLAME;

	} else if (!strcmp(subcommand, "grep")) {
		request = REQ_VIEW_GREP;
		rev_parse = FALSE;

	} else if (!strcmp(subcommand, "show")) {
		request = REQ_VIEW_DIFF;

	} else if (!strcmp(subcommand, "log")) {
		request = REQ_VIEW_LOG;

	} else if (!strcmp(subcommand, "stash")) {
		request = REQ_VIEW_STASH;

	} else {
		subcommand = NULL;
	}

	for (i = 1 + !!subcommand; i < argc; i++) {
		const char *opt = argv[i];

		// stop parsing our options after -- and let rev-parse handle the rest
		if (!seen_dashdash) {
			if (!strcmp(opt, "--")) {
				seen_dashdash = TRUE;
				continue;

			} else if (!strcmp(opt, "-v") || !strcmp(opt, "--version")) {
				printf("tig version %s\n", TIG_VERSION);
				exit(EXIT_SUCCESS);

			} else if (!strcmp(opt, "-h") || !strcmp(opt, "--help")) {
				printf("%s\n", usage_string);
				exit(EXIT_SUCCESS);

			} else if (strlen(opt) >= 2 && *opt == '+' && string_isnumber(opt + 1)) {
				int lineno = atoi(opt + 1);

				argv_env.lineno = lineno > 0 ? lineno - 1 : 0;
				continue;

			}
		}

		if (!argv_append(&filter_argv, opt))
			die("command too long");
	}

	if (filter_argv)
		filter_options(filter_argv, rev_parse);

	return request;
}

static enum request
open_pager_mode(enum request request)
{
	enum open_flags flags = OPEN_DEFAULT;

	if (request == REQ_VIEW_PAGER) {
		/* Detect if the user requested the main view. */
		if (argv_contains(opt_rev_argv, "--stdin")) {
			request = REQ_VIEW_MAIN;
			flags |= OPEN_FORWARD_STDIN;
		} else if (argv_contains(opt_cmdline_argv, "--pretty=raw")) {
			request = REQ_VIEW_MAIN;
			flags |= OPEN_STDIN;
		} else {
			flags |= OPEN_STDIN;
		}

	} else if (request == REQ_VIEW_DIFF) {
		if (argv_contains(opt_rev_argv, "--stdin"))
			flags |= OPEN_FORWARD_STDIN;
	}

	/* Open the requested view even if the pager mode is enabled so
	 * the warning message below is displayed correctly. */
	open_view(NULL, request, flags);

	if (!open_in_pager_mode(flags)) {
		close(STDIN_FILENO);
		report("Ignoring stdin.");
	}

	return REQ_NONE;
}

struct prompt_toggle {
	const char *name;
	const char *type;
	enum view_flag flags;
	void *opt;
};

static enum view_flag
prompt_toggle_option(struct view *view, const char *argv[],
		     struct prompt_toggle *toggle, char msg[SIZEOF_STR])
{
	static struct {
		const char *type;
		const struct enum_map *map;
	} mappings[] = {
#define DEFINE_ENUM_MAPPING(name, macro) { #name, name##_map },
		ENUM_INFO(DEFINE_ENUM_MAPPING)
	};

	if (!strcmp(toggle->type, "bool")) {
		bool *opt = toggle->opt;

		*opt = !*opt;
		string_format_size(msg, SIZEOF_STR, "set %s = %s", toggle->name, *opt ? "yes" : "no");

	} else if (!strncmp(toggle->type, "enum", 4)) {
		const char *type = toggle->type + STRING_SIZE("enum ");
		enum author *opt = toggle->opt;
		int j;

		for (j = 0; j < ARRAY_SIZE(mappings); j++) {
			if (!strcmp(type, mappings[j].type)) {
				*opt = (*opt + 1) % mappings[j].map->size;
				string_format_size(msg, SIZEOF_STR,
						   "set %s = %s", toggle->name,
						    enum_name(mappings[j].map->entries[*opt]));
				break;
			}
		}

	} else if (!strcmp(toggle->type, "int")) {
		const char *arg = argv[2];
		int diff = atoi(arg);
		int *opt = toggle->opt;

		if (!diff)
			diff = *arg == '-' ? -1 : 1;

		if (opt == &opt_diff_context && !*opt && diff < 0) {
			report("Diff context cannot be less than zero");
			return VIEW_NO_FLAGS;
		}

		if (opt == &opt_title_overflow) {
			*opt = *opt ? -*opt : 50;
			if (*opt < 0) {
				string_format_size(msg, SIZEOF_STR, "set %s = no", toggle->name);
				return VIEW_NO_FLAGS;
			}
		}

		*opt += diff;
		string_format_size(msg, SIZEOF_STR, "set %s = %d", toggle->name, *opt);

	} else {
		die(":toggle %s (%s)", toggle->name, toggle->type);
	}

	return toggle->flags;
}

static enum view_flag
prompt_toggle(struct view *view, const char *argv[], char msg[SIZEOF_STR])
{
	struct prompt_toggle option_toggles[] = {
#define TOGGLE_OPTIONS(name, type, flags) { #name, #type, flags, &opt_ ## name },
		OPTION_INFO(TOGGLE_OPTIONS)
	};
	const char *name = argv[1];
	size_t namelen = strlen(name);
	int i;

	if (!name) {
		string_format_size(msg, SIZEOF_STR, "%s", "No option name given to :toggle");
		return VIEW_NO_FLAGS;
	}

	for (i = 0; i < ARRAY_SIZE(option_toggles); i++) {
		struct prompt_toggle *toggle = &option_toggles[i];

		if (namelen == strlen(toggle->name) &&
		    !string_enum_compare(toggle->name, name, namelen))
			return prompt_toggle_option(view, argv, toggle, msg);
	}

	string_format_size(msg, SIZEOF_STR, "`:toggle %s` not supported", name);
	return VIEW_NO_FLAGS;
}

static enum request
run_prompt_command(struct view *view, const char *argv[])
{
	enum request request;
	const char *cmd = argv[0];

	if (!cmd)
		return REQ_NONE;

	if (string_isnumber(cmd)) {
		int lineno = view->pos.lineno + 1;

		if (parse_int(&lineno, cmd, 1, view->lines + 1) == SUCCESS) {
			select_view_line(view, lineno - 1);
			report_clear();
		} else {
			report("Unable to parse '%s' as a line number", cmd);
		}
	} else if (iscommit(cmd)) {
		string_ncopy(view->env->search, cmd, strlen(cmd));

		request = view_request(view, REQ_JUMP_COMMIT);
		if (request == REQ_JUMP_COMMIT) {
			report("Jumping to commits is not supported by the '%s' view", view->name);
		}

	} else if (strlen(cmd) == 1) {
		struct key_input input = { { cmd[0] } };

		return get_keybinding(&view->ops->keymap, &input);

	} else if (cmd[0] == '/' || cmd[0] == '?') {
		char search[SIZEOF_STR];

		if (!argv_to_string(argv, search, sizeof(search), " ")) {
			report("Failed to copy search string");
			return REQ_NONE;
		}

		if (!strcmp(search + 1, view->env->search))
			return cmd[0] == '/' ? REQ_FIND_NEXT : REQ_FIND_PREV;

		string_ncopy(view->env->search, search + 1, strlen(search + 1));
		return cmd[0] == '/' ? REQ_SEARCH : REQ_SEARCH_BACK;

	} else if (cmd[0] == '!') {
		struct view *next = VIEW(REQ_VIEW_PAGER);
		bool copied;

		/* Trim the leading '!'. */
		argv[0] = cmd + 1;
		copied = argv_format(view->env, &next->argv, argv, FALSE, TRUE);
		argv[0] = cmd;

		if (!copied) {
			report("Argument formatting failed");
		} else {
			/* When running random commands, initially show the
			 * command in the title. However, it maybe later be
			 * overwritten if a commit line is selected. */
			argv_to_string(next->argv, next->ref, sizeof(next->ref), " ");

			next->dir = NULL;
			open_view(view, REQ_VIEW_PAGER, OPEN_PREPARED);
		}

	} else if (!strcmp(cmd, "toggle")) {
		char action[SIZEOF_STR] = "";
		enum view_flag flags = prompt_toggle(view, argv, action);
		int i;
	
		if (flags == VIEW_FLAG_RESET_DISPLAY) {
			resize_display();
			redraw_display(TRUE);
		} else {
			foreach_displayed_view(view, i) {
				if (view_has_flags(view, flags) && !view->unrefreshable)
					reload_view(view);
				else
					redraw_view(view);
			}
		}

		if (*action)
			report("%s", action);

	} else {
		request = get_request(cmd);
		if (request != REQ_UNKNOWN)
			return request;

		if (set_option(argv[0], argv_size(argv + 1), &argv[1]) == SUCCESS) {
			request = !view->unrefreshable ? REQ_REFRESH : REQ_SCREEN_REDRAW;
			if (!strcmp(cmd, "color"))
				init_colors();
			resize_display();
			redraw_display(TRUE);
		}
		return request;
	}
	return REQ_NONE;
}

#ifdef NCURSES_MOUSE_VERSION
static struct view *
find_clicked_view(MEVENT *event)
{
	struct view *view;
	int i;

	foreach_displayed_view (view, i) {
		int beg_y = 0, beg_x = 0;

		getbegyx(view->win, beg_y, beg_x);

		if (beg_y <= event->y && event->y < beg_y + view->height
		    && beg_x <= event->x && event->x < beg_x + view->width) {
			if (i != current_view) {
				current_view = i;
			}
			return view;
		}
	}

	return NULL;
}

static enum request
handle_mouse_event(void)
{
	MEVENT event;
	struct view *view;

	if (getmouse(&event) != OK)
		return REQ_NONE;

	view = find_clicked_view(&event);
	if (!view)
		return REQ_NONE;

	if (event.bstate & BUTTON2_PRESSED)
		return REQ_SCROLL_WHEEL_DOWN;

	if (event.bstate & BUTTON4_PRESSED)
		return REQ_SCROLL_WHEEL_UP;

	if (event.bstate & BUTTON1_PRESSED) {
		if (event.y == view->pos.lineno - view->pos.offset) {
			/* Click is on the same line, perform an "ENTER" */
			return REQ_ENTER;

		} else {
			int y = getbegy(view->win);
			unsigned long lineno = (event.y - y) + view->pos.offset;

			select_view_line(view, lineno);
			update_view_title(view);
			report_clear();
		}
	}

	return REQ_NONE;
}
#endif

int
main(int argc, const char *argv[])
{
	const char *codeset = ENCODING_UTF8;
	bool pager_mode = !isatty(STDIN_FILENO);
	enum request request = parse_options(argc, argv, pager_mode);
	struct view *view;
	int i;

	signal(SIGPIPE, SIG_IGN);

	if (setlocale(LC_ALL, "")) {
		codeset = nl_langinfo(CODESET);
	}

	foreach_view(view, i) {
		add_keymap(&view->ops->keymap);
	}

	if (load_repo_info() == ERR)
		die("Failed to load repo info.");

	if (load_options() == ERR)
		die("Failed to load user config.");

	if (load_git_config() == ERR)
		die("Failed to load repo config.");

	/* Require a git repository unless when running in pager mode. */
	if (!repo.git_dir[0] && request != REQ_VIEW_PAGER)
		die("Not a git repository");

	if (codeset && strcmp(codeset, ENCODING_UTF8)) {
		char translit[SIZEOF_STR];

		if (string_format(translit, "%s%s", codeset, ICONV_TRANSLIT))
			opt_iconv_out = iconv_open(translit, ENCODING_UTF8);
		else
			opt_iconv_out = iconv_open(codeset, ENCODING_UTF8);
		if (opt_iconv_out == ICONV_NONE)
			die("Failed to initialize character set conversion");
	}

	if (load_refs(FALSE) == ERR)
		die("Failed to load refs.");

	init_display();

	if (pager_mode)
		request = open_pager_mode(request);

	while (view_driver(display[current_view], request)) {
		struct key_input input;
		int key = get_input(0, &input, TRUE);

#ifdef NCURSES_MOUSE_VERSION
		if (key == KEY_MOUSE) {
			request = handle_mouse_event();
			continue;
		}
#endif

		view = display[current_view];
		request = get_keybinding(&view->ops->keymap, &input);

		/* Some low-level request handling. This keeps access to
		 * status_win restricted. */
		switch (request) {
		case REQ_NONE:
			report("Unknown key, press %s for help",
			       get_view_key(view, REQ_VIEW_HELP));
			break;
		case REQ_PROMPT:
		{
			char *cmd = read_prompt(":");
			const char *argv[SIZEOF_ARG] = { NULL };
			int argc = 0;

			if (cmd && !argv_from_string(argv, &argc, cmd))
				report("Too many arguments");
			else
				request = run_prompt_command(view, argv);
			break;
		}
		case REQ_SEARCH:
		case REQ_SEARCH_BACK:
		{
			const char *prompt = request == REQ_SEARCH ? "/" : "?";
			char *search = read_prompt(prompt);

			if (search)
				string_ncopy(argv_env.search, search, strlen(search));
			else if (*argv_env.search)
				request = request == REQ_SEARCH ?
					REQ_FIND_NEXT :
					REQ_FIND_PREV;
			else
				request = REQ_NONE;
			break;
		}
		default:
			break;
		}
	}

	exit(EXIT_SUCCESS);

	return 0;
}

/* vim: set ts=8 sw=8 noexpandtab: */
