/*

Copyright (c) 2014 Harm Hanemaaijer <fgenfb@yahoo.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/


bool X11OpenDisplay();
void X11CreateWindow(int width, int height, XVisualInfo *vi, const char *title);
void *X11GetDisplay();
long unsigned int X11GetWindow();
int X11GetScreenIndex();
void X11ToggleFullScreenMode(int &width, int& height, bool pan_with_mouse);
void X11DestroyWindow();
void X11CloseDisplay();
void X11ProcessGUIEvents();
double X11GetCurrentTime();
void X11ToggleFullScreenMode(int& width, int& height, bool pan_with_mouse);
void X11HideCursor();
void X11RestoreCursor();
void X11WarpCursor(int x, int y);
