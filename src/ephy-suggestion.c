/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright © 2017 Igalia S.L.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-suggestion.h"

#include <dazzle.h>
#include <glib.h>
#include <string.h>

struct _EphySuggestion {
  DzlSuggestion parent;
};

G_DEFINE_TYPE (EphySuggestion, ephy_suggestion, DZL_TYPE_SUGGESTION)

char *
ephy_suggestion_suggest_suffix (DzlSuggestion *self,
                                const char    *typed_text)
{
  const char *suffix;
  const char *url;

  if (typed_text == NULL || strlen (typed_text) == 0 ||
      g_str_has_prefix ("www.", typed_text) ||
      g_str_has_prefix ("http://www.", typed_text) ||
      g_str_has_prefix ("https://www.", typed_text))
    return NULL;

  url = dzl_suggestion_get_id (self);
  suffix = strstr (url, typed_text);

  return suffix ? g_strdup (&suffix[strlen (typed_text)]) : NULL;
}

char *
ephy_suggestion_replace_typed_text (DzlSuggestion *self,
                                    const char    *typed_text)
{
  const char *url = dzl_suggestion_get_id (self);

  return g_strdup (url);
}

static void
ephy_suggestion_class_init (EphySuggestionClass *klass)
{
  DzlSuggestionClass *dzl_suggestion_class = DZL_SUGGESTION_CLASS (klass);

  dzl_suggestion_class->suggest_suffix = ephy_suggestion_suggest_suffix;
  dzl_suggestion_class->replace_typed_text = ephy_suggestion_replace_typed_text;
}

static void
ephy_suggestion_init (EphySuggestion *self)
{
}

EphySuggestion *
ephy_suggestion_new (const char *title,
                     const char *uri)
{
  EphySuggestion *suggestion;
  char *escaped_title = g_markup_escape_text (title, -1);
  char *escaped_uri = g_markup_escape_text (uri, -1);

  suggestion = g_object_new (EPHY_TYPE_SUGGESTION,
                             "icon-name", "web-browser-symbolic",
                             "id", uri,
                             "title", escaped_title,
                             "subtitle", escaped_uri,
                             NULL);
  g_free (escaped_title);
  g_free (escaped_uri);

  return suggestion;
}