#include "server.h"

static void new_header(str_builder *builder, const char *key, const char *value) {
    builder_append_cstr(builder, key);
    builder_append_cstr(builder, value);
    builder_append_cstr(builder, "\r\n");
}

string cooltext = slit(
" _____    _     __    __          __ _  __ __ _ \\    / __ _ \n"
"(_  | |V||_)|  |_    /     \\    /|_ |_)(_ |_ |_) \\  / |_ |_)\n"
"__)_|_| ||  |__|__   \\__    \\/\\/ |__|_)__)|__| \\  \\/  |__| \\\n\n");

bool debug_route(Server *server, str_builder *resp, str_builder *headers)
{
    new_header(headers, "Content-Type: ", "text/html");

    builder_append_cstr(resp, "<pre>");
    builder_append_buf(resp, cooltext.cstr, cooltext.len);
    builder_append_cstr(resp, "</pre>"
                              "<h1>Small webserver written in pure C!</h1>"
                              "<h3>Routes binded:</h3>"
                              "<ul>");
    for (int i = 0; i < server->route_amt; i++)
    {
        builder_printf(resp, "<li><a href=\"%s\">%s</a></li>", server->routes[i].route.cstr, server->routes[i].route.cstr);
    }
    builder_append_cstr(resp, "</ul>"
                              "<h3>Currently cached files:</h3>"
                              "<ul>");
    for (int i = 0; i < CACHE_RING_BUFFER_LEN; i++)
    {
        if (server->cache[i].data) {
            builder_printf(resp, "<li>%s</li>", server->cache[i].path);
        }
    }
    builder_append_cstr(resp, "</ul>");

    return true;
}

bool flush_caches_route(Server *server, str_builder *resp, str_builder *headers)
{
    for (int i = 0; i < CACHE_RING_BUFFER_LEN; i++)
    {
        if (server->cache[i].data){
            free(server->cache[i].data);
            string_free(&server->cache[i].path);
            server->cache[i].data = NULL;
            server->cache[i].path = (string){0};
        }
    }
    
    builder_append_cstr(resp, 
        "<h1>Flushed cache</h1>"
        "<a href=\"/__debug__\">Debug panel</a>");

    return true;
}

bool route_html(Server *s, str_builder *resp, str_builder *headers)
{
    builder_append_cstr(resp, "<h1>Hello!</h1>");
    // Return simple html

    return true;
}

bool route_jpg(Server *s, str_builder *resp, str_builder *headers)
{
    new_header(headers, "Content-Type: ", "image/jpeg");

    char *ret; int64_t len;
    
    if (!read_file("www/among_drip.jpg", &ret, &len)) return false;
    // Will just read the file and not be cached.

    builder_append_buf(resp, ret, len);
    free(ret);

    return true;
}

bool ls_route(Server *server, str_builder *resp, str_builder *headers)
{
    struct dirent *file;
    
    DIR *dir = opendir(server->webroot.cstr);
    assert(dir != NULL); // its already checked in init.
    
    builder_append_cstr(resp, "<h1>Files and directories at the webroot:</h1>"
                              "<ul>");
    while ((file = readdir(dir)) != NULL){
        char *fname = file->d_name;
        if ( fname[0] == '\0' || 
           ( fname[0] == '.' && fname[1] == '\0') ||
           ( fname[0] == '.' && fname[1] == '.' && fname[2] == '\0')
           ) continue;

        builder_printf(resp, "<li><a href=\"/%s\">%s</a></li>", fname, fname);
    }
    builder_append_cstr(resp, "</ul>");

    closedir(dir);
    return true;
}

bool fail_route(Server *server, str_builder *resp, str_builder *headers)
{
    builder_append_cstr(resp, "<h1>This will never be displayed</h1>"
                              "<p>It will result in a 500 Internal Server Error status code</p>");
    return false;
}