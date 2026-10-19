#pragma once

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

namespace tv {

/** Load the application TV icon, or an empty RefPtr if unavailable. */
Glib::RefPtr<Gdk::Pixbuf> load_app_icon(int size = 256);

/** Apply the TV icon as the default window icon for this process. */
void apply_app_icon();

}  // namespace tv
