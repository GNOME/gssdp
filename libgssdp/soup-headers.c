/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-headers.c: HTTP message header parsing
 *
 * Copyright (C) 2001-2003, Ximian, Inc.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "soup-headers.h"

/**
 * soup_str_case_hash:
 * @key: ASCII string to hash
 *
 * Hashes @key in a case-insensitive manner.
 *
 * Return value: the hash code.
 **/
static guint
soup_str_case_hash (gconstpointer key)
{
	const char *p = key;
	guint h = g_ascii_toupper(*p);

	if (h)
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + g_ascii_toupper(*p);

	return h;
}

/**
 * soup_str_case_equal:
 * @v1: an ASCII string
 * @v2: another ASCII string
 *
 * Compares @v1 and @v2 in a case-insensitive manner
 *
 * Return value: %TRUE if they are equal (modulo case)
 **/
static gboolean
soup_str_case_equal (gconstpointer v1,
		     gconstpointer v2)
{
	const char *string1 = v1;
	const char *string2 = v2;

	return g_ascii_strcasecmp (string1, string2) == 0;
}

/*
 * "HTTP/1.1 200 OK\r\nContent-Length: 1234\r\n          567\r\n\r\n"
 *                     ^             ^ ^    ^            ^   ^
 *                     |             | |    |            |   |
 *                    key            0 val  0          val+  0
 *                                         , <---memmove-...
 * 
 * key: "Content-Length"
 * val: "1234, 567"
 */
static gboolean
soup_headers_parse (char       *str, 
		    int         len, 
		    GHashTable *dest)
{
	char *key = NULL, *val = NULL, *end = NULL;
	int offset = 0, lws = 0;

	key = strstr (str, "\r\n");
	key += 2;

	/* join continuation headers, using a comma */
	while ((key = strstr (key, "\r\n"))) {
		key += 2;
		offset = key - str;

		if (!*key)
			break;

		/* check if first character on the line is whitespace */
		if (*key == ' ' || *key == '\t') {
			key -= 2;

			/* eat any trailing space from the previous line*/
			while (key [-1] == ' ' || key [-1] == '\t') key--;

			/* count how many characters are whitespace */
			lws = strspn (key, " \t\r\n");

			/* if continuation line, replace whitespace with ", " */
			if (key [-1] != ':') {
				lws -= 2;
				key [0] = ',';
				key [1] = ' ';
			}

			g_memmove (key, &key [lws], len - offset - lws);
		}
	}

	key = str;

	/* set eos for header key and value and add to hashtable */
        while ((key = strstr (key, "\r\n"))) {
		GSList *exist_hdrs;
		
		/* set end of last val, or end of http reason phrase */
                key [0] = '\0';
		key += 2;

		if (!*key)
			break;

                val = strchr (key, ':'); /* find start of val */

		if (!val || val > strchr (key, '\r'))
			break;

		/* set end of key */
		val [0] = '\0';
		
		val++;
		val += strspn (val, " \t");  /* skip whitespace */

		/* find the end of the value */
		end = strstr (val, "\r\n");
		if (!end)
			return FALSE;

		exist_hdrs = g_hash_table_lookup (dest, key);
		exist_hdrs = g_slist_append (exist_hdrs, 
					     g_strndup (val, end - val));

		if (!exist_hdrs->next)
			g_hash_table_insert (dest, g_strdup (key), exist_hdrs);

		key = end;
        }

	return TRUE;
}

gboolean
soup_headers_parse_request (char             *str, 
			    int               len, 
			    GHashTable       *dest, 
			    char            **req_method,
			    char            **req_path,
			    SoupHttpVersion  *ver) 
{
	gulong http_major, http_minor;
	char *s1, *s2, *cr, *p;

	if (!str || !*str)
		return FALSE;

	cr = memchr (str, '\r', len);
	if (!cr)
		return FALSE;

	s1 = memchr (str, ' ', cr - str);
	if (!s1)
		return FALSE;
	s2 = memchr (s1 + 1, ' ', cr - (s1 + 1));
	if (!s2)
		return FALSE;

	if (strncmp (s2, " HTTP/", 6) != 0)
		return FALSE;
	http_major = strtoul (s2 + 6, &p, 10);
	if (*p != '.')
		return FALSE;
	http_minor = strtoul (p + 1, &p, 10);
	if (p != cr)
		return FALSE;

	if (!soup_headers_parse (str, len, dest)) 
		return FALSE;

        if (req_method)
        	*req_method = g_strndup (str, s1 - str);

        if (req_path)
        	*req_path = g_strndup (s1 + 1, s2 - (s1 + 1));

	if (ver) {
		if (http_major == 1 && http_minor == 1) 
			*ver = SOUP_HTTP_1_1;
		else 
			*ver = SOUP_HTTP_1_0;
	}

	return TRUE;
}

