/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Pasteboard.h"

#include "CString.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "Image.h"
#include "RenderImage.h"
#include "KURL.h"
#include "markup.h"

#include <gtk/gtk.h>

namespace WebCore {

/* FIXME: we must get rid of this and use the enum in webkitwebview.h someway */
typedef enum
{
    WEBKIT_WEB_VIEW_TARGET_INFO_HTML = - 1,
    WEBKIT_WEB_VIEW_TARGET_INFO_TEXT = - 2
} WebKitWebViewTargetInfo;

class PasteboardSelectionData {
public:
    PasteboardSelectionData(gchar* text, gchar* markup)
        : m_text(text)
        , m_markup(markup) { }

    ~PasteboardSelectionData() {
        g_free(m_text);
        g_free(m_markup);
    }

    const gchar* text() const { return m_text; }
    const gchar* markup() const { return m_markup; }

private:
    gchar* m_text;
    gchar* m_markup;
};

static void clipboard_get_contents_cb(GtkClipboard *clipboard, GtkSelectionData *selection_data,
                                      guint info, gpointer data) {
    PasteboardSelectionData* clipboardData = reinterpret_cast<PasteboardSelectionData*>(data);
    ASSERT(clipboardData);
    if ((gint)info == WEBKIT_WEB_VIEW_TARGET_INFO_HTML) {
        gtk_selection_data_set(selection_data, selection_data->target, 8,
                               reinterpret_cast<const guchar*>(clipboardData->markup()),
                               g_utf8_strlen(clipboardData->markup(), -1));
    } else
        gtk_selection_data_set_text(selection_data, clipboardData->text(), -1);
}

static void clipboard_clear_contents_cb(GtkClipboard *clipboard, gpointer data) {
    PasteboardSelectionData* clipboardData = reinterpret_cast<PasteboardSelectionData*>(data);
    ASSERT(clipboardData);
    delete clipboardData;
}


Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
    notImplemented();
}

Pasteboard::~Pasteboard()
{
    delete m_helper;
}

void Pasteboard::setHelper(PasteboardHelper* helper)
{
    m_helper = helper;
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    GtkClipboard* clipboard = m_helper->getClipboard(frame);
#if GTK_CHECK_VERSION(2,10,0)
    gchar* text = g_strdup(frame->selectedText().utf8().data());
    gchar* markup = g_strdup(createMarkup(selectedRange, 0, AnnotateForInterchange).utf8().data());
    PasteboardSelectionData* data = new PasteboardSelectionData(text, markup);

    gint n_targets;
    GtkTargetEntry* targets = gtk_target_table_new_from_list(m_helper->getCopyTargetList(frame), &n_targets);
    gtk_clipboard_set_with_data(clipboard, targets, n_targets,
                                clipboard_get_contents_cb, clipboard_clear_contents_cb, data);
    gtk_target_table_free(targets, n_targets);
#else
    gtk_clipboard_set_text(clipboard, frame->selectedText().utf8().data(), frame->selectedText().utf8().length());
#endif
}

void Pasteboard::writeURL(const KURL& url, const String&, Frame* frame)
{
    if (url.isEmpty())
        return;

    GtkClipboard* clipboard = m_helper->getClipboard(frame);
    gtk_clipboard_set_text(clipboard, url.string().utf8().data(), url.string().utf8().length());
}

void Pasteboard::writeImage(Node* node, const KURL&, const String&)
{
    // TODO: Enable this when Image gets GdkPixbuf support

    /*
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    ASSERT(node && node->renderer() && node->renderer()->isImage());
    RenderImage* renderer = static_cast<RenderImage*>(node->renderer());
    CachedImage* cachedImage = static_cast<CachedImage*>(renderer->cachedImage());
    ASSERT(cachedImage);
    Image* image = cachedImage->image();
    ASSERT(image);

    gtk_clipboard_set_image(clipboard, image->pixbuf());
    */

    notImplemented();
}

void Pasteboard::clear()
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    gtk_clipboard_clear(clipboard);
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
#if GTK_CHECK_VERSION(2,10,0)
    GdkAtom textHtml = gdk_atom_intern_static_string("text/html");
#else
    GdkAtom textHtml = gdk_atom_intern("text/html", false);
#endif
    GtkClipboard* clipboard = m_helper->getClipboard(frame);
    chosePlainText = false;

    if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, textHtml)) {
        ASSERT(data->data);
        String html = String::fromUTF8(reinterpret_cast<gchar*>(data->data), data->length * data->format / 8);
        gtk_selection_data_free(data);

        if (!html.isEmpty()) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), html, "");
            if (fragment)
                return fragment.release();
        }
    }

    if (!allowPlainText)
        return 0;

    if (gchar* utf8 = gtk_clipboard_wait_for_text(clipboard)) {
        String text = String::fromUTF8(utf8);
        g_free(utf8);

        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), text);
        if (fragment)
            return fragment.release();
    }

    return 0;
}

String Pasteboard::plainText(Frame* frame)
{
    GtkClipboard* clipboard = m_helper->getClipboard(frame);

    gchar* utf8 = gtk_clipboard_wait_for_text(clipboard);

    if (!utf8)
        return String();

    String text = String::fromUTF8(utf8);
    g_free(utf8);

    return text;
}

}
