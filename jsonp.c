#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "log.h"
#include "jsmn.h"
#include "parameters.h"

#define	MAX_JSON_TOKENS	256

typedef enum { START, KEY, VALUE, STOP } parse_state;

char * read_parameters(char *filename)
{
    char *buf=NULL;
    long length;

    FILE *fp = fopen(filename, "rb");
    if (fp) {
	fseek(fp, 0, SEEK_END);	
        length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
        if ( (length < MIN_PARAMETER_FILE_LEN) || (length > MAX_PARAMETER_FILE_LEN) ) {
	    fclose(fp);
	    return NULL;
	}

	buf = malloc(length);
	if (buf) {
	    fread(buf, 1, length, fp);
	    fclose(fp);
	    return buf;
	}
	fclose(fp);
	return NULL;
    }

    return NULL;
}

jsmntok_t * json_tokenise(char *js)
{
    jsmn_parser parser;
    jsmn_init(&parser);

    unsigned int n = MAX_JSON_TOKENS;
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * n);
    log_null(tokens);

    int ret = jsmn_parse(&parser, js, strlen(js), tokens, n);

    while (ret == JSMN_ERROR_NOMEM)
    {
        n = n * 2 + 1;
        tokens = realloc(tokens, sizeof(jsmntok_t) * n);
        log_null(tokens);
        ret = jsmn_parse(&parser, js, strlen(js), tokens, n);
    }

    if (ret == JSMN_ERROR_INVAL)
        log_die("jsmn_parse: invalid JSON string");
    if (ret == JSMN_ERROR_PART)
        log_die("jsmn_parse: truncated JSON string");

    return tokens;
}

int json_token_streq(char *js, jsmntok_t *t, char *s)
{
    return (strncmp(js + t->start, s, t->end - t->start) == 0
            && strlen(s) == (size_t) (t->end - t->start));
}

char * json_token_tostr(char *js, jsmntok_t *t)
{
    js[t->end] = '\0';
    return js + t->start;
}


int main(void) 
{
    int rc;
    size_t i, j, ii;
    char *jsonpBuf=NULL;
    jsmntok_t *tokens=NULL;
    parse_state state = START;
    size_t object_tokens = 0;

    jsonpBuf = read_parameters("./parameters.json");
    if (jsonpBuf == NULL)
        printf("READ parameter file failed\n");

    tokens = json_tokenise(jsonpBuf);
    for (i=0, j=1; j>0; i++,j--)
    {
        jsmntok_t *t = &tokens[i];

        if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT)
	    j += (t->size*2);

        switch (state) 
	   {
	    case START:
		if (t->type != JSMN_OBJECT)
		    log_die("Invalid format: root element must be an object.");

                state = KEY;
	        object_tokens = t->size;

		if (object_tokens == 0)
		    state = STOP;

		break;

        case KEY:
            if (t->type == JSMN_STRING) {
                   object_tokens--;
		   char *key = json_token_tostr(jsonpBuf, t);
    		   printf("%s: ", key);
		   state = VALUE;
		} else if (t->type == JSMN_OBJECT) {
		   state = KEY;
		   object_tokens += t->size;
		}
                break;

            case VALUE:
                if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE && \
		    t->type != JSMN_OBJECT && t->type != JSMN_ARRAY)
                    log_die("Invalid format: object values must be strings or primitives.");

		if (t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE) {
	            char *value = json_token_tostr(jsonpBuf, t);
                    puts(value);
		} else if ((t->type == JSMN_OBJECT) || (t->type == JSMN_ARRAY)) {		
		    object_tokens += t->size;		     
		}
		    
                state = KEY;

                if (object_tokens == 0)
                    state = STOP;
                break;

            case STOP:
                // Just consume the tokens
                break;

            default:
                log_die("Invalid state %u", state);
	   }
    }

    return 0;
}
