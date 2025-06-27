#pragma once

void cef_view_load(void);
void cef_view_unload(void);

void cef_view_open_url(const char *url_str);
void cef_view_resize_browser(int width, int height);
