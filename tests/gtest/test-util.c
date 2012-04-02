#include "test-util.h"

void
on_resource_unavailable_assert_not_reached (GSSDPResourceBrowser *src,
                                            const char           *usn,
                                            gpointer              user_data)
{
        g_assert_not_reached ();
}

void
on_resource_available_assert_not_reached (GSSDPResourceBrowser *src,
                                          const char           *usn,
                                          GList                *locations,
                                          gpointer              user_data)
{
        g_assert_not_reached ();
}

gboolean
quit_loop (gpointer user_data)
{
        g_main_loop_quit ((GMainLoop *) user_data);

        return FALSE;
}