gboolean
soup_headers_parse_status_line (const char       *status_line,
				SoupHttpVersion  *ver,
				guint            *status_code,
				char            **status_phrase)
{
	guint http_major, http_minor, code;
	guint phrase_start = 0;

	if (sscanf (status_line, 
		    "HTTP/%1u.%1u %3u %n", 
		    &http_major,
		    &http_minor,
		    &code, 
		    &phrase_start) < 3 || !phrase_start)
		return FALSE;

	if (ver) {
		if (http_major == 1 && http_minor == 1) 
			*ver = SOUP_HTTP_1_1;
		else 
			*ver = SOUP_HTTP_1_0;
	}

	if (status_code)
		*status_code = code;

	if (status_phrase)
		*status_phrase = g_strdup (status_line + phrase_start);

	return TRUE;
}

/**
 * soup_headers_parse_response:
 * @str: the header string (including the trailing blank line)
 * @len: length of @str
 * @dest: #GHashTable to store the header values in
 * @ver: on return, will contain the HTTP version
 * @status_code: on return, will contain the HTTP status code
 * @status_pharse: on return, will contain the status phrase
 *
 * Parses the headers of an HTTP response in @str and stores the
 * results in @ver, @status_code, @status_phrase, and @dest.
 *
 * Return value: success or failure.
 **/
gboolean
soup_headers_parse_response (char             *str, 
			     int               len, 
			     GHashTable       *dest,
			     SoupHttpVersion  *ver,
			     guint            *status_code,
			     char            **status_phrase)
{
	if (!str || !*str || len < sizeof ("HTTP/0.0 000 A\r\n\r\n"))
		return FALSE;

	if (!soup_headers_parse (str, len, dest)) 
		return FALSE;

	if (!soup_headers_parse_status_line (str, 
					     ver, 
					     status_code, 
					     status_phrase))
		return FALSE;

	return TRUE;
}


/*
 * HTTP parameterized header parsing
 */

char *
soup_header_param_copy_token (GHashTable *tokens, char *t)
{
	char *data;

	g_return_val_if_fail (tokens, NULL);
	g_return_val_if_fail (t, NULL);

	if ( (data = g_hash_table_lookup (tokens, t)))
		return g_strdup (data);
	else
		return NULL;
}

static void
decode_lwsp (char **in)
{
	char *inptr = *in;

	while (isspace (*inptr))
		inptr++;

	*in = inptr;
}

static char *
decode_quoted_string (char **in)
{
	char *inptr = *in;
	char *out = NULL, *outptr;
	int outlen;
	int c;

	decode_lwsp (&inptr);
	if (*inptr == '"') {
		char *intmp;
		int skip = 0;

                /* first, calc length */
                inptr++;
                intmp = inptr;
                while ( (c = *intmp++) && c != '"') {
                        if (c == '\\' && *intmp) {
                                intmp++;
                                skip++;
                        }
                }

                outlen = intmp - inptr - skip;
                out = outptr = g_malloc (outlen + 1);

                while ( (c = *inptr++) && c != '"') {
                        if (c == '\\' && *inptr) {
                                c = *inptr++;
                        }
                        *outptr++ = c;
                }
                *outptr = 0;
        }

        *in = inptr;

        return out;
}

char *
soup_header_param_decode_token (char **in)
{
	char *inptr = *in;
	char *start;

	decode_lwsp (&inptr);
	start = inptr;

	while (*inptr && *inptr != '=' && *inptr != ',')
		inptr++;

	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, inptr - start);
	}
	else
		return NULL;
}

static char *
decode_value (char **in)
{
	char *inptr = *in;

	decode_lwsp (&inptr);
	if (*inptr == '"')
		return decode_quoted_string (in);
	else
		return soup_header_param_decode_token (in);
}

GHashTable *
soup_header_param_parse_list (const char *header)
{
	char *ptr;
	gboolean added = FALSE;
	GHashTable *params = g_hash_table_new (soup_str_case_hash, 
					       soup_str_case_equal);

	ptr = (char *) header;
	while (ptr && *ptr) {
		char *name;
		char *value;

		name = soup_header_param_decode_token (&ptr);
		if (*ptr == '=') {
			ptr++;
			value = decode_value (&ptr);
			g_hash_table_insert (params, name, value);
			added = TRUE;
		}

		if (*ptr == ',')
			ptr++;
	}

	if (!added) {
		g_hash_table_destroy (params);
		params = NULL;
	}

	return params;
}

static void
destroy_param_hash_elements (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

void
soup_header_param_destroy_hash (GHashTable *table)
{
	g_hash_table_foreach (table, destroy_param_hash_elements, NULL);
	g_hash_table_destroy (table);
}
