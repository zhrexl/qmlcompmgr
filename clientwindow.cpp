#include "clientwindow.h"

#include "windowpixmap.h"

ClientWindow::ClientWindow(xcb_connection_t *connection, xcb_window_t window, QObject *parent)
    : QObject(parent),
      connection_(connection),
      window_(window),
      windowClass_(XCB_WINDOW_CLASS_COPY_FROM_PARENT),
      valid_(false),
      mapped_(false),
      pixmapRealloc_(true),
      above_(XCB_NONE)
{
    auto attributesCookie = xcb_get_window_attributes(connection_, window_);
    auto geometryCookie = xcb_get_geometry(connection_, window_);

    auto attributes = xcb_get_window_attributes_reply(connection_, attributesCookie, Q_NULLPTR);
    auto geometry = xcb_get_geometry_reply(connection_, geometryCookie, Q_NULLPTR);

    if (!attributes || !geometry) {
        std::free(attributes);
        std::free(geometry);
        return;
    }

    valid_ = true;
    windowClass_ = static_cast<xcb_window_class_t>(attributes->_class);
    geometry_ = QRect(geometry->x, geometry->y, geometry->width, geometry->height);
    mapped_ = (attributes->map_state == XCB_MAP_STATE_VIEWABLE);

    std::free(attributes);
    std::free(geometry);
}

ClientWindow::~ClientWindow()
{
}

const QSharedPointer<WindowPixmap> &ClientWindow::pixmap()
{
    if (!pixmapRealloc_ || !mapped_) {
        return pixmap_;
    }
    pixmapRealloc_ = false;
    QSharedPointer<WindowPixmap> newPixmap(new WindowPixmap(connection_, window_)); // TODO: replace with ::create
    if (newPixmap->isValid()) {
        pixmap_ = newPixmap;
        Q_EMIT pixmapChanged(pixmap_.data());
    }
    return pixmap_;
}

void ClientWindow::invalidate()
{
    if (valid_) {
        valid_ = false;
        Q_EMIT invalidated();
    }
}

void ClientWindow::setGeometry(const QRect &geometry)
{
    if (geometry_ != geometry) {
        if (geometry.size() != geometry_.size()) {
            pixmapRealloc_ = true;
        }
        geometry_ = geometry;
        Q_EMIT geometryChanged(geometry);
    }
}

void ClientWindow::setMapped(bool mapped)
{
    if (mapped_ != mapped) {
        mapped_ = mapped;
        Q_EMIT mapStateChanged(mapped);
    }
}

void ClientWindow::xcbEvent(const xcb_configure_notify_event_t *e)
{
    Q_ASSERT(e->window == window_);
    setGeometry(QRect(e->x, e->y, e->width, e->height));
    if (e->above_sibling != above_) {
        Q_EMIT stackingOrderChanged();
    }
}

void ClientWindow::xcbEvent(const xcb_map_notify_event_t *e)
{
    Q_ASSERT(e->window == window_);
    pixmapRealloc_ = true;
    setMapped(true);
}

void ClientWindow::xcbEvent(const xcb_unmap_notify_event_t *e)
{
    Q_ASSERT(e->window == window_);
    setMapped(false);
}

void ClientWindow::xcbEvent(const xcb_reparent_notify_event_t *e)
{
    Q_ASSERT(e->window == window_);
    setGeometry(QRect(e->x, e->y, geometry_.width(), geometry_.height()));
}

void ClientWindow::xcbEvent(const xcb_gravity_notify_event_t *e)
{
    Q_ASSERT(e->window == window_);
    setGeometry(QRect(e->x, e->y, geometry_.width(), geometry_.height()));
}
