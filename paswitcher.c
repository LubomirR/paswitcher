/*
The MIT License (MIT)

Copyright (c) 2014 Ľubomír Remák <lubomirr@lubomirr.eu>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/introspect.h>
#include <pulse/context.h>
#include <pulse/mainloop.h>

#define MAX_SOURCEOUTS 256

#define RET_SUCCESS         0
#define RET_MISSING_PARAMS  1
#define RET_ERROR           2

struct app_settings
{
    pa_mainloop * mainloop;
    const char * appname;
    uint32_t source;
    int num_sourceouts;
    uint32_t sourceouts[MAX_SOURCEOUTS];
};

static void context_success_callback(pa_context * context, int success, void * userdata)
{
    struct app_settings * apps = (struct app_settings *) userdata;

    /* Decrement the number of source outputs, so we know when we should quit the main loop. */
    apps->num_sourceouts--;

    if (apps->num_sourceouts == 0) {
        /* No more source outputs, flee! */
        pa_mainloop_quit(apps->mainloop, RET_SUCCESS);
    }
}

static void source_output_info_callback(pa_context * context, const pa_source_output_info * info, int eol, void * userdata)
{
    struct app_settings * apps = (struct app_settings *) userdata;

    if (!eol) {
        const char * appname = pa_proplist_gets(info->proplist, "application.name");
        if (strcmp(appname, apps->appname) == 0) {
            printf("Found source output '%s'.\n", info->name);
            /* This is our application. Add this source output to the list. */
            if (apps->num_sourceouts < MAX_SOURCEOUTS) {
                apps->sourceouts[apps->num_sourceouts++] = info->index;
            }
        }
    } else {
        /* Now change our source outputs' sources to a source chosen by the user. (This sounds so weird.) */
        if (apps->num_sourceouts > 0) {
            int i;
            for (i = 0; i < apps->num_sourceouts; i++) {
                pa_operation * op = pa_context_move_source_output_by_index(context, apps->sourceouts[i], apps->source, context_success_callback, apps);
                pa_operation_unref(op);
            }
        } else {
            /* Or quit if there's nothing to do. */
            pa_mainloop_quit(apps->mainloop, RET_SUCCESS);
        }
    }
}

static void context_state_callback(pa_context * context, void * userdata)
{
    struct app_settings * apps = (struct app_settings *) userdata;

    /* Get our context's current state. */
    switch (pa_context_get_state(context)) {
        /* The context is ready for our magic. */
        case PA_CONTEXT_READY:
            {
                /* Create a new operation for getting source */
                pa_operation * op = pa_context_get_source_output_info_list(context, source_output_info_callback, apps);
                pa_operation_unref(op);
            }

            break;

        /* The context isn't ready for anything, because the connection failed. :-( */
        case PA_CONTEXT_FAILED:
            printf("Connection failed!\n");
            pa_mainloop_quit(apps->mainloop, RET_ERROR);
            break;

        /* Ignore other states. */
        default:
            break;
    }
}

static void print_usage()
{
    fprintf(stderr, "paswitcher appname #source\n");
}

int main(int argc, char * argv[])
{
    if (argc != 3) {
        print_usage();
        return RET_MISSING_PARAMS;
    }

    struct app_settings apps;

    apps.appname = argv[1];
    apps.source = atoi(argv[2]);
    apps.num_sourceouts = 0;

    /* Create a new event loop. */
    pa_mainloop * loop = pa_mainloop_new();
    apps.mainloop = loop;

    /* Create a new context from this loop. */
    pa_context * context = pa_context_new(pa_mainloop_get_api(loop), "paswitcher");

    /* Set event callback for waiting until the connection is established. */
    pa_context_set_state_callback(context, context_state_callback, &apps);

    /* Connect to the local server. */
    pa_context_connect(context, NULL, 0, NULL);

    int retval;
    /* Run the event loop. */
    pa_mainloop_run(loop, &retval);

    /* Disconnect from the server. */
    pa_context_disconnect(context);

    /* Destroy the event loop. */
    pa_mainloop_free(loop);

    return retval;

    /* And then all this beauty vanished before my very eyes... */
}
