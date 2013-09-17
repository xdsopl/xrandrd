
/*
xrandrd - try to do the right thing without user interaction if a display gets (dis-)connected
Written in 2013 by <Ahmet Inan> <ainan@mathematik.uni-freiburg.de>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <X11/extensions/Xrandr.h>
#include <stdio.h>

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
	fprintf(stderr, "%s %s %s", oi->name, con_str(oi->connection), mode);
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

int main()
{
	Display *display = XOpenDisplay(0);
	if (!display) {
		fprintf(stderr, "could not open display \"%s\"\n", XDisplayName(0));
		return 1;
	}

	int event_base_return, error_base_return;
	if (!XRRQueryExtension(display, &event_base_return, &error_base_return)) {
		XCloseDisplay(display);
		fprintf(stderr, "you do not have the Xrandr extension!\n");
		return 1;
	}

	Window root = DefaultRootWindow(display);
	current_state(display, root);
	XRRSelectInput(display, root, RROutputChangeNotifyMask);

	while (1) {
		XEvent event;
		XNextEvent(display, &event);
		if ((event.type-event_base_return) != RRNotify) {
			fprintf(stderr, "Ignoring event type %d\n", event.type);
			continue;
		}
		XRRNotifyEvent *ne = (XRRNotifyEvent *)&event;
		if (ne->subtype != RRNotify_OutputChange) {
			fprintf(stderr, "Ignoring subtype %d\n", ne->subtype);
			continue;
		}
		XRROutputChangeNotifyEvent *ocne = (XRROutputChangeNotifyEvent *)&event;
//		fprintf(stderr, "output: %lu connection: %d crtc: %lu mode: %lu\n", ocne->output, ocne->connection, ocne->crtc, ocne->mode);
		XRRScreenResources *sr = XRRGetScreenResources(display, ne->window);
		output_info(display, sr, ocne->output);
		XRRFreeScreenResources(sr);
	}
	XCloseDisplay(display);

	return 0;
}

