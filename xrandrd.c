
/*
xrandrd - try to do the right thing without user interaction if a display gets (dis-)connected
Written in 2013 by <Ahmet Inan> <ainan@mathematik.uni-freiburg.de>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <X11/extensions/Xrandr.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

char *con_str(Connection connection)
{
	switch (connection) {
		case RR_Connected: return "connected";
		case RR_Disconnected: return "disconnected";
		case RR_UnknownConnection: return "unknown connection";
		default: return "dunno";
	}
}

char *mode_str(XRRScreenResources *sr, RRMode mode)
{
	if (!mode)
		return "disabled";
	for (int i = 0; i < sr->nmode; i++)
		if (sr->modes[i].id == mode)
			return sr->modes[i].name;
	return "dunno";
}

void output_info(Display *display, XRRScreenResources *sr, RROutput output)
{
	XRROutputInfo *oi = XRRGetOutputInfo(display, sr, output);
	char *mode = "disabled";
	if (oi->crtc) {
		XRRCrtcInfo *ci = XRRGetCrtcInfo(display, sr, oi->crtc);
		mode = mode_str(sr, ci->mode);
		XRRFreeCrtcInfo(ci);
	}
	fprintf(stderr, "xrandrd: %s %s %s", oi->name, con_str(oi->connection), mode);
	for (int i = 0; i < oi->nmode; i++)
		fprintf(stderr, " %s", mode_str(sr, oi->modes[i]));
	fprintf(stderr, "\n");

	XRRFreeOutputInfo(oi);
}

void current_state(Display *display, Window window)
{
	XRRScreenResources *sr = XRRGetScreenResources(display, window);
	for (int i = 0; i < sr->noutput; i++)
		output_info(display, sr, sr->outputs[i]);
	XRRFreeScreenResources(sr);
}

int get_wxh(XRRScreenResources *sr, RRMode mode, unsigned int *width, unsigned int *height)
{
	if (!mode)
		return 0;
	for (int i = 0; i < sr->nmode; i++) {
		if (sr->modes[i].id == mode) {
			*width = sr->modes[i].width;
			*height = sr->modes[i].height;
			return 1;
		}
	}
	return 0;
}

int has_wxh_preferred(XRRScreenResources *sr, XRROutputInfo *oi, unsigned int width, unsigned int height)
{
	unsigned int last_w = -1, last_h = -1;
	for (int i = 0; i < oi->nmode; i++) {
		unsigned int w, h;
		if (!get_wxh(sr, oi->modes[i], &w, &h))
			return 0;
		if (last_w < w && last_h < h)
			return 0;
		if (w == width && h == height)
			return 1;
		last_w = w;
		last_h = h;
	}
	return 0;
}

XRROutputInfo *first_connected(XRRScreenResources *sr, XRROutputInfo **ois)
{
	for (int i = 0; i < sr->noutput; i++)
		if (ois[i]->connection == RR_Connected)
			return ois[i];
	return 0;
}

int num_connected(XRRScreenResources *sr, XRROutputInfo **ois)
{
	int counter = 0;
	for (int i = 0; i < sr->noutput; i++)
		if (ois[i]->connection == RR_Connected)
			counter++;
	return counter;
}

int preferred_common_mode(Display *display, Window window, unsigned int *width, unsigned int *height)
{
	XRRScreenResources *sr = XRRGetScreenResources(display, window);
	XRROutputInfo **ois = malloc(sizeof(XRROutputInfo *) * sr->noutput);
	for (int i = 0; i < sr->noutput; i++)
		ois[i] = XRRGetOutputInfo(display, sr, sr->outputs[i]);
	int num = num_connected(sr, ois);
	if (num <= 1) {
		for (int i = 0; i < sr->noutput; i++)
			XRRFreeOutputInfo(ois[i]);
		free(ois);
		XRRFreeScreenResources(sr);
		return 0;
	}
	XRROutputInfo *first = first_connected(sr, ois);
	int counter = 0;
	unsigned int last_w = -1, last_h = -1;
	for (int k = 0; k < first->nmode; k++) {
		counter = 1;
		if (!get_wxh(sr, first->modes[k], width, height))
			exit(1);
		if (last_w < *width && last_h < *height)
			return 0;
		last_w = *width;
		last_h = *height;
		for (int j = 0; j < sr->noutput; j++) {
			if (ois[j]->connection != RR_Connected)
				continue;
			if (ois[j] == first)
				continue;
			if (has_wxh_preferred(sr, ois[j], *width, *height))
				counter++;
		}
		if (num == counter)
			break;
	}

	for (int i = 0; i < sr->noutput; i++)
		XRRFreeOutputInfo(ois[i]);
	free(ois);
	XRRFreeScreenResources(sr);
	return num == counter;
}

int is_wxh(Display *display, XRRScreenResources *sr, XRROutputInfo *oi, unsigned int width, unsigned int height)
{
	if (!oi->crtc)
		return 0;
	XRRCrtcInfo *ci = XRRGetCrtcInfo(display, sr, oi->crtc);
	if (!width || !height) {
		int tmp = ci->mode == oi->modes[0];
		XRRFreeCrtcInfo(ci);
		return tmp;
	}
	int tmp = 0;
	for (int i = 0; i < sr->nmode; i++) {
		if (sr->modes[i].id == ci->mode) {
			if (width == sr->modes[i].width && height == sr->modes[i].height)
				tmp = 1;
			break;
		}
	}
	XRRFreeCrtcInfo(ci);
	return tmp;
}

void set_mode(Display *display, Window window, unsigned int width, unsigned int height)
{
	char mode[32] = "--auto";
	if (width && height)
		snprintf(mode, 32, "--mode %dx%d", width, height);
	char str[4096] = "xrandr";
	size_t before = strlen(str);
	XRRScreenResources *sr = XRRGetScreenResources(display, window);
	for (int i = 0; i < sr->noutput; i++) {
		XRROutputInfo *oi = XRRGetOutputInfo(display, sr, sr->outputs[i]);
		if (oi->connection == RR_Connected && !is_wxh(display, sr, oi, width, height))
			snprintf(str + strlen(str), sizeof(str) - strlen(str) - 1, " --output %s %s", oi->name, mode);
		else if (oi->connection == RR_Disconnected && oi->crtc)
			snprintf(str + strlen(str), sizeof(str) - strlen(str) - 1, " --output %s --off", oi->name);
		XRRFreeOutputInfo(oi);
	}
	XRRFreeScreenResources(sr);
	size_t after = strlen(str);
	if (before == after) {
		fprintf(stderr, "xrandrd: configuration ok, nothing to be done.\n");
		return;
	}
	if (system(str))
		fprintf(stderr, "xrandrd: executing \"%s\" failed!\n", str);
}

void lets_rock(Display *display, Window window)
{
	unsigned int width, height;
	if (preferred_common_mode(display, window, &width, &height)) {
		fprintf(stderr, "xrandrd: setting common mode: %dx%d\n", width, height);
		set_mode(display, window, width, height);
	} else {
		fprintf(stderr, "xrandrd: no common mode, setting auto.\n");
		set_mode(display, window, 0, 0);
	}
}

int main()
{
	Display *display = XOpenDisplay(0);
	if (!display) {
		fprintf(stderr, "xrandrd: could not open display \"%s\"\n", XDisplayName(0));
		return 1;
	}

	int event_base_return, error_base_return;
	if (!XRRQueryExtension(display, &event_base_return, &error_base_return)) {
		XCloseDisplay(display);
		fprintf(stderr, "xrandrd: you do not have the Xrandr extension!\n");
		return 1;
	}

	Window root = DefaultRootWindow(display);
	current_state(display, root);
	lets_rock(display, root);
	XRRSelectInput(display, root, RROutputChangeNotifyMask);

	int wait_seconds = 3;
	int countdown = 0;
	while (1) {
		XEvent event;
		if (!countdown || XPending(display)) {
			XNextEvent(display, &event);
			countdown = wait_seconds;
		} else {
			if (--countdown) {
				sleep(1);
			} else {
				fprintf(stderr, "xrandrd: %d seconds elapsed since last event.\n", wait_seconds);
				lets_rock(display, root);
			}
			continue;
		}
		if ((event.type-event_base_return) != RRNotify) {
			fprintf(stderr, "xrandrd: Ignoring event type %d\n", event.type);
			continue;
		}
		XRRNotifyEvent *ne = (XRRNotifyEvent *)&event;
		if (ne->subtype != RRNotify_OutputChange) {
			fprintf(stderr, "xrandrd: Ignoring subtype %d\n", ne->subtype);
			continue;
		}
		XRROutputChangeNotifyEvent *ocne = (XRROutputChangeNotifyEvent *)&event;
//		fprintf(stderr, "xrandrd: output: %lu connection: %d crtc: %lu mode: %lu\n", ocne->output, ocne->connection, ocne->crtc, ocne->mode);
		XRRScreenResources *sr = XRRGetScreenResources(display, ne->window);
		output_info(display, sr, ocne->output);
		XRRFreeScreenResources(sr);
	}
	XCloseDisplay(display);

	return 0;
}

