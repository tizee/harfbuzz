/*
 * Copyright © 2011  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef TEXT_OPTIONS_HH
#define TEXT_OPTIONS_HH

#include "options.hh"

struct text_options_t
{
  text_options_t ()
  : gs (g_string_new (nullptr))
  {}
  ~text_options_t ()
  {
    g_free (text);
    g_free (text_file);
    if (gs)
      g_string_free (gs, true);
    if (in_fp && in_fp != stdin)
      fclose (in_fp);
  }

  void add_options (option_parser_t *parser);

  void post_parse (GError **error G_GNUC_UNUSED)
  {
    if (!text && !text_file)
      text_file = g_strdup ("-");

    if (text && text_file)
    {
      g_set_error (error,
		   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Only one of text and text-file can be set");
      return;
    }

    if (text_file)
    {
      if (0 != strcmp (text_file, "-"))
	in_fp = fopen (text_file, "r");
      else
	in_fp = stdin;

      if (!in_fp)
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		     "Failed opening text file `%s': %s",
		     text_file, strerror (errno));
    }
  }

  const char *get_line (unsigned int *len);

  int text_len = -1;
  char *text = nullptr;
  char *text_file = nullptr;

  private:
  FILE *in_fp = nullptr;
  GString *gs = nullptr;
};

struct shape_text_options_t : text_options_t
{
  ~shape_text_options_t ()
  {
    g_free (text_before);
    g_free (text_after);
  }

  void add_options (option_parser_t *parser);

  char *text_before = nullptr;
  char *text_after = nullptr;
};


static gboolean
parse_text (const char *name G_GNUC_UNUSED,
	    const char *arg,
	    gpointer    data,
	    GError    **error G_GNUC_UNUSED)
{
  text_options_t *text_opts = (text_options_t *) data;

  if (text_opts->text)
  {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		 "Either --text or --unicodes can be provided but not both");
    return false;
  }

  text_opts->text_len = -1;
  text_opts->text = g_strdup (arg);
  return true;
}


static gboolean
parse_unicodes (const char *name G_GNUC_UNUSED,
		const char *arg,
		gpointer    data,
		GError    **error G_GNUC_UNUSED)
{
  text_options_t *text_opts = (text_options_t *) data;

  if (text_opts->text)
  {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		 "Either --text or --unicodes can be provided but not both");
    return false;
  }

  GString *gs = g_string_new (nullptr);
  if (0 == strcmp (arg, "*"))
  {
    g_string_append_c (gs, '*');
  }
  else
  {

    char *s = (char *) arg;
    char *p;

    while (s && *s)
    {
#define DELIMITERS "<+>{},;&#\\xXuUnNiI\n\t\v\f\r "

      while (*s && strchr (DELIMITERS, *s))
	s++;
      if (!*s)
	break;

      errno = 0;
      hb_codepoint_t u = strtoul (s, &p, 16);
      if (errno || s == p)
      {
	g_string_free (gs, TRUE);
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		     "Failed parsing Unicode values at: '%s'", s);
	return false;
      }

      g_string_append_unichar (gs, u);

      s = p;
    }
  }

  text_opts->text_len = gs->len;
  text_opts->text = g_string_free (gs, FALSE);
  return true;
}

const char *
text_options_t::get_line (unsigned int *len)
{
  if (text)
  {
    if (text_len == -2)
    {
      *len = 0;
      return nullptr;
    }

    if (text_len == -1)
      text_len = strlen (text);

    *len = text_len;
    text_len = -2;
    return text;
  }

  g_string_set_size (gs, 0);
  char buf[BUFSIZ];
  while (fgets (buf, sizeof (buf), in_fp))
  {
    unsigned bytes = strlen (buf);
    if (bytes && buf[bytes - 1] == '\n')
    {
      bytes--;
      g_string_append_len (gs, buf, bytes);
      break;
    }
    g_string_append_len (gs, buf, bytes);
  }
  if (ferror (in_fp))
    fail (false, "Failed reading text: %s", strerror (errno));
  *len = gs->len;
  return !*len && feof (in_fp) ? nullptr : gs->str;
}

void
text_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"text",		0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_text,		"Set input text",			"string"},
    {"text-file",	0, 0, G_OPTION_ARG_STRING,	&this->text_file,		"Set input text file-name",		"filename"},
    {"unicodes",      'u', 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_unicodes,	"Set input Unicode codepoints\n\n    If no text is provided, standard input is used for input.",		"list of hex numbers"},
    {nullptr}
  };
  parser->add_group (entries,
		     "text",
		     "Text options:",
		     "Options for the input text",
		     this);
}

void
shape_text_options_t::add_options (option_parser_t *parser)
{
  text_options_t::add_options (parser);

  GOptionEntry entries[] =
  {
    {"text-before",	0, 0, G_OPTION_ARG_STRING,	&this->text_before,		"Set text context before each line",	"string"},
    {"text-after",	0, 0, G_OPTION_ARG_STRING,	&this->text_after,		"Set text context after each line",	"string"},
    {nullptr}
  };
  parser->add_group (entries,
		     "text-context",
		     "Textual context options:",
		     "Options for the input context text",
		     this);
}

#endif
