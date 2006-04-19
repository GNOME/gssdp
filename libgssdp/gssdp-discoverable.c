/*
 * Copyright (C) 2006 Garmin
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 */

#include <config.h>

#include "foinse-app.h"

G_DEFINE_TYPE (FoinseApp,
               foinse_app,
               GTK_TYPE_WINDOW);

enum {
        PROP_0,
        PROP_SCREEN
};

#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 272

static void
foinse_app_init (FoinseApp *app)
{
        /* Make sure we assume the right size initially, so that the WM
         * does not need to resize the window (which would cause extra
         * overhead) */
        gtk_widget_set_size_request (GTK_WIDGET (app),
                                     DISPLAY_WIDTH,
                                     DISPLAY_HEIGHT);
}

static void
foinse_app_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
        FoinseApp *app;

        app = FOINSE_APP (object);

        switch (property_id) {
        case PROP_SCREEN:
                foinse_app_set_screen (app, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
foinse_app_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
        FoinseApp *app;

        app = FOINSE_APP (object);

        switch (property_id) {
        case PROP_SCREEN:
                g_value_set_object (value, foinse_app_get_screen (app));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
foinse_app_class_init (FoinseAppClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = foinse_app_set_property;
	object_class->get_property = foinse_app_get_property;

        g_object_class_install_property
                (object_class,
                 PROP_SCREEN,
                 g_param_spec_object
                         ("screen",
                          "Screen",
                          "The current FoinseScreen.",
                          FOINSE_TYPE_SCREEN,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));
}

/**
 * foinse_app_get_instance
 *
 * Retrieves the #FoinseApp for the current process. The object is created
 * on first call.
 *
 * Return value: The #FoinseApp for the current process.
 **/
FoinseApp *
foinse_app_get_instance (void)
{
        static FoinseApp *app = NULL;

        if (!app)
                app = g_object_new (FOINSE_TYPE_APP, NULL);

        return app;
}

/**
 * foinse_app_set_screen
 * @app: A #FoinseApp
 * @screen: A #FoinseScreen, or NULL
 *
 * Sets the current #FoinseScreen to @screen.
 **/
void
foinse_app_set_screen (FoinseApp    *app,
                       FoinseScreen *screen)
{
        FoinseScreen *old_screen;

        g_return_if_fail (FOINSE_IS_APP (app));
        g_return_if_fail (FOINSE_IS_SCREEN (screen) || screen == NULL);

        old_screen = foinse_app_get_screen (app);
        if (old_screen == screen)
                return;

        if (old_screen) {
                gtk_container_remove (GTK_CONTAINER (app),
                                      GTK_WIDGET (old_screen));
        }

        if (screen) {
                gtk_container_add (GTK_CONTAINER (app),
                                   GTK_WIDGET (screen));

                gtk_widget_show (GTK_WIDGET (screen));
        }

        g_object_notify (G_OBJECT (app), "screen");
}

/**
 * foinse_app_get_screen
 * @app: A #FoinseApp
 *
 * Retrieves the current #FoinseScreen.
 *
 * Return value: The current #FoinseScreen, or NULL if none set.
 **/
FoinseScreen *
foinse_app_get_screen (FoinseApp *app)
{
        g_return_val_if_fail (FOINSE_IS_APP (app), NULL);

        return FOINSE_SCREEN (GTK_BIN (app)->child);
}
